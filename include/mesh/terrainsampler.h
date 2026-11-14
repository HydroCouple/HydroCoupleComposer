/*!
 * \file   terrainsampler.h
 * \author Caleb Buahin
 * \brief  Putting a coverage's elevations onto a mesh's vertices.
 *
 * The step that turns a fetched raster from a picture behind a model into
 * data inside one. A WCS coverage of ground elevations is millions of
 * measurements; a mesh is tens of thousands of vertices that need one each.
 *
 * Deliberately a free function over a GdalRasterLayer rather than something
 * that knows about coverages. A WcsCoverageLayer IS one of those, and so is
 * a GeoTIFF opened from disk — where the elevations came from is not this
 * step's business, and a modeller with a local survey should not be made to
 * publish it to a service first.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_TERRAINSAMPLER_H
#define HYDROCOUPLECOMPOSER_MESH_TERRAINSAMPLER_H

#include <hydrocouplesdk/io/meshdefinition.h>

#include <QString>

namespace HydroCouple::Composer
{
  class GdalRasterLayer;
  class SpatialReference;

  /*!
   * \brief What sampling a terrain onto a mesh did.
   *
   * Reported per vertex rather than as a single yes, because the interesting
   * outcome is almost never total: a mesh usually reaches a little past the
   * ground that was fetched, and a coverage usually has holes in it.
   */
  struct TerrainSampleResult
  {
      bool ok = false;

      //! What went wrong, when nothing could be sampled at all.
      QString message;

      //! Vertices that got an elevation.
      int sampled = 0;

      /*!
       * \brief Vertices the coverage does not reach, or has no value for.
       *
       * Left as they were rather than zeroed. A vertex quietly set to zero
       * is a hole punched to sea level in the middle of a catchment, and it
       * looks like data.
       */
      int missed = 0;
  };

  /*!
   * \brief Writes \a raster's values into \a mesh as node elevations.
   *
   * \param raster   The elevations to read.
   * \param meshCrs  The system \a mesh's coordinates are in. Null means it
   *                 is already in the raster's, which is asserted rather
   *                 than assumed: sampling a mesh in degrees against a
   *                 raster in metres silently misses every vertex.
   * \param mesh     Modified in place; nodeZ is filled.
   * \returns What happened, per vertex.
   */
  [[nodiscard]] TerrainSampleResult sampleTerrain(
    const GdalRasterLayer &raster, const SpatialReference *meshCrs,
    HydroCouple::SDK::IO::MeshDefinition &mesh);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_TERRAINSAMPLER_H
