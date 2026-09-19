#include "map/mapcanvas.h"

#include "core/preferencesmanager.h"
#include "gis/spatialreference.h"
#include "layers/featurelayer.h"
#include "map/extentmath.h"
#include "map/layerstackmodel.h"
#include "map/maplayer.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>
#include <QWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QVector>
#include <QWheelEvent>

#include <cstdlib>

#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Wheel notches are 120 eighths of a degree; one notch is one step.
    constexpr double kWheelZoomFactor = 1.2;

    /*!
     * \brief Air left around data when a command frames it.
     *
     * Belongs to "zoom to this data", not to "show this rectangle": the
     * second is used to carry a view between the map and the 3D scene, and a
     * margin charged there is charged again on every exchange.
     */
    constexpr double kFramingMargin = 0.05;

    /*!
     * \brief Reprojects a rectangle by sampling its boundary.
     *
     * Corners alone are not enough: a projection bends straight lines, so the
     * extreme of a reprojected edge frequently lies at its middle rather than
     * at either end, and a corners-only bound cuts geometry off the map.
     * Sampling the midpoints as well costs four more points and catches the
     * common single-bulge case.
     */
    QRectF reprojectExtent(const QRectF &extent,
                           const CoordinateTransform &transform)
    {
      if (extent.isNull())
      {
        return {};
      }

      QVector<QPointF> samples = {
        extent.topLeft(),     extent.topRight(),
        extent.bottomLeft(),  extent.bottomRight(),
        QPointF(extent.center().x(), extent.top()),
        QPointF(extent.center().x(), extent.bottom()),
        QPointF(extent.left(), extent.center().y()),
        QPointF(extent.right(), extent.center().y())};

      const int failed = transform.transformInPlace(samples);

      // A partly-failed reprojection would bound a mixture of two coordinate
      // systems, which is worse than declining to answer.
      if (failed > 0)
      {
        return {};
      }

      QRectF bounds(samples.first(), samples.first());

      for (const QPointF &point : samples)
      {
        bounds.setLeft(std::min(bounds.left(), point.x()));
        bounds.setRight(std::max(bounds.right(), point.x()));
        bounds.setTop(std::min(bounds.top(), point.y()));
        bounds.setBottom(std::max(bounds.bottom(), point.y()));
      }

      return bounds;
    }
  }

  MapCanvas::MapCanvas(QWidget *parent)
    : QWidget(parent),
      m_background(Qt::white)
  {
    setObjectName(QStringLiteral("mapCanvas"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Qt only delivers a background otherwise, and the canvas paints every
    // pixel itself.
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_transform.setViewport(QSizeF(size()));

    // The selection colour is read by the layers as they paint, so a change
    // to it needs nothing more than a repaint.
    connect(PreferencesManager::instance(),
            &PreferencesManager::preferenceChanged, this,
            [this](const QString &group, const QString &)
            {
              if (group == QLatin1String("Selection"))
              {
                update();
              }
            });
  }

  MapCanvas::~MapCanvas() = default;

  void MapCanvas::setModel(LayerStackModel *model)
  {
    if (m_model == model)
    {
      return;
    }

    if (m_model)
    {
      disconnect(m_model, nullptr, this, nullptr);
    }

    m_model = model;

    if (m_model)
    {
      // One connection covers order, membership, visibility and content: the
      // stack folds all of them into renderChanged precisely so a view cannot
      // subscribe to some and silently miss others.
      connect(m_model, &LayerStackModel::renderChanged, this,
              &MapCanvas::onStackChanged);

      // A layer arriving has to be told what CRS it is being drawn into
      // before it first paints, or it reprojects on the second frame and the
      // first one lands somewhere else entirely.
      connect(m_model, &LayerStackModel::rowsInserted, this,
              [this](const QModelIndex &parent, int first, int last)
              {
                if (parent.isValid())
                {
                  return;
                }

                for (int row = first; row <= last; ++row)
                {
                  if (MapLayer *layer = m_model->layerAt(row))
                  {
                    layer->setMapCrs(m_crs);
                  }
                }
              });

      publishCrs();
    }

    update();
  }

  LayerStackModel *MapCanvas::model() const
  {
    return m_model;
  }

  const SpatialReference *MapCanvas::crs() const
  {
    return m_crs.get();
  }

  void MapCanvas::setCrs(std::shared_ptr<SpatialReference> crs)
  {
    m_crs = std::move(crs);

    publishCrs();
    update();

    Q_EMIT crsChanged();

    // The scale is measured in metres on the ground, and how many metres a
    // world unit is worth is a property of the system — so a change of CRS
    // changes the scale without the view having moved at all.
    announceTransformChanged();
  }

  void MapCanvas::publishCrs()
  {
    if (!m_model)
    {
      return;
    }

    // Every layer reprojects into the map's CRS, so every layer has to be
    // told when it changes — including the ones already loaded.
    for (MapLayer *layer : m_model->layers())
    {
      layer->setMapCrs(m_crs);
    }
  }

  const MapTransform &MapCanvas::transform() const
  {
    syncViewport();

    return m_transform;
  }

  void MapCanvas::syncViewport() const
  {
    const QSizeF current(size());

    if (current == m_transform.viewport())
    {
      return;
    }

    m_transform.setViewport(current);

    // Re-apply the standing framing at the new size. This lives here rather
    // than in resizeEvent because Qt does not deliver a resize event to a
    // widget that has never been shown — which is precisely the case that
    // needs it, a map framed while its tab was still hidden.
    if (!m_viewMovedByUser && !m_framedExtent.isNull())
    {
      m_transform.fit(m_framedExtent, current);
    }
  }

  void MapCanvas::setVisibleExtent(const QRectF &extent,
                                   double marginFraction)
  {
    if (extent.isNull())
    {
      return;
    }

    syncViewport();
    m_transform.fit(extent, QSizeF(size()), marginFraction);

    m_fitted = true;
    m_framedExtent = extent;
    m_viewMovedByUser = false;

    announceTransformChanged();
    update();
  }

  QRectF MapCanvas::fullExtent() const
  {
    if (!m_model)
    {
      return {};
    }

    QRectF united;
    QRectF backdrop;
    bool unitedValid = false;
    bool backdropValid = false;

    for (const MapLayer *layer : m_model->layers())
    {
      if (!layer->isVisible())
      {
        continue;
      }

      const QRectF extent = layerExtentInMapCrs(layer);

      if (extent.isNull())
      {
        continue;
      }

      // A basemap covers the whole world, so including it would make
      // "zoom to everything" always mean "zoom to the planet" and never to
      // the data drawn on top of it.
      if (layer->isBasemap())
      {
        expandTo(backdrop, backdropValid, extent);
        continue;
      }

      expandTo(united, unitedValid, extent);
    }

    // With nothing but a basemap loaded, the world is the right answer.
    return unitedValid ? united : backdrop;
  }

  void MapCanvas::zoomToFullExtent()
  {
    setVisibleExtent(fullExtent(), kFramingMargin);
  }

  void MapCanvas::zoomToLayer(const MapLayer *layer)
  {
    if (!layer)
    {
      return;
    }

    setVisibleExtent(layerExtentInMapCrs(layer), kFramingMargin);
  }

  void MapCanvas::zoomBy(double factor)
  {
    syncViewport();
    m_transform.zoomAt(factor, QPointF(width() * 0.5, height() * 0.5));
    m_viewMovedByUser = true;

    announceTransformChanged();
    update();
  }

  namespace
  {
    //! Metres per inch, for turning a screen's DPI into a physical pixel size.
    constexpr double kMetresPerInch = 0.0254;

    //! What to assume before the widget belongs to a window with a screen.
    constexpr double kFallbackDpi = 96.0;

    //! The earth's equatorial radius, for sizing a degree of longitude.
    constexpr double kEarthRadiusMetres = 6378137.0;

    /*!
     * \brief The screen's dots per inch, or a sensible assumption.
     *
     * Read rather than assumed: a hard-coded 96 is out by about twice on a
     * Retina display, which makes every scale readout wrong by the same.
     *
     * \param widget The widget whose screen to ask.
     */
    double screenDpi(const QWidget *widget)
    {
      const QScreen *screen = nullptr;

      if (const QWindow *window =
            widget->window() ? widget->window()->windowHandle() : nullptr)
      {
        screen = window->screen();
      }

      if (!screen)
      {
        screen = QGuiApplication::primaryScreen();
      }

      const double dpi = screen ? screen->logicalDotsPerInchX() : 0.0;

      return dpi > 0.0 ? dpi : kFallbackDpi;
    }
  }

  double MapCanvas::metresPerWorldUnit() const
  {
    if (!m_crs)
    {
      // Nothing declared: the numbers are taken as they are, which for a
      // scale means treating them as metres. Saying 1:1 instead would be a
      // different kind of wrong, not a safer one.
      return 1.0;
    }

    if (m_crs->isGeographic())
    {
      // A degree of longitude shrinks with the cosine of latitude, so the
      // same view is a different scale in Norway than at the equator. Taken
      // at the centre of what is on screen.
      const double latitude = m_transform.visibleExtent().center().y();
      const double metres = (M_PI / 180.0) * kEarthRadiusMetres
                            * std::abs(std::cos(qDegreesToRadians(latitude)));

      // At the pole the cosine is zero and every scale would be infinite;
      // one metre per degree is meaningless but finite, and the alternative
      // is a readout of "inf".
      return metres > 1.0 ? metres : 1.0;
    }

    return m_crs->linearUnitsToMetres();
  }

  double MapCanvas::scaleDenominator() const
  {
    syncViewport();

    if (!m_transform.isValid() || m_transform.scale() <= 0.0)
    {
      return 1.0;
    }

    // scale() is pixels per world unit, so its reciprocal is what one pixel
    // covers on the ground.
    const double groundMetresPerPixel =
      metresPerWorldUnit() / m_transform.scale();

    return groundMetresPerPixel / (kMetresPerInch / screenDpi(this));
  }

  void MapCanvas::setScaleDenominator(double denominator)
  {
    syncViewport();

    if (denominator <= 0.0 || !m_transform.isValid())
    {
      return;
    }

    const double current = scaleDenominator();

    if (current <= 0.0)
    {
      return;
    }

    // Expressed as a zoom about the viewport centre rather than as a rebuilt
    // extent: the centre is then held exactly, and the arithmetic that turns
    // a denominator into world units lives in one place instead of two that
    // can disagree.
    m_transform.zoomAt(current / denominator,
                       QPointF(width() * 0.5, height() * 0.5));
    m_viewMovedByUser = true;

    announceTransformChanged();
    update();
  }

  void MapCanvas::announceTransformChanged()
  {
    Q_EMIT transformChanged();

    const double denominator = scaleDenominator();

    // Only when it moved. A pan changes the view without changing the scale,
    // and a readout rewritten on every pan would fight anyone typing into it.
    if (!qFuzzyCompare(denominator + 1.0, m_lastDenominator + 1.0))
    {
      m_lastDenominator = denominator;

      Q_EMIT scaleChanged(denominator);
    }
  }

  QColor MapCanvas::backgroundColor() const
  {
    return m_background;
  }

  void MapCanvas::setBackgroundColor(const QColor &color)
  {
    m_background = color;
    update();
  }

  QSize MapCanvas::sizeHint() const
  {
    return QSize(640, 480);
  }

  void MapCanvas::paintEvent(QPaintEvent *event)
  {
    Q_UNUSED(event)

    syncViewport();

    QPainter painter(this);
    painter.fillRect(rect(), m_background);

    if (!m_model || !m_transform.isValid())
    {
      return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Bottom of the stack first, so row 0 — the top of the layer tree — is
    // drawn last and lands on top, as the tree shows it.
    for (MapLayer *layer : m_model->renderOrder())
    {
      if (!layer->isVisible() || layer->opacity() <= 0.0)
      {
        continue;
      }

      // Saving around each layer means one layer's pen, brush or clip cannot
      // leak into the next one's drawing.
      painter.save();
      painter.setOpacity(layer->opacity());
      layer->render(painter, m_transform);
      painter.restore();
    }

    paintTransectLine(painter);
    paintSketch(painter);
    paintVertexHandles(painter);
    paintAttribution(painter);
  }

  void MapCanvas::paintSketch(QPainter &painter) const
  {
    if (m_sketch.isEmpty())
    {
      return;
    }

    QPolygonF onScreen;
    onScreen.reserve(m_sketch.size());

    for (const QPointF &world : m_sketch)
    {
      onScreen.append(m_transform.toScreen(world));
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setOpacity(1.0);
    painter.setBrush(Qt::NoBrush);

    // Dashed, so a shape still being drawn is never mistaken for one that
    // has been committed and is now part of the domain.
    QPen pen(QColor(30, 90, 200), 1.5, Qt::DashLine);
    painter.setPen(pen);

    if (m_sketchClosed && onScreen.size() > 2)
    {
      painter.drawPolygon(onScreen);
    }
    else
    {
      painter.drawPolyline(onScreen);
    }

    painter.setPen(QPen(QColor(30, 90, 200), 1.0));
    painter.setBrush(QColor(255, 255, 255));

    for (const QPointF &vertex : onScreen)
    {
      painter.drawEllipse(vertex, 3.0, 3.0);
    }

    painter.restore();
  }

  void MapCanvas::paintVertexHandles(QPainter &painter) const
  {
    if (m_vertexHandles.isEmpty())
    {
      return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setOpacity(1.0);

    // Square handles, where the sketch draws round vertices: one shape says
    // "this is being drawn" and the other says "this can be grabbed", and a
    // user editing a domain they have just drawn should be able to tell
    // which of the two they are looking at.
    for (int index = 0; index < m_vertexHandles.size(); ++index)
    {
      const QPointF centre = m_transform.toScreen(m_vertexHandles.at(index));
      const bool active = index == m_activeVertexHandle;
      const double half = active ? 5.0 : 3.5;

      painter.setPen(QPen(QColor(20, 20, 20), 1.0));
      painter.setBrush(active ? QColor(255, 170, 40) : QColor(255, 255, 255));
      painter.drawRect(QRectF(centre.x() - half, centre.y() - half,
                              2.0 * half, 2.0 * half));
    }

    painter.restore();
  }

  void MapCanvas::paintTransectLine(QPainter &painter) const
  {
    if (m_transectLine.size() < 2)
    {
      return;
    }

    QPolygonF onScreen;
    onScreen.reserve(m_transectLine.size());

    for (const QPointF &world : m_transectLine)
    {
      onScreen.append(m_transform.toScreen(world));
    }

    // Over the layers, not under them: the line is an annotation on the map
    // rather than a thing in it, and a section line hidden by the very mesh
    // it cuts through is a section line nobody can place.
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setOpacity(1.0);
    painter.setPen(QPen(QColor(220, 40, 40), 2.0));
    painter.drawPolyline(onScreen);

    painter.setBrush(QColor(220, 40, 40));
    painter.drawEllipse(onScreen.first(), 3.0, 3.0);
    painter.drawEllipse(onScreen.last(), 3.0, 3.0);
    painter.restore();
  }

  void MapCanvas::paintAttribution(QPainter &painter)
  {
    QStringList credits;

    for (const MapLayer *layer : m_model->layers())
    {
      const QString credit = layer->attribution();

      // Hidden layers are not on the map, so their provider is owed nothing.
      if (layer->isVisible() && !credit.isEmpty()
          && !credits.contains(credit))
      {
        credits.append(credit);
      }
    }

    if (credits.isEmpty())
    {
      return;
    }

    const QString text = credits.join(QStringLiteral(" · "));

    QFont font = painter.font();
    font.setPointSizeF(qMax(7.0, font.pointSizeF() - 2.0));

    const QFontMetricsF metrics(font);
    const QRectF box(width() - metrics.horizontalAdvance(text) - 10.0,
                     height() - metrics.height() - 4.0,
                     metrics.horizontalAdvance(text) + 6.0,
                     metrics.height() + 2.0);

    // Drawn last and over everything, on its own backing: a licence
    // condition that a data layer can paint over is not met.
    painter.save();
    painter.setOpacity(1.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 190));
    painter.drawRect(box);

    painter.setFont(font);
    painter.setPen(QColor(40, 40, 40));
    painter.drawText(box, Qt::AlignCenter, text);
    painter.restore();
  }

  void MapCanvas::resizeEvent(QResizeEvent *event)
  {
    QWidget::resizeEvent(event);

    // Through syncViewport, never setViewport directly: the viewport change
    // is what triggers re-framing, and setting it here would leave the sizes
    // already matching so the later sync had nothing left to notice.
    syncViewport();

    if (!m_fitted)
    {
      onStackChanged();
    }

    announceTransformChanged();
  }

  void MapCanvas::onStackChanged()
  {
    // The first layer to arrive frames the view. Without this the canvas
    // shows an empty white rectangle after a successful load, since the
    // default view is around the origin and most data is nowhere near it.
    syncViewport();

    if (!m_fitted && width() > 0 && height() > 0)
    {
      const QRectF extent = fullExtent();

      if (!extent.isNull())
      {
        m_transform.fit(extent, QSizeF(size()));

        m_fitted = true;
        m_framedExtent = extent;

        announceTransformChanged();
      }
    }

    update();
  }

  MapToolKind MapCanvas::toolKind() const
  {
    return m_toolKind;
  }

  void MapCanvas::setTransectLine(const QPolygonF &world)
  {
    if (m_transectLine == world)
    {
      return;
    }

    m_transectLine = world;
    update();

    Q_EMIT transectDrawn(m_transectLine);
  }

  const QPolygonF &MapCanvas::transectLine() const
  {
    return m_transectLine;
  }

  void MapCanvas::setSketch(const QPolygonF &world, bool closed)
  {
    if (m_sketch == world && m_sketchClosed == closed)
    {
      return;
    }

    m_sketch = world;
    m_sketchClosed = closed;
    update();
  }

  const QPolygonF &MapCanvas::sketch() const
  {
    return m_sketch;
  }

  void MapCanvas::setVertexHandles(const QVector<QPointF> &world, int active)
  {
    if (m_vertexHandles == world && m_activeVertexHandle == active)
    {
      return;
    }

    m_vertexHandles = world;
    m_activeVertexHandle = active;

    update();
  }

  const QVector<QPointF> &MapCanvas::vertexHandles() const
  {
    return m_vertexHandles;
  }

  int MapCanvas::activeVertexHandle() const
  {
    return m_activeVertexHandle;
  }

  void MapCanvas::setTool(std::unique_ptr<MapTool> tool)
  {
    if (m_tool)
    {
      m_tool->cancel();
    }

    if (!tool)
    {
      setToolKind(MapToolKind::Pan);
      return;
    }

    m_tool = std::move(tool);
    m_customTool = true;
    setCursor(m_tool->idleCursor());
  }

  void MapCanvas::setToolKind(MapToolKind kind)
  {
    // m_customTool as well as the kind: a custom tool leaves m_toolKind
    // naming whatever was installed before it, so the kind alone would
    // report that the requested tool was already in place and leave the
    // custom one running for good.
    if (m_tool && !m_customTool && m_toolKind == kind)
    {
      return;
    }

    m_customTool = false;

    // Cancelled, then replaced. A rubber band would end by construction —
    // it is a child of the tool — but the section tool's preview lives on
    // the canvas, and a tool cannot take that back from its own destructor
    // without reaching into a canvas that may itself be going away.
    if (m_tool)
    {
      m_tool->cancel();
    }

    m_toolKind = kind;

    switch (kind)
    {
      case MapToolKind::Select:
        m_tool = std::make_unique<SelectTool>(this);
        break;

      case MapToolKind::ZoomIn:
        m_tool = std::make_unique<ZoomInTool>(this);
        break;

      case MapToolKind::ZoomOut:
        m_tool = std::make_unique<ZoomOutTool>(this);
        break;

      case MapToolKind::Pan:
        m_tool = std::make_unique<PanTool>(this);
        break;

      case MapToolKind::Transect:
        m_tool = std::make_unique<TransectTool>(this);
        break;
    }

    setCursor(m_tool->idleCursor());
  }

  MapTool *MapCanvas::activeTool()
  {
    // Built on first use rather than in the constructor, so a canvas that is
    // never interacted with never builds one.
    if (!m_tool)
    {
      setToolKind(m_toolKind);
    }

    return m_tool.get();
  }

  void MapCanvas::panByPixels(const QPointF &pixels)
  {
    syncViewport();

    if (!m_transform.isValid())
    {
      return;
    }

    m_transform.panByPixels(pixels);
    m_viewMovedByUser = true;

    announceTransformChanged();
    update();
  }

  void MapCanvas::zoomAtPixel(double factor, const QPoint &pixel)
  {
    syncViewport();

    if (!m_transform.isValid() || factor <= 0.0)
    {
      return;
    }

    m_transform.zoomAt(factor, QPointF(pixel));
    m_viewMovedByUser = true;

    announceTransformChanged();
    update();
  }

  void MapCanvas::zoomToScreenRect(const QRect &rectangle)
  {
    syncViewport();

    // Width and height explicitly: a zero-area QRect reports itself null, so
    // the obvious guard would be true for a rectangle that is merely thin.
    if (!m_transform.isValid() || rectangle.width() <= 0 ||
        rectangle.height() <= 0)
    {
      return;
    }

    const QPointF first = m_transform.toWorld(QPointF(rectangle.topLeft()));
    const QPointF second =
      m_transform.toWorld(QPointF(rectangle.bottomRight()));

    const QRectF world = QRectF(first, second).normalized();

    if (world.isEmpty())
    {
      return;
    }

    // No margin: the rectangle is what was asked for. Breathing room belongs
    // to the commands that frame data, not to one the user drew.
    setVisibleExtent(world);
    m_viewMovedByUser = true;
  }

  void MapCanvas::zoomOutToScreenRect(const QRect &rectangle)
  {
    syncViewport();

    if (!m_transform.isValid() || rectangle.width() <= 0
        || rectangle.height() <= 0 || width() <= 0 || height() <= 0)
    {
      return;
    }

    // The viewport over the box, on whichever axis needs the most room:
    // fitting the view *into* the box means every part of what is on screen
    // has to end up inside it, and the tighter axis is what decides that.
    const double factor =
      std::min(double(rectangle.width()) / double(width()),
               double(rectangle.height()) / double(height()));

    if (factor <= 0.0)
    {
      return;
    }

    // Anchored at the box's centre, so what the user drew stays where they
    // drew it rather than sliding to the middle of the window.
    zoomAtPixel(factor, rectangle.center());
  }

  void MapCanvas::selectIn(const QRect &rectangle)
  {
    syncViewport();

    if (!m_model || !m_transform.isValid() || rectangle.width() <= 0
        || rectangle.height() <= 0)
    {
      return;
    }

    const QRectF world =
      QRectF(m_transform.toWorld(QPointF(rectangle.topLeft())),
             m_transform.toWorld(QPointF(rectangle.bottomRight())))
        .normalized();

    // layers() is top-first, so the first layer that catches anything is the
    // one the user can see — the same rule a click follows.
    for (MapLayer *layer : m_model->layers())
    {
      if (!layer->isVisible())
      {
        continue;
      }

      auto *features = dynamic_cast<FeatureLayer *>(layer);

      if (!features)
      {
        continue;
      }

      const QSet<int> caught = features->pickIn(world);

      if (!caught.isEmpty())
      {
        m_model->selectOnly(features, caught);

        // The count, not one index: a band that caught forty features has
        // no single feature to name, and -1 would read as "nothing".
        Q_EMIT featurePicked(features, *caught.constBegin());

        return;
      }
    }

    // Caught nothing, which is how a user says "nothing" — the same as a
    // click on empty map.
    m_model->selectOnly(nullptr, QSet<int>{});

    Q_EMIT featurePicked(nullptr, -1);
  }

  void MapCanvas::pickAndSelectAt(const QPoint &screen)
  {
    int feature = -1;
    FeatureLayer *layer = pickAt(screen, feature);

    // Clicking empty map clears the selection, which is how a user says
    // "nothing" — leaving the last selection standing would make the table
    // beside it describe somewhere they have navigated away from.
    if (m_model)
    {
      m_model->selectOnly(layer, feature);
    }

    Q_EMIT featurePicked(layer, feature);
  }

  void MapCanvas::mousePressEvent(QMouseEvent *event)
  {
    syncViewport();

    if (activeTool()->press(event))
    {
      event->accept();
      return;
    }

    QWidget::mousePressEvent(event);
  }

  void MapCanvas::mouseMoveEvent(QMouseEvent *event)
  {
    syncViewport();

    if (m_transform.isValid())
    {
      Q_EMIT cursorMoved(m_transform.toWorld(QPointF(event->pos())));
    }

    if (activeTool()->move(event))
    {
      event->accept();
      return;
    }

    QWidget::mouseMoveEvent(event);
  }

  void MapCanvas::mouseReleaseEvent(QMouseEvent *event)
  {
    if (activeTool()->release(event))
    {
      event->accept();
      return;
    }

    QWidget::mouseReleaseEvent(event);
  }

  FeatureLayer *MapCanvas::pickAt(const QPoint &screen, int &feature) const
  {
    feature = -1;

    if (!m_model || !m_transform.isValid() || m_transform.scale() <= 0.0)
    {
      return nullptr;
    }

    // The tolerance is a preference in pixels rather than map units because
    // it is a property of pointing, not of the data: the same conduit is
    // equally hard to hit at any zoom, and a tolerance in metres is generous
    // on a city and useless on a pipe. Read here, on every pick, so a change
    // in the preferences dialog applies to the next click.
    const QPointF world = m_transform.toWorld(QPointF(screen));
    const double tolerance =
      PreferencesManager::instance()->pickTolerancePixels()
      / m_transform.scale();

    // layers() is top-first, which is the order the answer has to come in:
    // the feature the user can see is the one on top.
    for (MapLayer *layer : m_model->layers())
    {
      if (!layer->isVisible())
      {
        continue;
      }

      auto *features = dynamic_cast<FeatureLayer *>(layer);

      if (!features)
      {
        continue;
      }

      const int hit = features->pickAt(world, tolerance);

      if (hit >= 0)
      {
        feature = hit;

        return features;
      }
    }

    return nullptr;
  }

  void MapCanvas::wheelEvent(QWheelEvent *event)
  {
    syncViewport();

    if (!m_transform.isValid())
    {
      QWidget::wheelEvent(event);
      return;
    }

    const double notches = event->angleDelta().y() / 120.0;

    if (qFuzzyIsNull(notches))
    {
      QWidget::wheelEvent(event);
      return;
    }

    // Anchored at the pointer: zooming about the centre slides whatever the
    // user is pointing at off-screen, which is what makes a map feel wrong.
    m_transform.zoomAt(std::pow(kWheelZoomFactor, notches),
                       event->position());
    m_viewMovedByUser = true;

    announceTransformChanged();
    update();

    event->accept();
  }

  QRectF MapCanvas::layerExtentInMapCrs(const MapLayer *layer) const
  {
    if (!layer)
    {
      return {};
    }

    const QRectF extent = layer->extent();

    if (extent.isNull())
    {
      return {};
    }

    const SpatialReference *source = layer->crs();

    // With either side unknown there is nothing to reproject between, so the
    // extent is taken at face value — the single-CRS case, which is most of
    // them.
    if (!source || !m_crs || source->isSameAs(*m_crs))
    {
      return extent;
    }

    QString message;
    const std::unique_ptr<CoordinateTransform> transform =
      CoordinateTransform::between(*source, *m_crs, message);

    if (!transform)
    {
      return {};
    }

    return reprojectExtent(extent, *transform);
  }

} // namespace HydroCouple::Composer
