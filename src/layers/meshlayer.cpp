#include "layers/meshlayer.h"

#include "gis/spatialreference.h"

#include "hydrocouplesdk/io/ugridreader.h"

#include "hydrocouple.h"
#include "hydrocouplespatial.h"

#include <QFileInfo>
#include <QObject>

#include <vector>

namespace HydroCouple::Composer
{
  using HydroCouple::SDK::IO::MeshDefinition;

  namespace
  {
    namespace Spatial = HydroCouple::Spatial;

    /*!
     * \brief Builds a mesh from a regular grid's nodes and active cells.
     *
     * \param grid The grid to convert.
     * \param[out] cellIndex Receives the grid cell each face came from, so
     *        values indexed by cell can be matched to the faces drawn.
     */
    MeshDefinition meshFromGrid(const Spatial::IRegularGrid2D &grid,
                                std::vector<int64_t> &cellIndex)
    {
      MeshDefinition mesh;
      mesh.meshName = "grid";

      const int xNodes = grid.numXNodes();
      const int yNodes = grid.numYNodes();

      if (xNodes < 2 || yNodes < 2)
      {
        return mesh;
      }

      mesh.nodeX.reserve(static_cast<size_t>(xNodes) * yNodes);
      mesh.nodeY.reserve(static_cast<size_t>(xNodes) * yNodes);

      for (int y = 0; y < yNodes; ++y)
      {
        for (int x = 0; x < xNodes; ++x)
        {
          mesh.nodeX.push_back(grid.xNodeLocation(x, y));
          mesh.nodeY.push_back(grid.yNodeLocation(x, y));
        }
      }

      const auto nodeAt = [xNodes](int x, int y)
      { return static_cast<int64_t>(y) * xNodes + x; };

      mesh.faceNodeOffsets.push_back(0);

      // Cells, not nodes: a grid of N by M nodes has N-1 by M-1 cells, and
      // values on a cell-centred grid are indexed that way.
      for (int y = 0; y + 1 < yNodes; ++y)
      {
        for (int x = 0; x + 1 < xNodes; ++x)
        {
          // Inactive cells are holes in the domain; drawing them would show
          // ground the model does not solve on.
          if (!grid.isActive(x, y))
          {
            continue;
          }

          mesh.faceNodes.push_back(nodeAt(x, y));
          mesh.faceNodes.push_back(nodeAt(x + 1, y));
          mesh.faceNodes.push_back(nodeAt(x + 1, y + 1));
          mesh.faceNodes.push_back(nodeAt(x, y + 1));
          mesh.faceNodeOffsets.push_back(
            static_cast<int64_t>(mesh.faceNodes.size()));

          cellIndex.push_back(static_cast<int64_t>(y) * (xNodes - 1) + x);
        }
      }

      return mesh;
    }
  }

  MeshLayer::MeshLayer(const QString &name, MeshEntity entity)
    : FeatureLayer(name), m_entity(entity)
  {
  }

  MeshLayer::~MeshLayer() = default;

  MeshEntity MeshLayer::entity() const
  {
    return m_entity;
  }

  const MeshDefinition &MeshLayer::mesh() const
  {
    return m_mesh;
  }

  QString MeshLayer::valueAttribute() const
  {
    return m_valueAttribute;
  }

  std::unique_ptr<MeshLayer> MeshLayer::create(const QString &name,
                                               const MeshDefinition &mesh,
                                               MeshEntity entity,
                                               QString &message)
  {
    if (mesh.nodeCount() <= 0)
    {
      message = QObject::tr("The mesh has no nodes.");
      return nullptr;
    }

    // A node-only mesh is valid — a point cloud, or the nodes of a 1-D
    // network — so asking for faces it does not have falls back rather than
    // failing.
    MeshEntity drawn = entity;

    if (drawn == MeshEntity::Face && mesh.faceCount() <= 0)
    {
      drawn = mesh.edgeCount() > 0 ? MeshEntity::Edge : MeshEntity::Node;
    }

    if (drawn == MeshEntity::Edge && mesh.edgeCount() <= 0)
    {
      drawn = MeshEntity::Node;
    }

    std::unique_ptr<MeshLayer> layer(new MeshLayer(name, drawn));
    layer->m_mesh = mesh;

    const auto nodeAt = [&mesh](int64_t index)
    {
      return QPointF(mesh.nodeX[static_cast<size_t>(index)],
                     mesh.nodeY[static_cast<size_t>(index)]);
    };

    const auto nodeInRange = [&mesh](int64_t index)
    { return index >= 0 && index < mesh.nodeCount(); };

    switch (drawn)
    {
      case MeshEntity::Face:
        for (int64_t face = 0; face < mesh.faceCount(); ++face)
        {
          const int64_t from =
            mesh.faceNodeOffsets[static_cast<size_t>(face)];
          const int64_t to =
            mesh.faceNodeOffsets[static_cast<size_t>(face) + 1];

          QPolygonF ring;
          ring.reserve(static_cast<int>(to - from) + 1);

          bool usable = true;

          for (int64_t slot = from; slot < to; ++slot)
          {
            const int64_t node = mesh.faceNodes[static_cast<size_t>(slot)];

            // A connectivity index outside the node array is a corrupt or
            // differently-based mesh; drawing it would read past the end.
            if (!nodeInRange(node))
            {
              usable = false;
              break;
            }

            ring.append(nodeAt(node));
          }

          if (!usable || ring.size() < 3)
          {
            continue;
          }

          // Closed explicitly: UGRID connectivity does not repeat the first
          // node, and an unclosed ring draws as a line with a missing side.
          ring.append(ring.first());

          VectorFeature feature;
          feature.kind = GeometryKind::Polygon;
          feature.parts.append(ring);

          layer->addFeature(std::move(feature));
        }
        break;

      case MeshEntity::Edge:
        for (const std::array<int64_t, 2> &edge : mesh.edgeNodes)
        {
          if (!nodeInRange(edge[0]) || !nodeInRange(edge[1]))
          {
            continue;
          }

          QPolygonF line;
          line.append(nodeAt(edge[0]));
          line.append(nodeAt(edge[1]));

          VectorFeature feature;
          feature.kind = GeometryKind::Line;
          feature.parts.append(line);

          layer->addFeature(std::move(feature));
        }
        break;

      case MeshEntity::Node:
        for (int64_t node = 0; node < mesh.nodeCount(); ++node)
        {
          QPolygonF point;
          point.append(nodeAt(node));

          VectorFeature feature;
          feature.kind = GeometryKind::Point;
          feature.parts.append(point);

          layer->addFeature(std::move(feature));
        }
        break;
    }

    if (layer->featureCount() == 0)
    {
      message = QObject::tr("The mesh holds no usable %1.")
                  .arg(drawn == MeshEntity::Face ? QObject::tr("faces")
                                                 : QObject::tr("entities"));

      return nullptr;
    }

    layer->finishLoading();

    return layer;
  }

  bool MeshLayer::ugridSupported()
  {
    return HydroCouple::SDK::IO::ugridReadSupported();
  }

  QStringList MeshLayer::ugridMeshNames(const QString &filePath)
  {
    std::string message;
    QStringList names;

    for (const std::string &name : HydroCouple::SDK::IO::ugridMeshNames(
           filePath.toStdString(), message))
    {
      names.append(QString::fromStdString(name));
    }

    return names;
  }

  std::unique_ptr<MeshLayer> MeshLayer::fromUGRIDFile(const QString &filePath,
                                                      const QString &meshName,
                                                      MeshEntity entity,
                                                      QString &message)
  {
    if (!ugridSupported())
    {
      message = QObject::tr(
        "This build of the HydroCouple SDK has no NetCDF support, so UGRID "
        "files cannot be read.");

      return nullptr;
    }

    MeshDefinition mesh;
    std::string reason;

    if (!HydroCouple::SDK::IO::readUGRIDMesh(filePath.toStdString(),
                                             meshName.toStdString(), mesh,
                                             reason))
    {
      message = QString::fromStdString(reason);
      return nullptr;
    }

    const QString name = QString::fromStdString(mesh.meshName);

    return create(name.isEmpty() ? QFileInfo(filePath).completeBaseName()
                                 : name,
                  mesh, entity, message);
  }

  bool MeshLayer::isRegularGrid(const HydroCouple::IComponentDataItem *item)
  {
    return dynamic_cast<const Spatial::IRegularGrid2DComponentDataItem *>(item)
           != nullptr;
  }

  std::unique_ptr<MeshLayer> MeshLayer::fromRegularGrid(
    HydroCouple::IComponentDataItem *item, QString &message)
  {
    const auto *gridItem =
      dynamic_cast<const Spatial::IRegularGrid2DComponentDataItem *>(item);

    if (!gridItem)
    {
      message = QObject::tr("This data item is not a regular grid.");
      return nullptr;
    }

    const Spatial::IRegularGrid2D *grid = gridItem->grid();

    if (!grid)
    {
      message = QObject::tr("The grid data item has no grid.");
      return nullptr;
    }

    std::vector<int64_t> cellIndex;
    const MeshDefinition mesh = meshFromGrid(*grid, cellIndex);

    std::unique_ptr<MeshLayer> layer = create(
      QObject::tr("Grid"), mesh, MeshEntity::Face, message);

    if (!layer)
    {
      return nullptr;
    }

    if (const Spatial::ISpatialReferenceSystem *reference =
          grid->spatialReferenceSystem())
    {
      if (!reference->srText().empty())
      {
        QString crsMessage;
        layer->setCrs(SpatialReference::fromDefinition(
          QString::fromStdString(reference->srText()), crsMessage));
      }
    }

    // Values are read straight off the item and matched to the faces by the
    // cell each face came from — inactive cells have no face, so a positional
    // match would put every value after the first hole on the wrong cell.
    const std::vector<int64_t> shape = item->shape();

    if (!shape.empty())
    {
      int64_t total = 1;

      for (const int64_t extent : shape)
      {
        total *= extent;
      }

      std::vector<double> buffer(static_cast<size_t>(total), 0.0);
      const int64_t bufferShape = total;

      HydroCouple::BufferDescriptor destination;
      destination.data = buffer.data();
      destination.kind = HydroCouple::DataKind::Float64;
      destination.rank = 1;
      destination.shape = &bufferShape;

      std::vector<int64_t> start(shape.size(), 0);

      if (item->getValuesInto(destination, start, shape, nullptr))
      {
        QVector<double> faceValues;
        faceValues.reserve(static_cast<int>(cellIndex.size()));

        for (const int64_t cell : cellIndex)
        {
          faceValues.append(cell < total ? buffer[static_cast<size_t>(cell)]
                                         : 0.0);
        }

        layer->setValues(QObject::tr("value"), faceValues);
      }
    }

    return layer;
  }

  bool MeshLayer::setValues(const QString &name, const QVector<double> &values)
  {
    if (values.size() != featureCount())
    {
      return false;
    }

    AttributeField field;
    field.name = name;
    field.displayName = name;
    field.isDynamic = true;

    setFields({field});
    m_valueAttribute = name;

    for (int i = 0; i < values.size(); ++i)
    {
      setFeatureAttributes(i, {QVariant(values.at(i))});
    }

    restyle();

    return true;
  }

} // namespace HydroCouple::Composer
