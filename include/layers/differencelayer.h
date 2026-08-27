/*!
 * \file   differencelayer.h
 * \author Caleb Buahin
 * \brief  DifferenceLayer — one variable, two runs, drawn as the difference.
 *
 * Comparing two runs is the reason a viewer holds two open. Side by side
 * answers "what does each look like"; this answers "where do they disagree,
 * and by how much" — which is the question that actually gets asked, and the
 * one two maps side by side are worst at, because the eye cannot subtract.
 *
 * The layer is the difference, not a view of two others: it holds its own
 * geometry and offers one attribute, so classification, the legend, the
 * attribute table, the 3D scene and the clock all treat it as an ordinary
 * layer and none of them needs to know it was derived.
 *
 * Two runs rarely share a time axis, so each instant of the base run is
 * matched against the nearest instant of the other. That is the same rule
 * the clock already uses to drive layers recorded on different axes, and it
 * is stated here rather than assumed because the alternative — pairing by
 * level index — silently compares hour three against hour thirty whenever
 * the two runs reported at different rates.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_DIFFERENCELAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_DIFFERENCELAYER_H

#include "layers/dataitemlayer.h"

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief One variable's difference between two runs, on the map.
   */
  class DifferenceLayer : public FeatureLayer, public ITimeLayer
  {
    public:
      /*!
       * \brief Builds the difference \a base minus \a other.
       *
       * Refused rather than approximated when the two were not recorded on
       * the same geometry. A difference of two meshes that merely happen to
       * have the same number of cells is a map of nothing, and it looks
       * exactly like a map of something.
       *
       * \param base The item to subtract from; its geometry and time axis
       *        are the layer's own.
       * \param other The item to subtract; must outlive the layer.
       * \param[out] message Diagnostic on failure.
       * \returns The layer, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<DifferenceLayer> create(
        HydroCouple::IComponentDataItem *base,
        HydroCouple::IComponentDataItem *other, QString &message);

      ~DifferenceLayer() override;

      /*!
       * \brief The name of the attribute holding the differences.
       */
      [[nodiscard]] QString valueAttribute() const;

      // ── ITimeLayer ───────────────────────────────────────────────────────

      /*!
       * \brief The base run's instants; 0 when neither was recorded in time.
       *
       * The base's rather than the union of both: the difference is stated
       * at the instants one of the two actually reported, and inventing a
       * merged axis would put values on the map at instants neither run has.
       */
      [[nodiscard]] int timeCount() const override;

      //! \copydoc ITimeLayer::timeIndex
      [[nodiscard]] int timeIndex() const override;

      //! \copydoc ITimeLayer::setTimeIndex
      bool setTimeIndex(int index) override;

      //! \copydoc ITimeLayer::timeAt
      [[nodiscard]] double timeAt(int index) const override;

      //! \copydoc ITimeLayer::nearestTime
      [[nodiscard]] int nearestTime(double julianDay) const override;

      /*!
       * \brief One feature's difference series, oldest instant first.
       *
       * Each of the base's instants against the other run's nearest, so the
       * series has the base's length whatever the other run reported at.
       */
      [[nodiscard]] bool valuesOverTime(int feature, QVector<double> &values,
                                        QString &message) const override;

      //! \copydoc ITimeLayer::times
      [[nodiscard]] QVector<double> times() const override;

      /*!
       * \brief Every difference, across every instant the base carries.
       *
       * Class breaks come from this rather than from the level on screen, so
       * a colour means the same number at every step — and on a difference
       * map that matters more than on any other, since the interesting
       * question is which step disagrees most.
       */
      [[nodiscard]] QVector<double> numericValues(
        const QString &field) const override;

    private:
      DifferenceLayer(const QString &name,
                      std::unique_ptr<DataItemLayer> base,
                      std::unique_ptr<DataItemLayer> other);

      /*!
       * \brief Re-reads both runs at the current instant and restyles.
       */
      bool refreshValues();

      /*!
       * \brief Which of the other run's levels stands at \a julianDay.
       * \param julianDay The instant the base names.
       * \returns The other run's nearest level, or its only one.
       */
      [[nodiscard]] int otherLevelFor(double julianDay) const;

      /*!
       * \brief The differences at \a index of the base's axis.
       * \param index Base time level.
       * \param[out] values One difference per feature.
       * \param[out] message Diagnostic on failure.
       */
      [[nodiscard]] bool differencesAt(int index, QVector<double> &values,
                                       QString &message) const;

      /*!
       * \brief Whether \a left and \a right drew the same ground.
       *
       * Compared vertex by vertex, within a tolerance taken from the extent,
       * because two runs of one model over one mesh write the same numbers
       * and two runs over different meshes do not — and a count that matches
       * proves neither.
       *
       * \param left One layer's geometry.
       * \param right The other's.
       * \param[out] message Why they are not the same ground.
       */
      [[nodiscard]] static bool sameGeometry(const FeatureLayer &left,
                                             const FeatureLayer &right,
                                             QString &message);

      /*!
       * \brief The two runs' layers.
       *
       * Held rather than borrowed from the stack, so removing the layer a
       * comparison was started from cannot leave this one reading freed
       * memory. They are never drawn: their geometry is the price of not
       * writing a second reader for the same hyperslabs.
       */
      std::unique_ptr<DataItemLayer> m_base;
      std::unique_ptr<DataItemLayer> m_other;

      QString m_valueAttribute;

      //! Values are the last field, so geometry-derived fields keep their
      //! indices when the differences are re-read.
      int m_valueFieldIndex = -1;

      //! Which of the base's levels is shown; -1 until chosen, the last.
      int m_timeIndex = -1;

      //! Differences pooled across every instant, and how many they cover.
      mutable QVector<double> m_acrossTime;
      mutable int m_acrossTimeLevels = 0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_DIFFERENCELAYER_H
