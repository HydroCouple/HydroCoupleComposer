#include "layers/ogrgeometryreader.h"

#include <ogr_geometry.h>

namespace HydroCouple::Composer
{
  namespace
  {
    void appendRing(QVector<QPolygonF> &parts, const OGRLinearRing *ring)
    {
      if (!ring)
      {
        return;
      }

      QPolygonF polygon;
      polygon.reserve(ring->getNumPoints());

      for (int i = 0; i < ring->getNumPoints(); ++i)
      {
        polygon.append(QPointF(ring->getX(i), ring->getY(i)));
      }

      parts.append(polygon);
    }

    void appendLine(QVector<QPolygonF> &parts, const OGRLineString *line)
    {
      if (!line)
      {
        return;
      }

      QPolygonF polygon;
      polygon.reserve(line->getNumPoints());

      for (int i = 0; i < line->getNumPoints(); ++i)
      {
        polygon.append(QPointF(line->getX(i), line->getY(i)));
      }

      parts.append(polygon);
    }

    bool collectGeometryInto(const OGRGeometry *geometry,
                         QVector<QPolygonF> &parts, GeometryKind &kind)
    {
      if (!geometry)
      {
        return false;
      }

      switch (wkbFlatten(geometry->getGeometryType()))
      {
        case wkbPoint:
        {
          const auto *point = geometry->toPoint();
          QPolygonF single;
          single.append(QPointF(point->getX(), point->getY()));
          parts.append(single);
          kind = GeometryKind::Point;

          return true;
        }

        case wkbLineString:
          appendLine(parts, geometry->toLineString());
          kind = GeometryKind::Line;

          return true;

        case wkbPolygon:
        {
          const auto *polygon = geometry->toPolygon();
          appendRing(parts, polygon->getExteriorRing());

          // Interior rings are collected so a polygon with a hole draws as
          // one, rather than as a filled shape with the hole painted over.
          for (int i = 0; i < polygon->getNumInteriorRings(); ++i)
          {
            appendRing(parts, polygon->getInteriorRing(i));
          }

          kind = GeometryKind::Polygon;

          return true;
        }

        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
          const auto *collection = geometry->toGeometryCollection();
          bool any = false;

          for (int i = 0; i < collection->getNumGeometries(); ++i)
          {
            any |= collectGeometryInto(collection->getGeometryRef(i), parts, kind);
          }

          return any;
        }

        default:
          return false;
      }
    }
  }

  bool collectOgrGeometry(const OGRGeometry *geometry,
                          QVector<QPolygonF> &parts, GeometryKind &kind)
  {
    return collectGeometryInto(geometry, parts, kind);
  }

  bool collectWkbGeometry(const std::vector<unsigned char> &wkb,
                          QVector<QPolygonF> &parts, GeometryKind &kind)
  {
    if (wkb.empty())
    {
      return false;
    }

    OGRGeometry *geometry = nullptr;

    if (OGRGeometryFactory::createFromWkb(wkb.data(), nullptr, &geometry,
                                          static_cast<int>(wkb.size()))
        != OGRERR_NONE)
    {
      return false;
    }

    const bool collected = collectGeometryInto(geometry, parts, kind);
    OGRGeometryFactory::destroyGeometry(geometry);

    return collected;
  }

} // namespace HydroCouple::Composer
