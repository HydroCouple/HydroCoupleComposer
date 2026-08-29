#include "mesh/meshdomain.h"

#include <QJsonArray>
#include <QLineF>
#include <QObject>

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    namespace Tools = HydroCouple::SDK::Tools;
    namespace Spatial = HydroCouple::SDK::Spatial;

    //! Rings smaller than this in area are treated as having none.
    constexpr double kAreaEpsilon = 1e-12;

    //! Whether every coordinate of \a points is a real number.
    [[nodiscard]] bool allFinite(const QPolygonF &points)
    {
      return std::all_of(points.begin(), points.end(),
                         [](const QPointF &point)
                         {
                           return std::isfinite(point.x())
                                  && std::isfinite(point.y());
                         });
    }

    //! \a ring as SDK points, first point not repeated.
    [[nodiscard]] std::vector<Spatial::Point> toPoints(const QPolygonF &ring)
    {
      std::vector<Spatial::Point> points;
      points.reserve(static_cast<size_t>(ring.size()));

      for (const QPointF &point : ring)
      {
        points.emplace_back(point.x(), point.y());
      }

      return points;
    }

    [[nodiscard]] QJsonArray ringToJson(const QPolygonF &ring)
    {
      QJsonArray array;

      for (const QPointF &point : ring)
      {
        // A flat pair per vertex rather than {"x":,"y":}: a boundary of ten
        // thousand points is a file somebody has to open.
        array.append(QJsonArray({point.x(), point.y()}));
      }

      return array;
    }

    [[nodiscard]] bool ringFromJson(const QJsonValue &value, QPolygonF &ring,
                                    QString &message)
    {
      ring.clear();

      if (!value.isArray())
      {
        message = QObject::tr("A ring must be an array of points.");
        return false;
      }

      const QJsonArray array = value.toArray();

      for (const QJsonValue &entry : array)
      {
        const QJsonArray pair = entry.toArray();

        if (pair.size() != 2 || !pair.at(0).isDouble()
            || !pair.at(1).isDouble())
        {
          message = QObject::tr("A point must be a pair of numbers.");
          ring.clear();
          return false;
        }

        ring.append(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
      }

      return true;
    }
  }

  double signedDoubleArea(const QPolygonF &ring)
  {
    if (ring.size() < 3)
    {
      return 0.0;
    }

    double total = 0.0;

    // The shoelace sum, closing the ring implicitly: the caller's rings do
    // not repeat their first point, and adding one to compute an area would
    // mean every caller had to remember to take it off again.
    for (int index = 0; index < ring.size(); ++index)
    {
      const QPointF &here = ring.at(index);
      const QPointF &next = ring.at((index + 1) % ring.size());

      total += here.x() * next.y() - next.x() * here.y();
    }

    return total;
  }

  bool MeshDomain::isEmpty() const
  {
    return boundary.isEmpty() && holes.isEmpty() && constraintLines.isEmpty()
           && points.isEmpty();
  }

  bool MeshDomain::isValid(QString &message) const
  {
    if (boundary.size() < 3)
    {
      message = QObject::tr("The boundary needs at least three points; it "
                            "has %1.")
                  .arg(boundary.size());
      return false;
    }

    if (!allFinite(boundary))
    {
      message = QObject::tr("The boundary has a point that is not a number.");
      return false;
    }

    // Area, not the bounding box. Three collinear points have a perfectly
    // good box, three vertices, and no ground inside them at all — and a
    // triangulator handed one produces nothing while looking as though it
    // was given something.
    if (std::abs(signedDoubleArea(boundary)) < kAreaEpsilon)
    {
      message = QObject::tr("The boundary encloses no area — its points are "
                            "in a line.");
      return false;
    }

    for (int index = 0; index < holes.size(); ++index)
    {
      const QPolygonF &hole = holes.at(index);

      if (hole.size() < 3 || !allFinite(hole)
          || std::abs(signedDoubleArea(hole)) < kAreaEpsilon)
      {
        message = QObject::tr("Hole %1 encloses no area.").arg(index + 1);
        return false;
      }

      // A hole outside the boundary is not degenerate, it is simply wrong:
      // it cuts nothing, so the mesh comes out whole and the user is left
      // looking at a domain that did not do what they drew.
      for (const QPointF &corner : hole)
      {
        if (!boundary.containsPoint(corner, Qt::OddEvenFill))
        {
          message = QObject::tr("Hole %1 lies outside the boundary, so it "
                                "would cut nothing.")
                      .arg(index + 1);
          return false;
        }
      }
    }

    for (int index = 0; index < constraintLines.size(); ++index)
    {
      const QPolygonF &line = constraintLines.at(index);

      if (line.size() < 2 || !allFinite(line))
      {
        message = QObject::tr("Breakline %1 needs at least two points.")
                    .arg(index + 1);
        return false;
      }

      // Length, so a line whose vertices all sit on one spot is caught. It
      // has the point count a line needs and no direction to constrain
      // anything along.
      const double length =
        QLineF(line.first(), line.last()).length();

      if (length <= 0.0 && line.size() == 2)
      {
        message = QObject::tr("Breakline %1 begins and ends at the same "
                              "point.")
                    .arg(index + 1);
        return false;
      }
    }

    for (const QPointF &point : points)
    {
      if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
      {
        message = QObject::tr("An interior point is not a number.");
        return false;
      }
    }

    if (!std::isfinite(maxEdgeLength) || maxEdgeLength < 0.0)
    {
      message = QObject::tr("The maximum edge length cannot be negative.");
      return false;
    }

    message.clear();
    return true;
  }

  Tools::TriangulationInput MeshDomain::toTriangulationInput() const
  {
    Tools::TriangulationInput input;

    // Counter-clockwise, as the triangulator requires. A ring drawn the
    // other way round is not an error the user should have to hear about —
    // it is the same ground, walked the other way.
    QPolygonF oriented = boundary;

    if (signedDoubleArea(oriented) < 0.0)
    {
      std::reverse(oriented.begin(), oriented.end());
    }

    input.boundary = toPoints(oriented);

    for (const QPolygonF &hole : holes)
    {
      input.holes.push_back(toPoints(hole));
    }

    // Polylines exploded into the pairs the triangulator takes.
    for (const QPolygonF &line : constraintLines)
    {
      for (int index = 0; index + 1 < line.size(); ++index)
      {
        input.constraintSegments.push_back(
          {Spatial::Point(line.at(index).x(), line.at(index).y()),
           Spatial::Point(line.at(index + 1).x(), line.at(index + 1).y())});
      }
    }

    for (const QPointF &point : points)
    {
      input.interiorPoints.emplace_back(point.x(), point.y());
    }

    input.maxEdgeLength = maxEdgeLength;

    return input;
  }

  QJsonObject MeshDomain::toJson() const
  {
    QJsonObject root;
    root.insert(QStringLiteral("boundary"), ringToJson(boundary));

    QJsonArray holeArray;

    for (const QPolygonF &hole : holes)
    {
      holeArray.append(ringToJson(hole));
    }

    root.insert(QStringLiteral("holes"), holeArray);

    QJsonArray lineArray;

    for (const QPolygonF &line : constraintLines)
    {
      lineArray.append(ringToJson(line));
    }

    root.insert(QStringLiteral("constraint_lines"), lineArray);

    QPolygonF asRing;

    for (const QPointF &point : points)
    {
      asRing.append(point);
    }

    root.insert(QStringLiteral("points"), ringToJson(asRing));
    root.insert(QStringLiteral("max_edge_length"), maxEdgeLength);

    return root;
  }

  bool MeshDomain::fromJson(const QJsonObject &json, MeshDomain &domain,
                            QString &message)
  {
    domain = MeshDomain{};

    if (!ringFromJson(json.value(QStringLiteral("boundary")), domain.boundary,
                      message))
    {
      return false;
    }

    const QJsonArray holes = json.value(QStringLiteral("holes")).toArray();

    for (const QJsonValue &entry : holes)
    {
      QPolygonF hole;

      if (!ringFromJson(entry, hole, message))
      {
        domain = MeshDomain{};
        return false;
      }

      domain.holes.append(hole);
    }

    const QJsonArray lines =
      json.value(QStringLiteral("constraint_lines")).toArray();

    for (const QJsonValue &entry : lines)
    {
      QPolygonF line;

      if (!ringFromJson(entry, line, message))
      {
        domain = MeshDomain{};
        return false;
      }

      domain.constraintLines.append(line);
    }

    QPolygonF interior;

    if (!ringFromJson(json.value(QStringLiteral("points")), interior, message))
    {
      domain = MeshDomain{};
      return false;
    }

    for (const QPointF &point : interior)
    {
      domain.points.append(point);
    }

    // Absent rather than zero is how an older sidecar reads, so a domain
    // written before this field existed keeps the triangulator's own
    // default instead of acquiring a refinement nobody asked for.
    const QJsonValue edge = json.value(QStringLiteral("max_edge_length"));
    domain.maxEdgeLength = edge.isDouble() ? edge.toDouble() : 0.0;

    message.clear();
    return true;
  }

} // namespace HydroCouple::Composer
