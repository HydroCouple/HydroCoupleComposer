#include "layers/featurelayer.h"

#include "gis/spatialreference.h"
#include "map/extentmath.h"
#include "map/maptransform.h"
#include "render/labelconfig.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <cmath>

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

  void FeatureLayer::rebuildProjected() const
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

  const QVector<QVector<QPolygonF>> &FeatureLayer::projectedFeatures() const
  {
    if (!m_projectionValid)
    {
      rebuildProjected();
    }

    return m_projected;
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

  namespace
  {
    /*!
     * \brief Subdivisions a single segment may be cut into.
     *
     * A bound, not a budget: one segment spanning a whole catchment against a
     * metre-scale terrain would otherwise ask for millions of vertices, and a
     * view that runs out of memory because a shapefile had a long straight
     * line in it is worse than one whose longest line is slightly faceted.
     */
    constexpr int kMaxSubdivisions = 512;

    //! Cuts \a part into pieces no longer than \a step.
    QVector<QPointF> densify(const QPolygonF &part, double step)
    {
      if (step <= 0.0 || part.size() < 2)
      {
        return QVector<QPointF>(part.begin(), part.end());
      }

      QVector<QPointF> dense;
      dense.append(part.first());

      for (int i = 0; i + 1 < part.size(); ++i)
      {
        const QPointF from = part.at(i);
        const QPointF to = part.at(i + 1);
        const double length = std::hypot(to.x() - from.x(), to.y() - from.y());
        const int steps = std::clamp(
          int(std::ceil(length / step)), 1, kMaxSubdivisions);

        for (int s = 1; s <= steps; ++s)
        {
          dense.append(from + (to - from) * (double(s) / double(steps)));
        }
      }

      return dense;
    }

    /*!
     * \brief Ground elevation under each point of \a path.
     *
     * Points the terrain does not answer for hold the last elevation it did
     * answer, and a run of them at the head of the path takes the first —
     * which is what makes a line running off the edge of a terrain continue
     * at the height it left at rather than fall to zero. Falling to zero is
     * indistinguishable from a hole in the data.
     */
    QVector<double> sampleGround(const ITerrainSource *terrain,
                                 const QVector<QPointF> &path)
    {
      QVector<double> ground(path.size(), 0.0);

      if (!terrain)
      {
        return ground;
      }

      int firstKnown = -1;
      double held = 0.0;

      for (int i = 0; i < path.size(); ++i)
      {
        double elevation = held;

        if (terrain->elevationAt(path.at(i), elevation))
        {
          held = elevation;

          if (firstKnown < 0)
          {
            firstKnown = i;
          }
        }

        ground[i] = elevation;
      }

      for (int i = 0; i < firstKnown; ++i)
      {
        ground[i] = ground.at(firstKnown);
      }

      return ground;
    }

  }

  SceneDrape FeatureLayer::sceneDrape() const
  {
    return m_drape;
  }

  void FeatureLayer::setSceneDrape(SceneDrape drape)
  {
    if (m_drape == drape)
    {
      return;
    }

    m_drape = drape;

    notifyAppearanceChanged();
  }

  double FeatureLayer::extrusionHeight() const
  {
    return m_extrusionHeight;
  }

  void FeatureLayer::setExtrusionHeight(double height)
  {
    if (qFuzzyCompare(m_extrusionHeight, height))
    {
      return;
    }

    m_extrusionHeight = height;

    notifyAppearanceChanged();
  }

  const ISceneSource *FeatureLayer::sceneSource() const
  {
    return m_kind == GeometryKind::Point ? nullptr : this;
  }

  Bounds3D FeatureLayer::sceneBounds() const
  {
    Bounds3D bounds;

    if (m_kind == GeometryKind::Point)
    {
      return bounds;
    }

    const double rise =
      m_drape == SceneDrape::Extruded ? m_extrusionHeight : 0.0;
    const double low = std::min(0.0, rise);
    const double high = std::max(0.0, rise);

    for (const QVector<QPolygonF> &parts : projectedFeatures())
    {
      for (const QPolygonF &part : parts)
      {
        for (const QPointF &vertex : part)
        {
          bounds.expandTo(
            QVector3D(float(vertex.x()), float(vertex.y()), float(low)));
          bounds.expandTo(
            QVector3D(float(vertex.x()), float(vertex.y()), float(high)));
        }
      }
    }

    return bounds;
  }

  QVector<SceneGeometry> FeatureLayer::sceneGeometry(
    const SceneContext &context) const
  {
    QVector<SceneGeometry> batches;

    if (m_kind == GeometryKind::Point)
    {
      return batches;
    }

    // Asking to drape on a stack that holds no terrain degrades to flat
    // rather than to nothing: the layer is still data, and a network that
    // vanishes because the mesh beside it was closed is a worse answer than
    // one lying at zero.
    const ITerrainSource *terrain =
      m_drape == SceneDrape::Flat ? nullptr : context.terrain;
    const double step = terrain ? terrain->terrainResolution() : 0.0;

    const bool extruding = m_drape == SceneDrape::Extruded &&
                           !qFuzzyIsNull(m_extrusionHeight);

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();
    const LayerStyle *layerStyle = style();

    SceneGeometry crest;
    crest.primitive = ScenePrimitive::Lines;

    SceneGeometry curtain;
    curtain.primitive = ScenePrimitive::Triangles;

    // Lines have no surface to face, so they are lit as if facing up; the
    // shader's headlight term then leaves them at full colour.
    const QVector3D up(0.0f, 0.0f, 1.0f);

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      // The map's own colour for this feature: an invalid one means the class
      // was switched off in the legend, and a scene that drew it anyway would
      // contradict the legend beside it.
      const QColor color =
        layerStyle ? layerStyle->colorFor(*this, feature) : QColor(Qt::gray);

      if (!color.isValid())
      {
        continue;
      }

      for (const QPolygonF &part : projected.at(feature))
      {
        const QVector<QPointF> path = densify(part, step);

        if (path.size() < 2)
        {
          continue;
        }

        const QVector<double> ground = sampleGround(terrain, path);

        quint32 previous = crest.addVertex(
          QVector3D(float(path.at(0).x()), float(path.at(0).y()),
                    float(ground.at(0))),
          up, color);

        for (int i = 1; i < path.size(); ++i)
        {
          const quint32 current = crest.addVertex(
            QVector3D(float(path.at(i).x()), float(path.at(i).y()),
                      float(ground.at(i))),
            up, color);

          crest.indices.append(previous);
          crest.indices.append(current);

          if (extruding)
          {
            const QPointF from = path.at(i - 1);
            const QPointF to = path.at(i);
            const QPointF along = to - from;
            const double length = std::hypot(along.x(), along.y());

            if (length <= 0.0)
            {
              previous = current;

              continue;
            }

            // Horizontal, across the segment. Which of the two ways it faces
            // does not matter: the material lights both sides, which it has
            // to anyway for a camera orbited past a curtain.
            const QVector3D normal(float(along.y() / length),
                                   float(-along.x() / length), 0.0f);

            const quint32 base = quint32(curtain.vertices.size());

            curtain.addVertex(QVector3D(float(from.x()), float(from.y()),
                                        float(ground.at(i - 1))),
                              normal, color);
            curtain.addVertex(QVector3D(float(to.x()), float(to.y()),
                                        float(ground.at(i))),
                              normal, color);
            curtain.addVertex(
              QVector3D(float(to.x()), float(to.y()),
                        float(ground.at(i) + m_extrusionHeight)),
              normal, color);
            curtain.addVertex(
              QVector3D(float(from.x()), float(from.y()),
                        float(ground.at(i - 1) + m_extrusionHeight)),
              normal, color);

            curtain.indices.append(base);
            curtain.indices.append(base + 1);
            curtain.indices.append(base + 2);
            curtain.indices.append(base);
            curtain.indices.append(base + 2);
            curtain.indices.append(base + 3);
          }

          previous = current;
        }
      }
    }

    // The curtain first, so the crest that caps it is drawn over its own
    // top edge rather than fighting it.
    if (!curtain.isEmpty())
    {
      batches.append(std::move(curtain));
    }

    if (!crest.isEmpty())
    {
      batches.append(std::move(crest));
    }

    return batches;
  }

} // namespace HydroCouple::Composer
