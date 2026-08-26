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
       * \brief How many time levels the item carries; 0 when it is static.
       *
       * From the item's own time-series interface rather than from its
       * shape: an axis of length 6 is only a time axis if the item says so,
       * and a mesh with six layers has the same shape as a mesh over six
       * times.
       */
      [[nodiscard]] int timeCount() const;

      /*!
       * \brief Which time level is being shown.
       *
       * The last level until something chooses otherwise: for a component
       * still running that is "now", and it is what a layer showed before
       * time was a choice.
       *
       * \returns The index, or -1 for a static item.
       */
      [[nodiscard]] int timeIndex() const;

      /*!
       * \brief Shows the values at \a index.
       *
       * Geometry is untouched — see refreshValues() — so stepping through a
       * run costs one hyperslab read per layer per step and nothing else.
       *
       * \param index Time level; clamped to what the item carries.
       * \returns True when the values were read.
       */
      bool setTimeIndex(int index);

      /*!
       * \brief The time at \a index, as a Julian day.
       *
       * What a clock shared between layers compares: two items recorded on
       * different axes have nothing in common but the instant they name.
       *
       * \param index Time level.
       * \returns The Julian day, or 0 when the item is static.
       */
      [[nodiscard]] double timeAt(int index) const;

      /*!
       * \brief The time level nearest \a julianDay.
       *
       * Nearest rather than interpolated: a value recorded at one instant
       * is what the model computed, and showing a blend of two would put a
       * number on the map the model never produced.
       *
       * \param julianDay The instant wanted.
       * \returns The index, or -1 for a static item.
       */
      [[nodiscard]] int nearestTime(double julianDay) const;

      /*!
       * \brief Every value of \a field, across every level the item carries.
       *
       * For the value attribute of an item recorded through time this is the
       * whole record, not the level on screen. Class breaks come from this,
       * so a colour means the same number at every step and the legend can
       * be read against the map while it animates. Any other field — one
       * derived from the geometry, which does not move — answers as usual.
       *
       * \param field Field to read.
       */
      [[nodiscard]] QVector<double> numericValues(
        const QString &field) const override;

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

      /*!
       * \brief Values across every level, for classification.
       *
       * Extended rather than rebuilt: a component still running records more
       * levels as it goes, and re-reading the whole record on each of them
       * would cost a pass per step instead of a pass per run.
       */
      [[nodiscard]] const QVector<double> &valuesAcrossTime() const;

      HydroCouple::IComponentDataItem *m_item = nullptr;
      QString m_valueAttribute;

      //! Values are the last field, so geometry-derived fields keep their
      //! indices when the values are re-read.
      int m_valueFieldIndex = -1;

      //! Which time level is shown; -1 until chosen, meaning the last.
      int m_timeIndex = -1;

      //! Values pooled across levels, and how many levels they cover.
      mutable QVector<double> m_acrossTime;
      mutable int m_acrossTimeLevels = 0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_DATAITEMLAYER_H
