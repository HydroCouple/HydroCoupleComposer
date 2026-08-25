/*!
 * \file   scenesource.h
 * \author Caleb Buahin
 * \brief  ISceneSource — a layer's contribution to the 3D scene.
 *
 * Asked of every layer in the stack, exactly as LayerStyle is: the scene puts
 * one question to the whole stack and builds nothing for the layers that
 * answer nullptr. That is what lets a basemap, a tile layer and a labelled
 * point layer sit in the same stack as a terrain mesh without any of them
 * knowing a 3D view exists.
 *
 * It is a separate interface rather than a method on MapLayer because most
 * layers have no 3D form and would carry an empty override forever — the same
 * reasoning that kept labelling and masks off MapLayer's base.
 *
 * Geometry is built against a SceneContext rather than from the layer alone,
 * because draping is not a layer-local property: where a network line sits in
 * space is decided by the terrain it is drawn over, which belongs to a
 * different layer entirely. Handing that context in keeps the layer ignorant
 * of the stack — it is told what it is being composed with, it does not go
 * looking.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H
#define HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H

#include "scene/scenegeometry.h"

#include <QPointF>
#include <QRectF>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief Answers how high the ground is, for the layers drawn over it.
   *
   * The 3D counterpart of a basemap: one layer in the stack defines the
   * surface, and everything else in the stack is composed against it. Kept
   * apart from ISceneSource because supplying geometry and being a surface
   * others rest on are different capabilities — a network line has the first
   * and not the second.
   *
   * Coordinates are the map's CRS, not the layer's own. A terrain answering
   * in its own projection would be sampled with points from another one, and
   * the drape would be wrong by however far the two disagree — which on a
   * small extent is small enough to look like a modelling artefact.
   */
  class ITerrainSource
  {
    public:
      virtual ~ITerrainSource() = default;

      /*!
       * \brief The ground elevation at \a point.
       *
       * \param point Map-CRS position to sample.
       * \param[out] elevation The ground elevation there.
       * \returns True when \a point lies on the terrain; false leaves
       *          \a elevation untouched, so a caller can decide between
       *          skipping the point and holding the last good value.
       */
      [[nodiscard]] virtual bool elevationAt(const QPointF &point,
                                             double &elevation) const = 0;

      /*!
       * \brief The map-CRS footprint the terrain answers over.
       */
      [[nodiscard]] virtual QRectF terrainExtent() const = 0;

      /*!
       * \brief A characteristic spacing between samples, in map units.
       *
       * What a drape densifies to. Asked of the terrain rather than guessed
       * at by the layer being draped, because the only length that matters
       * here is the one over which the surface can change — a step finer than
       * the terrain's own cells buys nothing but vertices, and one coarser
       * cuts corners off the relief it was supposed to follow.
       */
      [[nodiscard]] virtual double terrainResolution() const = 0;
  };

  /*!
   * \brief What a layer is being composed with, when it builds its geometry.
   */
  struct SceneContext
  {
      //! The surface to drape on, or nullptr when the stack has none.
      const ITerrainSource *terrain = nullptr;
  };

  /*!
   * \brief Supplies geometry for the 3D scene.
   */
  class ISceneSource
  {
    public:
      virtual ~ISceneSource() = default;

      /*!
       * \brief The geometry batches to draw, in draw order.
       *
       * A list rather than one batch because a single layer routinely needs
       * more than one primitive type — a filled surface and the edges that
       * make its structure legible cannot share an index buffer.
       *
       * Building is on demand and may be expensive; callers cache.
       *
       * \param context What the layer is being composed with.
       */
      [[nodiscard]] virtual QVector<SceneGeometry> sceneGeometry(
        const SceneContext &context) const = 0;

      /*!
       * \brief The box the geometry occupies, in world coordinates.
       *
       * Answerable without building the geometry, because framing the view
       * must not cost a full tessellation of every layer in the stack.
       */
      [[nodiscard]] virtual Bounds3D sceneBounds() const = 0;

      /*!
       * \brief This source as a surface others may rest on, or nullptr.
       *
       * Hung off the scene source rather than off MapLayer because only
       * something already contributing to the scene can be terrain for it:
       * a surface nothing draws is a surface nothing can be checked against.
       */
      [[nodiscard]] virtual const ITerrainSource *terrain() const
      {
        return nullptr;
      }
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H
