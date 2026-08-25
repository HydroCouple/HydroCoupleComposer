/*!
 * \file   meshlayer.h
 * \author Caleb Buahin
 * \brief  MeshLayer — a UGRID mesh, drawn as faces, edges or nodes.
 *
 * The SDK's MeshDefinition is the vocabulary its writers, its meshing tools
 * and the standard's own IMeshView all speak, so the layer takes that rather
 * than a file format: a mesh generated in memory and one read back from
 * NetCDF then display through exactly the same path.
 *
 * Faces come as CSR connectivity, which is what makes a mixed triangle-and-
 * quad mesh a single array pair rather than two special cases.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_MESHLAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_MESHLAYER_H

#include "layers/featurelayer.h"
#include "layers/layeredmesh.h"
#include "scene/scenesource.h"

#include "hydrocouplesdk/io/meshdefinition.h"

#include <QStringList>

#include <memory>

namespace HydroCouple
{
  class IComponentDataItem;
}

namespace HydroCouple::Composer
{

  /*!
   * \brief Which mesh entity a layer draws.
   */
  enum class MeshEntity
  {
    Face,  //!< Cells, as filled polygons.
    Edge,  //!< Edges, as lines.
    Node   //!< Nodes, as points.
  };

  /*!
   * \brief A UGRID mesh drawn on the map.
   */
  class MeshLayer : public FeatureLayer, public ISceneSource
  {
    public:
      /*!
       * \brief Builds a layer from \a mesh.
       * \param name User-visible layer name.
       * \param mesh The mesh to draw.
       * \param entity Which entity to draw; Face falls back to Edge then
       *        Node when the mesh has none.
       * \param[out] message Diagnostic on failure.
       * \returns The layer, or nullptr when the mesh has no nodes.
       */
      [[nodiscard]] static std::unique_ptr<MeshLayer> create(
        const QString &name, const HydroCouple::SDK::IO::MeshDefinition &mesh,
        MeshEntity entity, QString &message);

      /*!
       * \brief Builds a layer from a regular-grid data item.
       *
       * A regular grid is a mesh whose faces happen to be quads, so it is
       * converted rather than given a rendering path of its own — which also
       * means it inherits classification, the legend and labelling unchanged.
       * Inactive cells are left out: they are holes in the domain, and
       * drawing them would show ground the model does not solve on.
       *
       * \param item The grid data item to draw.
       * \param[out] message Diagnostic on failure.
       * \returns The layer, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<MeshLayer> fromRegularGrid(
        HydroCouple::IComponentDataItem *item, QString &message);

      /*!
       * \brief Whether \a item is a regular grid this layer can draw.
       * \param item The data item to test.
       */
      [[nodiscard]] static bool isRegularGrid(
        const HydroCouple::IComponentDataItem *item);

      /*!
       * \brief Builds a layer from a UGRID file, through the SDK's reader.
       * \param filePath NetCDF-UGRID file to read.
       * \param meshName Mesh to read; empty takes the first.
       * \param entity Which entity to draw.
       * \param[out] message Diagnostic on failure.
       * \returns The layer, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<MeshLayer> fromUGRIDFile(
        const QString &filePath, const QString &meshName, MeshEntity entity,
        QString &message);

      /*!
       * \brief The mesh names a UGRID file holds.
       *
       * A file may carry several — a 1-D network and a 2-D floodplain, say —
       * and opening only the first would silently ignore the rest.
       *
       * \param filePath File to inspect.
       */
      [[nodiscard]] static QStringList ugridMeshNames(const QString &filePath);

      /*!
       * \brief Whether the SDK this build links can read UGRID files.
       *
       * NetCDF is optional in the SDK, so the command is offered only when
       * it would work rather than failing at the point of use.
       */
      [[nodiscard]] static bool ugridSupported();

      ~MeshLayer() override;

      /*!
       * \brief Which entity this layer draws.
       */
      [[nodiscard]] MeshEntity entity() const;

      /*!
       * \brief The mesh this layer was built from.
       */
      [[nodiscard]] const HydroCouple::SDK::IO::MeshDefinition &mesh() const;

      /*!
       * \brief Attaches one value per drawn entity.
       *
       * The count must match the entity count: a mesh coloured by values that
       * belong to a different entity is worse than an uncoloured one, because
       * it looks like an answer.
       *
       * \param name Attribute name to offer the values under.
       * \param values One value per face, edge or node, as this layer draws.
       * \returns True when the values matched and were attached.
       */
      bool setValues(const QString &name, const QVector<double> &values);

      /*!
       * \brief The attribute the values were attached under, or empty.
       */
      [[nodiscard]] QString valueAttribute() const;

      /*!
       * \brief Gives the layer a vertical discretisation.
       *
       * The map is unaffected — in plan view a layered mesh is its own
       * horizontal mesh — but the 3D scene switches from drawing a surface
       * to drawing the columns of prismatic cells the layering describes.
       *
       * \param mesh The layered mesh; its horizontal part must match this
       *        layer's own, since the features were built from it.
       * \param[out] message Diagnostic on failure.
       * \returns True when the layering was accepted.
       */
      bool setLayering(LayeredMesh mesh, QString &message);

      /*!
       * \brief Whether the layer has a vertical discretisation.
       */
      [[nodiscard]] bool isLayered() const;

      /*!
       * \brief The vertical discretisation; layerCount is zero without one.
       */
      [[nodiscard]] const LayeredMesh &layering() const;

      /*!
       * \brief Attaches one value per cell of a layered mesh.
       *
       * Indexed as FVQual indexes them — `column * layerCount + k` — because
       * a field read from one of its files is in that order, and reading it
       * in the other transposes the water column into something that still
       * looks like data.
       *
       * \param name Attribute name to offer the values under.
       * \param values One value per cell.
       * \returns True when the count matched and the values were attached.
       */
      bool setLayeredValues(const QString &name, const QVector<double> &values);

      /*!
       * \brief The layers the scene draws, inclusive.
       *
       * Peeling: the whole point of a layered view is to look *inside*, and
       * a full stack of prisms shows only its own outer skin. Clamped to the
       * mesh, and an inverted range is taken as the single layer \a first.
       *
       * \param first Topmost layer to draw; 0 is the surface layer.
       * \param last Bottommost layer to draw.
       */
      void setVisibleLayers(int first, int last);

      //! \copybrief setVisibleLayers
      [[nodiscard]] int firstVisibleLayer() const;

      //! \copybrief setVisibleLayers
      [[nodiscard]] int lastVisibleLayer() const;

      /*!
       * \brief This layer, as the 3D scene's geometry supplier.
       */
      [[nodiscard]] const ISceneSource *sceneSource() const override;

      /*!
       * \brief Surfaces for faces, segments for edges, nothing for nodes.
       *
       * A node mesh is a point cloud, which the map already draws and which
       * the plan's 3D work does not cover; it answers with no geometry rather
       * than with an invented representation of itself.
       *
       * Colours come from the layer's own style, so a mesh classified in the
       * map and the same mesh in the scene cannot disagree.
       */
      [[nodiscard]] QVector<SceneGeometry> sceneGeometry() const override;

      /*!
       * \brief The mesh's box: its map-CRS footprint and its node elevations.
       */
      [[nodiscard]] Bounds3D sceneBounds() const override;

    private:
      MeshLayer(const QString &name, MeshEntity entity);

      //! Elevation of a node, or zero for a mesh that carries none.
      [[nodiscard]] double nodeElevation(qint64 node) const;

      //! Emits the flat surface a mesh without layering draws.
      [[nodiscard]] QVector<SceneGeometry> surfaceGeometry() const;

      //! Emits the prismatic cells a layered mesh draws.
      [[nodiscard]] QVector<SceneGeometry> prismGeometry() const;

      //! Colour for a cell value, straight from the style's classification.
      [[nodiscard]] QColor colorForCellValue(double value) const;

      HydroCouple::SDK::IO::MeshDefinition m_mesh;
      MeshEntity m_entity = MeshEntity::Face;
      QString m_valueAttribute;

      LayeredMesh m_layering;
      QVector<double> m_cellValues;
      int m_firstVisibleLayer = 0;
      int m_lastVisibleLayer = -1;

      //! The mesh entity each feature came from. Not the feature's own index:
      //! faces whose connectivity points outside the node array are skipped,
      //! so the two sequences diverge on exactly the meshes where guessing
      //! would attach the wrong elevations.
      QVector<qint64> m_entityIndex;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_MESHLAYER_H
