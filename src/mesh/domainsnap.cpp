#include "mesh/domainsnap.h"

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Squared distance, so the search never takes a root it does not need.
    [[nodiscard]] double squaredDistance(const QPointF &left,
                                         const QPointF &right)
    {
      const double dx = left.x() - right.x();
      const double dy = left.y() - right.y();

      return dx * dx + dy * dy;
    }

    /*!
     * \brief The point on segment \a from - \a to nearest \a world.
     *
     * Clamped to the segment: the nearest point on the infinite line through
     * two vertices can lie a long way off the end of the edge that was
     * actually drawn, and inserting a vertex there would move the shape
     * somewhere nobody clicked.
     */
    [[nodiscard]] QPointF closestOnSegment(const QPointF &from,
                                           const QPointF &to,
                                           const QPointF &world)
    {
      const double dx = to.x() - from.x();
      const double dy = to.y() - from.y();
      const double lengthSquared = dx * dx + dy * dy;

      if (lengthSquared <= 0.0)
      {
        return from;
      }

      double along = ((world.x() - from.x()) * dx
                      + (world.y() - from.y()) * dy)
                     / lengthSquared;

      along = std::clamp(along, 0.0, 1.0);

      return QPointF(from.x() + along * dx, from.y() + along * dy);
    }

    //! The shapes of \a part that have edges, in address order.
    [[nodiscard]] QVector<QPolygonF> edgedShapes(const MeshDomain &domain,
                                                 DomainPart part)
    {
      switch (part)
      {
        case DomainPart::Boundary:
          return domain.boundary.isEmpty() ? QVector<QPolygonF>()
                                           : QVector<QPolygonF>{
                                               domain.boundary};

        case DomainPart::Holes:
          return domain.holes;

        case DomainPart::Breaklines:
          return domain.constraintLines;

        case DomainPart::ForcedPoints:
          return {};
      }

      return {};
    }
  }

  double worldTolerance(double pixels, double scale)
  {
    if (scale <= 0.0 || !std::isfinite(scale))
    {
      return 0.0;
    }

    return pixels / scale;
  }

  QVector<QPointF> domainVertices(const MeshDomain &domain,
                                  QVector<DomainVertex> &addresses)
  {
    QVector<QPointF> vertices;
    addresses.clear();

    for (const DomainPart part :
         {DomainPart::Boundary, DomainPart::Holes, DomainPart::Breaklines,
          DomainPart::ForcedPoints})
    {
      const int shapes = shapeCount(domain, part);

      for (int shape = 0; shape < shapes; ++shape)
      {
        const int count = shapeVertexCount(domain, part, shape);

        for (int vertex = 0; vertex < count; ++vertex)
        {
          const DomainVertex at{part, shape, vertex};
          QPointF position;

          if (vertexPosition(domain, at, position))
          {
            addresses.append(at);
            vertices.append(position);
          }
        }
      }
    }

    return vertices;
  }

  DomainSnap nearestVertex(const MeshDomain &domain, const QPointF &world,
                           double tolerance, const DomainVertex &exclude)
  {
    DomainSnap result;

    if (tolerance <= 0.0)
    {
      return result;
    }

    QVector<DomainVertex> addresses;
    const QVector<QPointF> vertices = domainVertices(domain, addresses);

    double best = tolerance * tolerance;

    for (int index = 0; index < vertices.size(); ++index)
    {
      if (addresses.at(index) == exclude)
      {
        continue;
      }

      const double distance = squaredDistance(vertices.at(index), world);

      if (distance <= best)
      {
        best = distance;
        result.hit = true;
        result.point = vertices.at(index);
        result.at = addresses.at(index);
      }
    }

    return result;
  }

  DomainSnap nearestEdge(const MeshDomain &domain, const QPointF &world,
                         double tolerance)
  {
    DomainSnap result;

    if (tolerance <= 0.0)
    {
      return result;
    }

    double best = tolerance * tolerance;

    for (const DomainPart part :
         {DomainPart::Boundary, DomainPart::Holes, DomainPart::Breaklines})
    {
      const QVector<QPolygonF> shapes = edgedShapes(domain, part);
      const bool ring = part != DomainPart::Breaklines;

      for (int shape = 0; shape < shapes.size(); ++shape)
      {
        const QPolygonF &vertices = shapes.at(shape);

        if (vertices.size() < 2)
        {
          continue;
        }

        // A ring is walked one edge further than a line: the closing edge is
        // drawn, so it can be clicked.
        const int edges = ring ? int(vertices.size())
                               : int(vertices.size()) - 1;

        for (int edge = 0; edge < edges; ++edge)
        {
          const QPointF &from = vertices.at(edge);
          const QPointF &to =
            vertices.at((edge + 1) % int(vertices.size()));

          const QPointF on = closestOnSegment(from, to, world);
          const double distance = squaredDistance(on, world);

          if (distance <= best)
          {
            best = distance;
            result.hit = true;
            result.point = on;

            // The address the new vertex would take: after the edge's first
            // vertex, which for a ring's closing edge is the end of the ring.
            result.at = DomainVertex{part, shape, edge + 1};
          }
        }
      }
    }

    return result;
  }

} // namespace HydroCouple::Composer
