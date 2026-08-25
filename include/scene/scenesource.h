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
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H
#define HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H

#include "scene/scenegeometry.h"

#include <QVector>

namespace HydroCouple::Composer
{

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
       */
      [[nodiscard]] virtual QVector<SceneGeometry> sceneGeometry() const = 0;

      /*!
       * \brief The box the geometry occupies, in world coordinates.
       *
       * Answerable without building the geometry, because framing the view
       * must not cost a full tessellation of every layer in the stack.
       */
      [[nodiscard]] virtual Bounds3D sceneBounds() const = 0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H
