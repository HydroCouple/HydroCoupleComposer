/*!
 * \file   attributeprovider.h
 * \author Caleb Buahin
 * \brief  IAttributeProvider — the seam between a layer's data and its style.
 *
 * Classification asks two questions of a layer: what can I theme by, and what
 * is this feature's value? A layer that can answer them can be styled, whether
 * its features come from a shapefile, a HydroCouple data item or a mesh.
 *
 * A plain abstract base rather than a QObject, so a layer keeps its single
 * QObject inheritance and any object at all can supply attributes to a test.
 */

#ifndef HYDROCOUPLECOMPOSER_RENDER_ATTRIBUTEPROVIDER_H
#define HYDROCOUPLECOMPOSER_RENDER_ATTRIBUTEPROVIDER_H

#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief One attribute a layer can be themed by.
   */
  struct AttributeField
  {
      QString name;         //!< Canonical key, as the style stores it.
      QString displayName;  //!< Label shown to the user; falls back to name.
      QString unit;         //!< Free-form unit, e.g. "m³/s"; may be empty.

      //! Storage type. Drives whether graduated classification is offered.
      QMetaType::Type type = QMetaType::Double;

      //! True when the value changes per time step, so breaks computed once
      //! go stale. Results layers set this; static geometry does not.
      bool isDynamic = false;
  };

  /*!
   * \brief Implemented by layers whose features carry attributes.
   */
  class IAttributeProvider
  {
    public:
      virtual ~IAttributeProvider() = default;

      /*!
       * \brief The fields this layer can be themed by.
       *
       * An empty list is a valid answer, and means the layer has no
       * per-feature attributes — it can still be drawn with a single symbol.
       */
      [[nodiscard]] virtual QVector<AttributeField> attributeFields() const = 0;

      /*!
       * \brief How many features the layer holds.
       */
      [[nodiscard]] virtual int featureCount() const = 0;

      /*!
       * \brief One feature's value for one field.
       * \param feature Feature index, in [0, featureCount).
       * \param field Field name from attributeFields().
       * \returns The value, or an invalid QVariant when either is unknown.
       */
      [[nodiscard]] virtual QVariant attributeValue(
        int feature, const QString &field) const = 0;

      /*!
       * \brief Every numeric value of \a field, for computing class breaks.
       *
       * Values that are absent or not numeric are skipped rather than read as
       * zero: a missing measurement is not a measurement of nothing, and
       * treating it as one drags the breaks towards the origin.
       *
       * Virtual because "every value of this field" is not always "every
       * value the layer is showing": a layer whose values were recorded
       * through time holds a level per instant, and breaks computed from the
       * level on screen would mean something different at every step of an
       * animation. Answering here rather than freezing the breaks afterwards
       * keeps the decision in the layer that owns the data, so a restyle
       * after a step and a restyle after a style edit agree without either
       * knowing that time exists.
       *
       * \param field Field to read.
       */
      [[nodiscard]] virtual QVector<double> numericValues(
        const QString &field) const;

      /*!
       * \brief Every distinct value of \a field, in first-seen order.
       *
       * First-seen rather than sorted, so a categorised legend keeps the
       * order the data arrived in when the values have no natural ordering.
       *
       * \param field Field to read.
       */
      [[nodiscard]] QVector<QVariant> distinctValues(
        const QString &field) const;

      /*!
       * \brief Whether \a field exists on this layer.
       * \param field Field name to look for.
       */
      [[nodiscard]] bool hasField(const QString &field) const;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RENDER_ATTRIBUTEPROVIDER_H
