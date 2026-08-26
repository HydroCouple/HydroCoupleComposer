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

class QTimer;

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

      // ── Playback ─────────────────────────────────────────────────────

      //! \returns Whether the clock is running.
      [[nodiscard]] bool isPlaying() const;

      //! \returns Steps shown per second while running.
      [[nodiscard]] double speed() const;

      /*!
       * \brief Sets how fast playback runs.
       * \param stepsPerSecond Clamped to [0.1, 120]; takes effect at once,
       *        so a run can be slowed down while it is playing.
       */
      void setSpeed(double stepsPerSecond);

      //! \returns Whether playback restarts after the last step.
      [[nodiscard]] bool isLooping() const;

      /*!
       * \brief Sets whether playback restarts after the last step.
       * \param looping True to cycle.
       */
      void setLooping(bool looping);

    public Q_SLOTS:
      /*!
       * \brief Starts playing, from the beginning when already at the end.
       *
       * Pressing play on a finished run should replay it rather than do
       * nothing, which is what starting from a clock parked on the last
       * step would otherwise mean.
       */
      void play();

      //! Stops playing, leaving the clock where it is.
      void pause();

      //! Shows the earliest instant any layer carries.
      void toFirst();

      //! Shows the latest instant any layer carries.
      void toLast();

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

      /*!
       * \brief Emitted when playback starts or stops.
       * \param playing Whether the clock is now running.
       *
       * Including when it stops by reaching the end, which is the case a
       * play button that only tracked its own clicks would get wrong.
       */
      void playingChanged(bool playing);

    private:
      //! Moves every time-aware layer to the level nearest the current time.
      void applyToLayers();

      //! Shows the next step, stopping or cycling at the end.
      void onTick();

      LayerStackModel *m_model = nullptr;

      QVector<double> m_steps;
      double m_current = 0.0;

      QTimer *m_timer = nullptr;
      double m_speed = 4.0;
      bool m_looping = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RESULTS_TIMECONTROLLER_H
