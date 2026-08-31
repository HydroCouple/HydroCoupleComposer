/*!
 * \file terrainsampler.cpp
 * \brief TerrainSampler implementation.
 */

#include "mesh/terrainsampler.h"

#include "gis/spatialreference.h"
#include "layers/gdalrasterlayer.h"

#include <QPointF>
#include <QVector>

#include <cmath>

namespace HydroCouple::Composer
{
  TerrainSampleResult sampleTerrain(const GdalRasterLayer &raster,
                                    const SpatialReference *meshCrs,
                                    HydroCouple::SDK::IO::MeshDefinition &mesh)
  {
    TerrainSampleResult result;

    const size_t count = mesh.nodeX.size();

    if (count == 0 || mesh.nodeY.size() != count)
    {
      result.message = QObject::tr("That mesh has no vertices to sample.");

      return result;
    }

    QVector<QPointF> points;
    points.reserve(static_cast<int>(count));

    for (size_t i = 0; i < count; ++i)
    {
      points.append(QPointF(mesh.nodeX[i], mesh.nodeY[i]));
    }

    // Into the raster's own system, because that is the only place its cells
    // mean anything. A mesh in degrees sampled against a raster in national
    // grid metres does not fail -- every vertex simply lands outside it, and
    // the answer is a mesh with no elevations and no explanation.
    if (meshCrs && raster.crs() && !meshCrs->isSameAs(*raster.crs()))
    {
      QString message;

      const std::unique_ptr<CoordinateTransform> toRaster =
        CoordinateTransform::between(*meshCrs, *raster.crs(), message);

      if (!toRaster)
      {
        result.message =
          QObject::tr("The mesh and the elevations cannot be lined up: %1")
            .arg(message);

        return result;
      }

      for (QPointF &point : points)
      {
        bool ok = true;
        const QPointF moved = toRaster->transform(point, &ok);

        // A point the projection cannot carry -- far outside the system's
        // area of use -- is left where it was and misses, rather than
        // landing somewhere plausible and wrong.
        if (ok)
        {
          point = moved;
        }
      }
    }

    QVector<double> values;
    QString message;

    if (!raster.sample(points, values, message))
    {
      result.message = message;

      return result;
    }

    mesh.nodeZ.assign(count, std::numeric_limits<double>::quiet_NaN());

    for (size_t i = 0; i < count; ++i)
    {
      const double value = values.at(static_cast<int>(i));

      if (std::isnan(value))
      {
        ++result.missed;

        continue;
      }

      mesh.nodeZ[i] = value;
      ++result.sampled;
    }

    // Nothing sampled is a failure worth naming rather than a mesh of NaNs:
    // it almost always means the two are nowhere near each other.
    if (result.sampled == 0)
    {
      mesh.nodeZ.clear();
      result.message =
        QObject::tr("None of the mesh's %1 vertices fall on those "
                    "elevations.")
          .arg(count);

      return result;
    }

    result.ok = true;

    return result;
  }

} // namespace HydroCouple::Composer
