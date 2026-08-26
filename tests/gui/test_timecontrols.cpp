/*!
 * \file   test_timecontrols.cpp
 * \brief  Phase D2c verification — playing a run, and the transport over it.
 *
 * Two claims, and they are separate. The clock plays: it advances on its own,
 * stops at the end rather than running past it, and starts over when asked to
 * cycle. And the panel is a view of that clock and holds no time of its own —
 * so the slider cannot come to rest anywhere the map is not showing, whether
 * the clock was moved from the panel or from anywhere else.
 */

#include "core/composerapplication.h"
#include "layers/dataitemlayer.h"
#include "map/layerstackmodel.h"
#include "results/timecontroller.h"
#include "spatialstubs.h"
#include "ui/panels/timecontrolpanel.h"

#include <gtest/gtest.h>

#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QSlider>
#include <QToolButton>

#include <memory>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  //! J2000, so the fixtures sit where real Julian days do.
  constexpr double kEpoch = 2451545.0;

  class TimeControlsTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_timecontrols";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        m_crs = std::make_unique<Testing::StubCrs>(4326);
        m_a = Testing::makePoint(0.0, 0.0, 0, m_crs.get());
        m_b = Testing::makePoint(1.0, 1.0, 1, m_crs.get());
      }

      //! A stack holding one layer of \a steps daily levels from J2000.
      void buildStack(int steps)
      {
        m_item = std::make_unique<Testing::StubTimeGeometryItem>(
          "depth", steps,
          std::vector<HydroCouple::Spatial::IGeometry *>{
            static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
            static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get())},
          kEpoch, 1.0);

        for (int step = 0; step < steps; ++step)
        {
          for (int geometry = 0; geometry < 2; ++geometry)
          {
            m_item->setValue(step, geometry, step * 100.0 + geometry);
          }
        }

        QString message;
        DataItemLayer *layer =
          DataItemLayer::create(m_item.get(), message).release();
        ASSERT_NE(layer, nullptr) << message.toStdString();
        ASSERT_GE(m_stack.addLayer(layer), 0);

        m_clock.setModel(&m_stack);
      }

      /*!
       * \brief Spins the event loop until \a done, or \a limitMs passes.
       * \returns Whether the condition came true.
       */
      static bool waitFor(const std::function<bool()> &done, int limitMs)
      {
        QElapsedTimer elapsed;
        elapsed.start();

        while (!done() && elapsed.elapsed() < limitMs)
        {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }

        return done();
      }

      std::unique_ptr<Testing::StubCrs> m_crs;
      std::unique_ptr<Testing::StubPoint> m_a;
      std::unique_ptr<Testing::StubPoint> m_b;
      std::unique_ptr<Testing::StubTimeGeometryItem> m_item;

      LayerStackModel m_stack;
      TimeController m_clock;

      static ComposerApplication *s_app;
  };

  ComposerApplication *TimeControlsTest::s_app = nullptr;
}

TEST_F(TimeControlsTest, PlayingAdvancesAndStopsAtTheEnd)
{
  buildStack(4);
  m_clock.setStep(0);
  m_clock.setSpeed(120.0);

  QSignalSpy playing(&m_clock, &TimeController::playingChanged);

  m_clock.play();
  EXPECT_TRUE(m_clock.isPlaying());

  ASSERT_TRUE(waitFor([this] { return !m_clock.isPlaying(); }, 5000))
    << "playback ran past the end of the record instead of stopping";

  // Stopped *at* the last step, not somewhere before it: an animation that
  // stops early never shows the state the run finished in.
  EXPECT_EQ(m_clock.step(), 3);

  // Both edges reported, so a play button can show what the clock is doing
  // rather than what it was last told to do.
  ASSERT_EQ(playing.size(), 2);
  EXPECT_TRUE(playing.at(0).at(0).toBool());
  EXPECT_FALSE(playing.at(1).at(0).toBool());
}

TEST_F(TimeControlsTest, PlayingFromTheEndStartsOver)
{
  buildStack(4);
  m_clock.toLast();
  ASSERT_EQ(m_clock.step(), 3);

  m_clock.setSpeed(0.1);
  m_clock.play();

  // Pressing play on a finished run replays it. Starting from a clock parked
  // on the last step would mean the button did nothing at all.
  EXPECT_EQ(m_clock.step(), 0);

  m_clock.pause();
}

TEST_F(TimeControlsTest, CyclingRestartsRatherThanStopping)
{
  buildStack(3);
  m_clock.setStep(0);
  m_clock.setSpeed(120.0);
  m_clock.setLooping(true);

  m_clock.play();

  // Round the end at least once and keep going, rather than stopping there.
  ASSERT_TRUE(waitFor([this] { return m_clock.step() == 2; }, 5000));
  ASSERT_TRUE(waitFor([this] { return m_clock.step() == 0; }, 5000))
    << "a cycling clock stopped at the end of the record";

  EXPECT_TRUE(m_clock.isPlaying());

  m_clock.pause();
  EXPECT_FALSE(m_clock.isPlaying());
}

TEST_F(TimeControlsTest, TheSliderScrubsTheClockAndTheClockMovesTheSlider)
{
  buildStack(5);

  TimeControlPanel panel;
  panel.setController(&m_clock);

  auto *slider = panel.findChild<QSlider *>(QStringLiteral("timeSlider"));
  ASSERT_NE(slider, nullptr);

  // The stops are the clock's, so the slider spans the record.
  EXPECT_EQ(slider->minimum(), 0);
  EXPECT_EQ(slider->maximum(), 4);

  slider->setValue(3);
  EXPECT_EQ(m_clock.step(), 3);

  // And back the other way: a clock moved from somewhere else — a plot
  // cursor, a play tick — has to bring the slider with it, or the panel
  // starts showing an instant the map is not.
  m_clock.setStep(1);
  EXPECT_EQ(slider->value(), 1);
}

TEST_F(TimeControlsTest, TheReadoutNamesTheInstantOnScreen)
{
  buildStack(4);

  TimeControlPanel panel;
  panel.setController(&m_clock);

  m_clock.setStep(0);

  // J2000 is noon on the first of January 2000 — a known instant, so this
  // catches a conversion that is a half-day or a day out as well as one that
  // simply prints something.
  EXPECT_EQ(panel.timeText(), QStringLiteral("2000-01-01 12:00:00"));

  m_clock.setStep(2);
  EXPECT_EQ(panel.timeText(), QStringLiteral("2000-01-03 12:00:00"));
}

TEST_F(TimeControlsTest, TheTransportSaysWhereTheRunIs)
{
  buildStack(3);

  TimeControlPanel panel;
  panel.setController(&m_clock);

  auto *first = panel.findChild<QToolButton *>(QStringLiteral("firstButton"));
  auto *back = panel.findChild<QToolButton *>(QStringLiteral("stepBackButton"));
  auto *forward =
    panel.findChild<QToolButton *>(QStringLiteral("stepForwardButton"));
  auto *last = panel.findChild<QToolButton *>(QStringLiteral("lastButton"));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(back, nullptr);
  ASSERT_NE(forward, nullptr);
  ASSERT_NE(last, nullptr);

  m_clock.setStep(0);
  EXPECT_FALSE(back->isEnabled());
  EXPECT_FALSE(first->isEnabled());
  EXPECT_TRUE(forward->isEnabled());
  EXPECT_TRUE(last->isEnabled());

  m_clock.toLast();
  EXPECT_TRUE(back->isEnabled());
  EXPECT_TRUE(first->isEnabled());
  EXPECT_FALSE(forward->isEnabled())
    << "the run offered a step past its last recorded time";
  EXPECT_FALSE(last->isEnabled());
}

TEST_F(TimeControlsTest, AStackWithNoRecordOffersNothingToPlay)
{
  TimeControlPanel panel;
  panel.setController(&m_clock);

  // Nothing loaded: a slider and a play button over an empty stack are
  // controls that cannot do anything, and saying so is better than moving.
  auto *slider = panel.findChild<QSlider *>(QStringLiteral("timeSlider"));
  auto *play = panel.findChild<QToolButton *>(QStringLiteral("playButton"));
  ASSERT_NE(slider, nullptr);
  ASSERT_NE(play, nullptr);

  EXPECT_FALSE(m_clock.hasTime());
  EXPECT_FALSE(slider->isEnabled());
  EXPECT_FALSE(play->isEnabled());
  EXPECT_EQ(panel.timeText(), QStringLiteral("—"));

  m_clock.play();
  EXPECT_FALSE(m_clock.isPlaying())
    << "a clock with no recorded time started playing";
}

TEST_F(TimeControlsTest, TheSpeedBoxAndTheClockAgree)
{
  buildStack(4);

  TimeControlPanel panel;
  panel.setController(&m_clock);

  auto *speed = panel.findChild<QDoubleSpinBox *>(QStringLiteral("speedSpin"));
  ASSERT_NE(speed, nullptr);

  // The box shows the clock's speed on arrival rather than its own default:
  // two numbers that disagree at rest are a control that lies until touched.
  EXPECT_NEAR(speed->value(), m_clock.speed(), 1.0e-9);

  speed->setValue(20.0);
  EXPECT_NEAR(m_clock.speed(), 20.0, 1.0e-9);

  // Out of range is clamped rather than refused, and the clock is the one
  // that decides the range.
  m_clock.setSpeed(1000.0);
  EXPECT_LE(m_clock.speed(), 120.0);
}

TEST_F(TimeControlsTest, SpeedChangesReachAClockThatIsAlreadyPlaying)
{
  buildStack(4);
  m_clock.setStep(0);

  // A step every ten seconds: slow enough that nothing can advance by
  // accident within this test.
  m_clock.setSpeed(0.1);
  m_clock.play();

  ASSERT_EQ(m_clock.step(), 0);

  m_clock.setSpeed(120.0);

  // An animation that can only be sped up or slowed down by stopping it
  // first cannot be slowed down to look at the moment that needed slowing.
  EXPECT_TRUE(waitFor([this] { return m_clock.step() > 0; }, 3000))
    << "a speed set while playing did not reach the running clock";

  m_clock.pause();
}

TEST_F(TimeControlsTest, TheClockIgnoresAnInstantItIsAlreadyShowing)
{
  buildStack(4);
  m_clock.setStep(2);

  QSignalSpy moved(&m_clock, &TimeController::currentChanged);

  m_clock.setStep(2);
  m_clock.setCurrent(m_clock.current());

  // What lets a view follow the clock and drive it through the same widget:
  // the slider moved to match an instant reports itself as a scrub back to
  // that same instant, and the clock ends it there rather than passing it on.
  EXPECT_EQ(moved.size(), 0)
    << "the clock re-announced an instant it was already showing";

  m_clock.setStep(3);
  EXPECT_EQ(moved.size(), 1);
}
