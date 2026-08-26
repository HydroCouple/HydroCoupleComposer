#include "scene/sceneview.h"

#include "layers/featurelayer.h"
#include "map/layerstackmodel.h"
#include "pick/terrainray.h"

#include <QMouseEvent>
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
     * not of the data — the same reasoning, and the same number, as the map's.
     */
    constexpr double kPickRadiusPixels = 6.0;

    /*!
     * \brief How far the mouse may move and still count as a click.
     *
     * Orbiting and picking share the left button, so they are told apart by
     * whether the scene turned.
     */
    constexpr int kClickSlopPixels = 3;

    //! Degrees of rotation per pixel dragged. Chosen so a drag across the
    //! width of a typical view is most of a turn, which is what makes
    //! orbiting feel like turning an object rather than nudging one.
    constexpr double kDegreesPerPixel = 0.4;

    constexpr double kZoomPerNotch = 1.15;

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

  void SceneView::zoomToFullExtent()
  {
    const Bounds3D bounds = m_renderer.sceneBounds();

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

    // Reframed, because exaggerating relief by an order of magnitude puts
    // most of the scene outside a view that was framed without it.
    zoomToFullExtent();
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

  void SceneView::render(QRhiCommandBuffer *cb)
  {
    frameOnFirstGeometry();

    m_renderer.render(cb, renderTarget(), m_camera, m_background);
  }

  void SceneView::releaseResources()
  {
    m_renderer.releaseResources();
  }

  void SceneView::mousePressEvent(QMouseEvent *event)
  {
    m_pressPosition = event->pos();
    m_lastMousePosition = event->pos();

    // Left orbits, middle and right pan — the convention every GIS 3D view
    // uses, so muscle memory carries over.
    m_orbiting = event->button() == Qt::LeftButton;
    m_panning = event->button() == Qt::MiddleButton ||
                event->button() == Qt::RightButton;

    QRhiWidget::mousePressEvent(event);
  }

  void SceneView::mouseMoveEvent(QMouseEvent *event)
  {
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
    const bool wasOrbiting = m_orbiting;

    m_orbiting = false;
    m_panning = false;

    // A left press that did not turn the scene was a click, not an orbit —
    // the same rule the map uses to tell a click from a pan, and for the same
    // reason: one button does both.
    const QPoint travelled = event->pos() - m_pressPosition;

    if (wasOrbiting && event->button() == Qt::LeftButton &&
        std::abs(travelled.x()) <= kClickSlopPixels &&
        std::abs(travelled.y()) <= kClickSlopPixels)
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
      groundUnder(pixel + QPoint(int(kPickRadiusPixels), 0), offset)
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
