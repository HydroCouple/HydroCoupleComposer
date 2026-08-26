/*!
 * \file   timecontroller.h
 * \author Caleb Buahin
 * \brief  TimeController — one instant, shared by everything that draws.
 *
 * Layers recorded on different axes have nothing in common but the instant
 * they name, so the clock holds a Julian day and each layer answers with the
 * level nearest to it. The alternative — a step counter — would put layers
 * recorded hourly and daily on the same frame number and show them centuries
 * apart.
 *
 * Nearest, not interpolated. A value recorded at an instant is what the model
 * computed there; a blend of two is a number the model never produced, and a
 * map that shows one is a map that cannot be checked against the run.
 */

#ifndef HYDROCOUPLECOMPOSER_RESULTS_TIMECONTROLLER_H
#define HYDROCOUPLECOMPOSER_RESULTS_TIMECONTROLLER_H

#include <QObject>
#include <QVector>

namespace HydroCouple::Composer
{
  class LayerStackModel;

  /*!
   * \brief The instant every time-aware layer is showing.
   */
  class TimeController : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the clock.
       * \param parent Owning object.
       */
      explicit TimeController(QObject *parent = nullptr);

      ~TimeController() override;

      /*!
       * \brief Drives the layers of \a model, or none when null.
       * \param model The stack to step; not owned.
       */
      void setModel(LayerStackModel *model);

      //! \returns The stack being driven, or nullptr.
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief Re-reads what times the stack's layers carry.
       *
       * Called when layers arrive or leave. The span is the union of every
       * time-aware layer's, so a layer covering an hour of a week-long run
       * is reachable rather than clipped out of the slider.
       */
      void refresh();

      //! \returns Whether any layer in the stack carries a time axis.
      [[nodiscard]] bool hasTime() const;

      //! \returns The earliest instant any layer carries, as a Julian day.
      [[nodiscard]] double first() const;

      //! \returns The latest instant any layer carries.
      [[nodiscard]] double last() const;

      /*!
       * \brief Every distinct instant the stack carries, in order.
       *
       * The union rather than one layer's: stepping should stop at each
       * instant *something* was recorded at, not only at the instants the
       * first layer happened to use.
       */
      [[nodiscard]] const QVector<double> &steps() const;

      //! \returns The instant being shown.
      [[nodiscard]] double current() const;

      /*!
       * \brief Shows \a julianDay, moving every time-aware layer to it.
       * \param julianDay The instant to show; clamped to the span.
       */
      void setCurrent(double julianDay);

      /*!
       * \brief Shows the instant at \a step of steps().
       * \param step Index into steps().
       */
      void setStep(int step);

      //! \returns Which of steps() is showing, or -1.
      [[nodiscard]] int step() const;

      /*!
       * \brief Moves \a delta steps, stopping at the ends.
       * \param delta Steps to move; negative goes back.
       */
      void advance(int delta);

    Q_SIGNALS:
      /*!
       * \brief Emitted when the instant shown changes.
       * \param julianDay The new instant.
       */
      void currentChanged(double julianDay);

      /*!
       * \brief Emitted when the span or the steps change.
       *
       * What a slider listens to: its range comes from the stack, and a
       * layer arriving with a longer record changes it.
       */
      void spanChanged();

    private:
      //! Moves every time-aware layer to the level nearest the current time.
      void applyToLayers();

      LayerStackModel *m_model = nullptr;

      QVector<double> m_steps;
      double m_current = 0.0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RESULTS_TIMECONTROLLER_H
