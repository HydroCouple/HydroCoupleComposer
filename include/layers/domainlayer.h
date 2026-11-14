/*!
 * \file   domainlayer.h
 * \author Caleb Buahin  
 * \brief  DomainLayer — one part of a mesh domain, drawn on the map.
 *
 * A view of MeshDomainModel, not a copy of it: the layer holds a pointer to
 * the model and rebuilds its features whenever the model says the domain
 * changed. Editing goes to the model and comes back here, which is what
 * keeps the map, the eventual list and any future editor from disagreeing.
 *
 * **One layer per part, not one layer for the domain.** FeatureLayer reports
 * a single geometryKind() — it takes the kind of the last feature added —
 * and its hit-testing and scene paths branch on that. A layer holding rings,
 * lines and points together therefore renders correctly and *picks*
 * incorrectly, which is precisely the bug that would matter once vertices
 * are draggable. Four homogeneous layers cost four rows in the tree and buy
 * correct picking, independent styling, and the ability to switch the
 * breaklines off while drawing the boundary.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_DOMAINLAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_DOMAINLAYER_H

#include "layers/featurelayer.h"
#include "mesh/meshdomain.h"

#include <memory>

namespace HydroCouple::Composer
{
  class MeshDomainModel;

  /*!
   * \brief One part of a mesh domain, drawn on the map.
   */
  class DomainLayer : public FeatureLayer
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds a layer showing \a part of \a model's domain.
       *
       * \param model The domain to follow; must outlive the layer, and is
       *        not owned — several layers share one.
       * \param part Which part to draw.
       * \returns The layer; never null, because a domain with nothing in it
       *          yet is an empty layer rather than a failure.
       */
      [[nodiscard]] static std::unique_ptr<DomainLayer> create(
        MeshDomainModel *model, DomainPart part);

      ~DomainLayer() override;

      //! \returns Which part of the domain this layer draws.
      [[nodiscard]] DomainPart part() const;

      //! \returns The domain being followed, or nullptr.
      [[nodiscard]] MeshDomainModel *model() const;

      /*!
       * \brief The default name for \a part.
       * \param part The part to name.
       */
      [[nodiscard]] static QString nameFor(DomainPart part);

    private:
      DomainLayer(const QString &name, MeshDomainModel *model,
                  DomainPart part);

      //! Re-reads the domain and replaces every feature.
      void rebuild();

      //! The colours and widths a part is drawn in.
      void applyDefaultStyle();

      MeshDomainModel *m_model = nullptr;
      DomainPart m_part = DomainPart::Boundary;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_DOMAINLAYER_H
