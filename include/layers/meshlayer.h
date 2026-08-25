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
  class MeshLayer : public FeatureLayer
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

    private:
      MeshLayer(const QString &name, MeshEntity entity);

      HydroCouple::SDK::IO::MeshDefinition m_mesh;
      MeshEntity m_entity = MeshEntity::Face;
      QString m_valueAttribute;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_MESHLAYER_H
