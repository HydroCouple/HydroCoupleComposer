#include "mesh/domainimport.h"

#include "layers/featurelayer.h"
#include "mesh/meshdomainmodel.h"

#include <QObject>

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief \a ring with its repeated closing point taken off.
     *
     * OGR closes a ring by repeating its first vertex and a MeshDomain ring
     * does not, so importing one unopened leaves a duplicate corner sitting
     * on another — invisible on the map, and a zero-length edge handed to
     * the triangulator.
     */
    [[nodiscard]] QPolygonF openedRing(const QPolygonF &ring)
    {
      QPolygonF opened = ring;

      while (opened.size() > 1 && opened.first() == opened.last())
      {
        opened.removeLast();
      }

      return opened;
    }

    /*!
     * \brief How many of \a rings enclose \a ring, not counting itself.
     *
     * Even is an outer ring and odd is a hole: a polygon's own interior ring
     * lies inside one ring, an island inside a hole lies inside two, and the
     * separate polygons of a MultiPolygon lie inside none of each other.
     * Winding order is not consulted — shapefiles and GeoJSON wind their
     * rings the opposite way round and OGR hands both over untouched.
     *
     * The ring's first vertex stands for the ring, which is sound for the
     * nesting a file describes and undefined only for rings that touch.
     *
     * \param rings Every ring of one feature.
     * \param index Which of them to place.
     */
    [[nodiscard]] int nestingDepth(const QVector<QPolygonF> &rings, int index)
    {
      int depth = 0;

      for (int other = 0; other < rings.size(); ++other)
      {
        if (other != index
            && rings.at(other).containsPoint(rings.at(index).first(),
                                             Qt::OddEvenFill))
        {
          ++depth;
        }
      }

      return depth;
    }

    //! What geometry \a part can be made of.
    [[nodiscard]] GeometryKind kindFor(DomainPart part)
    {
      switch (part)
      {
        case DomainPart::Boundary:
        case DomainPart::Holes:
          return GeometryKind::Polygon;

        case DomainPart::Breaklines:
          return GeometryKind::Line;

        case DomainPart::ForcedPoints:
          return GeometryKind::Point;
      }

      return GeometryKind::Polygon;
    }

    [[nodiscard]] QString nameOf(GeometryKind kind)
    {
      switch (kind)
      {
        case GeometryKind::Point:
          return QObject::tr("points");

        case GeometryKind::Line:
          return QObject::tr("lines");

        case GeometryKind::Polygon:
          return QObject::tr("polygons");
      }

      return QObject::tr("features");
    }

    //! What was imported, named so the user can check it against the file.
    [[nodiscard]] QString countedPhrase(const DomainImportResult &result)
    {
      QStringList parts;

      if (result.boundaries > 0)
      {
        parts.append(QObject::tr("a boundary"));
      }

      if (result.holes > 0)
      {
        parts.append(QObject::tr("%n hole(s)", nullptr, result.holes));
      }

      if (result.breaklines > 0)
      {
        parts.append(QObject::tr("%n breakline(s)", nullptr,
                                 result.breaklines));
      }

      if (result.points > 0)
      {
        parts.append(QObject::tr("%n forced point(s)", nullptr,
                                 result.points));
      }

      return parts.join(QObject::tr(" and "));
    }

    //! The selected features, in a fixed order.
    [[nodiscard]] QList<int> orderedSelection(const FeatureLayer &layer)
    {
      // Sorted, because a QSet is walked in whatever order its hashes fall
      // in: without this the holes would arrive in a different order from
      // one run to the next, and which ring won a tie would be luck.
      QList<int> ordered(layer.selection().begin(), layer.selection().end());
      std::sort(ordered.begin(), ordered.end());

      return ordered;
    }
  }

  DomainImportResult importSelectionAsDomainPart(const FeatureLayer &layer,
                                                 DomainPart part,
                                                 MeshDomainModel &model)
  {
    DomainImportResult result;

    const QList<int> selected = orderedSelection(layer);

    if (selected.isEmpty())
    {
      result.message =
        QObject::tr("Select the features to import on \"%1\" first.")
          .arg(layer.name());

      return result;
    }

    const GeometryKind wanted = kindFor(part);

    if (layer.geometryKind() != wanted)
    {
      result.message =
        QObject::tr("\"%1\" holds %2, and this part is made of %3.")
          .arg(layer.name(), nameOf(layer.geometryKind()), nameOf(wanted));

      return result;
    }

    const QVector<QVector<QPolygonF>> &geometry = layer.projectedFeatures();

    // Built on a copy and handed over once: an import is one thing the user
    // did, and adding twelve holes one call at a time would redraw the map
    // twelve times and put eleven half-finished domains on it.
    MeshDomain domain = model.domain();

    // The outer ring of each selected polygon, and every interior ring of
    // all of them. Which is which comes from the file's own ordering.
    QVector<QPolygonF> outers;
    QVector<QPolygonF> inners;

    for (const int index : selected)
    {
      if (index < 0 || index >= geometry.size())
      {
        continue;
      }

      const QVector<QPolygonF> &parts = geometry.at(index);

      if (part == DomainPart::Boundary || part == DomainPart::Holes)
      {
        // The feature's rings are placed against each other, so this is a
        // pass over the whole feature rather than over one piece at a time.
        QVector<QPolygonF> rings;

        for (const QPolygonF &piece : parts)
        {
          const QPolygonF ring = openedRing(piece);

          if (ringEnclosesArea(ring))
          {
            rings.append(ring);
          }
          else
          {
            ++result.skipped;
          }
        }

        for (int ring = 0; ring < rings.size(); ++ring)
        {
          const int depth = nestingDepth(rings, ring);

          if (depth == 0)
          {
            outers.append(rings.at(ring));
          }
          else if (depth == 1)
          {
            inners.append(rings.at(ring));
          }
          else
          {
            // Ground standing inside a hole. A domain cuts holes out of one
            // boundary and has nowhere to put it back.
            ++result.islands;
          }
        }

        continue;
      }

      for (int piece = 0; piece < parts.size(); ++piece)
      {
        switch (part)
        {
          case DomainPart::Boundary:
          case DomainPart::Holes:
            break;

          case DomainPart::Breaklines:
          {
            const QPolygonF line = parts.at(piece);

            // Two distinct points is a line. Not opened like a ring: a
            // breakline that closes on itself is a legitimate constraint
            // and losing its last vertex would open it.
            if (line.size() < 2)
            {
              ++result.skipped;
              break;
            }

            domain.constraintLines.append(line);
            ++result.breaklines;

            break;
          }

          case DomainPart::ForcedPoints:
          {
            // Every vertex of every piece, so a multipoint feature forces
            // all of its points rather than only the first.
            for (const QPointF &point : parts.at(piece))
            {
              domain.points.append(point);
              ++result.points;
            }

            break;
          }
        }
      }
    }

    if (part == DomainPart::Boundary || part == DomainPart::Holes)
    {
      if (part == DomainPart::Boundary)
      {
        if (outers.isEmpty())
        {
          result.message =
            QObject::tr("None of the selected polygons encloses any ground.");

          return result;
        }

        // The largest outer ring is the one the others sit inside — the
        // shape a catchment-and-its-neighbours file already describes.
        // Absolute area, because a ring wound clockwise is the same ground
        // walked the other way.
        const auto widest = std::max_element(
          outers.begin(), outers.end(),
          [](const QPolygonF &left, const QPolygonF &right)
          {
            return std::abs(signedDoubleArea(left))
                   < std::abs(signedDoubleArea(right));
          });

        domain.boundary = *widest;
        result.boundaries = 1;

        outers.erase(widest);
      }

      // Whatever outer rings are left are holes: as the other polygons of a
      // boundary import, or as the whole of a hole import.
      for (const QPolygonF &ring : std::as_const(outers))
      {
        domain.holes.append(ring);
        ++result.holes;
      }

      if (part == DomainPart::Boundary)
      {
        // A polygon's own holes are holes of the domain too, wherever its
        // outer ring ended up.
        for (const QPolygonF &ring : std::as_const(inners))
        {
          domain.holes.append(ring);
          ++result.holes;
        }
      }
      else
      {
        // A hole's own hole is ground inside it, and there is nowhere for
        // ground to go in a flat list of rings cut out of one boundary.
        result.islands += int(inners.size());
      }
    }

    if (result.boundaries == 0 && result.holes == 0 && result.breaklines == 0
        && result.points == 0)
    {
      result.message =
        QObject::tr("Nothing in the selection could be imported.");

      return result;
    }

    model.setDomain(domain);

    result.ok = true;
    result.message = QObject::tr("Imported %1 from \"%2\".")
                       .arg(countedPhrase(result), layer.name());

    // Said out loud, both of them: geometry that went missing quietly is
    // indistinguishable from a file that never held it.
    if (result.skipped > 0)
    {
      result.message += QObject::tr(" %n of them enclosed no ground and were "
                                    "left out.", nullptr, result.skipped);
    }

    if (result.islands > 0)
    {
      result.message += QObject::tr(" %n interior ring(s) were ignored: a "
                                    "hole cannot hold an island.", nullptr,
                                    result.islands);
    }

    return result;
  }

} // namespace HydroCouple::Composer
