#include "layers/meshlayer.h"

#include "gis/spatialreference.h"

#include "hydrocouplesdk/io/ugridreader.h"

#include "hydrocouple.h"
#include "hydrocouplespatial.h"

#include <QFileInfo>
#include <QObject>

#include <algorithm>
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

          layer->m_entityIndex.append(face);
          layer->addFeature(std::move(feature));
        }
        break;

      case MeshEntity::Edge:
        for (int64_t index = 0; index < mesh.edgeCount(); ++index)
        {
          const std::array<int64_t, 2> &edge =
            mesh.edgeNodes[static_cast<size_t>(index)];

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

          layer->m_entityIndex.append(index);
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

          layer->m_entityIndex.append(node);
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


  const ISceneSource *MeshLayer::sceneSource() const
  {
    return this;
  }

  double MeshLayer::nodeElevation(qint64 node) const
  {
    if (m_mesh.nodeZ.empty() || node < 0 ||
        node >= static_cast<qint64>(m_mesh.nodeZ.size()))
    {
      return 0.0;
    }

    return m_mesh.nodeZ[static_cast<size_t>(node)];
  }

  Bounds3D MeshLayer::sceneBounds() const
  {
    Bounds3D bounds;

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      if (projected[feature].isEmpty())
      {
        continue;
      }

      // Answered from the projected footprint and the node elevations rather
      // than by building the geometry: framing the view must not cost a
      // tessellation of every layer in the stack.
      const QRectF footprint = projected[feature].first().boundingRect();

      double low = 0.0;
      double high = 0.0;

      if (m_entity == MeshEntity::Face && feature < m_entityIndex.size())
      {
        const qint64 face = m_entityIndex[feature];
        const int64_t from = m_mesh.faceNodeOffsets[static_cast<size_t>(face)];
        const int64_t to =
          m_mesh.faceNodeOffsets[static_cast<size_t>(face) + 1];

        low = nodeElevation(m_mesh.faceNodes[static_cast<size_t>(from)]);
        high = low;

        for (int64_t slot = from + 1; slot < to; ++slot)
        {
          const double z =
            nodeElevation(m_mesh.faceNodes[static_cast<size_t>(slot)]);
          low = std::min(low, z);
          high = std::max(high, z);
        }
      }
      else if (feature < m_entityIndex.size())
      {
        const qint64 entity = m_entityIndex[feature];

        if (m_entity == MeshEntity::Edge && entity < m_mesh.edgeCount())
        {
          const std::array<int64_t, 2> &edge =
            m_mesh.edgeNodes[static_cast<size_t>(entity)];
          low = std::min(nodeElevation(edge[0]), nodeElevation(edge[1]));
          high = std::max(nodeElevation(edge[0]), nodeElevation(edge[1]));
        }
        else
        {
          low = nodeElevation(entity);
          high = low;
        }
      }

      bounds.expandTo(QVector3D(float(footprint.left()),
                                float(footprint.top()), float(low)));
      bounds.expandTo(QVector3D(float(footprint.right()),
                                float(footprint.bottom()), float(high)));
    }

    return bounds;
  }

  QVector<SceneGeometry> MeshLayer::sceneGeometry() const
  {
    QVector<SceneGeometry> batches;

    if (m_entity == MeshEntity::Node)
    {
      return batches;
    }

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();
    const LayerStyle *layerStyle = style();

    SceneGeometry geometry;
    geometry.primitive = m_entity == MeshEntity::Face
                           ? ScenePrimitive::Triangles
                           : ScenePrimitive::Lines;

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      if (projected[feature].isEmpty() || feature >= m_entityIndex.size())
      {
        continue;
      }

      // The map's own colour for this feature: an invalid one means the
      // class was switched off in the legend, and a scene that drew it
      // anyway would contradict the legend beside it.
      const QColor color =
        layerStyle ? layerStyle->colorFor(*this, feature) : QColor(Qt::gray);

      if (!color.isValid())
      {
        continue;
      }

      const QPolygonF &part = projected[feature].first();
      const qint64 entity = m_entityIndex[feature];

      if (m_entity == MeshEntity::Edge)
      {
        if (part.size() < 2 || entity >= m_mesh.edgeCount())
        {
          continue;
        }

        const std::array<int64_t, 2> &edge =
          m_mesh.edgeNodes[static_cast<size_t>(entity)];

        // Edges have no surface to face, so they are lit as if facing up;
        // the shader's headlight term then leaves them at full colour.
        const QVector3D up(0.0f, 0.0f, 1.0f);

        const quint32 first = geometry.addVertex(
          QVector3D(float(part[0].x()), float(part[0].y()),
                    float(nodeElevation(edge[0]))),
          up, color);
        const quint32 second = geometry.addVertex(
          QVector3D(float(part[1].x()), float(part[1].y()),
                    float(nodeElevation(edge[1]))),
          up, color);

        geometry.indices.append(first);
        geometry.indices.append(second);

        continue;
      }

      const int64_t from = m_mesh.faceNodeOffsets[static_cast<size_t>(entity)];
      const int64_t to =
        m_mesh.faceNodeOffsets[static_cast<size_t>(entity) + 1];
      const int corners = int(to - from);

      // The ring carries a closing duplicate the connectivity does not, so
      // the two are only parallel over the connectivity's own length.
      if (corners < 3 || part.size() < corners)
      {
        continue;
      }

      QVector<QVector3D> ring;
      ring.reserve(corners);

      for (int corner = 0; corner < corners; ++corner)
      {
        ring.append(QVector3D(
          float(part[corner].x()), float(part[corner].y()),
          float(nodeElevation(
            m_mesh.faceNodes[static_cast<size_t>(from + corner)]))));
      }

      // Newell's method, because a quad whose four nodes carry four different
      // elevations is not planar and a normal taken from any three of its
      // corners would depend on which three.
      //
      // Its direction follows the ring's winding, and is left that way: the
      // material lights both sides, which it has to anyway for a camera
      // orbited beneath a surface. Normalising the winding here as well would
      // leave two mechanisms for one property and neither clearly in charge.
      QVector3D normal;

      for (int corner = 0; corner < corners; ++corner)
      {
        const QVector3D &current = ring[corner];
        const QVector3D &next = ring[(corner + 1) % corners];

        normal += QVector3D(
          (current.y() - next.y()) * (current.z() + next.z()),
          (current.z() - next.z()) * (current.x() + next.x()),
          (current.x() - next.x()) * (current.y() + next.y()));
      }

      if (!normal.isNull())
      {
        normal.normalize();
      }
      else
      {
        normal = QVector3D(0.0f, 0.0f, 1.0f);
      }

      const quint32 base = quint32(geometry.vertices.size());

      for (const QVector3D &vertex : ring)
      {
        geometry.addVertex(vertex, normal, color);
      }

      for (int corner = 1; corner + 1 < corners; ++corner)
      {
        geometry.indices.append(base);
        geometry.indices.append(base + quint32(corner));
        geometry.indices.append(base + quint32(corner) + 1);
      }
    }

    if (!geometry.isEmpty())
    {
      batches.append(std::move(geometry));
    }

    return batches;
  }

} // namespace HydroCouple::Composer
