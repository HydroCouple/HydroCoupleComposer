/*!
 * \file   timelayer.h
 * \author Caleb Buahin
 * \brief  ITimeLayer — a layer whose values were recorded through time.
 *
 * The clock and the plot panel both need the same four questions answered —
 * how many instants, which instant is level N, which level is nearest an
 * instant, and show me that level — and neither of them cares what kind of
 * layer is answering. Asking a concrete class was fine while exactly one
 * kind of layer was recorded through time; a second one is where that stops,
 * because the alternative is a chain of dynamic_casts that has to be
 * extended in every caller each time a third arrives.
 *
 * Instants are Julian days, which is the only thing two runs recorded on
 * different axes have in common.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_TIMELAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_TIMELAYER_H

#include <QString>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief A layer that carries values at more than one instant.
   */
  class ITimeLayer
  {
    public:
      virtual ~ITimeLayer() = default;

      /*!
       * \brief How many instants this layer carries; 0 when it is static.
       */
      [[nodiscard]] virtual int timeCount() const = 0;

      /*!
       * \brief Which level is being shown, or -1 for a static layer.
       */
      [[nodiscard]] virtual int timeIndex() const = 0;

      /*!
       * \brief Shows the values at \a index.
       * \param index Time level; clamped to what the layer carries.
       * \returns True when the values were read.
       */
      virtual bool setTimeIndex(int index) = 0;

      /*!
       * \brief The instant at \a index, as a Julian day.
       * \param index Time level.
       * \returns The Julian day, or 0 when the layer is static.
       */
      [[nodiscard]] virtual double timeAt(int index) const = 0;

      /*!
       * \brief The level nearest \a julianDay.
       *
       * Nearest rather than interpolated: a value recorded at one instant is
       * what the model computed, and a blend of two would put a number on
       * the map the model never produced.
       *
       * \param julianDay The instant wanted.
       * \returns The index, or -1 for a static layer.
       */
      [[nodiscard]] virtual int nearestTime(double julianDay) const = 0;

      /*!
       * \brief One feature's whole recorded series, oldest instant first.
       *
       * \param feature Feature index, in [0, featureCount).
       * \param[out] values Receives one value per instant.
       * \param[out] message Diagnostic on failure.
       * \returns True when the series was read; false for a static layer.
       */
      [[nodiscard]] virtual bool valuesOverTime(int feature,
                                                QVector<double> &values,
                                                QString &message) const = 0;

      /*!
       * \brief Every instant this layer carries, as Julian days.
       *
       * The x of a plot, paired with valuesOverTime()'s y.
       */
      [[nodiscard]] virtual QVector<double> times() const = 0;

      /*!
       * \brief The attribute the recorded values are offered under.
       *
       * Here rather than guessed at from the field list, because a plot
       * overlaying two layers has to know whether they are showing the same
       * variable before it puts one label on the axis for both.
       */
      [[nodiscard]] virtual QString valueAttribute() const = 0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_TIMELAYER_H
