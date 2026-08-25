#include "layers/layeredmesh.h"

#include <QObject>

namespace HydroCouple::Composer
{

  bool LayeredMesh::isValid(QString &message) const
  {
    if (layerCount < 1)
    {
      message = QObject::tr("The mesh has no layers.");

      return false;
    }

    if (columnCount() < 1)
    {
      message = QObject::tr("The mesh has no columns.");

      return false;
    }

    const int64_t expected =
      columnCount() * static_cast<int64_t>(interfaceCount());

    if (static_cast<int64_t>(interfaceZ.size()) != expected)
    {
      message = QObject::tr("The mesh has %1 columns of %2 interfaces, which "
                            "needs %3 elevations, but %4 were given.")
                  .arg(columnCount())
                  .arg(interfaceCount())
                  .arg(expected)
                  .arg(interfaceZ.size());

      return false;
    }

    return true;
  }

  LayeredMesh LayeredMesh::fromCfSigma(
    HydroCouple::SDK::IO::MeshDefinition horizontal,
    const std::vector<double> &cfSigmaInterfaces,
    const std::vector<double> &depthPerColumn,
    const std::vector<double> &surfacePerColumn, QString &message)
  {
    LayeredMesh mesh;
    mesh.horizontal = std::move(horizontal);

    const int64_t columns = mesh.columnCount();

    if (cfSigmaInterfaces.size() < 2)
    {
      message = QObject::tr("A sigma coordinate needs at least two "
                            "interfaces; %1 were given.")
                  .arg(cfSigmaInterfaces.size());

      return {};
    }

    if (static_cast<int64_t>(depthPerColumn.size()) != columns ||
        static_cast<int64_t>(surfacePerColumn.size()) != columns)
    {
      message = QObject::tr("The mesh has %1 columns, but %2 depths and %3 "
                            "surface elevations were given.")
                  .arg(columns)
                  .arg(depthPerColumn.size())
                  .arg(surfacePerColumn.size());

      return {};
    }

    mesh.layerCount = static_cast<int>(cfSigmaInterfaces.size()) - 1;
    mesh.interfaceZ.resize(
      static_cast<size_t>(columns * mesh.interfaceCount()));

    for (int64_t column = 0; column < columns; ++column)
    {
      const double eta = surfacePerColumn[static_cast<size_t>(column)];
      const double depth = depthPerColumn[static_cast<size_t>(column)];

      // z = eta + sigma * (depth + eta): at sigma 0 the surface, at sigma -1
      // the bed, since depth is measured positive down from the datum.
      const double column_depth = depth + eta;

      for (int k = 0; k < mesh.interfaceCount(); ++k)
      {
        mesh.interfaceZ[static_cast<size_t>(mesh.interfaceSlot(column, k))] =
          eta + cfSigmaInterfaces[static_cast<size_t>(k)] * column_depth;
      }
    }

    return mesh;
  }

} // namespace HydroCouple::Composer
