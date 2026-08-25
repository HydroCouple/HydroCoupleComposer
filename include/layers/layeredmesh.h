/*!
 * \file   layeredmesh.h
 * \author Caleb Buahin
 * \brief  LayeredMesh — a sigma-layered water column mesh, for the 3D scene.
 *
 * A horizontal mesh plus a vertical discretisation: every 2-D face becomes a
 * column of prismatic cells, stacked between the bed and the water surface.
 * This is what FVQual solves on and what its layered UGRID files describe, so
 * the conventions here are deliberately FVQual's rather than convenient ones:
 *
 *   - **Interface 0 is the surface**, interface `layerCount` is the bed. A
 *     viewer that assumed the opposite would draw every reservoir upside
 *     down and look entirely plausible doing it.
 *   - Interface elevations are indexed `column * (layerCount + 1) + k`, and
 *     cell values `column * layerCount + k` — column-major, vertical index
 *     fastest, matching `FVQual::Mesh::LayeredMesh::interfaceSlot()` and
 *     `cell()`. Reading a field written by FVQual with the other order
 *     transposes the water column silently.
 *   - Elevations are metres, positive up.
 *
 * Interfaces are per *column*, not per node, because the cells are finite
 * volumes: a cell's top and bottom are flat and its sides are vertical.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_LAYEREDMESH_H
#define HYDROCOUPLECOMPOSER_LAYERS_LAYEREDMESH_H

#include "hydrocouplesdk/io/meshdefinition.h"

#include <QString>

#include <cstdint>
#include <vector>

namespace HydroCouple::Composer
{

  /*!
   * \brief A horizontal mesh with a vertical layering.
   */
  struct LayeredMesh
  {
      //! The plan-view mesh. Its faces are the columns.
      HydroCouple::SDK::IO::MeshDefinition horizontal;

      //! Number of layers in every column.
      int layerCount = 0;

      /*!
       * \brief Interface elevations, `columnCount() * interfaceCount()` long.
       *
       * Indexed by interfaceSlot(); surface first within each column.
       */
      std::vector<double> interfaceZ;

      //! Columns, one per horizontal face.
      [[nodiscard]] int64_t columnCount() const
      {
        return horizontal.faceCount();
      }

      //! Interfaces per column: one more than the layers between them.
      [[nodiscard]] int interfaceCount() const
      {
        return layerCount + 1;
      }

      //! Total cells.
      [[nodiscard]] int64_t cellCount() const
      {
        return columnCount() * static_cast<int64_t>(layerCount);
      }

      //! Index into interfaceZ for \a column's \a k-th interface.
      [[nodiscard]] int64_t interfaceSlot(int64_t column, int k) const
      {
        return column * static_cast<int64_t>(interfaceCount()) + k;
      }

      //! Index into a per-cell field for \a column's \a k-th layer.
      [[nodiscard]] int64_t cell(int64_t column, int k) const
      {
        return column * static_cast<int64_t>(layerCount) + k;
      }

      //! Elevation of \a column's \a k-th interface.
      [[nodiscard]] double z(int64_t column, int k) const
      {
        return interfaceZ[static_cast<size_t>(interfaceSlot(column, k))];
      }

      /*!
       * \brief Whether the mesh is usable, and why not when it is not.
       *
       * Checked rather than assumed because the elevations arrive from a
       * file: a column count that disagrees with the interface array is the
       * difference between a wrong picture and a read past the end.
       *
       * \param[out] message Diagnostic on failure.
       */
      [[nodiscard]] bool isValid(QString &message) const;

      /*!
       * \brief Builds interface elevations from a sigma distribution.
       *
       * The CF form FVQual writes: `z = eta + sigma * (depth + eta)`, with
       * sigma running 0 at the surface to -1 at the bed and depth positive
       * down. Composer converts once, here, rather than in every caller.
       *
       * \param horizontal The plan-view mesh.
       * \param cfSigmaInterfaces Interfaces in CF convention, 0 to -1.
       * \param depthPerColumn Bed depth below datum, positive down.
       * \param surfacePerColumn Water surface elevation, positive up.
       * \param[out] message Diagnostic on failure.
       * \returns The mesh, or one whose layerCount is zero on failure.
       */
      [[nodiscard]] static LayeredMesh fromCfSigma(
        HydroCouple::SDK::IO::MeshDefinition horizontal,
        const std::vector<double> &cfSigmaInterfaces,
        const std::vector<double> &depthPerColumn,
        const std::vector<double> &surfacePerColumn, QString &message);
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_LAYEREDMESH_H
