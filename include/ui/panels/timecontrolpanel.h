/*!
 * \file   timecontrolpanel.h
 * \author Caleb Buahin
 * \brief  TimeControlPanel — the transport controls for a run.
 *
 * A view of TimeController and nothing else: the slider, the buttons and the
 * readout all drive the one clock, and the clock drives the layers. Nothing
 * here holds a time of its own, so the slider cannot come to rest anywhere
 * the map is not showing.
 *
 * The stops are the clock's, which are the union of every layer's record
 * rather than one layer's. A slider numbered by one layer's steps would put
 * a layer recorded hourly and one recorded daily on the same frame number
 * and show them centuries apart.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_TIMECONTROLPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_TIMECONTROLPANEL_H

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QSlider;
class QToolButton;

namespace HydroCouple::Composer
{
  class TimeController;

  /*!
   * \brief Plays, pauses and scrubs a run.
   */
  class TimeControlPanel : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the panel.
       * \param parent Parent widget.
       */
      explicit TimeControlPanel(QWidget *parent = nullptr);

      ~TimeControlPanel() override;

      /*!
       * \brief Drives \a clock, or nothing when null.
       * \param clock The clock to control; not owned.
       */
      void setController(TimeController *clock);

      //! \returns The clock being controlled, or nullptr.
      [[nodiscard]] TimeController *controller() const;

      /*!
       * \brief The instant shown, formatted as it appears in the readout.
       *
       * Exposed so a test can check what the panel says rather than only
       * that it says something.
       */
      [[nodiscard]] QString timeText() const;

    private:
      //! Takes the range and the stops from the clock.
      void refreshSpan();

      //! Moves the slider and the readout to where the clock is.
      void refreshPosition();

      //! Enables what there is to do, and shows play or pause.
      void refreshState();

      QSlider *m_slider = nullptr;
      QToolButton *m_firstButton = nullptr;
      QToolButton *m_backButton = nullptr;
      QToolButton *m_playButton = nullptr;
      QToolButton *m_forwardButton = nullptr;
      QToolButton *m_lastButton = nullptr;
      QToolButton *m_cycleButton = nullptr;
      QDoubleSpinBox *m_speed = nullptr;
      QLabel *m_time = nullptr;

      TimeController *m_clock = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_TIMECONTROLPANEL_H
