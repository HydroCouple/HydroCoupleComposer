#include "results/timecontroller.h"

#include "layers/timelayer.h"
#include "map/layerstackmodel.h"
#include "map/maplayer.h"

#include <QTimer>

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief How close two instants must be to count as the same one.
     *
     * A Julian day is a large number carrying a small interval, so two
     * layers recording "the same" instant routinely differ in the last bits.
     * A tenth of a second is finer than any model step and coarser than that
     * rounding.
     */
    constexpr double kSameInstant = 1.0 / (24.0 * 60.0 * 60.0 * 10.0);
  }

  TimeController::TimeController(QObject *parent) : QObject(parent) {}

  TimeController::~TimeController() = default;

  void TimeController::setModel(LayerStackModel *model)
  {
    if (m_model == model)
    {
      return;
    }

    if (m_model)
    {
      m_model->disconnect(this);
    }

    m_model = model;

    if (m_model)
    {
      // A layer arriving or leaving changes what times exist, and the model
      // signals both as row changes.
      connect(m_model, &QAbstractItemModel::rowsInserted, this,
              [this](const QModelIndex &, int, int) { refresh(); });
      connect(m_model, &QAbstractItemModel::rowsRemoved, this,
              [this](const QModelIndex &, int, int) { refresh(); });
      connect(m_model, &QObject::destroyed, this,
              [this] { setModel(nullptr); });
    }

    refresh();
  }

  LayerStackModel *TimeController::model() const
  {
    return m_model;
  }

  void TimeController::refresh()
  {
    QVector<double> gathered;

    if (m_model)
    {
      for (MapLayer *layer : m_model->layers())
      {
        auto *item = dynamic_cast<ITimeLayer *>(layer);

        if (!item)
        {
          continue;
        }

        for (int index = 0; index < item->timeCount(); ++index)
        {
          gathered.append(item->timeAt(index));
        }
      }
    }

    std::sort(gathered.begin(), gathered.end());

    QVector<double> distinct;

    for (const double instant : gathered)
    {
      // Merged within a tolerance rather than by equality: two layers
      // recording the same instant differ in the last bits of a Julian day,
      // and a slider with two indistinguishable stops beside each other is
      // a slider that skips.
      if (distinct.isEmpty()
          || std::abs(instant - distinct.last()) > kSameInstant)
      {
        distinct.append(instant);
      }
    }

    const bool changed = distinct != m_steps;

    m_steps = distinct;

    if (!m_steps.isEmpty())
    {
      // Held where it was when the new span still contains it, so adding a
      // layer does not jump the view back to the beginning of the run.
      m_current = std::clamp(m_current, m_steps.first(), m_steps.last());

      if (step() < 0)
      {
        m_current = m_steps.first();
      }
    }

    if (changed)
    {
      Q_EMIT spanChanged();
    }

    applyToLayers();
  }

  bool TimeController::hasTime() const
  {
    return !m_steps.isEmpty();
  }

  double TimeController::first() const
  {
    return m_steps.isEmpty() ? 0.0 : m_steps.first();
  }

  double TimeController::last() const
  {
    return m_steps.isEmpty() ? 0.0 : m_steps.last();
  }

  const QVector<double> &TimeController::steps() const
  {
    return m_steps;
  }

  double TimeController::current() const
  {
    return m_current;
  }

  void TimeController::setCurrent(double julianDay)
  {
    if (m_steps.isEmpty())
    {
      return;
    }

    const double wanted =
      std::clamp(julianDay, m_steps.first(), m_steps.last());

    if (std::abs(wanted - m_current) <= kSameInstant)
    {
      return;
    }

    m_current = wanted;

    applyToLayers();

    Q_EMIT currentChanged(m_current);
  }

  void TimeController::setStep(int step)
  {
    if (step < 0 || step >= m_steps.size())
    {
      return;
    }

    setCurrent(m_steps.at(step));
  }

  int TimeController::step() const
  {
    for (int index = 0; index < m_steps.size(); ++index)
    {
      if (std::abs(m_steps.at(index) - m_current) <= kSameInstant)
      {
        return index;
      }
    }

    return -1;
  }

  void TimeController::advance(int delta)
  {
    if (m_steps.isEmpty())
    {
      return;
    }

    const int from = step();

    // From where the slider is, or from the nearest stop when the clock sits
    // between two — a step from a time nothing was recorded at still has to
    // land on one.
    int base = from;

    if (base < 0)
    {
      base = 0;

      for (int index = 1; index < m_steps.size(); ++index)
      {
        if (std::abs(m_steps.at(index) - m_current)
            < std::abs(m_steps.at(base) - m_current))
        {
          base = index;
        }
      }
    }

    setStep(std::clamp(base + delta, 0, int(m_steps.size()) - 1));
  }

  // ── Playback ─────────────────────────────────────────────────────────────

  bool TimeController::isPlaying() const
  {
    return m_timer && m_timer->isActive();
  }

  double TimeController::speed() const
  {
    return m_speed;
  }

  void TimeController::setSpeed(double stepsPerSecond)
  {
    m_speed = std::clamp(stepsPerSecond, 0.1, 120.0);

    if (m_timer)
    {
      // Applied while running as well as before: an animation that can only
      // be slowed down by stopping it first cannot be slowed down to look
      // at the moment that needed slowing down.
      m_timer->setInterval(static_cast<int>(std::lround(1000.0 / m_speed)));
    }
  }

  bool TimeController::isLooping() const
  {
    return m_looping;
  }

  void TimeController::setLooping(bool looping)
  {
    m_looping = looping;
  }

  void TimeController::play()
  {
    if (m_steps.size() < 2 || isPlaying())
    {
      return;
    }

    // Parked on the last step, play means replay. Otherwise pressing play on
    // a run that has just finished does nothing at all.
    if (step() == m_steps.size() - 1)
    {
      setStep(0);
    }

    if (!m_timer)
    {
      m_timer = new QTimer(this);
      connect(m_timer, &QTimer::timeout, this, &TimeController::onTick);
    }

    m_timer->setInterval(static_cast<int>(std::lround(1000.0 / m_speed)));
    m_timer->start();

    Q_EMIT playingChanged(true);
  }

  void TimeController::pause()
  {
    if (!isPlaying())
    {
      return;
    }

    m_timer->stop();

    Q_EMIT playingChanged(false);
  }

  void TimeController::toFirst()
  {
    setStep(0);
  }

  void TimeController::toLast()
  {
    setStep(m_steps.size() - 1);
  }

  void TimeController::onTick()
  {
    const int last = m_steps.size() - 1;

    if (step() >= last)
    {
      if (!m_looping)
      {
        pause();
        return;
      }

      setStep(0);
      return;
    }

    advance(1);
  }

  void TimeController::applyToLayers()
  {
    if (!m_model)
    {
      return;
    }

    for (MapLayer *layer : m_model->layers())
    {
      auto *item = dynamic_cast<ITimeLayer *>(layer);

      if (!item)
      {
        continue;
      }

      // Each layer answers for itself: two recorded on different axes have
      // nothing in common but the instant, so the clock names one and every
      // layer finds its own nearest level to it.
      //
      // A static layer answers -1 and refuses the level, so it needs no
      // guard here. Two were written and deleted — one on timeCount(), one
      // on the -1 — because setTimeIndex() already says no.
      item->setTimeIndex(item->nearestTime(m_current));
    }
  }

} // namespace HydroCouple::Composer
