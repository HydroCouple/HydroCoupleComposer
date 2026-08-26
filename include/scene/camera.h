/*!
 * \file   camera.h
 * \author Caleb Buahin
 * \brief  Camera — the 3D scene's view, and its correspondence with the map.
 *
 * The map has MapTransform; the scene has this. They are separate objects
 * because a 3D camera is not a 2D transform with a third number bolted on —
 * it has an orientation, a projection and a near/far range that the map has
 * no meaning for. What they do share is a *ground extent*: the part of the
 * world the user is looking at. Switching between the two views is defined
 * entirely in terms of that rectangle, which is why groundExtent() and
 * setGroundExtent() are the only members either view needs from the other.
 *
 * The camera is pure arithmetic and touches no GPU type, so every property
 * this file claims is testable without a graphics device. The one matrix it
 * deliberately does *not* apply is the backend's clip-space correction: that
 * belongs to whoever is rendering, since it differs between Metal, Vulkan and
 * OpenGL and would make the camera's output backend-dependent.
 *
 * Angles are degrees throughout, because every value a user ever types for a
 * camera is in degrees and a units mismatch here is invisible until the view
 * is wrong.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_CAMERA_H
#define HYDROCOUPLECOMPOSER_SCENE_CAMERA_H

#include "scene/scenegeometry.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QVector3D>

namespace HydroCouple::Composer
{

  /*!
   * \brief How the camera projects the scene.
   */
  enum class CameraProjection
  {
    /*!
     * \brief Parallel projection — the map's projection.
     *
     * A top-down orthographic camera reproduces MapTransform exactly, which
     * is what makes the 2D map and the 3D view two settings of one thing
     * rather than two renderers that happen to show the same data.
     */
    Orthographic,

    //! Perspective projection, for the oblique views 3D is wanted for.
    Perspective
  };

  /*!
   * \brief An orbiting camera over a world-coordinate scene.
   */
  class Camera
  {
    public:
      Camera();

      /*!
       * \brief The point the camera looks at and orbits about.
       */
      [[nodiscard]] QVector3D target() const;

      /*!
       * \brief Moves the camera's target without changing its orientation.
       * \param target New world-coordinate target.
       */
      void setTarget(const QVector3D &target);

      /*!
       * \brief Distance from the eye to the target, in world units.
       */
      [[nodiscard]] double distance() const;

      /*!
       * \brief Sets the eye-to-target distance.
       * \param distance Clamped to a small positive minimum.
       */
      void setDistance(double distance);

      /*!
       * \brief Rotation about the vertical axis, in degrees.
       *
       * Zero looks north — the eye is due south of the target — so that a
       * default 3D view and a north-up map agree on which way is up.
       */
      [[nodiscard]] double azimuth() const;

      /*!
       * \brief Sets the rotation about the vertical axis.
       * \param degrees Wrapped into [0, 360).
       */
      void setAzimuth(double degrees);

      /*!
       * \brief Angle above the horizon, in degrees; 90 looks straight down.
       */
      [[nodiscard]] double elevation() const;

      /*!
       * \brief Sets the angle above the horizon.
       *
       * Clamped short of the poles: exactly 90 degrees makes the view
       * direction parallel to the up vector and the view matrix degenerate.
       *
       * \param degrees Clamped to (-90, 90].
       */
      void setElevation(double degrees);

      /*!
       * \brief Vertical field of view in degrees; perspective only.
       */
      [[nodiscard]] double fieldOfView() const;

      /*!
       * \brief Sets the vertical field of view.
       * \param degrees Clamped to a sane (1, 179) range.
       */
      void setFieldOfView(double degrees);

      /*!
       * \brief The projection in use.
       */
      [[nodiscard]] CameraProjection projection() const;

      /*!
       * \brief Switches projection, preserving what is on screen.
       *
       * The two projections are matched through the ground extent, so a
       * switch does not jump: whatever ground was visible stays visible.
       *
       * \param projection The projection to use.
       * \param aspect Viewport width divided by height.
       */
      void setProjection(CameraProjection projection, double aspect);

      /*!
       * \brief The factor world Z is multiplied by before display.
       *
       * Terrain relief is routinely three orders of magnitude smaller than
       * the extent it covers, so an unexaggerated catchment renders as a flat
       * sheet. This is a display property, not a data one: it appears in the
       * model matrix and nothing else, so no geometry is rebuilt to change it
       * and no measurement reads it back.
       */
      [[nodiscard]] double verticalExaggeration() const;

      /*!
       * \brief Sets the vertical exaggeration.
       * \param factor Clamped to a small positive minimum.
       */
      void setVerticalExaggeration(double factor);

      /*!
       * \brief The eye position in world coordinates.
       */
      [[nodiscard]] QVector3D eye() const;

      /*!
       * \brief The scaling applied to geometry before viewing.
       */
      [[nodiscard]] QMatrix4x4 modelMatrix() const;

      /*!
       * \brief The world-to-eye transform.
       */
      [[nodiscard]] QMatrix4x4 viewMatrix() const;

      /*!
       * \brief The eye-to-clip transform.
       * \param aspect Viewport width divided by height.
       */
      [[nodiscard]] QMatrix4x4 projectionMatrix(double aspect) const;

      /*!
       * \brief Model, view and projection combined.
       * \param aspect Viewport width divided by height.
       */
      [[nodiscard]] QMatrix4x4 modelViewProjection(double aspect) const;

      /*!
       * \brief The world rectangle visible on the ground plane.
       *
       * Ground means z = 0 after exaggeration, which is where the map's
       * geometry lives. For a tilted camera the visible ground is a trapezoid
       * and this returns its bounding rectangle; rays that escape above the
       * horizon are cut at the far plane, so the answer stays finite at any
       * tilt.
       *
       * \param aspect Viewport width divided by height.
       */
      [[nodiscard]] QRectF groundExtent(double aspect) const;

      /*!
       * \brief Frames \a extent on the ground plane.
       *
       * Aspect ratio is preserved, so the visible ground contains the
       * requested rectangle and usually exceeds it on one axis — the same
       * contract as MapTransform::fit, which is what makes switching between
       * the views lossless in the case that matters: a rectangle whose aspect
       * already matches the viewport comes back unchanged.
       *
       * Containment holds at any tilt, which takes solving rather than simply
       * looking at the rectangle's centre. The target is left on the ground
       * plane, since framing ground is what was asked for. Qt's vector maths
       * is single precision, so it holds to about a part in 10^5 rather than
       * to the last bit.
       *
       * \param extent World rectangle that must be visible.
       * \param aspect Viewport width divided by height.
       */
      void setGroundExtent(const QRectF &extent, double aspect);

      /*!
       * \brief Frames a whole box, exaggeration included.
       *
       * Frames the box's bounding sphere rather than its ground footprint,
       * so the fit survives being orbited to any angle afterwards — and, more
       * immediately, so a concave scene is not fitted from inside itself.
       * The target is raised off the ground to the middle of the relief, so
       * groundExtent() afterwards is not the footprint.
       *
       * \param bounds Box that must be visible; ignored when empty.
       * \param aspect Viewport width divided by height.
       */
      void fitTo(const Bounds3D &bounds, double aspect);

      /*!
       * \brief Rotates about the target.
       * \param deltaAzimuth Degrees to turn.
       * \param deltaElevation Degrees to raise.
       */
      void orbit(double deltaAzimuth, double deltaElevation);

      /*!
       * \brief Slides the target across the view plane.
       * \param pixels Drag in widget pixels.
       * \param viewportHeight Viewport height in pixels.
       * \param aspect Viewport width divided by height.
       */
      void pan(const QPointF &pixels, double viewportHeight, double aspect);

      /*!
       * \brief Moves the eye toward or away from the target.
       *
       * The target is held, so dollying never loses what is being looked at.
       *
       * \param factor Values below 1 move closer.
       */
      void dolly(double factor);

      /*!
       * \brief The world-space ray through a pixel of the viewport.
       *
       * In *world* coordinates, with the model matrix undone — so the answer
       * does not move when vertical exaggeration changes, and a pick lands on
       * the same feature at any exaggeration. That is the whole reason the
       * model matrix is a display property.
       *
       * The backend's clip-space correction is deliberately absent, as it is
       * everywhere else in this class: it differs between Metal, Vulkan and
       * OpenGL, and a ray that depended on it would pick differently on
       * different machines.
       *
       * \param pixel Position within the viewport, y down as a widget gives.
       * \param viewport Viewport size in pixels.
       * \param[out] origin Where the ray starts, on the near plane.
       * \param[out] direction Which way it goes; unit length.
       * \returns False when the viewport is degenerate.
       */
      [[nodiscard]] bool rayThrough(const QPointF &pixel,
                                    const QSize &viewport, QVector3D &origin,
                                    QVector3D &direction) const;

      /*!
       * \brief The near and far clip distances for the current view.
       *
       * Derived rather than stored: a fixed near plane either clips the scene
       * when the camera is close or destroys depth precision when it is far,
       * and both look like renderer bugs.
       */
      [[nodiscard]] double nearPlane() const;

      /*!
       * \brief \copybrief nearPlane
       */
      [[nodiscard]] double farPlane() const;

    private:
      //! Half the visible ground height at the target, in world units.
      [[nodiscard]] double groundHalfHeight() const;

      //! Sets the one length that sizes the view, whichever it is for the
      //! projection in use, so a scale factor means the same thing in both.
      void applyGroundScale(double scale);

      QVector3D m_target;
      double m_distance = 1.0;
      double m_azimuth = 0.0;
      double m_elevation = 45.0;
      double m_fieldOfView = 45.0;
      double m_verticalExaggeration = 1.0;
      double m_orthoHalfHeight = 1.0;
      CameraProjection m_projection = CameraProjection::Perspective;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_CAMERA_H
