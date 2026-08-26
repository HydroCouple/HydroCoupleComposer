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
#include "render/layerstyle.h"
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

// ── D2b: class breaks that hold still while the map animates ──────────────
//
// A graduated style reclassifies on every restyle, and a data-item layer
// restyles on every step. Left alone, that recomputes the class breaks from
// whichever level is on screen: the same colour stands for a different number
// at every frame, and the legend beside the map is only correct for the frame
// it was last computed on. The layer answers with every level it carries, so
// the breaks are the run's, not the frame's.

namespace
{
  //! Turns \a layer's style into a graduated theme over its recorded values.
  void themeByValue(DataItemLayer &layer, int classes)
  {
    LayerStyle *style = layer.style();
    style->setMode(StyleMode::Graduated);
    style->setAttribute(layer.valueAttribute());
    style->classification().setMethod(ClassificationMethod::EqualInterval);
    style->classification().setClassCount(classes);
  }

  //! The full span the classes cover, low to high.
  [[nodiscard]] QPair<double, double> coveredRange(const DataItemLayer &layer)
  {
    const QVector<ClassBreak> &breaks = layer.style()->classification().breaks();

    return breaks.isEmpty() ? QPair<double, double>{0.0, 0.0}
                            : QPair<double, double>{breaks.first().lower,
                                                    breaks.last().upper};
  }
}

TEST_F(TimeControllerTest, BreaksCoverTheRunAndNotTheFrameOnScreen)
{
  // Values are step*100 + geometry over four steps: 0, 1, 100, 101, …, 301.
  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  themeByValue(*layer, 4);
  ASSERT_TRUE(layer->restyle());

  // The whole record, not the last level's 300–301. A theme built from one
  // frame would give a range of width 1 here, which is the vacuous result
  // this test exists to rule out.
  const QPair<double, double> whole = coveredRange(*layer);
  EXPECT_NEAR(whole.first, 0.0, 1.0e-9);
  EXPECT_NEAR(whole.second, 301.0, 1.0e-9);

  // And the first level's 0–1 is not the answer either, from either end.
  ASSERT_TRUE(layer->setTimeIndex(0));

  const QPair<double, double> afterStepping = coveredRange(*layer);
  EXPECT_NEAR(afterStepping.first, whole.first, 1.0e-9);
  EXPECT_NEAR(afterStepping.second, whole.second, 1.0e-9)
    << "stepping to the first level shrank the theme to that level";
}

TEST_F(TimeControllerTest, AValueKeepsItsColourAsTimeMoves)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  themeByValue(*layer, 4);
  ASSERT_TRUE(layer->restyle());

  const QVector<ClassBreak> reference =
    layer->style()->classification().breaks();
  ASSERT_FALSE(reference.isEmpty());

  for (int level = 0; level < layer->timeCount(); ++level)
  {
    ASSERT_TRUE(layer->setTimeIndex(level));

    const QVector<ClassBreak> &now = layer->style()->classification().breaks();

    ASSERT_EQ(now.size(), reference.size())
      << "the class count moved at level " << level;

    for (int index = 0; index < now.size(); ++index)
    {
      EXPECT_NEAR(now.at(index).lower, reference.at(index).lower, 1.0e-9)
        << "class " << index << " moved at level " << level;
      EXPECT_NEAR(now.at(index).upper, reference.at(index).upper, 1.0e-9)
        << "class " << index << " moved at level " << level;
      EXPECT_EQ(now.at(index).color, reference.at(index).color)
        << "class " << index << " changed colour at level " << level;
    }

    // Nothing falls out of the map on the way through. A break set computed
    // from a subset of the levels leaves values past its end unclassified,
    // and an unclassified value is not drawn at all.
    for (int feature = 0; feature < layer->featureCount(); ++feature)
    {
      EXPECT_TRUE(layer->style()->colorFor(*layer, feature).isValid())
        << "feature " << feature << " was not drawn at level " << level;
    }
  }
}

TEST_F(TimeControllerTest, EachLevelIsReadOnceForTheTheme)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  themeByValue(*layer, 4);
  ASSERT_TRUE(layer->restyle());

  item->resetReads();

  // Stepping costs the one slab the step itself needs. Pooling the record
  // again on every step would make an animation cost a pass through the run
  // per frame instead of a pass through the run per layer.
  ASSERT_TRUE(layer->setTimeIndex(1));

  EXPECT_EQ(item->reads(), 1)
    << "stepping re-read levels that had already been pooled";
}

TEST_F(TimeControllerTest, ALayerOfOneLevelClassifiesOverWhatItHolds)
{
  // One level is the whole record, so pooling and showing agree — and the
  // theme must not somehow come out different from the values on screen.
  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 1, kEpoch, 1.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  themeByValue(*layer, 2);
  ASSERT_TRUE(layer->restyle());

  const QPair<double, double> covered = coveredRange(*layer);
  EXPECT_NEAR(covered.first, 0.0, 1.0e-9);
  EXPECT_NEAR(covered.second, 1.0, 1.0e-9);
}

TEST_F(TimeControllerTest, AStaticLayerIsThemedByWhatItHolds)
{
  // A static item has no levels to pool. Answering the classifier with the
  // pooled read anyway would hand it an empty set and leave the layer with
  // no classes at all — a layer that draws nothing rather than a layer with
  // one colour.
  Testing::StubGeometryItem still(
    "bathymetry",
    std::vector<HydroCouple::Spatial::IGeometry *>{
      static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get())});
  still.setValue(0, 7.0);
  still.setValue(1, 9.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&still, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_EQ(layer->timeCount(), 0);

  themeByValue(*layer, 2);
  ASSERT_TRUE(layer->restyle());

  const QPair<double, double> covered = coveredRange(*layer);
  EXPECT_NEAR(covered.first, 7.0, 1.0e-9);
  EXPECT_NEAR(covered.second, 9.0, 1.0e-9);
}

TEST_F(TimeControllerTest, AnotherFieldIsNotAnsweredWithTheRecord)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Only the recorded values are pooled across levels. A style carried over
  // from another dataset names a field this layer does not have, and the
  // honest answer to that is nothing — not this layer's whole record under
  // someone else's column name.
  EXPECT_TRUE(layer->numericValues(QStringLiteral("elevation")).isEmpty())
    << "a field the layer does not have was answered with its values";

  EXPECT_EQ(layer->numericValues(layer->valueAttribute()).size(),
            layer->featureCount() * layer->timeCount());
}

// ── D2d: the legend beside a running map ─────────────────────────────────
//
// The layer tree derives its legend rows from the style on every read rather
// than storing them, which is what stops a map and its legend from drifting
// apart. What that leaves to check is the other half: that the theme itself
// holds while the run plays, so the legend read at one instant is still the
// legend for the map at the next.

TEST_F(TimeControllerTest, TheLegendNamesTheThemesClasses)
{
  LayerStackModel stack;

  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();

  themeByValue(*layer, 4);
  ASSERT_GE(stack.addLayer(layer), 0);
  ASSERT_TRUE(layer->restyle());

  const QModelIndex layerRow = stack.index(0, 0);
  ASSERT_TRUE(layerRow.isValid());

  const QVector<ClassBreak> &breaks =
    layer->style()->classification().breaks();
  ASSERT_FALSE(breaks.isEmpty());

  ASSERT_EQ(stack.rowCount(layerRow), breaks.size());

  for (int row = 0; row < breaks.size(); ++row)
  {
    const QModelIndex legendRow = stack.index(row, 0, layerRow);

    EXPECT_TRUE(stack.data(legendRow, LayerStackModel::IsLegendRole).toBool());
    EXPECT_EQ(stack.data(legendRow, Qt::DisplayRole).toString(),
              breaks.at(row).label)
      << "legend row " << row << " does not name the class it stands for";
  }
}

TEST_F(TimeControllerTest, TheLegendHoldsWhileTheRunPlays)
{
  LayerStackModel stack;

  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 4, kEpoch, 1.0);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();

  themeByValue(*layer, 4);
  ASSERT_GE(stack.addLayer(layer), 0);
  ASSERT_TRUE(layer->restyle());

  const QModelIndex layerRow = stack.index(0, 0);

  QStringList reference;

  for (int row = 0; row < stack.rowCount(layerRow); ++row)
  {
    reference.append(
      stack.data(stack.index(row, 0, layerRow), Qt::DisplayRole).toString());
  }

  ASSERT_FALSE(reference.isEmpty());

  for (int level = 0; level < layer->timeCount(); ++level)
  {
    ASSERT_TRUE(layer->setTimeIndex(level));

    QStringList now;

    for (int row = 0; row < stack.rowCount(layerRow); ++row)
    {
      now.append(
        stack.data(stack.index(row, 0, layerRow), Qt::DisplayRole).toString());
    }

    EXPECT_EQ(now, reference)
      << "the legend was rewritten at level " << level
      << ", so a reader comparing two frames is comparing two scales";
  }
}

TEST_F(TimeControllerTest, TheLegendFollowsARethemeMidRun)
{
  LayerStackModel stack;

  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem("depth", 6, kEpoch, 1.0);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();

  themeByValue(*layer, 3);
  ASSERT_GE(stack.addLayer(layer), 0);
  ASSERT_TRUE(layer->restyle());

  const QModelIndex layerRow = stack.index(0, 0);
  ASSERT_EQ(stack.rowCount(layerRow), 3);

  QSignalSpy inserted(&stack, &QAbstractItemModel::rowsInserted);

  // Re-theming mid-run changes how many classes there are, which is a
  // structural change to the tree and not only new text in it: a view told
  // otherwise keeps addressing legend rows that no longer exist.
  layer->style()->classification().setClassCount(6);
  ASSERT_TRUE(layer->restyle());

  EXPECT_EQ(stack.rowCount(layerRow), 6);

  ASSERT_EQ(inserted.size(), 1)
    << "the tree was not told its legend rows had changed";
  EXPECT_EQ(inserted.at(0).at(0).value<QModelIndex>(), layerRow);
}
