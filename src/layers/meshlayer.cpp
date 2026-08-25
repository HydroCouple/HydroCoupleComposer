#include "layers/meshlayer.h"

#include "gis/spatialreference.h"

#include "hydrocouplesdk/io/ugridreader.h"

#include "hydrocouple.h"
#include "hydrocouplespatial.h"

#include <QFileInfo>
#include <QObject>

#include <QHash>
#include <QPair>

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

  QVector<SceneGeometry> MeshLayer::surfaceGeometry() const
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


  QVector<SceneGeometry> MeshLayer::sceneGeometry() const
  {
    return isLayered() ? prismGeometry() : surfaceGeometry();
  }

  bool MeshLayer::setLayering(LayeredMesh mesh, QString &message)
  {
    if (!mesh.isValid(message))
    {
      return false;
    }

    // The features were built from the horizontal mesh, and the columns are
    // those faces. A layering built on a different mesh would index into the
    // wrong columns and look almost right.
    if (mesh.columnCount() != m_mesh.faceCount())
    {
      message = QObject::tr("The layering describes %1 columns but the layer "
                            "draws a mesh of %2 faces.")
                  .arg(mesh.columnCount())
                  .arg(m_mesh.faceCount());

      return false;
    }

    m_layering = std::move(mesh);
    m_cellValues.clear();
    m_firstVisibleLayer = 0;
    m_lastVisibleLayer = m_layering.layerCount - 1;

    notifyAppearanceChanged();

    return true;
  }

  bool MeshLayer::isLayered() const
  {
    return m_layering.layerCount > 0;
  }

  const LayeredMesh &MeshLayer::layering() const
  {
    return m_layering;
  }

  bool MeshLayer::setLayeredValues(const QString &name,
                                   const QVector<double> &values)
  {
    if (!isLayered() || values.size() != m_layering.cellCount())
    {
      return false;
    }

    m_cellValues = values;
    m_valueAttribute = name;

    notifyAppearanceChanged();

    return true;
  }

  void MeshLayer::setVisibleLayers(int first, int last)
  {
    if (!isLayered())
    {
      return;
    }

    const int top = std::clamp(first, 0, m_layering.layerCount - 1);
    const int bottom = std::clamp(last, top, m_layering.layerCount - 1);

    if (top == m_firstVisibleLayer && bottom == m_lastVisibleLayer)
    {
      return;
    }

    m_firstVisibleLayer = top;
    m_lastVisibleLayer = bottom;

    notifyAppearanceChanged();
  }

  int MeshLayer::firstVisibleLayer() const
  {
    return m_firstVisibleLayer;
  }

  int MeshLayer::lastVisibleLayer() const
  {
    return m_lastVisibleLayer;
  }

  QColor MeshLayer::colorForCellValue(double value) const
  {
    const LayerStyle *layerStyle = style();

    if (!layerStyle)
    {
      return QColor(Qt::gray);
    }

    // Cell values do not belong to a feature — there are layerCount of them
    // per face — so the classification is asked directly rather than through
    // the feature-indexed colorFor(). It is the same classification the map
    // and the legend read, which is what keeps the three consistent.
    const Classification &classification = layerStyle->classification();

    if (layerStyle->mode() == StyleMode::Single || classification.isEmpty())
    {
      return layerStyle->symbol().fill;
    }

    const int index = classification.indexFor(value);

    if (index < 0 || index >= classification.breaks().size())
    {
      return {};
    }

    const ClassBreak &band = classification.breaks()[index];

    return band.visible ? band.color : QColor();
  }


  QVector<SceneGeometry> MeshLayer::prismGeometry() const
  {
    QVector<SceneGeometry> batches;

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();

    // ── Which columns share each edge ─────────────────────────────────────
    //
    // A vertical wall between two columns is inside the water, and drawing
    // it would put three quarters of a large mesh's triangles where nobody
    // can see them. Adjacency is what says which walls are on the outside;
    // it is keyed on the node pair rather than on geometry, so two columns
    // meeting along an edge are neighbours regardless of how their corners
    // were wound.
    QHash<QPair<qint64, qint64>, QPair<int, int>> sharedEdges;

    const auto edgeKey = [](qint64 a, qint64 b)
    { return a < b ? QPair<qint64, qint64>(a, b) : QPair<qint64, qint64>(b, a); };

    const auto ringNodes = [this](qint64 column, QVector<qint64> &nodes)
    {
      const int64_t from = m_mesh.faceNodeOffsets[static_cast<size_t>(column)];
      const int64_t to = m_mesh.faceNodeOffsets[static_cast<size_t>(column) + 1];

      nodes.clear();
      nodes.reserve(int(to - from));

      for (int64_t slot = from; slot < to; ++slot)
      {
        nodes.append(m_mesh.faceNodes[static_cast<size_t>(slot)]);
      }
    };

    QVector<qint64> nodes;

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      if (feature >= m_entityIndex.size())
      {
        continue;
      }

      ringNodes(m_entityIndex[feature], nodes);

      for (int corner = 0; corner < nodes.size(); ++corner)
      {
        const QPair<qint64, qint64> key =
          edgeKey(nodes[corner], nodes[(corner + 1) % nodes.size()]);

        auto existing = sharedEdges.find(key);

        if (existing == sharedEdges.end())
        {
          sharedEdges.insert(key, { feature, -1 });
        }
        else if (existing->second < 0)
        {
          existing->second = feature;
        }
      }
    }

    // ── Geometry ──────────────────────────────────────────────────────────
    SceneGeometry geometry;
    geometry.primitive = ScenePrimitive::Triangles;

    const auto slabTop = [this](qint64 column)
    { return m_layering.z(column, m_firstVisibleLayer); };

    const auto slabBottom = [this](qint64 column)
    { return m_layering.z(column, m_lastVisibleLayer + 1); };

    const auto cellColor = [this](qint64 column, int layer) -> QColor
    {
      if (m_cellValues.isEmpty())
      {
        const LayerStyle *layerStyle = style();

        return layerStyle ? layerStyle->symbol().fill : QColor(Qt::gray);
      }

      return colorForCellValue(
        m_cellValues[int(m_layering.cell(column, layer))]);
    };

    //! Adds a horizontal polygon at one elevation.
    const auto addCap = [&geometry](const QPolygonF &ring, int corners,
                                    double elevation, const QVector3D &normal,
                                    const QColor &color)
    {
      const quint32 base = quint32(geometry.vertices.size());

      for (int corner = 0; corner < corners; ++corner)
      {
        geometry.addVertex(QVector3D(float(ring[corner].x()),
                                     float(ring[corner].y()),
                                     float(elevation)),
                           normal, color);
      }

      for (int corner = 1; corner + 1 < corners; ++corner)
      {
        geometry.indices.append(base);
        geometry.indices.append(base + quint32(corner));
        geometry.indices.append(base + quint32(corner) + 1);
      }
    };

    //! Adds one vertical quad along an edge, between two elevations.
    const auto addWall = [&geometry](const QPointF &a, const QPointF &b,
                                     double low, double high,
                                     const QColor &color)
    {
      if (high - low <= 0.0)
      {
        return;
      }

      QVector3D normal(float(b.y() - a.y()), float(a.x() - b.x()), 0.0f);

      if (normal.isNull())
      {
        return;
      }

      normal.normalize();

      const quint32 base = quint32(geometry.vertices.size());

      geometry.addVertex(QVector3D(float(a.x()), float(a.y()), float(low)),
                         normal, color);
      geometry.addVertex(QVector3D(float(b.x()), float(b.y()), float(low)),
                         normal, color);
      geometry.addVertex(QVector3D(float(b.x()), float(b.y()), float(high)),
                         normal, color);
      geometry.addVertex(QVector3D(float(a.x()), float(a.y()), float(high)),
                         normal, color);

      for (const quint32 offset : { 0u, 1u, 2u, 0u, 2u, 3u })
      {
        geometry.indices.append(base + offset);
      }
    };

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      if (projected[feature].isEmpty() || feature >= m_entityIndex.size())
      {
        continue;
      }

      const qint64 column = m_entityIndex[feature];
      const QPolygonF &ring = projected[feature].first();

      ringNodes(column, nodes);

      const int corners = nodes.size();

      // The ring carries a closing duplicate the connectivity does not.
      if (corners < 3 || ring.size() < corners)
      {
        continue;
      }

      const QColor topColor = cellColor(column, m_firstVisibleLayer);
      const QColor bottomColor = cellColor(column, m_lastVisibleLayer);

      // Caps. Nothing sits above the topmost visible layer or below the
      // bottommost — peeling is exactly what exposes them — so both are
      // always drawn, while the interfaces *between* visible layers are
      // interior and never are.
      if (topColor.isValid())
      {
        addCap(ring, corners, slabTop(column), QVector3D(0.0f, 0.0f, 1.0f),
               topColor);
      }

      if (bottomColor.isValid())
      {
        addCap(ring, corners, slabBottom(column),
               QVector3D(0.0f, 0.0f, -1.0f), bottomColor);
      }

      for (int corner = 0; corner < corners; ++corner)
      {
        const QPair<qint64, qint64> key =
          edgeKey(nodes[corner], nodes[(corner + 1) % corners]);

        const auto shared = sharedEdges.constFind(key);
        int neighbour = -1;

        if (shared != sharedEdges.constEnd())
        {
          neighbour = shared->first == feature ? shared->second : shared->first;
        }

        // What the neighbouring column's own slab covers. Sigma layers
        // follow the bed, so two adjacent columns' slabs rarely line up:
        // the part of this wall standing proud of the neighbour is real,
        // visible geometry, and dropping the whole wall because an edge is
        // interior punches a hole into the mesh wherever the bed steps.
        double coveredLow = 0.0;
        double coveredHigh = 0.0;
        bool covered = false;

        if (neighbour >= 0 && neighbour < m_entityIndex.size())
        {
          const qint64 other = m_entityIndex[neighbour];
          coveredLow = slabBottom(other);
          coveredHigh = slabTop(other);
          covered = true;
        }

        const QPointF &a = ring[corner];
        const QPointF &b = ring[(corner + 1) % corners];

        for (int layer = m_firstVisibleLayer; layer <= m_lastVisibleLayer;
             ++layer)
        {
          const QColor color = cellColor(column, layer);

          if (!color.isValid())
          {
            continue;
          }

          const double high = m_layering.z(column, layer);
          const double low = m_layering.z(column, layer + 1);

          if (!covered)
          {
            addWall(a, b, low, high, color);

            continue;
          }

          // The exposed part is what is left of this segment once the
          // neighbour's slab is removed: at most a piece above it and a
          // piece below it.
          addWall(a, b, std::max(low, coveredHigh), high, color);
          addWall(a, b, low, std::min(high, coveredLow), color);
        }
      }
    }

    if (!geometry.isEmpty())
    {
      batches.append(std::move(geometry));
    }

    return batches;
  }


  std::unique_ptr<MeshLayer> MeshLayer::fromLayeredUGRIDFile(
    const QString &filePath, const QString &meshName, int timeIndex,
    QString &message)
  {
    std::unique_ptr<MeshLayer> layer =
      fromUGRIDFile(filePath, meshName, MeshEntity::Face, message);

    if (!layer)
    {
      return nullptr;
    }

    HydroCouple::SDK::IO::VerticalCoordinate vertical;
    std::string readerMessage;

    // A file with no vertical coordinate is not a failure: most UGRID meshes
    // are two-dimensional, and the flat surface is the right answer for them.
    if (!HydroCouple::SDK::IO::readVerticalCoordinate(
          filePath.toStdString(), meshName.toStdString(),
          static_cast<size_t>(std::max(0, timeIndex)), vertical,
          readerMessage))
    {
      return layer;
    }

    QString layeringMessage;
    const LayeredMesh layered = LayeredMesh::fromCfSigma(
      layer->mesh(), vertical.interfaceSigma, vertical.depth,
      vertical.surface, layeringMessage);

    if (layered.layerCount < 1 || !layer->setLayering(layered, layeringMessage))
    {
      // The mesh is still usable in plan view, so the layering is reported
      // rather than allowed to lose the layer entirely.
      message = QObject::tr("The mesh was read, but its water column was "
                            "not: %1")
                  .arg(layeringMessage);
    }

    return layer;
  }

  bool MeshLayer::isLayeredUGRIDFile(const QString &filePath,
                                     const QString &meshName)
  {
    if (!ugridSupported())
    {
      return false;
    }

    return HydroCouple::SDK::IO::hasVerticalCoordinate(
      filePath.toStdString(), meshName.toStdString());
  }

  int MeshLayer::ugridTimeCount(const QString &filePath,
                                const QString &meshName)
  {
    if (!ugridSupported())
    {
      return 0;
    }

    HydroCouple::SDK::IO::VerticalCoordinate vertical;
    std::string message;

    if (!HydroCouple::SDK::IO::readVerticalCoordinate(
          filePath.toStdString(), meshName.toStdString(), 0, vertical,
          message))
    {
      return 0;
    }

    return int(vertical.timeCount);
  }

} // namespace HydroCouple::Composer
