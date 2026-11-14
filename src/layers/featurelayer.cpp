#include "layers/featurelayer.h"

#include "core/preferencesmanager.h"
#include "gis/spatialreference.h"
#include "map/extentmath.h"
#include "map/maptransform.h"
#include "render/labelconfig.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief Whether \a ring turns the same way at every corner.
     *
     * A fan from the first vertex tessellates a convex ring correctly and
     * a concave one wrongly — it spans the notch. Mesh faces are convex by
     * construction, which is the case ring filling exists for; anything
     * else is left as an outline rather than drawn wrong.
     *
     * Collinear corners contribute no turn and are skipped rather than
     * failing the test: a densified ring is full of them.
     */
    bool isConvexRing(const QVector<QPointF> &ring)
    {
      if (ring.size() < 3)
      {
        return false;
      }

      int sign = 0;

      for (int i = 0; i < ring.size(); ++i)
      {
        const QPointF &a = ring.at(i);
        const QPointF &b = ring.at((i + 1) % ring.size());
        const QPointF &c = ring.at((i + 2) % ring.size());

        const double cross = (b.x() - a.x()) * (c.y() - b.y())
                             - (b.y() - a.y()) * (c.x() - b.x());

        if (qFuzzyIsNull(cross))
        {
          continue;
        }

        const int turn = cross > 0.0 ? 1 : -1;

        if (sign == 0)
        {
          sign = turn;
        }
        else if (turn != sign)
        {
          return false;
        }
      }

      // Every corner collinear is a degenerate ring, not a convex one.
      return sign != 0;
    }

    /*!
     * \brief Appends an octahedral marker of \a size centred on \a centre.
     *
     * Six vertices, eight faces: the smallest solid that reads as a point
     * from any direction and needs no billboarding. Its own centre sits at
     * the sampled height, so half of it stands above the surface — which is
     * also why it needs no coplanar nudge to be seen on one.
     */
    void appendMarker(SceneGeometry &into, const QVector3D &centre,
                      float size, const QColor &color)
    {
      const float half = size * 0.5f;
      const quint32 base = quint32(into.vertices.size());

      // Normals point out along each axis; the material lights both sides,
      // so a face lit from behind still reads.
      const QVector3D directions[6] = {
        { 1.0f, 0.0f, 0.0f },  { -1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },  { 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },  { 0.0f, 0.0f, -1.0f },
      };

      for (const QVector3D &direction : directions)
      {
        into.addVertex(centre + direction * half, direction, color);
      }

      // Each face joins one horizontal pair to a pole: +x/+y/top and so on
      // round the equator, then the same four to the bottom.
      const quint32 faces[8][3] = {
        { 0, 2, 4 }, { 2, 1, 4 }, { 1, 3, 4 }, { 3, 0, 4 },
        { 2, 0, 5 }, { 1, 2, 5 }, { 3, 1, 5 }, { 0, 3, 5 },
      };

      for (const auto &face : faces)
      {
        into.indices.append(base + face[0]);
        into.indices.append(base + face[1]);
        into.indices.append(base + face[2]);
      }
    }

    /*!
     * \brief The colour a selected feature is outlined in.
     *
     * One colour rather than a themed pair: a selection has to stand out
     * against whatever the layer beneath it happens to be, and a highlight
     * that follows the theme is a highlight that matches its surroundings.
     * The default is cyan, because it survives both a dark basemap and a
     * pale one, and because almost nothing in hydrology is naturally that
     * colour. A preference, read as the layer paints, so the map and the
     * scene cannot hold two different answers.
     */
    QColor selectionColor()
    {
      return PreferencesManager::instance()->selectionColor();
    }

    //! Wide enough to read as a halo around the feature, not as its stroke.
    constexpr double kSelectionWidth = 3.0;

  }


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
    m_pickIndexValid = false;

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
    m_pickIndexValid = false;
    m_selection.clear();

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

  void FeatureLayer::onProjectionChanged()
  {
    m_projectionValid = false;

    // The pick index holds projected positions, so it goes with them.
    m_pickIndexValid = false;

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

        // Drawn over the feature rather than instead of it, so that what is
        // selected is still legible as what it is: a highlight that replaces
        // the fill tells you a catchment is selected and nothing else about
        // it.
        if (m_selection.contains(i))
        {
          painter.setPen(QPen(selectionColor(), kSelectionWidth));
          painter.setBrush(Qt::NoBrush);

          switch (m_features.at(i).kind)
          {
            case GeometryKind::Point:
              for (const QPointF &point : screen)
              {
                painter.drawEllipse(point, symbol.size * 0.5 + kSelectionWidth,
                                    symbol.size * 0.5 + kSelectionWidth);
              }
              break;

            case GeometryKind::Line:
              painter.drawPolyline(screen);
              break;

            case GeometryKind::Polygon:
              painter.drawPolygon(screen);
              break;
          }
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
     * \brief Whether any segment of \a part crosses \a box.
     *
     * The case a vertex test misses: a line that runs clean across the
     * rectangle has no vertex inside it, and is still something the user
     * dragged over.
     */
    bool crossesRect(const QPolygonF &part, const QRectF &box)
    {
      const QPointF corners[5] = {box.topLeft(), box.topRight(),
                                  box.bottomRight(), box.bottomLeft(),
                                  box.topLeft()};

      for (int i = 0; i + 1 < part.size(); ++i)
      {
        const QLineF segment(part.at(i), part.at(i + 1));

        for (int edge = 0; edge < 4; ++edge)
        {
          QPointF crossing;

          if (segment.intersects(QLineF(corners[edge], corners[edge + 1]),
                                 &crossing)
              == QLineF::BoundedIntersection)
          {
            return true;
          }
        }
      }

      return false;
    }

    /*!
     * \brief Shortest distance from \a point to the segment \a from-\a to.
     */
    double distanceToSegment(const QPointF &point, const QPointF &from,
                             const QPointF &to)
    {
      const QPointF along = to - from;
      const double length =
        along.x() * along.x() + along.y() * along.y();

      if (length <= 0.0)
      {
        return std::hypot(point.x() - from.x(), point.y() - from.y());
      }

      // Clamped, so that the answer for a point beyond either end is the
      // distance to that end rather than to the infinite line through them.
      const double along01 = std::clamp(
        ((point.x() - from.x()) * along.x() +
         (point.y() - from.y()) * along.y()) /
          length,
        0.0, 1.0);

      const QPointF nearest = from + along * along01;

      return std::hypot(point.x() - nearest.x(), point.y() - nearest.y());
    }

    //! Shortest distance from \a point to \a part's own vertices and edges.
    double distanceToOutline(const QPointF &point, const QPolygonF &part)
    {
      if (part.isEmpty())
      {
        return std::numeric_limits<double>::infinity();
      }

      if (part.size() == 1)
      {
        return std::hypot(point.x() - part.at(0).x(),
                          point.y() - part.at(0).y());
      }

      double nearest = std::numeric_limits<double>::infinity();

      for (int i = 0; i + 1 < part.size(); ++i)
      {
        nearest = std::min(
          nearest, distanceToSegment(point, part.at(i), part.at(i + 1)));
      }

      return nearest;
    }

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

  void FeatureLayer::ensurePickIndex() const
  {
    if (m_pickIndexValid)
    {
      return;
    }

    m_pickIndex.clear();

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      // Accumulated by hand rather than through QRectF::united(), and judged
      // by a counter rather than by isNull(): a point feature's bounding
      // rectangle has zero area, which makes it null, and both of those would
      // quietly drop every point layer out of the index.
      double minimumX = 0.0;
      double minimumY = 0.0;
      double maximumX = 0.0;
      double maximumY = 0.0;
      int vertices = 0;

      for (const QPolygonF &part : projected.at(feature))
      {
        for (const QPointF &vertex : part)
        {
          if (vertices == 0)
          {
            minimumX = maximumX = vertex.x();
            minimumY = maximumY = vertex.y();
          }
          else
          {
            minimumX = std::min(minimumX, vertex.x());
            maximumX = std::max(maximumX, vertex.x());
            minimumY = std::min(minimumY, vertex.y());
            maximumY = std::max(maximumY, vertex.y());
          }

          ++vertices;
        }
      }

      if (vertices == 0)
      {
        continue;
      }

      // Half the diagonal: the reach from the centroid that covers every
      // vertex, which is what makes the index's widened search exact. Zero
      // for a point, which is correct — a point reaches nowhere, and the
      // click tolerance is what gets it hit.
      m_pickIndex.add(
        QPointF(0.5 * (minimumX + maximumX), 0.5 * (minimumY + maximumY)),
        0.5 * std::hypot(maximumX - minimumX, maximumY - minimumY), feature);
    }

    m_pickIndex.build();
    m_pickIndexValid = true;
  }

  bool FeatureLayer::featureHit(int feature, const QPointF &point,
                                double tolerance) const
  {
    const QVector<QPolygonF> &parts = projectedFeatures().at(feature);

    for (const QPolygonF &part : parts)
    {
      // Inside counts for a polygon, and so does near its edge: a catchment
      // drawn a few pixels across has no inside to click in, and one drawn
      // across the whole window is most easily hit at its boundary.
      if (m_kind == GeometryKind::Polygon && part.size() > 2 &&
          part.containsPoint(point, Qt::OddEvenFill))
      {
        return true;
      }

      if (distanceToOutline(point, part) <= tolerance)
      {
        return true;
      }
    }

    return false;
  }

  int FeatureLayer::pickAt(const QPointF &point, double tolerance) const
  {
    ensurePickIndex();

    const double reach = std::max(0.0, tolerance);

    return m_pickIndex.findNearest(
      point, reach,
      [&](int feature) { return featureHit(feature, point, reach); });
  }

  QSet<int> FeatureLayer::pickIn(const QRectF &rectangle) const
  {
    QSet<int> caught;

    const QRectF box = rectangle.normalized();

    if (box.isEmpty())
    {
      return caught;
    }

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      for (const QPolygonF &part : projected.at(feature))
      {
        // A part's own bounds first, which rejects most of a large layer
        // for the cost of four comparisons.
        //
        // Compared edge by edge rather than with QRectF::intersects(),
        // which answers false for an empty rectangle — and the bounds of a
        // vertical conduit, a horizontal one, or a single point are all
        // empty. That is the same degenerate-rectangle trap C4a hit with
        // isNull() and united(), and it silently selected nothing here.
        const QRectF bounds = part.boundingRect();

        if (bounds.left() > box.right() || bounds.right() < box.left()
            || bounds.top() > box.bottom() || bounds.bottom() < box.top())
        {
          continue;
        }

        bool hit = false;

        for (const QPointF &vertex : part)
        {
          if (box.contains(vertex))
          {
            hit = true;
            break;
          }
        }

        // A segment can cross the box with neither end inside it — a long
        // conduit over a small band — and a polygon can swallow the box
        // whole. Both are things the user dragged over.
        if (!hit)
        {
          hit = crossesRect(part, box)
                || (m_kind == GeometryKind::Polygon && part.size() > 2
                    && part.containsPoint(box.center(), Qt::OddEvenFill));
        }

        if (hit)
        {
          caught.insert(feature);
          break;
        }
      }
    }

    return caught;
  }

  const QSet<int> &FeatureLayer::selection() const
  {
    return m_selection;
  }

  void FeatureLayer::setSelection(QSet<int> features)
  {
    QSet<int> kept;

    for (int feature : features)
    {
      if (feature >= 0 && feature < m_features.size())
      {
        kept.insert(feature);
      }
    }

    if (kept == m_selection)
    {
      return;
    }

    m_selection = std::move(kept);

    // Selection is an appearance: every view that shows this layer has to
    // redraw, and there is nothing else any of them would do with a separate
    // signal. The attribute table reads the selection back on the same one.
    notifyAppearanceChanged();
  }

  void FeatureLayer::clearSelection()
  {
    setSelection({});
  }

  void FeatureLayer::setZPolicy(const ZPolicy &policy)
  {
    if (policy == zPolicy())
    {
      return;
    }

    ISceneSource::setZPolicy(policy);

    notifyAppearanceChanged();
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
    // Every kind now, points included: they are drawn as solid markers
    // rather than left out, so a gauge network is visible in the view that
    // shows the terrain it stands on (U3a). Before this, a point layer
    // contributed bounds but nothing to look at.
    return this;
  }

  bool FeatureLayer::supportsAttributeZ() const
  {
    return true;
  }

  bool FeatureLayer::supportsRingFill() const
  {
    // Only a polygon layer has rings to fill. Offering it on lines would be
    // offering a control that does nothing.
    return m_kind == GeometryKind::Polygon;
  }

  Bounds3D FeatureLayer::sceneBounds() const
  {
    Bounds3D bounds;

    // Point layers answer too, though sceneGeometry() declines them: bounds
    // are about where the data IS, not what can be drawn. Leaving points out
    // made "Zoom to Full Extent" frame a different world in each view, and
    // left the basemap's ground plane stopping short of the gauges standing
    // on it.
    // Where the base sits depends on the policy; OnTerrain keeps the zero
    // baseline these bounds have always assumed, since sampling a terrain
    // just to frame the view is the one cost bounds exist to avoid.
    const ZPolicy &policy = zPolicy();

    double base = policy.mode == ZMode::Constant ? policy.constant
                                                 : policy.offset;
    double baseHigh = base;

    if (policy.mode == ZMode::FromAttribute && featureCount() > 0)
    {
      // The field's span. A per-revision cache is the follow-up if this
      // scan ever shows up in a profile; bounds run twice per rebuild.
      double lowest = std::numeric_limits<double>::max();
      double highest = std::numeric_limits<double>::lowest();

      for (int feature = 0; feature < featureCount(); ++feature)
      {
        bool numeric = false;
        const double value =
          attributeValue(feature, policy.field).toDouble(&numeric);

        if (!numeric)
        {
          continue;
        }

        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
      }

      if (lowest <= highest)
      {
        base = lowest + policy.offset;
        baseHigh = highest + policy.offset;
      }
    }

    const double rise = m_extrusionHeight;
    const double low = std::min({0.0, base, base + rise});
    const double high = std::max({0.0, baseHigh, baseHigh + rise});

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

  QColor FeatureLayer::sceneColorFor(const LayerStyle *style,
                                     int feature) const
  {
    const QColor color =
      style ? style->colorFor(*this, feature) : QColor(Qt::gray);

    if (!color.isValid())
    {
      return color;
    }

    return m_selection.contains(feature) ? selectionColor() : color;
  }

  double FeatureLayer::resolvedMarkerSize() const
  {
    if (m_markerSize > 0.0)
    {
      return m_markerSize;
    }

    const QRectF box = extent();

    // The diagonal, not isEmpty(): a row of gauges along a river has a real
    // extent and a height of exactly zero, and QRectF calls that empty.
    // This program has paid for that rectangle three times now — D26's
    // extent maths, C4a's point bounds, and here, where it made every
    // collinear point layer invisible until a gate caught it.
    const double diagonal = std::hypot(box.width(), box.height());

    if (diagonal <= 0.0)
    {
      // One point, or several in the same place: nothing to scale from, and
      // guessing a size in map units — metres? degrees? — would be guessing
      // by a factor of a hundred thousand. Nothing is drawn until a size is
      // given, which the layer properties dialog offers.
      return 0.0;
    }

    return diagonal / 50.0;
  }

  SceneGeometry FeatureLayer::markerGeometry(
    const ITerrainSource *terrain, const LayerStyle *layerStyle) const
  {
    SceneGeometry markers;
    markers.primitive = ScenePrimitive::Triangles;

    const double size = resolvedMarkerSize();

    if (size <= 0.0)
    {
      return markers;
    }

    const ZPolicy &policy = zPolicy();
    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      const QColor color = sceneColorFor(layerStyle, feature);

      if (!color.isValid())
      {
        continue;
      }

      double flatBase = policy.constant;

      if (policy.mode == ZMode::FromAttribute)
      {
        bool numeric = false;
        const double value =
          attributeValue(feature, policy.field).toDouble(&numeric);

        flatBase = (numeric ? value : 0.0) + policy.offset;
      }

      for (const QPolygonF &part : projected.at(feature))
      {
        for (const QPointF &point : part)
        {
          double z = flatBase;

          if (policy.mode == ZMode::OnTerrain)
          {
            // The same sampler the drapes use, so a station and the line
            // through it sit at the same height on the same ground.
            const QVector<double> sampled =
              sampleGround(terrain, QVector<QPointF>{ point });

            z = (sampled.isEmpty() ? 0.0 : sampled.first()) + policy.offset;
          }

          appendMarker(markers,
                       QVector3D(float(point.x()), float(point.y()), float(z)),
                       float(size), color);
        }
      }
    }

    return markers;
  }

  QVector<SceneGeometry> FeatureLayer::sceneGeometry(
    const SceneContext &context) const
  {
    QVector<SceneGeometry> batches;

    // Asking to drape on a stack that holds no terrain degrades to flat
    // rather than to nothing: the layer is still data, and a network that
    // vanishes because the mesh beside it was closed is a worse answer than
    // one lying at zero.
    const ZPolicy &policy = zPolicy();
    const ITerrainSource *terrain = terrainFor(context);
    const double step = terrain ? terrain->terrainResolution() : 0.0;

    // Extrusion is its own setting now, whatever the placement: a curtain
    // can rise from a constant datum or from a per-feature invert as well
    // as from the terrain.
    const bool extruding =
      supportsExtrusion() && !qFuzzyIsNull(m_extrusionHeight);

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();
    const LayerStyle *layerStyle = style();

    if (m_kind == GeometryKind::Point)
    {
      SceneGeometry markers = markerGeometry(terrain, layerStyle);

      // Appended only when there is something in it, as every other batch
      // here is: an empty batch is not nothing, it is a batch the renderer
      // uploads and draws no triangles from.
      if (!markers.isEmpty())
      {
        batches.append(std::move(markers));
      }

      return batches;
    }

    SceneGeometry crest;
    crest.primitive = ScenePrimitive::Lines;

    SceneGeometry curtain;
    curtain.primitive = ScenePrimitive::Triangles;

    // Filled rings, when the layer asks and the ring admits a fan. Drawn
    // before the crest, which then reads as the edge of its own face.
    const bool filling = m_fillRings && supportsRingFill();

    SceneGeometry fill;
    fill.primitive = ScenePrimitive::Triangles;

    // Lines have no surface to face, so they are lit as if facing up; the
    // shader's headlight term then leaves them at full colour.
    const QVector3D up(0.0f, 0.0f, 1.0f);

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      // The map's own colour for this feature, selection laid over it: an
      // invalid one means the class was switched off in the legend, and a
      // scene that drew it anyway would contradict the legend beside it.
      const QColor color = sceneColorFor(layerStyle, feature);

      if (!color.isValid())
      {
        continue;
      }

      // The feature's own base, resolved once beside its colour: constant
      // for Constant, its attribute plus the offset for FromAttribute, and
      // the per-vertex terrain plus the offset when draped.
      double flatBase = policy.constant;

      if (policy.mode == ZMode::FromAttribute)
      {
        bool numeric = false;
        const double value =
          attributeValue(feature, policy.field).toDouble(&numeric);

        // A feature whose field is missing or not a number sits at the
        // offset alone rather than vanishing: data with a broken row is
        // still data.
        flatBase = (numeric ? value : 0.0) + policy.offset;
      }

      for (const QPolygonF &part : projected.at(feature))
      {
        const QVector<QPointF> path = densify(part, step);

        if (path.size() < 2)
        {
          continue;
        }

        QVector<double> ground = sampleGround(terrain, path);

        if (policy.mode == ZMode::OnTerrain)
        {
          if (!qFuzzyIsNull(policy.offset))
          {
            for (double &z : ground)
            {
              z += policy.offset;
            }
          }
        }
        else
        {
          ground.fill(flatBase);
        }

        if (filling && path.size() >= 3 && isConvexRing(path))
        {
          // A fan from the first vertex. Lit as facing up: a draped face
          // follows the ground, and deriving a normal per triangle would
          // make a flat field of faces read as a crumpled one.
          const quint32 anchor = fill.addVertex(
            QVector3D(float(path.at(0).x()), float(path.at(0).y()),
                      float(ground.at(0))),
            up, color);

          for (int i = 1; i + 1 < path.size(); ++i)
          {
            const quint32 first = fill.addVertex(
              QVector3D(float(path.at(i).x()), float(path.at(i).y()),
                        float(ground.at(i))),
              up, color);
            const quint32 second = fill.addVertex(
              QVector3D(float(path.at(i + 1).x()), float(path.at(i + 1).y()),
                        float(ground.at(i + 1))),
              up, color);

            fill.indices.append(anchor);
            fill.indices.append(first);
            fill.indices.append(second);
          }
        }

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

    // Faces first, then the curtain, then the crest: each is drawn over
    // the one it caps rather than fighting it.
    if (!fill.isEmpty())
    {
      batches.append(std::move(fill));
    }

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

  bool sameGeometry(const FeatureLayer &left, const FeatureLayer &right,
                    QString &message)
  {
    if (left.featureCount() != right.featureCount())
    {
      message =
        QObject::tr("The two runs recorded %1 and %2 features, so they are "
                    "not the same geometry.")
          .arg(left.featureCount())
          .arg(right.featureCount());
      return false;
    }

    if (left.featureCount() == 0)
    {
      message = QObject::tr("Neither run recorded any geometry to compare.");
      return false;
    }

    // Taken from the extent rather than fixed, so the same mesh compares the
    // same way whether it is measured in metres or in degrees.
    const QRectF extent = left.extent();
    const double diagonal =
      std::hypot(extent.width(), extent.height());
    const double tolerance =
      diagonal > 0.0 ? diagonal * 1e-9 : 1e-9;

    const QVector<VectorFeature> &here = left.features();
    const QVector<VectorFeature> &there = right.features();

    for (int index = 0; index < here.size(); ++index)
    {
      if (here.at(index).kind != there.at(index).kind
          || here.at(index).parts.size() != there.at(index).parts.size())
      {
        message = QObject::tr("The two runs' geometries differ at feature %1, "
                              "so they are not the same ground.")
                    .arg(index);
        return false;
      }

      for (int part = 0; part < here.at(index).parts.size(); ++part)
      {
        const QPolygonF &mine = here.at(index).parts.at(part);
        const QPolygonF &yours = there.at(index).parts.at(part);

        if (mine.size() != yours.size())
        {
          message =
            QObject::tr("The two runs' geometries differ at feature %1, so "
                        "they are not the same ground.")
              .arg(index);
          return false;
        }

        for (int vertex = 0; vertex < mine.size(); ++vertex)
        {
          if (std::abs(mine.at(vertex).x() - yours.at(vertex).x()) > tolerance
              || std::abs(mine.at(vertex).y() - yours.at(vertex).y())
                   > tolerance)
          {
            message =
              QObject::tr("The two runs' geometries differ at feature %1, so "
                          "they are not the same ground.")
                .arg(index);
            return false;
          }
        }
      }
    }

    message.clear();
    return true;
  }

} // namespace HydroCouple::Composer
