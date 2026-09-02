#include "layers/meshlayer.h"

#include "pick/centroidindex.h"

#include "gis/spatialreference.h"

#include "hydrocouplesdk/io/ugridreader.h"

#include "hydrocouple.h"
#include "hydrocouplespatial.h"

#include <QFileInfo>
#include <QLineF>
#include <QObject>

#include <QHash>
#include <QPair>
#include <QVarLengthArray>


#include <cmath>

#include <algorithm>
#include <vector>

namespace HydroCouple::Composer
{
  using HydroCouple::SDK::IO::MeshDefinition;

  namespace
  {
    namespace Spatial = HydroCouple::Spatial;

    /*!
     * \brief Interpolates \a point across the triangle \a a \a b \a c.
     *
     * Barycentric, so the answer is the plane through the three corners
     * evaluated at the point — which is precisely what the renderer draws
     * between them.
     *
     * \returns False when \a point is outside the triangle, or the triangle
     *          is degenerate.
     */
    bool interpolateTriangle(const QPointF &point, const QPointF &a,
                             const QPointF &b, const QPointF &c, double za,
                             double zb, double zc, double &elevation)
    {
      const double area = (b.y() - c.y()) * (a.x() - c.x()) +
                          (c.x() - b.x()) * (a.y() - c.y());

      if (area == 0.0)
      {
        return false;
      }

      const double first = ((b.y() - c.y()) * (point.x() - c.x()) +
                            (c.x() - b.x()) * (point.y() - c.y())) /
                           area;
      const double second = ((c.y() - a.y()) * (point.x() - c.x()) +
                             (a.x() - c.x()) * (point.y() - c.y())) /
                            area;
      const double third = 1.0 - first - second;

      // The tolerance is on dimensionless weights, so it means the same thing
      // on a metre-scale mesh and a degree-scale one. Without it a point on a
      // shared edge belongs to neither of the two faces that meet there, and
      // a drape develops pinholes along every cell boundary it crosses.
      constexpr double kEdgeTolerance = -1.0e-9;

      if (first < kEdgeTolerance || second < kEdgeTolerance ||
          third < kEdgeTolerance)
      {
        return false;
      }

      elevation = first * za + second * zb + third * zc;

      return true;
    }

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
    layer->applyDefaultElevationStyle();

    return layer;
  }

  void MeshLayer::applyDefaultElevationStyle()
  {
    if (m_entity != MeshEntity::Face || m_mesh.nodeZ.empty())
    {
      return;
    }

    // Only ground with relief: a sheet at one height graduated over a zero
    // span is one class pretending to be seven.
    const auto [low, high] =
      std::minmax_element(m_mesh.nodeZ.begin(), m_mesh.nodeZ.end());

    if (!(*high > *low))
    {
      return;
    }

    // Only a style nobody has touched: the same call runs when Sample
    // Elevations gives a flat mesh its heights, and stamping the default
    // over a classification someone built would be vandalism.
    if (style()->mode() != StyleMode::Single
        || !style()->attribute().isEmpty())
    {
      return;
    }

    // A DEM used to arrive in StyleMode::Single and render one flat default
    // blue -- "terrain not rendered nicely" in its purest form. Its ground
    // is the one attribute it always carries, so that is the default theme;
    // anyone who wants a plain colour picks Single in the dialog, which is
    // one click, where finding out why a terrain is blue was not.
    LayerStyle *layerStyle = style();
    layerStyle->setMode(StyleMode::Graduated);
    layerStyle->setAttribute(QStringLiteral("elevation"));
    layerStyle->classification().setMethod(
      ClassificationMethod::EqualInterval);
    layerStyle->classification().setClassCount(7);
    restyle();
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

    std::unique_ptr<MeshLayer> layer =
      create(name.isEmpty() ? QFileInfo(filePath).completeBaseName() : name,
             mesh, entity, message);

    if (layer)
    {
      // Recorded here rather than in create(), which also builds meshes that
      // came from a component's data item and have no file behind them.
      layer->setSourceDescription(filePath);
    }

    return layer;
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

  namespace
  {
    //! The synthetic field's canonical key, as the style would store it.
    const QString kElevationField = QStringLiteral("elevation");
  }

  QVector<AttributeField> MeshLayer::attributeFields() const
  {
    QVector<AttributeField> fields = FeatureLayer::attributeFields();

    if (m_entity == MeshEntity::Face && !m_mesh.nodeZ.empty())
    {
      AttributeField elevation;
      elevation.name = kElevationField;
      elevation.displayName = QObject::tr("Elevation");
      fields.append(elevation);
    }

    return fields;
  }

  QVariant MeshLayer::attributeValue(int feature, const QString &field) const
  {
    if (field != kElevationField)
    {
      return FeatureLayer::attributeValue(feature, field);
    }

    if (m_entity != MeshEntity::Face || m_mesh.nodeZ.empty() || feature < 0
        || feature >= m_entityIndex.size())
    {
      return {};
    }

    // The face's mean node height: one number per feature, which is what a
    // graduated ramp classifies.
    const int64_t face = m_entityIndex.at(feature);
    const int64_t from = m_mesh.faceNodeOffsets[static_cast<size_t>(face)];
    const int64_t to = m_mesh.faceNodeOffsets[static_cast<size_t>(face) + 1];

    if (to <= from)
    {
      return {};
    }

    double sum = 0.0;

    for (int64_t slot = from; slot < to; ++slot)
    {
      sum += nodeElevation(m_mesh.faceNodes[static_cast<size_t>(slot)]);
    }

    return sum / double(to - from);
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

  void MeshLayer::setFlatShading(bool flat)
  {
    if (flat == m_flatShading)
    {
      return;
    }

    m_flatShading = flat;
    notifyAppearanceChanged();
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

    // Newell's method, because a quad whose four nodes carry four different
    // elevations is not planar and a normal taken from any three of its
    // corners would depend on which three.
    const auto newellNormal = [](const QVector<QVector3D> &ring)
    {
      QVector3D normal;

      for (int corner = 0; corner < ring.size(); ++corner)
      {
        const QVector3D &current = ring[corner];
        const QVector3D &next = ring[(corner + 1) % ring.size()];

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

      return normal;
    };

    const auto ringOf = [&](int feature, int64_t from, int corners)
    {
      const QPolygonF &part = projected[feature].first();

      QVector<QVector3D> ring;
      ring.reserve(corners);

      for (int corner = 0; corner < corners; ++corner)
      {
        ring.append(QVector3D(
          float(part[corner].x()), float(part[corner].y()),
          float(nodeElevation(
            m_mesh.faceNodes[static_cast<size_t>(from + corner)]))));
      }

      return ring;
    };

    // Smooth shading's prepass: each node accumulates the normals of every
    // face that meets it, each oriented up before it joins the average --
    // a terrain's faces wind however the file wound them, and two
    // neighbours wound opposite ways would otherwise cancel to nothing.
    // Every face contributes, legend-hidden ones included: the ground
    // exists whether or not a class is drawn, and a hillside must not
    // change shape because half of it was switched off.
    QVector<QVector3D> nodeNormals;

    if (m_entity == MeshEntity::Face && !m_flatShading)
    {
      nodeNormals.resize(int(m_mesh.nodeCount()));

      for (int feature = 0; feature < projected.size(); ++feature)
      {
        if (projected[feature].isEmpty() || feature >= m_entityIndex.size())
        {
          continue;
        }

        const qint64 entity = m_entityIndex[feature];
        const int64_t from =
          m_mesh.faceNodeOffsets[static_cast<size_t>(entity)];
        const int64_t to =
          m_mesh.faceNodeOffsets[static_cast<size_t>(entity) + 1];
        const int corners = int(to - from);

        if (corners < 3 || projected[feature].first().size() < corners)
        {
          continue;
        }

        QVector3D normal = newellNormal(ringOf(feature, from, corners));

        if (normal.z() < 0.0f)
        {
          normal = -normal;
        }

        for (int corner = 0; corner < corners; ++corner)
        {
          const int64_t node =
            m_mesh.faceNodes[static_cast<size_t>(from + corner)];
          nodeNormals[int(node)] += normal;
        }
      }

      for (QVector3D &normal : nodeNormals)
      {
        if (!normal.isNull())
        {
          normal.normalize();
        }
      }
    }

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      if (projected[feature].isEmpty() || feature >= m_entityIndex.size())
      {
        continue;
      }

      // The map's own colour for this feature, selection laid over it: an
      // invalid one means the class was switched off in the legend, and a
      // scene that drew it anyway would contradict the legend beside it.
      const QColor color = sceneColorFor(layerStyle, feature);

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

      const QVector<QVector3D> ring = ringOf(feature, from, corners);

      // The face's own normal: for flat shading it is every corner's, and
      // for smooth it is the fallback for a corner whose node accumulated
      // nothing. Its direction follows the ring's winding, and is left that
      // way: the material lights both sides, which it has to anyway for a
      // camera orbited beneath a surface.
      const QVector3D faceNormal = newellNormal(ring);

      const quint32 base = quint32(geometry.vertices.size());

      for (int corner = 0; corner < corners; ++corner)
      {
        QVector3D normal = faceNormal;

        if (!nodeNormals.isEmpty())
        {
          const int64_t node =
            m_mesh.faceNodes[static_cast<size_t>(from + corner)];
          const QVector3D &averaged = nodeNormals[int(node)];

          if (!averaged.isNull())
          {
            normal = averaged;
          }
        }

        geometry.addVertex(ring[corner], normal, color);
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


  QVector<SceneGeometry> MeshLayer::sceneGeometry(
    const SceneContext &context) const
  {
    Q_UNUSED(context)

    return isLayered() ? prismGeometry() : surfaceGeometry();
  }

  // ── ITerrainSource ─────────────────────────────────────────────────────────

  struct MeshLayer::TerrainIndex
  {
      CentroidIndex faces;

      //! Mean face edge length — what a drape densifies to.
      double resolution = 0.0;

      QRectF extent;
  };

  void MeshLayer::onProjectionChanged()
  {
    FeatureLayer::onProjectionChanged();

    // The index holds projected positions, so a new map CRS invalidates every
    // one of them. Dropped rather than rebuilt: a stack whose CRS changed may
    // never be sampled again.
    m_terrainIndex.reset();
  }

  void MeshLayer::ensureTerrainIndex() const
  {
    if (m_terrainIndex)
    {
      return;
    }

    auto index = std::make_unique<TerrainIndex>();

    if (m_entity == MeshEntity::Face)
    {
      const QVector<QVector<QPolygonF>> &projected = projectedFeatures();
      double edgeSum = 0.0;
      int counted = 0;

      for (int feature = 0; feature < projected.size(); ++feature)
      {
        if (projected[feature].isEmpty() || feature >= m_entityIndex.size())
        {
          continue;
        }

        const qint64 entity = m_entityIndex[feature];

        if (entity >= m_mesh.faceCount())
        {
          continue;
        }

        const int64_t from =
          m_mesh.faceNodeOffsets[static_cast<size_t>(entity)];
        const int64_t to =
          m_mesh.faceNodeOffsets[static_cast<size_t>(entity) + 1];
        const int corners = int(to - from);
        const QPolygonF &ring = projected[feature].first();

        if (corners < 3 || ring.size() < corners)
        {
          continue;
        }

        QPointF centroid;

        for (int corner = 0; corner < corners; ++corner)
        {
          centroid += ring.at(corner);
        }

        centroid /= double(corners);

        double radius = 0.0;
        double perimeter = 0.0;

        for (int corner = 0; corner < corners; ++corner)
        {
          const QPointF offset = ring.at(corner) - centroid;
          radius = std::max(radius, std::hypot(offset.x(), offset.y()));

          const QPointF edge =
            ring.at((corner + 1) % corners) - ring.at(corner);
          perimeter += std::hypot(edge.x(), edge.y());
        }

        index->faces.add(centroid, radius, feature);
        ++counted;

        // Mean edge length, not the cell's width across: the spacing that
        // matters is the one over which the surface can turn, and a cell
        // turns at its edges.
        edgeSum += perimeter / double(corners);

        const QRectF box = ring.boundingRect();
        index->extent =
          index->extent.isNull() ? box : index->extent.united(box);
      }

      if (counted > 0)
      {
        index->resolution = edgeSum / double(counted);
        index->faces.build();
      }
    }

    m_terrainIndex = std::move(index);
  }

  bool MeshLayer::setNodeElevations(const std::vector<double> &elevations)
  {
    if (elevations.size() != m_mesh.nodeX.size())
    {
      return false;
    }

    m_mesh.nodeZ = elevations;

    // The terrain index is deliberately NOT dropped. It holds face
    // centroids, a mean edge length and an XY extent -- all of it geometry,
    // none of it heights -- and elevationAt reads nodeZ afresh on every
    // query. New elevations do not move a single face, so rebuilding the
    // index would cost the walk and change nothing.
    notifyAppearanceChanged();

    return true;
  }

  const ITerrainSource *MeshLayer::terrain() const
  {
    // Elevations are what makes a mesh a surface. Without them it is a sheet
    // at zero, and draping a network onto that moves nothing — so it declines
    // rather than answering a terrain that is indistinguishable from none.
    if (m_entity != MeshEntity::Face || m_mesh.faceCount() == 0 ||
        m_mesh.nodeZ.empty())
    {
      return nullptr;
    }

    return this;
  }

  bool MeshLayer::elevationAt(const QPointF &point, double &elevation) const
  {
    ensureTerrainIndex();

    if (!m_terrainIndex->faces.isValid() ||
        !m_terrainIndex->extent.contains(point))
    {
      return false;
    }

    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();

    double sampled = 0.0;

    const auto sampleFace = [&](int feature) -> bool
    {
      const qint64 entity = m_entityIndex[feature];
      const int64_t from = m_mesh.faceNodeOffsets[static_cast<size_t>(entity)];
      const int64_t to =
        m_mesh.faceNodeOffsets[static_cast<size_t>(entity) + 1];
      const int corners = int(to - from);
      const QPolygonF &ring = projected[feature].first();

      // The same fan the renderer triangulates with — from the ring's first
      // corner — because a sample taken off a different tessellation of a
      // non-planar face lies off the surface that is actually drawn.
      for (int corner = 1; corner + 1 < corners; ++corner)
      {
        if (interpolateTriangle(
              point, ring.at(0), ring.at(corner), ring.at(corner + 1),
              nodeElevation(m_mesh.faceNodes[static_cast<size_t>(from)]),
              nodeElevation(
                m_mesh.faceNodes[static_cast<size_t>(from + corner)]),
              nodeElevation(
                m_mesh.faceNodes[static_cast<size_t>(from + corner + 1)]),
              sampled))
        {
          return true;
        }
      }

      return false;
    };

    if (m_terrainIndex->faces.findNearest(point, 0.0, sampleFace) < 0)
    {
      return false;
    }

    elevation = sampled;

    return true;
  }

  QRectF MeshLayer::terrainExtent() const
  {
    ensureTerrainIndex();

    return m_terrainIndex->extent;
  }

  double MeshLayer::terrainResolution() const
  {
    ensureTerrainIndex();

    return m_terrainIndex->resolution;
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
    m_adjacencyValid = false;
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

  bool MeshLayer::columnProfile(int column, QVector<double> &values,
                                QVector<double> &elevations,
                                QString &message) const
  {
    values.clear();
    elevations.clear();

    if (!isLayered())
    {
      message = QObject::tr("\"%1\" has no vertical layering to profile.")
                  .arg(name());
      return false;
    }

    if (column < 0 || column >= m_layering.columnCount())
    {
      message = QObject::tr("There is no column %1 to profile.").arg(column);
      return false;
    }

    if (m_cellValues.size() != m_layering.cellCount())
    {
      // Said rather than profiled from whatever is there: a mesh carrying
      // its shape and not its values would otherwise draw a column of
      // nothing, which looks like a model that computed zeros.
      message = QObject::tr("\"%1\" carries no layered values to profile.")
                  .arg(name());
      return false;
    }

    values.reserve(m_layering.layerCount);
    elevations.reserve(m_layering.layerCount);

    for (int layer = 0; layer < m_layering.layerCount; ++layer)
    {
      values.append(m_cellValues.at(
        static_cast<int>(m_layering.cell(column, layer))));

      // The centre of the layer, between the interface above it and the one
      // below. Plotting at an interface would put a cell's value at a
      // boundary it shares with the cell above.
      elevations.append(
        (m_layering.z(column, layer) + m_layering.z(column, layer + 1)) / 2.0);
    }

    message.clear();
    return true;
  }

  bool MeshLayer::transect(const QPolygonF &line, TransectSection &section,
                           QString &message) const
  {
    section = TransectSection{};

    if (line.size() < 2)
    {
      message = QObject::tr("A section needs a line of at least two points.");
      return false;
    }

    if (!isLayered())
    {
      message = QObject::tr("\"%1\" has no vertical layering to cut.")
                  .arg(name());
      return false;
    }

    if (m_entity != MeshEntity::Face)
    {
      message = QObject::tr("\"%1\" is drawn by %2, and a section cuts "
                            "through columns, which hang under faces.")
                  .arg(name(), m_entity == MeshEntity::Edge
                                 ? QObject::tr("edge")
                                 : QObject::tr("node"));
      return false;
    }

    if (m_cellValues.size() != m_layering.cellCount())
    {
      // Said rather than cut from whatever is there — columnProfile()'s
      // reasoning, and the same failure: a section of zeros is a picture of
      // a model that computed them.
      message = QObject::tr("\"%1\" carries no layered values to cut.")
                  .arg(name());
      return false;
    }

    // The projected rings, not the stored mesh: a section is cut on the map
    // the user drew it on. Features whose connectivity fell outside the node
    // array were skipped when the layer was built, so the column each ring
    // belongs to is carried alongside rather than assumed to be its index.
    const QVector<QVector<QPolygonF>> &projected = projectedFeatures();

    QVector<QPolygonF> rings;
    QVector<int> columns;
    rings.reserve(projected.size());
    columns.reserve(projected.size());

    for (int feature = 0; feature < projected.size(); ++feature)
    {
      if (projected[feature].isEmpty() || feature >= m_entityIndex.size())
      {
        continue;
      }

      rings.append(projected[feature].first());
      columns.append(static_cast<int>(m_entityIndex[feature]));
    }

    for (int vertex = 0; vertex + 1 < line.size(); ++vertex)
    {
      section.length += QLineF(line.at(vertex), line.at(vertex + 1)).length();
    }

    if (section.length <= 0.0)
    {
      message = QObject::tr("A section line of zero length cuts nothing.");
      return false;
    }

    const QVector<TransectSpan> spans = spansAlongLine(rings, line);

    for (const TransectSpan &span : spans)
    {
      const int column = columns.at(span.ring);

      if (column < 0 || column >= m_layering.columnCount())
      {
        continue;
      }

      for (int layer = 0; layer < m_layering.layerCount; ++layer)
      {
        TransectCell cell;
        cell.column = column;
        cell.layer = layer;
        cell.startDistance = span.start;
        cell.endDistance = span.end;
        cell.topElevation = m_layering.z(column, layer);
        cell.bottomElevation = m_layering.z(column, layer + 1);
        cell.value =
          m_cellValues.at(static_cast<int>(m_layering.cell(column, layer)));

        section.cells.append(cell);
      }
    }

    if (section.cells.isEmpty())
    {
      message = QObject::tr("The section line does not cross \"%1\".")
                  .arg(name());
      return false;
    }

    message.clear();
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

    ensurePrismAdjacency();

    // ── Geometry ──────────────────────────────────────────────────────────
    SceneGeometry geometry;
    geometry.primitive = ScenePrimitive::Triangles;

    // Only the caps, whose size is known exactly: two per column, of the
    // column's own corners. The walls are left to grow.
    //
    // Sizing this from the previous build instead — the obvious trick — is
    // actively worse. A peel to one layer needs a fraction of what the full
    // stack did, so the reserve allocates tens of megabytes it will not use,
    // and touching those pages costs more than the reallocations it saved:
    // measured at 42 ms against 26 ms with no reserve at all.
    geometry.vertices.reserve(2 * m_adjacency.totalCorners);

    const auto slabTop = [this](qint64 column)
    { return m_layering.z(column, m_firstVisibleLayer); };

    const auto slabBottom = [this](qint64 column)
    { return m_layering.z(column, m_lastVisibleLayer + 1); };

    // The visible layers' colours, resolved once per column rather than
    // once per edge per layer. Classifying is a search, and a column's four
    // edges were each repeating the same one.
    const LayerStyle *layerStyle = style();
    const QColor plainFill =
      layerStyle ? layerStyle->symbol().fill : QColor(Qt::gray);
    const bool classified = !m_cellValues.isEmpty();

    QVarLengthArray<QColor, 32> layerColors(
      m_lastVisibleLayer - m_firstVisibleLayer + 1, plainFill);

    const auto resolveColumnColors = [&](qint64 column)
    {
      if (!classified)
      {
        // Unclassified: one fill for every layer, resolved once for the
        // whole mesh rather than per column.
        return;
      }

      for (int layer = m_firstVisibleLayer; layer <= m_lastVisibleLayer;
           ++layer)
      {
        layerColors[layer - m_firstVisibleLayer] = colorForCellValue(
          m_cellValues[int(m_layering.cell(column, layer))]);
      }
    };

    const auto cellColor = [&](int layer) -> const QColor &
    { return layerColors[layer - m_firstVisibleLayer]; };

    //! A colour converted once, rather than at every vertex that wears it.
    struct Rgba
    {
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    };

    const auto toRgba = [](const QColor &color)
    {
      return Rgba{ float(color.redF()), float(color.greenF()),
                   float(color.blueF()), float(color.alphaF()) };
    };

    //! Adds a horizontal polygon at one elevation.
    const auto addCap = [&geometry](const QPolygonF &ring, int corners,
                                    double elevation, float normalZ,
                                    const Rgba &color)
    {
      const quint32 base = quint32(geometry.vertices.size());

      SceneVertex vertex;
      vertex.nx = 0.0f;
      vertex.ny = 0.0f;
      vertex.nz = normalZ;
      vertex.z = float(elevation);
      vertex.r = color.r;
      vertex.g = color.g;
      vertex.b = color.b;
      vertex.a = color.a;

      for (int corner = 0; corner < corners; ++corner)
      {
        vertex.x = float(ring[corner].x());
        vertex.y = float(ring[corner].y());
        geometry.appendVertex(vertex);
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
                                     const Rgba &color)
    {
      if (high - low <= 0.0)
      {
        return;
      }

      const double dx = b.y() - a.y();
      const double dy = a.x() - b.x();
      const double length = std::hypot(dx, dy);

      if (length <= 0.0)
      {
        return;
      }

      const quint32 base = quint32(geometry.vertices.size());

      SceneVertex vertex;
      vertex.nx = float(dx / length);
      vertex.ny = float(dy / length);
      vertex.nz = 0.0f;
      vertex.r = color.r;
      vertex.g = color.g;
      vertex.b = color.b;
      vertex.a = color.a;

      const double xs[4] = { a.x(), b.x(), b.x(), a.x() };
      const double ys[4] = { a.y(), b.y(), b.y(), a.y() };
      const double zs[4] = { low, low, high, high };

      for (int corner = 0; corner < 4; ++corner)
      {
        vertex.x = float(xs[corner]);
        vertex.y = float(ys[corner]);
        vertex.z = float(zs[corner]);
        geometry.appendVertex(vertex);
      }

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

      const int corners = m_adjacency.corners[feature];
      const int edgeBase = m_adjacency.offsets[feature];

      // The ring carries a closing duplicate the connectivity does not.
      if (corners < 3 || ring.size() < corners)
      {
        continue;
      }

      resolveColumnColors(column);

      const QColor &topColor = cellColor(m_firstVisibleLayer);
      const QColor &bottomColor = cellColor(m_lastVisibleLayer);

      const double columnTop = slabTop(column);
      const double columnBottom = slabBottom(column);

      // Caps. Nothing sits above the topmost visible layer or below the
      // bottommost — peeling is exactly what exposes them — so both are
      // always drawn, while the interfaces *between* visible layers are
      // interior and never are.
      if (topColor.isValid())
      {
        addCap(ring, corners, columnTop, 1.0f, toRgba(topColor));
      }

      if (bottomColor.isValid())
      {
        addCap(ring, corners, columnBottom, -1.0f, toRgba(bottomColor));
      }

      for (int corner = 0; corner < corners; ++corner)
      {
        const int neighbour = m_adjacency.neighbours[edgeBase + corner];

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
          const QColor &color = cellColor(layer);

          if (!color.isValid())
          {
            continue;
          }

          const Rgba rgba = toRgba(color);

          const double high = m_layering.z(column, layer);
          const double low = m_layering.z(column, layer + 1);

          if (!covered)
          {
            addWall(a, b, low, high, rgba);

            continue;
          }

          // The exposed part is what is left of this segment once the
          // neighbour's slab is removed: at most a piece above it and a
          // piece below it.
          addWall(a, b, std::max(low, coveredHigh), high, rgba);
          addWall(a, b, low, std::min(high, coveredLow), rgba);
        }
      }
    }

    // The bounds are whatever was actually built, taken in one pass at the
    // end. Growing them as pieces are emitted needs both the caps and the
    // walls to do it, and either alone covers the box in almost every mesh —
    // so neither is really load-bearing and a fault in either one hides.
    // One pass over a contiguous array also costs less than the scattered
    // updates it replaces.
    if (!geometry.vertices.isEmpty())
    {
      // Reduced on plain floats rather than through Bounds3D::expandTo per
      // vertex: that goes via QVector3D's accessors and its has-anything-yet
      // branch, and at these counts the difference is most of the pass.
      const SceneVertex *first = geometry.vertices.constData();
      float lowX = first->x, lowY = first->y, lowZ = first->z;
      float highX = lowX, highY = lowY, highZ = lowZ;

      for (const SceneVertex &vertex : geometry.vertices)
      {
        lowX = std::min(lowX, vertex.x);
        lowY = std::min(lowY, vertex.y);
        lowZ = std::min(lowZ, vertex.z);
        highX = std::max(highX, vertex.x);
        highY = std::max(highY, vertex.y);
        highZ = std::max(highZ, vertex.z);
      }

      geometry.bounds.expandTo(QVector3D(lowX, lowY, lowZ));
      geometry.bounds.expandTo(QVector3D(highX, highY, highZ));
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


  void MeshLayer::ensurePrismAdjacency() const
  {
    if (m_adjacencyValid)
    {
      return;
    }

    m_adjacencyValid = true;
    m_adjacency = {};

    const int features = m_entityIndex.size();

    m_adjacency.corners.resize(features);
    m_adjacency.offsets.resize(features);

    int total = 0;

    for (int feature = 0; feature < features; ++feature)
    {
      const qint64 column = m_entityIndex[feature];
      const int corners =
        int(m_mesh.faceNodeOffsets[static_cast<size_t>(column) + 1] -
            m_mesh.faceNodeOffsets[static_cast<size_t>(column)]);

      m_adjacency.corners[feature] = corners;
      m_adjacency.offsets[feature] = total;
      total += corners;
    }

    m_adjacency.totalCorners = total;

    m_adjacency.neighbours.assign(total, -1);

    // One pass, one hash: an edge's second owner patches the first's slot as
    // well as its own, so each edge is looked up once rather than once per
    // side.
    // The first owner's feature and its slot, so the second can patch both
    // sides without needing to map a slot back to a feature.
    QHash<QPair<qint64, qint64>, QPair<int, int>> firstOwner;
    firstOwner.reserve(total);

    for (int feature = 0; feature < features; ++feature)
    {
      const qint64 column = m_entityIndex[feature];
      const int64_t from = m_mesh.faceNodeOffsets[static_cast<size_t>(column)];
      const int corners = m_adjacency.corners[feature];
      const int base = m_adjacency.offsets[feature];

      for (int corner = 0; corner < corners; ++corner)
      {
        const qint64 a =
          m_mesh.faceNodes[static_cast<size_t>(from + corner)];
        const qint64 b = m_mesh.faceNodes[static_cast<size_t>(
          from + (corner + 1) % corners)];

        const QPair<qint64, qint64> key =
          a < b ? QPair<qint64, qint64>(a, b) : QPair<qint64, qint64>(b, a);

        const auto existing = firstOwner.constFind(key);

        if (existing == firstOwner.constEnd())
        {
          firstOwner.insert(key, { feature, base + corner });
        }
        else
        {
          // Both sides learn about each other here, so the geometry pass
          // needs no lookup at all.
          m_adjacency.neighbours[base + corner] = existing->first;
          m_adjacency.neighbours[existing->second] = feature;
        }
      }
    }
  }

} // namespace HydroCouple::Composer
