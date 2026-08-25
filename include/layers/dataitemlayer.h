/*!
 * \file   dataitemlayer.h
 * \author Caleb Buahin
 * \brief  DataItemLayer — a component's spatial data item, on the map.
 *
 * The point of the map in a coupling tool is to show what the components
 * themselves carry, not only what was imported from a file. A geometry item,
 * a network and a polyhedral surface are all features once read, so all three
 * arrive as a FeatureLayer and inherit its classification, labelling and
 * legend without a second rendering path.
 *
 * Values are read through the item's hyperslab API and offered as an ordinary
 * attribute, which is what lets a component's output be classified with the
 * same controls as a shapefile column.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_DATAITEMLAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_DATAITEMLAYER_H

#include "layers/featurelayer.h"

#include <memory>

namespace HydroCouple
{
  class IComponentDataItem;
}

namespace HydroCouple::Composer
{

  /*!
   * \brief A spatial component data item drawn on the map.
   */
  class DataItemLayer : public FeatureLayer
  {
    public:
      /*!
       * \brief Whether \a item carries geometry this layer can draw.
       * \param item The data item to test; null is not spatial.
       */
      [[nodiscard]] static bool isSpatial(
        const HydroCouple::IComponentDataItem *item);

      /*!
       * \brief Builds a layer from \a item.
       *
       * The item must outlive the layer: the layer re-reads values from it on
       * refresh, which is the entire point of showing a live component rather
       * than a snapshot of one.
       *
       * \param item The data item to draw.
       * \param[out] message Diagnostic on failure.
       * \returns The layer, or nullptr when the item carries no geometry.
       */
      [[nodiscard]] static std::unique_ptr<DataItemLayer> create(
        HydroCouple::IComponentDataItem *item, QString &message);

      ~DataItemLayer() override;

      /*!
       * \brief The item this layer draws.
       */
      [[nodiscard]] HydroCouple::IComponentDataItem *dataItem() const;

      /*!
       * \brief The name of the attribute holding the item's values.
       */
      [[nodiscard]] QString valueAttribute() const;

      /*!
       * \brief Re-reads the item's values and restyles.
       *
       * Geometry is not re-read: a component's mesh does not move between
       * time steps, and re-reading it per step would dominate the cost of
       * showing a running model.
       *
       * \returns True when values were read.
       */
      bool refreshValues();

    private:
      DataItemLayer(const QString &name, HydroCouple::IComponentDataItem *item);

      bool loadGeometry(QString &message);

      /*!
       * \brief Which axis of the item's shape holds one value per entity.
       */
      [[nodiscard]] int entityAxis() const;

      HydroCouple::IComponentDataItem *m_item = nullptr;
      QString m_valueAttribute;

      //! Values are the last field, so geometry-derived fields keep their
      //! indices when the values are re-read.
      int m_valueFieldIndex = -1;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_DATAITEMLAYER_H
