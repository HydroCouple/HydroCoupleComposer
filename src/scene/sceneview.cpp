#include "scene/sceneview.h"

#include "core/preferencesmanager.h"
#include "layers/featurelayer.h"
#include "map/layerstackmodel.h"
#include "map/maplayer.h"
#include "pick/terrainray.h"

#include <QMouseEvent>
#include <QRubberBand>
#include <QWheelEvent>

#include <rhi/qrhi.h>

#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief How near a click has to land, in pixels.
     *
     * In pixels rather than world units because it is a property of pointing,
     * not of the data — the same preference the map reads, read on every
     * pick so a change in the dialog applies to the next click.
     */
    double pickRadiusPixels()
    {
      return PreferencesManager::instance()->pickTolerancePixels();
    }

    /*!
     * \brief How far the mouse may move and still count as a click.
     *
     * Orbiting and picking share the left button, so they are told apart by
     * whether the scene turned. The same preference as the map's.
     */
    int clickSlopPixels()
    {
      return PreferencesManager::instance()->dragThresholdPixels();
    }

    //! Degrees of rotation per pixel dragged. Chosen so a drag across the
    //! width of a typical view is most of a turn, which is what makes
    //! orbiting feel like turning an object rather than nudging one.
    constexpr double kDegreesPerPixel = 0.4;

    constexpr double kZoomPerNotch = 1.15;

    //! What a click under the zoom tools moves by, matching the map's.
    constexpr double kBandZoomFactor = 2.0;

  }

  SceneView::SceneView(QWidget *parent) : QRhiWidget(parent)
  {
    // Repaint when the renderer says its geometry is stale, not when the
    // stack says it changed: the renderer is the thing holding the cache,
    // and two listeners on the stack would be two chances to disagree about
    // whether the frame on screen is current.
    connect(&m_renderer, &SceneRenderer::sceneChanged, this,
            [this]
            {
              // Framed when the geometry arrives, not when the next frame is
              // about to be drawn. Those are the same moment on a machine
              // with a graphics device and are not the same moment anywhere
              // else — and "what is this view looking at" is a question that
              // should have an answer either way.
              frameOnFirstGeometry();
              update();
            });

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // The depth buffer the pipeline's depth test needs comes with
    // QRhiWidget's automatic render target, which is on by default.

    // The background is a preference; the selection colour is baked into the
    // vertices (C3a), so a change to it means rebuilding the batches, not
    // only repainting.
    PreferencesManager *prefs = PreferencesManager::instance();
    m_background = prefs->sceneBackgroundColor();
    m_showGizmo = prefs->showAxisGizmo();
    m_gizmoSize = prefs->axisGizmoSizePixels();
    m_gizmoCorner = gizmoCornerFromName(prefs->axisGizmoCorner());

    connect(prefs, &PreferencesManager::preferenceChanged, this,
            [this, prefs](const QString &group, const QString &name)
            {
              if (group == QLatin1String("3D View")
                  && name == QLatin1String("backgroundColor"))
              {
                setBackgroundColor(prefs->sceneBackgroundColor());
              }
              else if (group == QLatin1String("3D View")
                       && name.startsWith(QLatin1String("axisGizmo")))
              {
                m_gizmoSize = prefs->axisGizmoSizePixels();
                m_gizmoCorner =
                  gizmoCornerFromName(prefs->axisGizmoCorner());
                update();
              }
              else if (group == QLatin1String("3D View")
                       && name == QLatin1String("showAxisGizmo"))
              {
                m_showGizmo = prefs->showAxisGizmo();
                update();
              }
              else if (group == QLatin1String("Selection"))
              {
                m_renderer.invalidate();
                update();
              }
            });
  }

  SceneView::~SceneView() = default;

  void SceneView::setModel(LayerStackModel *model)
  {
    m_renderer.setModel(model);
    m_framed = false;

    update();
  }

  LayerStackModel *SceneView::model() const
  {
    return m_renderer.model();
  }

  const Camera &SceneView::camera() const
  {
    return m_camera;
  }

  void SceneView::setCamera(const Camera &camera)
  {
    m_camera = camera;

    // Set explicitly, so the automatic first framing must not overrule it.
    m_framed = true;

    update();
    Q_EMIT cameraChanged();
  }

  void SceneView::zoomIn()
  {
    // The same step a wheel notch takes, so the shortcut and the wheel move
    // by amounts a user can predict from one another.
    m_camera.dolly(1.0 / kZoomPerNotch);

    update();
    Q_EMIT cameraChanged();
  }

  void SceneView::zoomOut()
  {
    m_camera.dolly(kZoomPerNotch);

    update();
    Q_EMIT cameraChanged();
  }

  void SceneView::zoomToFullExtent()
  {
    frameBounds(m_renderer.sceneBounds());
  }

  void SceneView::frameBounds(const Bounds3D &bounds)
  {
    // Not for fitTo's sake -- the camera refuses invalid bounds itself --
    // but for m_framed's: marking an empty framing as deliberate would stop
    // the first-geometry auto-frame, and a model loaded afterwards would
    // never be framed at all.
    if (!bounds.isValid())
    {
      return;
    }

    const double aspect =
      height() > 0 ? double(width()) / double(height()) : 1.0;

    m_camera.fitTo(bounds, aspect);
    m_framed = true;

    update();
    Q_EMIT cameraChanged();
  }

  QRectF SceneView::groundExtent() const
  {
    // Nothing to show means nothing is being looked at. Without this the
    // default camera's rectangle around the origin reads as a considered
    // view, and handing it to the map replaces a framing the user chose with
    // a couple of world units of nowhere.
    if (height() <= 0 || width() <= 0 || !m_renderer.sceneBounds().isValid())
    {
      return {};
    }

    return m_camera.groundExtent(double(width()) / double(height()));
  }

  void SceneView::showGroundExtent(const QRectF &extent)
  {
    // isNull, matching what Camera itself refuses, so that "framed" is
    // recorded exactly when the camera was framed. The size check is the one
    // that is really needed: a widget that has not been laid out has no
    // aspect ratio to frame against.
    if (extent.isNull() || height() <= 0 || width() <= 0)
    {
      return;
    }

    m_camera.setGroundExtent(extent.normalized(),
                             double(width()) / double(height()));

    // The view has been framed deliberately, so the first-geometry framing
    // must not come along afterwards and overrule it — which it would, since
    // arriving from the map is usually the first time this widget draws.
    m_framed = true;

    update();
    Q_EMIT cameraChanged();
  }

  void SceneView::setVerticalExaggeration(double factor)
  {
    if (qFuzzyCompare(m_camera.verticalExaggeration(), factor))
    {
      return;
    }

    m_camera.setVerticalExaggeration(factor);

    // The framing is kept. This used to reframe on every change, which
    // sounded protective and threw away whatever framing the map had just
    // handed over -- and the clip planes are derived from the camera's
    // distance every frame, so nothing here can clip wrong. Relief stretched
    // beyond the top of the view is what the spin was asked to do, and the
    // wheel is right there.
    update();
    Q_EMIT cameraChanged();
  }

  double SceneView::verticalExaggeration() const
  {
    return m_camera.verticalExaggeration();
  }

  CameraProjection SceneView::projection() const
  {
    return m_camera.projection();
  }

  void SceneView::setProjection(CameraProjection projection)
  {
    if (m_camera.projection() == projection)
    {
      return;
    }

    const double aspect =
      height() > 0 ? double(width()) / double(height()) : 1.0;

    m_camera.setProjection(projection, aspect);

    update();
    Q_EMIT cameraChanged();
  }

  QColor SceneView::backgroundColor() const
  {
    return m_background;
  }

  void SceneView::setBackgroundColor(const QColor &color)
  {
    if (m_background == color)
    {
      return;
    }

    m_background = color;
    update();
  }

  QSize SceneView::sizeHint() const
  {
    return { 640, 480 };
  }

  void SceneView::frameOnFirstGeometry()
  {
    if (m_framed)
    {
      return;
    }

    // Layers arrive after the widget does, so framing has to wait for the
    // first frame in which there is something to frame rather than happen
    // once at construction.
    if (m_renderer.sceneBounds().isValid())
    {
      zoomToFullExtent();
    }
  }

  void SceneView::initialize(QRhiCommandBuffer *cb)
  {
    Q_UNUSED(cb)

    QString message;

    if (!m_renderer.initialize(rhi(), renderTarget()->renderPassDescriptor(),
                               renderTarget()->sampleCount(), message))
    {
      qWarning("SceneView: %s", qPrintable(message));
    }
  }

  QRect SceneView::gizmoRect() const
  {
    if (!m_showGizmo)
    {
      return {};
    }

    return axisGizmoRect(size(), m_gizmoSize, m_gizmoCorner);
  }

  bool SceneView::takeGizmoPress(const QPoint &pixel)
  {
    const QRect rect = gizmoRect();

    if (!rect.contains(pixel))
    {
      return false;
    }

    const GizmoAxis axis = axisGizmoHit(axisGizmoPoint(pixel, rect),
                                        m_camera.azimuth(),
                                        m_camera.elevation());

    double azimuth = m_camera.azimuth();
    double elevation = m_camera.elevation();

    if (!axisGizmoView(axis, azimuth, elevation))
    {
      // A press inside the cue's square but not on an arm is still the
      // cue's: letting it fall through would start an orbit from a click
      // the user aimed at a button, which feels like a slipped grip.
      return true;
    }

    m_camera.setAzimuth(azimuth);
    m_camera.setElevation(elevation);
    update();

    return true;
  }

  void SceneView::render(QRhiCommandBuffer *cb)
  {
    frameOnFirstGeometry();

    // The same rectangle the hit test uses, in device pixels. Scaled from
    // the one rect rather than recomputed from the device size, because a
    // corner worked out twice is a corner that will eventually be two
    // corners — and on a retina display the inset alone would differ.
    const QRect logical = gizmoRect();
    const double ratio = devicePixelRatioF();

    m_renderer.setAxisGizmoViewport(
      logical.isEmpty()
        ? QRect()
        : QRect(int(std::lround(logical.x() * ratio)),
                int(std::lround(logical.y() * ratio)),
                int(std::lround(logical.width() * ratio)),
                int(std::lround(logical.height() * ratio))));

    m_renderer.render(cb, renderTarget(), m_camera, m_background);
  }

  void SceneView::releaseResources()
  {
    m_renderer.releaseResources();
  }

  void SceneView::mousePressEvent(QMouseEvent *event)
  {
    // First: a press on the cue is a press on a control, not the start of
    // a gesture over the scene.
    if (event->button() == Qt::LeftButton && takeGizmoPress(event->pos()))
    {
      // Said rather than assumed. The release handler reads these, and
      // returning early without clearing them would leave the view
      // holding whatever the last gesture left behind — which happens to
      // be harmless today only because release clears them, and that is
      // a poor thing for correctness here to rest on.
      m_orbiting = false;
      m_panning = false;
      m_banding = false;
      m_pressPosition = event->pos();
      m_lastMousePosition = event->pos();

      event->accept();

      return;
    }

    m_pressPosition = event->pos();
    m_lastMousePosition = event->pos();

    // Middle and right always pan, under every tool — the convention every
    // GIS 3D view uses, so muscle memory carries over and panning stays
    // reachable whatever the left button has been given to.
    m_panning = event->button() == Qt::MiddleButton ||
                event->button() == Qt::RightButton;

    m_orbiting = event->button() == Qt::LeftButton
                 && m_toolKind == SceneToolKind::Orbit;

    m_banding = event->button() == Qt::LeftButton
                && m_toolKind != SceneToolKind::Orbit;

    if (m_banding)
    {
      if (!m_band)
      {
        m_band = std::make_unique<QRubberBand>(QRubberBand::Rectangle, this);
        m_band->setObjectName(QStringLiteral("sceneRubberBand"));
      }

      m_band->setGeometry(QRect(m_pressPosition, QSize()));
      m_band->show();
    }

    QRhiWidget::mousePressEvent(event);
  }

  void SceneView::mouseMoveEvent(QMouseEvent *event)
  {
    if (m_banding && m_band)
    {
      // normalized(), so dragging up and to the left draws a rectangle
      // rather than a negative one that shows nothing.
      m_band->setGeometry(QRect(m_pressPosition, event->pos()).normalized());

      event->accept();

      return;
    }

    if (!m_orbiting && !m_panning)
    {
      QRhiWidget::mouseMoveEvent(event);

      return;
    }

    const QPoint delta = event->pos() - m_lastMousePosition;
    m_lastMousePosition = event->pos();

    const double aspect =
      height() > 0 ? double(width()) / double(height()) : 1.0;

    if (m_orbiting)
    {
      // Dragging right turns the scene right, which means turning the camera
      // the other way.
      m_camera.orbit(-delta.x() * kDegreesPerPixel,
                     delta.y() * kDegreesPerPixel);
    }
    else
    {
      m_camera.pan(QPointF(delta.x(), delta.y()), height(), aspect);
    }

    update();
    Q_EMIT cameraChanged();

    QRhiWidget::mouseMoveEvent(event);
  }

  void SceneView::mouseReleaseEvent(QMouseEvent *event)
  {
    if (m_banding && event->button() == Qt::LeftButton)
    {
      const QRect rectangle =
        QRect(m_pressPosition, event->pos()).normalized();

      m_banding = false;

      if (m_band)
      {
        m_band->hide();
      }

      // Width and height explicitly, not isNull(): a zero-area QRect reports
      // itself null, which would throw away a legitimate thin drag.
      const bool dragged = rectangle.width() > clickSlopPixels()
                           && rectangle.height() > clickSlopPixels();

      applyBand(rectangle, dragged, event->pos());

      event->accept();

      return;
    }

    const bool wasOrbiting = m_orbiting;

    m_orbiting = false;
    m_panning = false;

    // A left press that did not turn the scene was a click, not an orbit —
    // the same rule the map uses to tell a click from a pan, and for the same
    // reason: one button does both.
    const QPoint travelled = event->pos() - m_pressPosition;

    if (wasOrbiting && event->button() == Qt::LeftButton &&
        std::abs(travelled.x()) <= clickSlopPixels() &&
        std::abs(travelled.y()) <= clickSlopPixels())
    {
      int feature = -1;
      FeatureLayer *layer = pickAt(event->pos(), feature);

      if (LayerStackModel *stack = m_renderer.model())
      {
        stack->selectOnly(layer, feature);
      }

      Q_EMIT featurePicked(layer, feature);
    }

    QRhiWidget::mouseReleaseEvent(event);
  }

  SceneToolKind SceneView::toolKind() const
  {
    return m_toolKind;
  }

  void SceneView::setToolKind(SceneToolKind kind)
  {
    if (m_toolKind == kind)
    {
      return;
    }

    m_toolKind = kind;

    // Abandoned rather than carried across: a band belonging to a tool that
    // is no longer active would stay on screen with nothing to finish it.
    m_banding = false;

    if (m_band)
    {
      m_band->hide();
    }

    setCursor(kind == SceneToolKind::Orbit ? Qt::ArrowCursor
                                           : Qt::CrossCursor);
  }

  bool SceneView::groundOrPlaneUnder(const QPoint &pixel,
                                     QPointF &ground) const
  {
    if (groundUnder(pixel, ground))
    {
      return true;
    }

    QVector3D origin;
    QVector3D direction;

    if (!m_camera.rayThrough(QPointF(pixel), size(), origin, direction))
    {
      return false;
    }

    // Nullptr terrain, which is how rayGroundPoint spells "the plane at
    // z = 0" — the surface the map is drawn on when nothing rises above it.
    return rayGroundPoint(origin, direction, nullptr, m_camera.farPlane(),
                          ground);
  }

  bool SceneView::groundRectUnder(const QRect &pixels, QRectF &ground) const
  {
    const QPoint corners[4] = {pixels.topLeft(), pixels.topRight(),
                               pixels.bottomRight(), pixels.bottomLeft()};

    QPointF first;

    if (!groundOrPlaneUnder(corners[0], first))
    {
      return false;
    }

    QRectF box(first, first);

    for (int corner = 1; corner < 4; ++corner)
    {
      QPointF landed;

      // A corner that misses the terrain falls back to the ground plane
      // rather than failing the gesture — see groundOrPlaneUnder().
      if (!groundOrPlaneUnder(corners[corner], landed))
      {
        return false;
      }

      // Grown by hand rather than with united(), whose argument is a
      // zero-area rectangle around a point — and QRectF calls that null.
      box.setLeft(std::min(box.left(), landed.x()));
      box.setRight(std::max(box.right(), landed.x()));
      box.setTop(std::min(box.top(), landed.y()));
      box.setBottom(std::max(box.bottom(), landed.y()));
    }

    ground = box.normalized();

    return !ground.isEmpty();
  }

  void SceneView::selectIn(const QRect &pixels)
  {
    LayerStackModel *stack = m_renderer.model();

    QRectF ground;

    if (!stack || !groundRectUnder(pixels, ground))
    {
      return;
    }

    // layers() is top-first, so the first that catches anything is the one
    // the user can see — the same rule the map's band follows, because the
    // two views share one selection.
    for (MapLayer *layer : stack->layers())
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

      const QSet<int> caught = features->pickIn(ground);

      if (!caught.isEmpty())
      {
        stack->selectOnly(features, caught);

        Q_EMIT featurePicked(features, *caught.constBegin());

        return;
      }
    }

    stack->selectOnly(nullptr, QSet<int>{});

    Q_EMIT featurePicked(nullptr, -1);
  }

  void SceneView::applyBand(const QRect &rectangle, bool dragged,
                            const QPoint &pixel)
  {
    switch (m_toolKind)
    {
      case SceneToolKind::Select:
        if (dragged)
        {
          selectIn(rectangle);
        }
        else
        {
          int feature = -1;
          FeatureLayer *layer = pickAt(pixel, feature);

          if (LayerStackModel *stack = m_renderer.model())
          {
            stack->selectOnly(layer, feature);
          }

          Q_EMIT featurePicked(layer, feature);
        }

        break;

      case SceneToolKind::ZoomIn:
      {
        QRectF ground;

        if (dragged && groundRectUnder(rectangle, ground))
        {
          showGroundExtent(ground);
        }
        else
        {
          m_camera.dolly(1.0 / kBandZoomFactor);

          update();
          Q_EMIT cameraChanged();
        }

        break;
      }

      case SceneToolKind::ZoomOut:
      {
        // The viewport over the box, on whichever axis needs the most room:
        // fitting the view *into* the box is what makes this the inverse of
        // zooming in on the same box rather than merely the other direction.
        const double factor =
          dragged && width() > 0 && height() > 0
            ? std::min(double(rectangle.width()) / double(width()),
                       double(rectangle.height()) / double(height()))
            : 1.0 / kBandZoomFactor;

        if (factor > 0.0)
        {
          m_camera.dolly(1.0 / factor);

          update();
          Q_EMIT cameraChanged();
        }

        break;
      }

      case SceneToolKind::Orbit:
        break;
    }
  }

  bool SceneView::groundUnder(const QPoint &pixel, QPointF &ground) const
  {
    QVector3D origin;
    QVector3D direction;

    if (!m_camera.rayThrough(QPointF(pixel), size(), origin, direction))
    {
      return false;
    }

    const ITerrainSource *terrain = m_renderer.terrain();
    const Bounds3D bounds = m_renderer.sceneBounds();

    // Far enough to cross the whole scene from wherever the eye is, and no
    // farther: the march's step comes from the terrain, so an arbitrarily
    // large reach would be an arbitrarily large number of steps.
    const double reach =
      bounds.isValid()
        ? double((bounds.center() - origin).length()) + bounds.diagonal()
        : m_camera.farPlane();

    return rayGroundPoint(origin, direction, terrain, reach, ground);
  }

  FeatureLayer *SceneView::pickAt(const QPoint &pixel, int &feature) const
  {
    feature = -1;

    LayerStackModel *stack = m_renderer.model();

    QPointF ground;

    if (!stack || !groundUnder(pixel, ground))
    {
      return nullptr;
    }

    // The click tolerance, measured on the ground rather than assumed: a
    // pixel covers a different amount of world at the near edge of a tilted
    // view than at the far one, and under perspective the difference across
    // one screen is easily tenfold.
    QPointF offset;
    const double tolerance =
      groundUnder(pixel + QPoint(int(pickRadiusPixels()), 0), offset)
        ? std::hypot(offset.x() - ground.x(), offset.y() - ground.y())
        : 0.0;

    // layers() is top-first, which is the order the answer has to come in.
    for (MapLayer *layer : stack->layers())
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

      const int hit = features->pickAt(ground, tolerance);

      if (hit >= 0)
      {
        feature = hit;

        return features;
      }
    }

    return nullptr;
  }

  void SceneView::wheelEvent(QWheelEvent *event)
  {
    const double notches = event->angleDelta().y() / 120.0;

    if (qFuzzyIsNull(notches))
    {
      QRhiWidget::wheelEvent(event);

      return;
    }

    m_camera.dolly(std::pow(1.0 / kZoomPerNotch, notches));

    update();
    Q_EMIT cameraChanged();

    event->accept();
  }

} // namespace HydroCouple::Composer
