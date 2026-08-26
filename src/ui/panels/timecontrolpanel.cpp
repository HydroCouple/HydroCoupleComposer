#include "ui/panels/timecontrolpanel.h"

#include "results/timecontroller.h"
#include "ui/theme/iconfactory.h"

#include "hydrocouplesdk/temporal/timedata.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QToolButton>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief \a julianDay as a calendar instant.
     *
     * Through the SDK's own conversion rather than a second one written
     * here: the times on the slider came from the SDK, and two conversions
     * that disagree by a rounding would put the readout a step away from
     * the map it labels.
     */
    QString formatInstant(double julianDay)
    {
      int year = 0;
      int month = 0;
      int day = 0;
      int hour = 0;
      int minute = 0;
      double second = 0.0;

      HydroCouple::SDK::Temporal::TimeData::julianDayToGregorian(
        julianDay, year, month, day, hour, minute, second);

      return QStringLiteral("%1-%2-%3 %4:%5:%6")
        .arg(year, 4, 10, QLatin1Char('0'))
        .arg(month, 2, 10, QLatin1Char('0'))
        .arg(day, 2, 10, QLatin1Char('0'))
        .arg(hour, 2, 10, QLatin1Char('0'))
        .arg(minute, 2, 10, QLatin1Char('0'))
        .arg(static_cast<int>(second), 2, 10, QLatin1Char('0'));
    }

    //! A transport button: icon, tooltip, and nothing else to get wrong.
    QToolButton *makeButton(QWidget *parent, const QString &objectName,
                            const QString &alias, const QString &tip)
    {
      auto *button = new QToolButton(parent);
      button->setObjectName(objectName);
      button->setIcon(IconFactory::icon(alias));
      button->setToolTip(tip);
      button->setAutoRaise(true);

      return button;
    }
  } // namespace

  TimeControlPanel::TimeControlPanel(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("timeControlPanel"));

    m_firstButton = makeButton(this, QStringLiteral("firstButton"),
                               QStringLiteral("skip_first"),
                               tr("Go to the first recorded time"));
    m_backButton = makeButton(this, QStringLiteral("stepBackButton"),
                              QStringLiteral("step_back"),
                              tr("Step back one time"));
    m_playButton = makeButton(this, QStringLiteral("playButton"),
                              QStringLiteral("play"), tr("Play"));
    m_forwardButton = makeButton(this, QStringLiteral("stepForwardButton"),
                                 QStringLiteral("step_forward"),
                                 tr("Step forward one time"));
    m_lastButton = makeButton(this, QStringLiteral("lastButton"),
                              QStringLiteral("skip_last"),
                              tr("Go to the last recorded time"));

    m_cycleButton = makeButton(this, QStringLiteral("cycleButton"),
                               QStringLiteral("cycle"),
                               tr("Repeat from the start at the end"));
    m_cycleButton->setCheckable(true);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("timeSlider"));
    m_slider->setSingleStep(1);
    m_slider->setPageStep(1);
    m_slider->setTracking(true);

    m_speed = new QDoubleSpinBox(this);
    m_speed->setObjectName(QStringLiteral("speedSpin"));
    m_speed->setRange(0.1, 120.0);
    m_speed->setDecimals(1);
    m_speed->setSingleStep(1.0);
    m_speed->setSuffix(tr(" steps/s"));
    m_speed->setToolTip(tr("How fast playback runs"));

    m_time = new QLabel(this);
    m_time->setObjectName(QStringLiteral("timeLabel"));
    m_time->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // Fixed width from the widest instant it will ever show, so the slider
    // beside it does not resize on every step of an animation.
    m_time->setMinimumWidth(
      m_time->fontMetrics().horizontalAdvance(
        QStringLiteral("0000-00-00 00:00:00")));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->addWidget(m_firstButton);
    layout->addWidget(m_backButton);
    layout->addWidget(m_playButton);
    layout->addWidget(m_forwardButton);
    layout->addWidget(m_lastButton);
    layout->addWidget(m_cycleButton);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_time);
    layout->addWidget(m_speed);

    connect(m_slider, &QSlider::valueChanged, this,
            [this](int value)
            {
              if (m_clock)
              {
                m_clock->setStep(value);
              }
            });

    connect(m_firstButton, &QToolButton::clicked, this,
            [this]
            {
              if (m_clock)
              {
                m_clock->toFirst();
              }
            });

    connect(m_backButton, &QToolButton::clicked, this,
            [this]
            {
              if (m_clock)
              {
                m_clock->advance(-1);
              }
            });

    connect(m_playButton, &QToolButton::clicked, this,
            [this]
            {
              if (!m_clock)
              {
                return;
              }

              // One button for both, because play and pause are one decision
              // and two buttons would let the map be shown as playing while
              // it is not.
              if (m_clock->isPlaying())
              {
                m_clock->pause();
              }
              else
              {
                m_clock->play();
              }
            });

    connect(m_forwardButton, &QToolButton::clicked, this,
            [this]
            {
              if (m_clock)
              {
                m_clock->advance(1);
              }
            });

    connect(m_lastButton, &QToolButton::clicked, this,
            [this]
            {
              if (m_clock)
              {
                m_clock->toLast();
              }
            });

    connect(m_cycleButton, &QToolButton::toggled, this,
            [this](bool on)
            {
              if (m_clock)
              {
                m_clock->setLooping(on);
              }
            });

    connect(m_speed, &QDoubleSpinBox::valueChanged, this,
            [this](double value)
            {
              if (m_clock)
              {
                m_clock->setSpeed(value);
              }
            });

    refreshSpan();
  }

  TimeControlPanel::~TimeControlPanel() = default;

  void TimeControlPanel::setController(TimeController *clock)
  {
    if (m_clock == clock)
    {
      return;
    }

    if (m_clock)
    {
      m_clock->disconnect(this);
    }

    m_clock = clock;

    if (m_clock)
    {
      connect(m_clock, &TimeController::spanChanged, this,
              [this] { refreshSpan(); });
      connect(m_clock, &TimeController::currentChanged, this,
              [this](double) { refreshPosition(); });
      connect(m_clock, &TimeController::playingChanged, this,
              [this](bool) { refreshState(); });
      connect(m_clock, &QObject::destroyed, this,
              [this] { setController(nullptr); });
    }

    refreshSpan();
  }

  TimeController *TimeControlPanel::controller() const
  {
    return m_clock;
  }

  QString TimeControlPanel::timeText() const
  {
    return m_time->text();
  }

  void TimeControlPanel::refreshSpan()
  {
    const int stops = m_clock ? static_cast<int>(m_clock->steps().size()) : 0;

    m_slider->setRange(0, std::max(0, stops - 1));

    if (m_clock)
    {
      m_speed->setValue(m_clock->speed());
      m_cycleButton->setChecked(m_clock->isLooping());
    }

    refreshPosition();
  }

  void TimeControlPanel::refreshPosition()
  {
    if (m_clock && m_clock->hasTime())
    {
      const int step = m_clock->step();

      if (step >= 0)
      {
        m_slider->setValue(step);
      }

      m_time->setText(formatInstant(m_clock->current()));
    }
    else
    {
      // An em dash rather than a zero instant: a stack with no recorded time
      // has no time to show, and 4713 BC is what a Julian day of nothing
      // formats as.
      m_time->setText(QStringLiteral("—"));
    }

    refreshState();
  }

  void TimeControlPanel::refreshState()
  {
    const bool has = m_clock && m_clock->hasTime();
    const int step = has ? m_clock->step() : -1;
    const int last = has ? static_cast<int>(m_clock->steps().size()) - 1 : -1;
    const bool playing = m_clock && m_clock->isPlaying();

    m_slider->setEnabled(has);
    m_speed->setEnabled(has);
    m_cycleButton->setEnabled(has);
    m_playButton->setEnabled(has && last > 0);

    // Disabled at the ends, so the controls say where the run is rather than
    // silently doing nothing when pressed.
    m_firstButton->setEnabled(has && step != 0);
    m_backButton->setEnabled(has && step != 0);
    m_forwardButton->setEnabled(has && step != last);
    m_lastButton->setEnabled(has && step != last);

    m_playButton->setIcon(IconFactory::icon(
      playing ? QStringLiteral("pause") : QStringLiteral("play")));
    m_playButton->setToolTip(playing ? tr("Pause") : tr("Play"));
  }

} // namespace HydroCouple::Composer
