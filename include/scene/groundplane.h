/*!
 * \file   groundplane.h
 * \author Caleb Buahin
 * \brief  Drawing an image layer as a surface in the 3D scene.
 *
 * A basemap or a raster has no geometry of its own — it is a picture of a
 * place. Its 3D form is therefore the ground itself, wearing that picture:
 * a surface over the layer's extent, draped on whatever terrain the stack
 * holds, textured with what the map would have drawn there.
 *
 * The texture is made by asking the layer to render itself, through exactly
 * the MapTransform the 2D canvas would have used. That is not a shortcut. It
 * means the ground plane is *by construction* the same picture as the map,
 * including whatever reprojection, ramp or tile mosaic the layer does inside
 * its own render() — so a tiled basemap in Web Mercator and a raster warped
 * from a state plane both come out correct in the map's CRS without either
 * this file or the scene knowing anything about how.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_GROUNDPLANE_H
#define HYDROCOUPLECOMPOSER_SCENE_GROUNDPLANE_H

#include "scene/scenegeometry.h"

#include <QImage>
#include <QRectF>

namespace HydroCouple::Composer
{
  class ITerrainSource;
  class MapLayer;

  /*!
   * \brief Longest side of a ground plane's texture, in pixels.
   *
   * A megapixel of RGBA, which is 4 MB per image layer. Larger is sharper
   * only while the camera is close enough to want it, and a basemap that
   * costs more memory than the model drawn on it has the balance wrong.
   */
  constexpr int kGroundTexturePixels = 1024;

  /*!
   * \brief An image and the world rectangle it covers exactly.
   */
  struct GroundImage
  {
      QImage image;

      /*!
       * \brief The world rectangle \c image covers.
       *
       * Not the rectangle that was asked for: fitting an extent to a viewport
       * preserves aspect ratio and so usually shows more than was requested.
       * Taking the transform's own answer is what keeps the texture mapping
       * exact rather than nearly right along one axis.
       */
      QRectF extent;

      [[nodiscard]] bool isValid() const
      {
        return !image.isNull() && !extent.isEmpty();
      }
  };

  /*!
   * \brief Renders \a layer as the map would, into an image.
   *
   * \param layer The layer to draw.
   * \param extent World rectangle to cover.
   * \param maximumPixels Longest side of the image, in pixels.
   * \returns The image and the extent it actually covers.
   */
  [[nodiscard]] GroundImage renderLayerToImage(MapLayer &layer,
                                               const QRectF &extent,
                                               int maximumPixels);

  /*!
   * \brief A textured surface over \a ground, laid on \a terrain.
   *
   * Tessellated to the terrain's own sample spacing where there is one, and a
   * single quad where there is not: a flat ground plane needs no vertices to
   * be flat with, and spending them anyway is how a basemap comes to cost
   * more than the model drawn on it.
   *
   * \param ground The image and the extent it covers.
   * \param terrain The surface to lay it on, or nullptr for z = 0.
   * \returns The geometry, empty when \a ground is not valid.
   */
  [[nodiscard]] SceneGeometry buildGroundPlane(const GroundImage &ground,
                                               const ITerrainSource *terrain);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_GROUNDPLANE_H
