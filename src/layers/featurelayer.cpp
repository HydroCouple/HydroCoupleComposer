#include "layers/featurelayer.h"

#include "gis/spatialreference.h"
#include "map/extentmath.h"
#include "map/maptransform.h"
#include "render/labelconfig.h"

#include <QFontMetricsF>
#include <QPainter>

namespace HydroCouple::Composer
{

  FeatureLayer::FeatureLayer(const QString &name) : MapLayer(name)
  {
  }

  FeatureLayer::~FeatureLayer() = default;

  const QVector<VectorFeature> &FeatureLayer::features() const
  {
    return m_features;
  }

  GeometryKind FeatureLayer::geometryKind() const
  {
    return m_kind;
  }

  QRectF FeatureLayer::extent() const
  {
    return m_extent;
  }

  const LayerStyle *FeatureLayer::style() const
  {
    return &m_style;
  }

  LayerStyle *FeatureLayer::style()
  {
    return &m_style;
  }

  QVector<AttributeField> FeatureLayer::attributeFields() const
  {
    return m_fields;
  }

  int FeatureLayer::featureCount() const
  {
    return static_cast<int>(m_features.size());
  }

  QVariant FeatureLayer::attributeValue(int feature, const QString &field) const
  {
    if (feature < 0 || feature >= m_features.size())
    {
      return {};
    }

    for (int i = 0; i < m_fields.size(); ++i)
    {
      if (m_fields.at(i).name == field)
      {
        return m_features.at(feature).attributes.value(i);
      }
    }

    return {};
  }

  bool FeatureLayer::restyle()
  {
    const bool built = m_style.rebuild(*this);
    notifyAppearanceChanged();

    return built;
  }

  void FeatureLayer::setFields(QVector<AttributeField> fields)
  {
    m_fields = std::move(fields);
  }

  void FeatureLayer::addFeature(VectorFeature feature)
  {
    QRectF bounds;
    bool valid = false;

    // Per vertex, not per part: a single-vertex part has a zero-area bounding
    // rect, which QRectF::united() drops on the floor.
    for (const QPolygonF &part : feature.parts)
    {
      for (const QPointF &vertex : part)
      {
        expandTo(bounds, valid, vertex);
      }
    }

    if (!valid)
    {
      return;
    }

    feature.bounds = bounds;
    expandTo(m_extent, m_extentValid, bounds);

    // The layer's kind is whatever its features are; a mixed layer takes the
    // kind of its last feature, and each feature still draws as itself.
    m_kind = feature.kind;

    m_features.append(std::move(feature));
    m_projectionValid = false;
  }

  void FeatureLayer::setFeatureAttributes(int feature,
                                          QVector<QVariant> attributes)
  {
    if (feature >= 0 && feature < m_features.size())
    {
      m_features[feature].attributes = std::move(attributes);
    }
  }

  void FeatureLayer::clearFeatures()
  {
    m_features.clear();
    m_projected.clear();
    m_extent = QRectF();
    m_extentValid = false;
    m_projectionValid = false;
  }

  void FeatureLayer::finishLoading()
  {
    // Polygons read better with an outline and a translucent fill, so what
    // lies beneath them stays visible; that is not a sensible default for
    // points.
    if (m_kind == GeometryKind::Polygon)
    {
      Symbol symbol = m_style.symbol();
      symbol.fill.setAlpha(120);
      m_style.setSymbol(symbol);
    }
  }

  void FeatureLayer::onMapCrsChanged()
  {
    m_projectionValid = false;
    notifyAppearanceChanged();
  }

  void FeatureLayer::rebuildProjected()
  {
    m_projected.clear();
    m_projected.reserve(m_features.size());

    std::unique_ptr<CoordinateTransform> transform;

    if (crs() && mapCrs() && !crs()->isSameAs(*mapCrs()))
    {
      QString message;
      transform = CoordinateTransform::between(*crs(), *mapCrs(), message);
    }

    for (const VectorFeature &feature : m_features)
    {
      QVector<QPolygonF> parts = feature.parts;

      if (transform)
      {
        for (QPolygonF &part : parts)
        {
          // In place, and vertices that fail are left where they are and
          // counted rather than dropped: dropping one deforms the geometry
          // instead of reporting a problem.
          transform->transformInPlace(part);
        }
      }

      m_projected.append(parts);
    }

    m_projectionValid = true;
  }

  void FeatureLayer::render(QPainter &painter, const MapTransform &transform)
  {
    if (!m_projectionValid)
    {
      rebuildProjected();
    }

    const QRectF visible = transform.visibleExtent();
    const Symbol &symbol = m_style.symbol();

    const LabelConfig &labels = m_style.labels();
    const bool labelling = labels.enabled && !labels.fieldName.isEmpty();

    // One per frame: the first label to claim a piece of canvas keeps it, so
    // draw order is priority order.
    LabelCollisionMap collisions;
    const QFontMetricsF metrics(labels.font);

    for (int i = 0; i < m_features.size() && i < m_projected.size(); ++i)
    {
      QColor color = m_style.colorFor(*this, i);

      // An invalid colour is the style declining to draw this feature — a
      // class switched off in the legend, or a value it cannot place.
      if (!color.isValid())
      {
        continue;
      }

      // Class colours arrive from the ramp fully opaque. For polygons that
      // would hide the basemap and every layer beneath, so the symbol's own
      // transparency is carried over onto the themed colour.
      if (m_features.at(i).kind == GeometryKind::Polygon
          && symbol.fill.alpha() < 255)
      {
        color.setAlpha(symbol.fill.alpha());
      }

      const QVector<QPolygonF> &parts = m_projected.at(i);

      QPointF anchor;
      bool anchored = false;

      for (const QPolygonF &part : parts)
      {
        // overlaps(), not QRectF::intersects(): a point's bounding rect has
        // no area, and intersects() answers false for those — every point
        // feature would be culled.
        if (part.isEmpty() || !overlaps(part.boundingRect(), visible))
        {
          continue;
        }

        QPolygonF screen;
        screen.reserve(part.size());

        for (const QPointF &vertex : part)
        {
          screen.append(transform.toScreen(vertex));
        }

        if (!anchored)
        {
          anchor = screen.boundingRect().center();
          anchored = true;
        }

        switch (m_features.at(i).kind)
        {
          case GeometryKind::Point:
            painter.setPen(QPen(symbol.stroke, symbol.strokeWidth));
            painter.setBrush(color);

            for (const QPointF &point : screen)
            {
              painter.drawEllipse(point, symbol.size * 0.5, symbol.size * 0.5);
            }
            break;

          case GeometryKind::Line:
            // Lines take the themed colour as the stroke; filling a line
            // would draw the area its vertices happen to enclose.
            painter.setPen(QPen(color, symbol.strokeWidth));
            painter.setBrush(Qt::NoBrush);
            painter.drawPolyline(screen);
            break;

          case GeometryKind::Polygon:
            painter.setPen(QPen(symbol.stroke, symbol.strokeWidth));
            painter.setBrush(color);
            painter.drawPolygon(screen);
            break;
        }
      }

      if (labelling && anchored)
      {
        const QString text =
          attributeValue(i, labels.fieldName).toString();

        if (!text.isEmpty())
        {
          const QRectF box = LabelPainter::labelRect(
            labels, anchor,
            QSizeF(metrics.horizontalAdvance(text), metrics.height()));

          if (collisions.tryPlace(box))
          {
            LabelPainter::draw(painter, labels, box, text);
          }
        }
      }
    }
  }

} // namespace HydroCouple::Composer
