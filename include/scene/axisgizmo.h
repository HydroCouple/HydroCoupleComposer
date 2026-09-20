/*!
 * \file   axisgizmo.h
 * \author Caleb Buahin
 * \brief  The three-cone axis marker, and what clicking it means.
 *
 * The orientation cue every 3D view has: three arrows from a common
 * origin, coloured by the convention nobody has to be taught — red east,
 * green north, blue up. Drawn in a small square at a corner of the view,
 * under a projection that takes the camera's *rotation* only, so it never
 * slides or scales with the scene and is untouched by vertical
 * exaggeration (which scales the model matrix the gizmo does not use).
 *
 * Geometry and hit-testing are free functions over plain values, with no
 * renderer and no widget in sight: the part of a gizmo that can be wrong
 * about which way north is, is exactly the part that can be checked
 * without a graphics device — and QRhiWidget cannot make one under the
 * offscreen platform anyway (D31).
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_AXISGIZMO_H
#define HYDROCOUPLECOMPOSER_SCENE_AXISGIZMO_H

#include "scene/scenegeometry.h"

#include <QMatrix4x4>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>

namespace HydroCouple::Composer
{
  /*!
   * \brief Which arm of the gizmo a click landed on.
   */
  enum class GizmoAxis
  {
    None,   //!< Not on an arm.
    East,   //!< +X, red.
    North,  //!< +Y, green.
    Up,     //!< +Z, blue.
  };

  /*!
   * \brief Which corner of the view the gizmo sits in.
   */
  enum class GizmoCorner
  {
    BottomLeft,
    BottomRight,
    TopLeft,
    TopRight,
  };

  /*!
   * \brief The gizmo's geometry, in its own unit-ish space.
   *
   * Arms run from the origin to \c ±1 along each axis; the caller's
   * projection decides how big that ends up on screen. Built once and
   * cached: it never changes, because the camera moves around it rather
   * than it moving.
   *
   * \param coneSegments Sides of each arrowhead; 3 is the useful minimum.
   */
  [[nodiscard]] SceneGeometry buildAxisGizmo(int coneSegments = 12);

  /*!
   * \brief The corner named by \a name, or BottomLeft if it names none.
   *
   * The names are what preferences stores — "BottomLeft", "BottomRight",
   * "TopLeft", "TopRight" — kept here beside the enum rather than in the
   * preferences manager, which every layer reads and which therefore has
   * no business knowing what a gizmo is.
   *
   * An unrecognised name is a corner rather than an error: a settings
   * file hand-edited into nonsense should put the gizmo somewhere, not
   * make the view undrawable.
   */
  [[nodiscard]] GizmoCorner gizmoCornerFromName(const QString &name);

  //! The stored name for \a corner; the inverse of gizmoCornerFromName().
  [[nodiscard]] QString gizmoCornerName(GizmoCorner corner);

  /*!
   * \brief Where the gizmo is drawn inside a view of \a viewport pixels.
   *
   * A square of \a size a side, inset from the named corner, clamped so
   * that a gizmo larger than the view it sits in shrinks to fit rather
   * than hanging off the edge or turning the viewport negative.
   */
  [[nodiscard]] QRect axisGizmoRect(const QSize &viewport, int size,
                                    GizmoCorner corner);

  /*!
   * \brief \a point in \a rect, expressed in the gizmo's -1…+1 square.
   *
   * Y is flipped on the way: widget coordinates grow downward and clip
   * space grows upward, and a gizmo that answered "north" for a click on
   * its south arm would be worse than one that answered nothing.
   */
  [[nodiscard]] QPointF axisGizmoPoint(const QPoint &point,
                                       const QRect &rect);

  /*!
   * \brief The gizmo's view-projection for a camera at \a azimuth/\a elevation.
   *
   * Rotation only — no translation, no distance, no exaggeration. An
   * orthographic box a little larger than the arms, so an arrowhead
   * pointing at the viewer is not clipped by the near plane.
   *
   * \param azimuthDegrees Camera azimuth; 0 looks north.
   * \param elevationDegrees Camera elevation above the horizon.
   */
  [[nodiscard]] QMatrix4x4 axisGizmoMatrix(double azimuthDegrees,
                                           double elevationDegrees);

  /*!
   * \brief The arm under \a point, or None.
   *
   * \param point Position inside the gizmo's square, normalised to
   *        -1…+1 with +y up — the same space the matrix projects into.
   * \param azimuthDegrees Camera azimuth.
   * \param elevationDegrees Camera elevation.
   * \param tolerance How near an arrowhead counts as on it, in the same
   *        normalised units.
   * \returns The arm whose head is nearest \a point within \a tolerance,
   *          preferring the one closest to the viewer when two overlap.
   */
  [[nodiscard]] GizmoAxis axisGizmoHit(const QPointF &point,
                                       double azimuthDegrees,
                                       double elevationDegrees,
                                       double tolerance = 0.35);

  /*!
   * \brief The camera angles that look along \a axis.
   *
   * Looking *at* the scene down that axis: Up gives a plan view, North and
   * East give elevations. The azimuth for Up is left to the caller, since
   * a plan view has no meaningful one — it keeps whatever it had, so
   * north stays where the user last put it.
   *
   * \param axis The arm clicked.
   * \param[out] azimuthDegrees Receives the azimuth; untouched for Up.
   * \param[out] elevationDegrees Receives the elevation.
   * \returns False for None, leaving both untouched.
   */
  bool axisGizmoView(GizmoAxis axis, double &azimuthDegrees,
                     double &elevationDegrees);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_AXISGIZMO_H
