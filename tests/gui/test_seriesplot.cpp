/*!
 * \file   test_seriesplot.cpp
 * \brief  Phase D3a verification — plotting what a picked feature recorded.
 *
 * The phase's own gate is that the plotted values equal a `getValuesInto()`
 * read of the same hyperslab. So the tests read the item a second time,
 * directly, and compare — rather than checking that a chart has series in it,
 * which a plot of the wrong feature, the wrong axis or the wrong level would
 * satisfy just as well.
 */

#include "core/composerapplication.h"
#include "layers/dataitemlayer.h"
#include "map/layerstackmodel.h"
#include "results/julianday.h"
#include "spatialstubs.h"
#include "ui/panels/seriesplotpanel.h"

#include <gtest/gtest.h>

#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QLineSeries>
#include <QValueAxis>

#include <memory>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  //! J2000, so the fixtures sit where real Julian days do.
  constexpr double kEpoch = 2451545.0;

  class SeriesPlotTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_seriesplot";
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
        m_c = Testing::makePoint(2.0, 2.0, 2, m_crs.get());
      }

      /*!
       * \brief An item of \a steps levels over three points.
       *
       * Value = step * 10 + feature, so a series read from the wrong feature
       * or the wrong level is a different set of numbers rather than a
       * plausible one.
       */
      std::unique_ptr<Testing::StubTimeGeometryItem> makeItem(int steps)
      {
        auto item = std::make_unique<Testing::StubTimeGeometryItem>(
          "depth", steps,
          std::vector<HydroCouple::Spatial::IGeometry *>{
            static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
            static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get()),
            static_cast<HydroCouple::Spatial::IGeometry *>(m_c.get())},
          kEpoch, 1.0);

        for (int step = 0; step < steps; ++step)
        {
          for (int feature = 0; feature < 3; ++feature)
          {
            item->setValue(step, feature, step * 10.0 + feature);
          }
        }

        return item;
      }

      //! Reads one feature's series straight from \a item, hyperslab by hand.
      static QVector<double> readSeriesDirectly(
        const Testing::StubTimeGeometryItem &item, int feature, int steps)
      {
        std::vector<double> buffer(static_cast<size_t>(steps), 0.0);
        const int64_t bufferShape = steps;

        HydroCouple::BufferDescriptor destination;
        destination.data = buffer.data();
        destination.kind = HydroCouple::DataKind::Float64;
        destination.rank = 1;
        destination.shape = &bufferShape;

        const std::array<int64_t, 2> start{0, feature};
        const std::array<int64_t, 2> count{steps, 1};

        std::string message;

        if (!item.getValuesInto(destination, start, count, &message))
        {
          return {};
        }

        QVector<double> values;

        for (double value : buffer)
        {
          values.append(value);
        }

        return values;
      }

      std::unique_ptr<Testing::StubCrs> m_crs;
      std::unique_ptr<Testing::StubPoint> m_a;
      std::unique_ptr<Testing::StubPoint> m_b;
      std::unique_ptr<Testing::StubPoint> m_c;

      static ComposerApplication *s_app;
  };

  ComposerApplication *SeriesPlotTest::s_app = nullptr;
}

TEST_F(SeriesPlotTest, ALayersSeriesIsTheItemsOwnHyperslab)
{
  constexpr int kSteps = 5;

  const std::unique_ptr<Testing::StubTimeGeometryItem> item =
    makeItem(kSteps);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  for (int feature = 0; feature < layer->featureCount(); ++feature)
  {
    SCOPED_TRACE("feature " + std::to_string(feature));

    QVector<double> series;
    ASSERT_TRUE(layer->valuesOverTime(feature, series, message))
      << message.toStdString();

    // The phase's gate, read a second time and by a different route: the
    // layer's transposed read against the item's own hyperslab.
    const QVector<double> direct =
      readSeriesDirectly(*item, feature, kSteps);

    ASSERT_EQ(series.size(), kSteps);
    ASSERT_EQ(direct.size(), kSteps);

    for (int level = 0; level < kSteps; ++level)
    {
      EXPECT_NEAR(series.at(level), direct.at(level), 1.0e-9)
        << "level " << level;

      // And against the value the fixture put there, so both reads being
      // wrong the same way is caught too.
      EXPECT_NEAR(series.at(level), level * 10.0 + feature, 1.0e-9)
        << "level " << level;
    }
  }
}

TEST_F(SeriesPlotTest, TheSeriesDoesNotMoveWhenTheMapDoes)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> item = makeItem(4);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  QVector<double> before;
  ASSERT_TRUE(layer->valuesOverTime(1, before, message));

  // Stepping the map changes which instant is *drawn*; a plot shows the
  // whole record and has nothing to change.
  ASSERT_TRUE(layer->setTimeIndex(0));

  QVector<double> after;
  ASSERT_TRUE(layer->valuesOverTime(1, after, message));

  EXPECT_EQ(before, after)
    << "the series followed the level the map was showing";
}

TEST_F(SeriesPlotTest, AStaticLayerHasNoSeriesToRead)
{
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

  QVector<double> series;

  // Refused with a reason rather than answered with one point: a single
  // instant is not a series, and a one-point plot implies a trend.
  EXPECT_FALSE(layer->valuesOverTime(0, series, message));
  EXPECT_FALSE(message.isEmpty());
  EXPECT_TRUE(series.isEmpty());
}

TEST_F(SeriesPlotTest, AFeatureOutsideTheLayerIsRefused)
{
  const std::unique_ptr<Testing::StubTimeGeometryItem> item = makeItem(3);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  QVector<double> series;

  EXPECT_FALSE(layer->valuesOverTime(layer->featureCount(), series, message));

  // Named, because the item's own slab check would also refuse this and say
  // only that a selection was out of range on some dimension -- which is
  // true of the wrong axis, the wrong level and the wrong feature alike.
  EXPECT_TRUE(message.contains(QString::number(layer->featureCount())))
    << message.toStdString();
  EXPECT_TRUE(message.contains(QStringLiteral("feature"), Qt::CaseInsensitive))
    << message.toStdString();

  EXPECT_FALSE(layer->valuesOverTime(-1, series, message));
  EXPECT_TRUE(message.contains(QStringLiteral("feature"), Qt::CaseInsensitive))
    << message.toStdString();
}

TEST_F(SeriesPlotTest, ThePlotFollowsTheSelection)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  // Nothing selected yet: the panel says what to do rather than showing an
  // empty chart, which reads as a run that recorded nothing.
  EXPECT_EQ(panel.seriesCount(), 0);
  EXPECT_FALSE(panel.statusText().isEmpty());

  const std::unique_ptr<Testing::StubTimeGeometryItem> item = makeItem(4);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{0, 2});

  ASSERT_EQ(panel.seriesCount(), 2);
  EXPECT_EQ(panel.layer(), layer);
  EXPECT_TRUE(panel.statusText().isEmpty());

  // Sorted by feature, so the same selection always plots in the same order
  // and the legend does not reshuffle between two reads of one run.
  const QVector<double> first = panel.seriesValues(0);
  const QVector<double> second = panel.seriesValues(1);

  ASSERT_EQ(first.size(), 4);
  ASSERT_EQ(second.size(), 4);

  for (int level = 0; level < 4; ++level)
  {
    EXPECT_NEAR(first.at(level), level * 10.0 + 0, 1.0e-9) << level;
    EXPECT_NEAR(second.at(level), level * 10.0 + 2, 1.0e-9) << level;
  }

  // Clearing the selection clears the plot rather than leaving the last one
  // on screen beside a map that no longer has anything picked.
  stack.selectOnly(nullptr, QSet<int>{});

  EXPECT_EQ(panel.seriesCount(), 0);
  EXPECT_FALSE(panel.statusText().isEmpty());
}

TEST_F(SeriesPlotTest, ASelectedStaticLayerSaysWhyItCannotBePlotted)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

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

  stack.selectOnly(layer, QSet<int>{0});

  EXPECT_EQ(panel.seriesCount(), 0);

  // "Nothing is selected" and "what is selected has no series" are different
  // answers, and only one of them tells the user to do something else.
  EXPECT_TRUE(panel.statusText().contains(QStringLiteral("bathymetry")))
    << panel.statusText().toStdString();
}

TEST_F(SeriesPlotTest, TheTimeAxisSpansTheRecordInCalendarTime)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> item = makeItem(4);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{1});
  ASSERT_EQ(panel.seriesCount(), 1);

  auto *view = panel.findChild<QChartView *>(QStringLiteral("seriesPlotView"));
  ASSERT_NE(view, nullptr);

  const QList<QAbstractAxis *> axes =
    view->chart()->axes(Qt::Horizontal);
  ASSERT_FALSE(axes.isEmpty());

  auto *timeAxis = qobject_cast<QDateTimeAxis *>(axes.first());
  ASSERT_NE(timeAxis, nullptr);

  // What the axis *reads*, not what it stores. J2000 is noon on 2000-01-01
  // and the fixture steps a day at a time, so a half-day slip in the
  // conversion -- the classic way to be one row out -- shows here, and so
  // does the axis quietly relabelling the run in the viewer's own time zone.
  EXPECT_EQ(timeAxis->min().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            QStringLiteral("2000-01-01 12:00:00"));
  EXPECT_EQ(timeAxis->max().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            QStringLiteral("2000-01-04 12:00:00"));

  // And the transport readout under the plot says the same thing about the
  // same instant, which is the whole reason one conversion is shared.
  EXPECT_EQ(dateTimeFromJulianDay(kEpoch).toString(
              QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            timeAxis->min().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

TEST_F(SeriesPlotTest, TheValueAxisCarriesTheComponentsOwnLabel)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  // A component that captioned its values, which is the case worth testing:
  // an item with no caption falls back to "Value", and an axis hard-coded to
  // "Value" would agree with it and prove nothing.
  auto item = std::make_unique<Testing::StubTimeGeometryItem>(
    "depth", 3,
    std::vector<HydroCouple::Spatial::IGeometry *>{
      static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_c.get())},
    kEpoch, 1.0, "Water depth (m)");

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{0});
  ASSERT_EQ(panel.seriesCount(), 1);

  auto *view = panel.findChild<QChartView *>(QStringLiteral("seriesPlotView"));
  ASSERT_NE(view, nullptr);

  const QList<QAbstractAxis *> axes = view->chart()->axes(Qt::Vertical);
  ASSERT_FALSE(axes.isEmpty());

  const QVector<AttributeField> fields = layer->attributeFields();
  QString expected;

  for (const AttributeField &field : fields)
  {
    if (field.name == layer->valueAttribute())
    {
      expected = field.displayName;
    }
  }

  ASSERT_EQ(expected, QStringLiteral("Water depth (m)"))
    << "the fixture's caption did not reach the layer";

  // The label the component supplied, not "Value" — which is the one thing
  // the axis could say that throws away what the component told us.
  EXPECT_EQ(axes.first()->titleText(), expected);
}

TEST_F(SeriesPlotTest, AnInstantOnTheHourIsLabelledOnTheHour)
{
  // A Julian day is a large number carrying a small interval, so an hour
  // added to it does not come back as an exact hour: truncating what is left
  // yields 12:59:59 and labels the whole plot a minute early, which reads as
  // a modelling result rather than as arithmetic.
  const QDateTime hour = dateTimeFromJulianDay(kEpoch + 1.0 / 24.0);
  ASSERT_TRUE(hour.isValid());
  EXPECT_EQ(hour.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            QStringLiteral("2000-01-01 13:00:00"));

  const QDateTime minute = dateTimeFromJulianDay(kEpoch + 1.0 / 1440.0);
  ASSERT_TRUE(minute.isValid());
  EXPECT_EQ(minute.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            QStringLiteral("2000-01-01 12:01:00"));
}

TEST_F(SeriesPlotTest, SeriesArePlottedInFeatureOrder)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  auto item = std::make_unique<Testing::StubTimeGeometryItem>(
    "depth", 2,
    std::vector<HydroCouple::Spatial::IGeometry *>{
      static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_c.get())},
    kEpoch, 1.0);

  for (int step = 0; step < 2; ++step)
  {
    for (int feature = 0; feature < 3; ++feature)
    {
      item->setValue(step, feature, feature);
    }
  }

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  // Selected back to front. A selection is a set, and a set hands its members
  // back in whatever order suits its hashing -- so the plot has to impose one
  // or the legend reshuffles between two reads of the same run.
  stack.selectOnly(layer, QSet<int>{2, 0, 1});

  ASSERT_EQ(panel.seriesCount(), 3);

  for (int series = 0; series < 3; ++series)
  {
    const QVector<double> values = panel.seriesValues(series);
    ASSERT_FALSE(values.isEmpty()) << series;

    // Value equals feature index in this fixture, so the plotted order is
    // readable straight off the values.
    EXPECT_NEAR(values.first(), series, 1.0e-9)
      << "series " << series << " is not feature " << series;
  }
}
