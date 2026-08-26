/*!
 * \file   test_timecontroller.cpp
 * \brief  Phase D2 verification — stepping a run through time.
 *
 * Two claims. A layer can show any time level its item carries, not only the
 * last one it used to be pinned to. And one clock drives layers recorded on
 * different axes to the same *instant* rather than to the same step number —
 * which is the whole difficulty, since a layer recorded hourly and one
 * recorded daily share no frame numbering at all.
 */

#include "core/composerapplication.h"
#include "layers/dataitemlayer.h"
#include "map/layerstackmodel.h"
#include "results/timecontroller.h"
#include "spatialstubs.h"

#include <gtest/gtest.h>

#include <QSignalSpy>

#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  //! J2000, so the fixtures sit where real Julian days do.
  constexpr double kEpoch = 2451545.0;

  class TimeControllerTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_timecontroller";
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

      /*!
       * \brief An item of \a steps levels, \a spacing days apart.
       *
       * Values are step*100 + geometry, so reading the wrong level or the
       * wrong axis produces a different set of numbers rather than a
       * plausible one.
       */
      std::unique_ptr<Testing::StubTimeGeometryItem> makeItem(
        const char *id, int steps, double first, double spacing)
      {
        auto item = std::make_unique<Testing::StubTimeGeometryItem>(
          id, steps,
          std::vector<HydroCouple::Spatial::IGeometry *>{
            static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
            static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get())},
          first, spacing);

        for (int step = 0; step < steps; ++step)
        {
          for (int geometry = 0; geometry < 2; ++geometry)
          {
            item->setValue(step, geometry, step * 100.0 + geometry);
          }
        }

        return item;
      }

      [[nodiscard]] static double valueOf(const DataItemLayer &layer,
                                          int feature)
      {
        return layer.attributeValue(feature, layer.valueAttribute())
          .toDouble();
      }

      std::unique_ptr<Testing::StubCrs> m_crs;
      std::unique_ptr<Testing::StubPoint> m_a;
      std::unique_ptr<Testing::StubPoint> m_b;

      static ComposerApplication *s_app;
  };

  ComposerApplication *TimeControllerTest::s_app = nullptr;
}

TEST_F(TimeControllerTest, ALayerShowsTheLevelItIsAskedFor)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_EQ(layer->timeCount(), 4);

  // The last level until told otherwise — for a component still running,
  // that is "now".
  EXPECT_EQ(layer->timeIndex(), 3);
  EXPECT_NEAR(valueOf(*layer, 0), 300.0, 1.0e-9);

  ASSERT_TRUE(layer->setTimeIndex(1));

  EXPECT_EQ(layer->timeIndex(), 1);
  EXPECT_NEAR(valueOf(*layer, 0), 100.0, 1.0e-9);
  EXPECT_NEAR(valueOf(*layer, 1), 101.0, 1.0e-9);

  // Out of range is clamped rather than refused: a clock running past the
  // end of one layer's record should leave it at its last level, not blank.
  ASSERT_TRUE(layer->setTimeIndex(99));
  EXPECT_EQ(layer->timeIndex(), 3);

  ASSERT_TRUE(layer->setTimeIndex(-5));
  EXPECT_EQ(layer->timeIndex(), 0);
  EXPECT_NEAR(valueOf(*layer, 0), 0.0, 1.0e-9);
}

TEST_F(TimeControllerTest, ALayerKnowsWhenEachOfItsLevelsIs)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 0.5);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr);

  EXPECT_NEAR(layer->timeAt(0), kEpoch, 1.0e-9);
  EXPECT_NEAR(layer->timeAt(3), kEpoch + 1.5, 1.0e-9);

  // Nearest, and the arithmetic is on the instant rather than on the index:
  // 0.4 days past the epoch is nearer the half-day level than the epoch.
  EXPECT_EQ(layer->nearestTime(kEpoch + 0.4), 1);
  EXPECT_EQ(layer->nearestTime(kEpoch + 0.24), 0);

  // Beyond either end clamps to the end rather than wrapping.
  EXPECT_EQ(layer->nearestTime(kEpoch - 100.0), 0);
  EXPECT_EQ(layer->nearestTime(kEpoch + 100.0), 3);
}

TEST_F(TimeControllerTest, TheClockSpansEveryLayersRecord)
{
  LayerStackModel stack;
  TimeController clock;
  clock.setModel(&stack);

  EXPECT_FALSE(clock.hasTime()) << "an empty stack has no time to show";

  // Daily over four days, and hourly over the first three hours: two axes
  // with nothing in common but the instants they name.
  const std::unique_ptr<Testing::StubTimeGeometryItem> daily =
    makeItem("daily", 4, kEpoch, 1.0);
  // Starting a hair after the epoch: about a millisecond, which is well
  // under the tenth of a second two instants may differ by and still be the
  // same one — and, importantly, well *above* the ulp of a Julian day, which
  // is about 5e-10. An offset finer than that is swallowed by the double and
  // tests nothing. Two layers that name the same instant almost never name
  // it with the same number, and a slider with two indistinguishable stops
  // beside each other is a slider that skips.
  const std::unique_ptr<Testing::StubTimeGeometryItem> hourly =
    makeItem("hourly", 3, kEpoch + 1.0e-8, 1.0 / 24.0);

  QString message;
  DataItemLayer *dailyLayer =
    DataItemLayer::create(daily.get(), message).release();
  DataItemLayer *hourlyLayer =
    DataItemLayer::create(hourly.get(), message).release();
  ASSERT_NE(dailyLayer, nullptr);
  ASSERT_NE(hourlyLayer, nullptr);

  QSignalSpy spanSpy(&clock, &TimeController::spanChanged);

  ASSERT_GE(stack.addLayer(dailyLayer), 0);
  ASSERT_GE(stack.addLayer(hourlyLayer), 0);

  EXPECT_TRUE(clock.hasTime());
  EXPECT_GE(spanSpy.count(), 1) << "the slider was never told its range";

  EXPECT_NEAR(clock.first(), kEpoch, 1.0e-9);
  EXPECT_NEAR(clock.last(), kEpoch + 3.0, 1.0e-9);

  // The union of both records: four daily instants and two more hourly ones
  // between the first two days — the epoch itself is shared, and counted
  // once.
  EXPECT_EQ(clock.steps().size(), 6)
    << "the steps are one layer's rather than every layer's, or the two "
       "layers' shared instant was counted twice";
}

TEST_F(TimeControllerTest, OneInstantMovesEveryLayerToItsOwnNearestLevel)
{
  LayerStackModel stack;
  TimeController clock;
  clock.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> daily =
    makeItem("daily", 4, kEpoch, 1.0);
  const std::unique_ptr<Testing::StubTimeGeometryItem> halfDaily =
    makeItem("half", 7, kEpoch, 0.5);

  QString message;
  DataItemLayer *dailyLayer =
    DataItemLayer::create(daily.get(), message).release();
  DataItemLayer *halfLayer =
    DataItemLayer::create(halfDaily.get(), message).release();

  ASSERT_GE(stack.addLayer(dailyLayer), 0);
  ASSERT_GE(stack.addLayer(halfLayer), 0);

  QSignalSpy currentSpy(&clock, &TimeController::currentChanged);

  // Two days past the epoch: the daily layer's level 2, and the half-daily
  // layer's level 4. A clock that shared a *step number* would put both on
  // level 2 and show the half-daily layer a day early.
  clock.setCurrent(kEpoch + 2.0);

  EXPECT_EQ(currentSpy.count(), 1);

  EXPECT_EQ(dailyLayer->timeIndex(), 2);
  EXPECT_EQ(halfLayer->timeIndex(), 4);

  EXPECT_NEAR(valueOf(*dailyLayer, 0), 200.0, 1.0e-9);
  EXPECT_NEAR(valueOf(*halfLayer, 0), 400.0, 1.0e-9);

  // An instant no layer recorded at still lands each on its nearest.
  clock.setCurrent(kEpoch + 2.3);

  EXPECT_EQ(dailyLayer->timeIndex(), 2);
  EXPECT_EQ(halfLayer->timeIndex(), 5)
    << "2.3 days is nearer the 2.5-day level than the 2.0-day one";
}

TEST_F(TimeControllerTest, SteppingMovesOneInstantAtATime)
{
  LayerStackModel stack;
  TimeController clock;
  clock.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_GE(stack.addLayer(layer), 0);

  clock.setStep(0);
  ASSERT_EQ(clock.step(), 0);

  clock.advance(1);
  EXPECT_EQ(clock.step(), 1);
  EXPECT_EQ(layer->timeIndex(), 1);

  clock.advance(2);
  EXPECT_EQ(clock.step(), 3);

  // Stops at the end rather than wrapping: a run does not start again after
  // its last step, and a slider that jumped back would look like a fault.
  clock.advance(5);
  EXPECT_EQ(clock.step(), 3);

  clock.advance(-99);
  EXPECT_EQ(clock.step(), 0);
  EXPECT_EQ(layer->timeIndex(), 0);
}

TEST_F(TimeControllerTest, AStaticLayerIsLeftAlone)
{
  LayerStackModel stack;
  TimeController clock;
  clock.setModel(&stack);

  Testing::StubGeometryItem still(
    "bathymetry",
    std::vector<HydroCouple::Spatial::IGeometry *>{
      static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get())});
  still.setValue(0, 7.0);
  still.setValue(1, 9.0);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(&still, message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  EXPECT_EQ(layer->timeCount(), 0);
  EXPECT_EQ(layer->timeIndex(), -1);

  // A stack of nothing but static layers has no time to step through, and
  // offering a slider over one would be offering a control with no effect.
  EXPECT_FALSE(clock.hasTime());

  EXPECT_FALSE(layer->setTimeIndex(2)) << "a static item accepted a level";
  EXPECT_NEAR(valueOf(*layer, 0), 7.0, 1.0e-9)
    << "stepping changed a layer that has no time axis";
}
