/*!
 * \file   componentlayer.h
 * \author Caleb Buahin
 * \brief  IComponentLayer — where a layer came from.
 *
 * A layer built from a component's data item knows which component and
 * which item it was built from. Nothing in the map needs that; the
 * selection hub does, because "select the component this feature belongs
 * to" cannot be answered by a layer that has forgotten.
 *
 * An interface rather than a member on each layer type, for D4a's reason:
 * one kind of layer made a `dynamic_cast<DataItemLayer *>` fine, a second
 * made it a chain every caller has to extend. ITimeLayer is the precedent.
 *
 * It is deliberately **not** on MapLayer (D15 — the base stays thin: the
 * stack and the canvas do not need provenance) and deliberately not
 * implemented by DifferenceLayer, which is derived from two runs and so
 * belongs to no single component; asking it would invite an answer that
 * is half true.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_COMPONENTLAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_COMPONENTLAYER_H

#include <QString>

namespace HydroCouple::Composer
{
  /*!
   * \brief A layer that was built from one component's data item.
   */
  class IComponentLayer
  {
    public:
      virtual ~IComponentLayer() = default;

      /*!
       * \brief The document id of the component this was built from.
       * \returns The id, or an empty string when it was not recorded.
       */
      [[nodiscard]] QString componentId() const { return m_componentId; }

      /*!
       * \brief The id of the data item within that component.
       */
      [[nodiscard]] QString dataItemId() const { return m_dataItemId; }

      /*!
       * \brief Records where this layer came from.
       *
       * Set by whoever built the layer, which is the only place both ids
       * are known: the item itself carries its own id but not its owner's.
       *
       * \param componentId The document id of the owning component.
       * \param dataItemId The id of the item within it.
       */
      void setProvenance(const QString &componentId, const QString &dataItemId)
      {
        m_componentId = componentId;
        m_dataItemId = dataItemId;
      }

    protected:
      QString m_componentId;
      QString m_dataItemId;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_COMPONENTLAYER_H
