/*!
 * \file   terrainray.h
 * \author Caleb Buahin
 * \brief  Where a ray from the 3D camera lands on the ground.
 *
 * The bridge between picking in the scene and picking in the map. Once a ray
 * is turned into a place on the ground, the question "what is here" is the
 * one the map already answers, and the two views cannot disagree about the
 * answer because there is only one of them.
 *
 * The alternative — intersecting the ray with every triangle the scene draws
 * — would answer a slightly different question, and answer it differently
 * from the map on exactly the features whose 3D form is not their 2D one:
 * an extruded curtain, a peeled column of prisms. Those are named limits
 * below rather than silent ones.
 */

#ifndef HYDROCOUPLECOMPOSER_PICK_TERRAINRAY_H
#define HYDROCOUPLECOMPOSER_PICK_TERRAINRAY_H

#include <QPointF>
#include <QVector3D>

namespace HydroCouple::Composer
{
  class ITerrainSource;

  /*!
   * \brief Where a ray meets the ground.
   *
   * With a terrain, the first crossing of its surface, found by marching the
   * ray at the terrain's own sample spacing and then bisecting. Marching can
   * step over a ridge thinner than that spacing; halving the step is what
   * makes that unlikely rather than what makes it impossible, and it is why
   * the terrain is asked for its resolution rather than told one.
   *
   * Without a terrain, the z = 0 plane, which is where the map's geometry
   * lives.
   *
   * \param origin Ray start, in world coordinates.
   * \param direction Ray direction; need not be unit length.
   * \param terrain The surface to land on, or nullptr for the ground plane.
   * \param reach How far along the ray to look, in world units.
   * \param[out] ground Where it landed, in map-CRS x and y.
   * \returns False when the ray never meets the ground within \a reach.
   */
  [[nodiscard]] bool rayGroundPoint(const QVector3D &origin,
                                    const QVector3D &direction,
                                    const ITerrainSource *terrain,
                                    double reach, QPointF &ground);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PICK_TERRAINRAY_H
