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
#include "scene/axisgizmo.h"
#include "scene/camera.h"
#include "scene/navigation.h"
#include "scene/scenegeometry.h"
#include "scene/scenerenderer.h"

#include <QColor>
#include <QPoint>
#include <QRhiWidget>

#include <memory>

class QRubberBand;

namespace HydroCouple::Composer
{
  /*!
   * \brief Which gesture set the 3D view is under.
   *
   * The map's four, with Orbit where Pan is: the two views are read the same
   * way and should be driven the same way, and turning a scene is what
   * dragging in one is for.
   */
  enum class SceneToolKind
  {
    //! Drag to orbit, click to identify. The default.
    Orbit,

    //! Click to select one feature; drag a box to take what it covers.
    Select,

    //! Drag a box to frame the ground under it; click to move closer.
    ZoomIn,

    //! Drag a box to fit the view into it; click to move away.
    ZoomOut,
  };

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
       * \brief Looks at \a bounds, keeping the camera's orientation.
       *
       * The framing zoomToFullExtent() applies to everything, offered for
       * one thing -- a layer, a selection. Invalid bounds frame nothing.
       */
      void frameBounds(const Bounds3D &bounds);

      /*!
       * \brief Moves the camera one step closer, as one wheel notch does.
       *
       * The step zoom the shortcut actions need: the target is held, so
       * zooming never loses what is being looked at.
       */
      void zoomIn();

      /*!
       * \brief Moves the camera one step further out.
       */
      void zoomOut();

      /*!
       * \brief Points the camera at a view the user named.
       *
       * The framing is left alone: "look down" is a question about
       * orientation, and answering it by also re-framing would undo a
       * zoom the user chose. What moves is the camera's angles, which is
       * all the names describe.
       */
      void showNamedView(NamedView view);

      /*!
       * \brief Frames what is selected, or does nothing if nothing is.
       *
       * Deliberately not "frame everything when nothing is selected".
       * Pressing this with an empty selection and having the view leap
       * to the whole model is the kind of helpfulness that loses someone
       * the position they spent a minute finding.
       *
       * \returns False when there was no selection to look at, so a
       *          caller can say so rather than leaving the press looking
       *          like it failed.
       */
      bool lookAtSelection();

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

      //! \returns The exaggeration currently applied to world Z.
      [[nodiscard]] double verticalExaggeration() const;

      //! \returns Whether the view is drawn in perspective or parallel.
      [[nodiscard]] CameraProjection projection() const;

      /*!
       * \brief Switches between perspective and parallel projection.
       *
       * The camera matches the two through the ground extent, so what is on
       * screen stays on screen across the switch — which is the whole point:
       * a toggle that reframed would read as a navigation command rather than
       * a change of how the same view is drawn.
       *
       * \param projection The projection to draw in.
       */
      void setProjection(CameraProjection projection);

      //! \returns Which gesture set the view is under.
      [[nodiscard]] SceneToolKind toolKind() const;

      /*!
       * \brief Switches gesture set.
       *
       * A band in progress is abandoned, so one belonging to a tool that is
       * no longer active cannot be left on screen.
       *
       * \param kind The gestures wanted.
       */
      void setToolKind(SceneToolKind kind);

      /*!
       * \brief The ground rectangle a screen rectangle covers.
       *
       * The bounding rectangle of where its four corners land, which is
       * exact looking straight down and generous when tilted — a tilted
       * camera sees a trapezoid, and its bounding box takes in a little
       * more ground than the pixels enclosed. Stated rather than hidden:
       * the alternative is a quadrilateral test that the layers have no
       * entry point for.
       *
       * \param pixels Screen rectangle.
       * \param[out] ground The ground it covers, in the map's CRS.
       * \returns False when a corner does not land on the terrain at all.
       */
      [[nodiscard]] bool groundRectUnder(const QRect &pixels,
                                         QRectF &ground) const;

      /*!
       * \brief Selects every feature under \a pixels.
       *
       * In the topmost visible layer that catches anything, exactly as the
       * map's band does — the two views share one selection, so they have to
       * share the rule that decides it.
       *
       * \param pixels Screen rectangle to select within.
       */
      void selectIn(const QRect &pixels);

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

      /*!
       * \brief Acts on a finished band, per the active tool.
       * \param rectangle The band's geometry, in widget pixels.
       * \param dragged False when it was too small to be a drag.
       * \param pixel Where the release landed.
       */
      void applyBand(const QRect &rectangle, bool dragged,
                     const QPoint &pixel);

      /*!
       * \brief Where \a pixel meets the terrain, or failing that the ground.
       *
       * A band's corner routinely points past the edge of a terrain that
       * covers only the modelled catchment, and refusing the whole gesture
       * because one corner did would make the tool unusable exactly where
       * models end. The plane at z = 0 is where the map's own geometry
       * lives, so it is the right answer there rather than a guess.
       *
       * Distinct from groundUnder(), which a *click* uses and where failing
       * is correct: clicking the sky should pick nothing.
       *
       * \param pixel Widget position.
       * \param[out] ground Where it landed, in the map's CRS.
       * \returns False only when the ray meets neither.
       */
      [[nodiscard]] bool groundOrPlaneUnder(const QPoint &pixel,
                                            QPointF &ground) const;

      /*!
       * \brief The orientation cue's square, in widget coordinates.
       *
       * Empty when the cue is switched off or the view has no room for
       * one. This is the single place the corner is worked out: the
       * renderer is handed the same rectangle scaled to device pixels,
       * so a click can never land on a cue drawn somewhere else.
       */
      [[nodiscard]] QRect gizmoRect() const;

      /*!
       * \brief Answers a press on the cue, if that is what it was.
       *
       * \returns True when the press was consumed, so the caller knows
       *          not to start an orbit with it as well.
       */
      [[nodiscard]] bool takeGizmoPress(const QPoint &pixel);

      SceneRenderer m_renderer;

      Camera m_camera;
      QColor m_background = QColor(0x1a, 0x1d, 0x21);

      QPoint m_pressPosition;
      QPoint m_lastMousePosition;
      bool m_orbiting = false;
      bool m_panning = false;
      bool m_banding = false;

      SceneToolKind m_toolKind = SceneToolKind::Orbit;

      //! Built on first use, because a view nobody drags never needs one.
      std::unique_ptr<QRubberBand> m_band;
      bool m_framed = false;

      //! How the view is driven, as preferences last described it.
      double m_orbitDegreesPerPixel = 0.4;
      bool m_invertWheel = false;
      PanModifier m_panModifier = PanModifier::MiddleDrag;

      //! The orientation cue, as preferences last described it.
      bool m_showGizmo = true;
      int m_gizmoSize = 96;
      GizmoCorner m_gizmoCorner = GizmoCorner::BottomLeft;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENEVIEW_H
