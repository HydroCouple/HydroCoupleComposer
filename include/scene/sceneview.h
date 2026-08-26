/*!
 * \file   sceneview.h
 * \author Caleb Buahin
 * \brief  SceneView — the widget the 3D scene is shown in.
 *
 * Deliberately thin. Everything that decides what a frame looks like lives in
 * SceneRenderer and Camera, which is what lets the scene be tested at all:
 * QRhiWidget cannot create a graphics device under the offscreen platform
 * plugin, so a renderer living inside this class would be unverifiable.
 * What is left here is the part that genuinely needs a window — a device to
 * draw into, and the mouse.
 *
 * The camera is a member rather than a caller's object because interaction is
 * this widget's job and a camera that could be swapped mid-drag would leave
 * the drag with nothing to move.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_SCENEVIEW_H
#define HYDROCOUPLECOMPOSER_SCENE_SCENEVIEW_H

#include "map/maplayer.h"
#include "scene/camera.h"
#include "scene/scenerenderer.h"

#include <QColor>
#include <QPoint>
#include <QRhiWidget>

namespace HydroCouple::Composer
{
  class FeatureLayer;
  class LayerStackModel;
  class MapLayer;

  /*!
   * \brief An orbitable 3D view of the layer stack.
   */
  class SceneView : public QRhiWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs an empty view.
       * \param parent Owning widget.
       */
      explicit SceneView(QWidget *parent = nullptr);

      ~SceneView() override;

      /*!
       * \brief Shows the layers of \a model.
       *
       * The same stack the map draws, so a layer hidden or restyled in the
       * tree changes in both views at once.
       *
       * \param model The stack to show; may be nullptr.
       */
      void setModel(LayerStackModel *model);

      /*!
       * \brief The stack being shown, or nullptr.
       */
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief The view, for reading or for setting it from the map.
       */
      [[nodiscard]] const Camera &camera() const;

      /*!
       * \brief Replaces the view.
       * \param camera The camera to adopt.
       */
      void setCamera(const Camera &camera);

      /*!
       * \brief Frames every layer that has 3D geometry.
       */
      void zoomToFullExtent();

      /*!
       * \brief The ground the view is looking at.
       *
       * The scene's half of the hand-off with the map. A tilted camera sees a
       * trapezoid of ground and this is its bounding rectangle, so handing it
       * to the map shows somewhat more than the scene did — which is the
       * honest answer, since the map has no way to show a trapezoid.
       *
       * Empty until there is something to look at — before the widget has a
       * size, or while no layer in the stack has 3D geometry. A view of
       * nothing is not a view, and handing one to the map would replace a
       * framing somebody chose with the default camera's couple of units
       * around the origin.
       */
      [[nodiscard]] QRectF groundExtent() const;

      /*!
       * \brief Looks at \a extent on the ground.
       *
       * The map's half of the hand-off. Only the framing moves: the camera
       * keeps its orientation, so switching to the 3D tab shows the ground
       * the map was showing, from wherever the scene was last looked at.
       *
       * \param extent World rectangle to frame; ignored when empty.
       */
      void showGroundExtent(const QRectF &extent);

      /*!
       * \brief Sets the display exaggeration of world Z.
       * \param factor Values above 1 make relief more legible.
       */
      void setVerticalExaggeration(double factor);

      /*!
       * \brief The colour drawn behind the scene.
       */
      [[nodiscard]] QColor backgroundColor() const;

      /*!
       * \brief Sets the colour drawn behind the scene.
       * \param color The background colour.
       */
      void setBackgroundColor(const QColor &color);

      [[nodiscard]] QSize sizeHint() const override;

      /*!
       * \brief The feature under \a pixel, and the layer it belongs to.
       *
       * The ray is turned into a place on the ground and the stack is then
       * asked the question the map already answers, so the two views cannot
       * disagree about what is at a coordinate. What that costs is the
       * features whose 3D form is not their 2D one: an extruded curtain is
       * picked where it stands rather than where its wall was clicked, and a
       * peeled column of prisms identifies the column rather than the cell.
       *
       * \param pixel Widget position, as a click gives.
       * \param[out] feature Index within the layer returned.
       * \returns The layer picked, or nullptr when nothing was under it.
       */
      [[nodiscard]] FeatureLayer *pickAt(const QPoint &pixel,
                                         int &feature) const;

      /*!
       * \brief Where a pixel of the viewport lands on the ground.
       *
       * Exposed because it is the whole of the correspondence between the
       * two views, and a correspondence nothing can measure is one that
       * quietly stops holding.
       *
       * \param pixel Widget position.
       * \param[out] ground Map-CRS position it lands on.
       * \returns False when the ray meets no ground.
       */
      [[nodiscard]] bool groundUnder(const QPoint &pixel,
                                     QPointF &ground) const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when a click selects a feature, or selects nothing.
       *
       * The same signal the map emits, carrying the base type for the same
       * reason: a listener wants to know what was picked and does not care
       * which view did the picking.
       *
       * \param layer The layer picked, or nullptr.
       * \param feature Index within \a layer, or -1.
       */
      void featurePicked(MapLayer *layer, int feature);

      /*!
       * \brief Emitted when the camera moves.
       *
       * How the map is kept in step with the scene: the ground the camera
       * sees is the rectangle the map should show.
       */
      void cameraChanged();

    protected:
      void initialize(QRhiCommandBuffer *cb) override;

      void render(QRhiCommandBuffer *cb) override;

      void releaseResources() override;

      void mousePressEvent(QMouseEvent *event) override;

      void mouseMoveEvent(QMouseEvent *event) override;

      void mouseReleaseEvent(QMouseEvent *event) override;

      void wheelEvent(QWheelEvent *event) override;

    private:
      //! Frames the whole scene the first time there is something to frame.
      void frameOnFirstGeometry();

      SceneRenderer m_renderer;
      Camera m_camera;
      QColor m_background = QColor(0x1a, 0x1d, 0x21);

      QPoint m_pressPosition;
      QPoint m_lastMousePosition;
      bool m_orbiting = false;
      bool m_panning = false;
      bool m_framed = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENEVIEW_H
