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
#include "layers/differencelayer.h"
#include "map/layerstackmodel.h"
#include "results/julianday.h"
#include "results/seriesexport.h"
#include "spatialstubs.h"
#include "ui/panels/seriesplotpanel.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QDir>
#include <QFile>
#include <QLineSeries>
#include <QToolButton>
#include <QValueAxis>

#include <cmath>
#include <limits>
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
  ASSERT_EQ(panel.plottedLayers().size(), 1);
  EXPECT_EQ(panel.plottedLayers().first(), layer);
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

// ── D3b: writing the plotted series out ─────────────────────────────────────
//
// The formats are the deliverable, so these check the text itself: a CSV that
// lines two series up against one column of instants, and a .dat SWMM can
// read back. Files land under tests/fixtures/results/export so they can be
// opened and read by hand.

namespace
{
  //! Where the export tests write, so their output can be inspected.
  QString exportDir()
  {
    const QString directory =
      QStringLiteral(COMPOSER_RESULTS_FIXTURE_DIR) + QStringLiteral("/export");

    QDir().mkpath(directory);

    return directory;
  }

  //! A named series of \a count daily values from \a first, starting at \a base.
  ExportSeries makeSeries(const QString &name, double first, int count,
                          double base)
  {
    ExportSeries series;
    series.name = name;

    for (int level = 0; level < count; ++level)
    {
      series.julianDays.append(first + level);
      series.values.append(base + level);
    }

    return series;
  }
}

TEST_F(SeriesPlotTest, CsvLinesEverySeriesUpAgainstOneColumnOfInstants)
{
  // Two series an instant apart: the first covers days 0-2, the second days
  // 1-3. Resampling one onto the other would invent readings; a shared time
  // column with gaps says exactly what each one recorded.
  const QVector<ExportSeries> series{
    makeSeries(QStringLiteral("early"), kEpoch, 3, 10.0),
    makeSeries(QStringLiteral("late"), kEpoch + 1.0, 3, 100.0)};

  const QStringList lines =
    seriesToCsv(series).split(QLatin1Char('\n'), Qt::SkipEmptyParts);

  ASSERT_EQ(lines.size(), 5) << "expected a header and four instants";

  EXPECT_EQ(lines.at(0), QStringLiteral("Date/Time,early,late"));

  // Day 0: only the first series has a reading, and the other field is empty
  // rather than zero -- a gap in a record is not a measurement of nothing.
  EXPECT_EQ(lines.at(1), QStringLiteral("2000-01-01 12:00:00,10,"));
  EXPECT_EQ(lines.at(2), QStringLiteral("2000-01-02 12:00:00,11,100"));
  EXPECT_EQ(lines.at(3), QStringLiteral("2000-01-03 12:00:00,12,101"));
  EXPECT_EQ(lines.at(4), QStringLiteral("2000-01-04 12:00:00,,102"));
}

TEST_F(SeriesPlotTest, InstantsThatDifferInTheLastBitsShareOneRow)
{
  ExportSeries first = makeSeries(QStringLiteral("a"), kEpoch, 3, 1.0);
  ExportSeries second = makeSeries(QStringLiteral("b"), kEpoch, 3, 10.0);

  // The same instants, arrived at by a different route. Two layers recording
  // "the same" time routinely differ in the last bits of a Julian day, and a
  // row keyed on the raw double would give them a row each -- two half-empty
  // rows a hundredth of a second apart, for one reading.
  for (double &instant : second.julianDays)
  {
    instant = std::nextafter(instant, std::numeric_limits<double>::max());
  }

  ASSERT_NE(first.julianDays.at(0), second.julianDays.at(0))
    << "the fixture did not actually perturb the instants";

  const QStringList lines =
    seriesToCsv({first, second}).split(QLatin1Char('\n'), Qt::SkipEmptyParts);

  ASSERT_EQ(lines.size(), 4) << "the same instant was written as two rows";
  EXPECT_EQ(lines.at(1), QStringLiteral("2000-01-01 12:00:00,1,10"));
  EXPECT_EQ(lines.at(3), QStringLiteral("2000-01-03 12:00:00,3,12"));
}

TEST_F(SeriesPlotTest, CsvQuotesOnlyTheNamesThatNeedIt)
{
  QVector<ExportSeries> series{makeSeries(QStringLiteral("plain"), kEpoch, 1,
                                          1.0),
                               makeSeries(QStringLiteral("flow, m3/s"), kEpoch,
                                          1, 2.0)};

  const QString header = seriesToCsv(series).section(QLatin1Char('\n'), 0, 0);

  // A comma inside a name would otherwise split the header into a column the
  // file does not have, and every row below it would be read one field out.
  EXPECT_EQ(header, QStringLiteral("Date/Time,plain,\"flow, m3/s\""));
}

TEST_F(SeriesPlotTest, DatCarriesItsNameAndSwmmsOwnDateFormat)
{
  ExportSeries series = makeSeries(QStringLiteral("depth"), kEpoch, 2, 5.0);

  // A gap the model never produced. SWMM reads a .dat as a stream of
  // readings, so a non-finite one has to be left out rather than written as
  // text SWMM would reject.
  series.julianDays.append(kEpoch + 2.0);
  series.values.append(std::numeric_limits<double>::quiet_NaN());

  const QStringList lines =
    seriesToDat(series).split(QLatin1Char('\n'), Qt::SkipEmptyParts);

  ASSERT_EQ(lines.size(), 3);
  EXPECT_EQ(lines.at(0), QStringLiteral(";depth"));
  EXPECT_EQ(lines.at(1), QStringLiteral("01/01/2000 12:00:00 5"));
  EXPECT_EQ(lines.at(2), QStringLiteral("01/02/2000 12:00:00 6"));
}

TEST_F(SeriesPlotTest, SeveralSeriesFanOutIntoSeveralDatFiles)
{
  const QVector<ExportSeries> series{
    makeSeries(QStringLiteral("Feature 0"), kEpoch, 2, 1.0),
    makeSeries(QStringLiteral("Feature 2"), kEpoch, 2, 3.0)};

  const QString path = exportDir() + QStringLiteral("/fanout.dat");
  QString message;

  const QStringList written = writeSeriesDat(path, series, message);

  // One .dat holds one series, so writing both into the file the user named
  // would produce something SWMM cannot read -- silently, since the extra
  // column looks like the next reading.
  ASSERT_EQ(written.size(), 2) << message.toStdString();

  for (int index = 0; index < written.size(); ++index)
  {
    SCOPED_TRACE(written.at(index).toStdString());

    EXPECT_TRUE(written.at(index).contains(
      sanitizedFileToken(series.at(index).name)))
      << "the file is not named after the series it holds";

    QFile file(written.at(index));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));

    const QString text = QString::fromUtf8(file.readAll());

    EXPECT_TRUE(text.startsWith(QLatin1Char(';') + series.at(index).name));
    EXPECT_EQ(text.count(QLatin1Char('\n')), 3);
  }

  // One series goes to the file the user named, rather than to a fan-out of
  // one with a suffix they did not ask for.
  const QString single = exportDir() + QStringLiteral("/single.dat");
  const QStringList one =
    writeSeriesDat(single, {series.first()}, message);

  ASSERT_EQ(one.size(), 1) << message.toStdString();
  EXPECT_EQ(one.first(), single);
}

TEST_F(SeriesPlotTest, AFileTokenNeverCollapsesToNothing)
{
  EXPECT_EQ(sanitizedFileToken(QStringLiteral("Feature 0")),
            QStringLiteral("Feature_0"));

  // Two series whose names are all punctuation would otherwise both become
  // the empty token and land on one file, the second overwriting the first.
  EXPECT_FALSE(sanitizedFileToken(QStringLiteral("///")).isEmpty());
  EXPECT_FALSE(sanitizedFileToken(QString()).isEmpty());
}

TEST_F(SeriesPlotTest, ThePanelExportsExactlyWhatItPlots)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> item = makeItem(4);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{0, 2});
  ASSERT_EQ(panel.seriesCount(), 2);

  const QVector<ExportSeries> &exported = panel.exportSeries();
  ASSERT_EQ(exported.size(), 2);

  for (int index = 0; index < exported.size(); ++index)
  {
    SCOPED_TRACE(index);

    // The instants as recorded, not as the axis positions them: the axis
    // carries a shifted instant so its labels read in UTC, and an export
    // taken from it would be off by the exporter's own time zone.
    ASSERT_EQ(exported.at(index).julianDays.size(), 4);
    EXPECT_NEAR(exported.at(index).julianDays.first(), kEpoch, 1.0e-9);
    EXPECT_NEAR(exported.at(index).julianDays.last(), kEpoch + 3.0, 1.0e-9);

    // And the same values the chart is drawing.
    EXPECT_EQ(exported.at(index).values, panel.seriesValues(index));
  }

  const QString path = exportDir() + QStringLiteral("/panel.csv");
  const QStringList written = panel.exportTo(path, message);

  ASSERT_EQ(written.size(), 1) << message.toStdString();

  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));

  const QStringList lines = QString::fromUtf8(file.readAll())
                              .split(QLatin1Char('\n'), Qt::SkipEmptyParts);

  ASSERT_EQ(lines.size(), 5);
  EXPECT_EQ(lines.at(0), QStringLiteral("Date/Time,Feature 0,Feature 2"));
  EXPECT_EQ(lines.at(1), QStringLiteral("2000-01-01 12:00:00,0,2"));
  EXPECT_EQ(lines.at(4), QStringLiteral("2000-01-04 12:00:00,30,32"));
}

TEST_F(SeriesPlotTest, ExportingNothingIsRefusedRatherThanWritingAnEmptyFile)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  auto *button =
    panel.findChild<QToolButton *>(QStringLiteral("exportSeriesButton"));
  ASSERT_NE(button, nullptr);

  // Nothing plotted: the control says so rather than offering to write a
  // file with a header and no rows, which reads as a run that recorded none.
  EXPECT_FALSE(button->isEnabled());

  QString message;
  const QString path = exportDir() + QStringLiteral("/empty.csv");

  // Removed first, because the claim below is that nothing was written --
  // and a file left behind by an earlier run would make that claim about
  // someone else's file. (A mutation run does exactly that.)
  QFile::remove(path);
  ASSERT_FALSE(QFile::exists(path));

  EXPECT_TRUE(panel.exportTo(path, message).isEmpty());
  EXPECT_FALSE(message.isEmpty());
  EXPECT_FALSE(QFile::exists(path)) << "an empty export left a file behind";

  const std::unique_ptr<Testing::StubTimeGeometryItem> item = makeItem(3);

  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{1});
  EXPECT_TRUE(button->isEnabled());

  stack.selectOnly(nullptr, QSet<int>{});
  EXPECT_FALSE(button->isEnabled());
}

// ── D4b — overlaying what more than one run recorded ────────────────────────

TEST_F(SeriesPlotTest, OneSelectionDrawsTheSameFeatureFromEveryRun)
{
  // The stack holds one selection, so an overlay cannot come from selecting
  // twice. Picking a place on one run draws what every run recorded there —
  // which is the gesture anyone comparing two runs would actually make.
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> first = makeItem(4);
  const std::unique_ptr<Testing::StubTimeGeometryItem> second = makeItem(4);

  for (int step = 0; step < 4; ++step)
  {
    for (int feature = 0; feature < 3; ++feature)
    {
      // Offset, so a curve read from the wrong run is a different set of
      // numbers rather than a second copy of the first.
      second->setValue(step, feature, step * 10.0 + feature + 1000.0);
    }
  }

  QString message;
  DataItemLayer *runA = DataItemLayer::create(first.get(), message).release();
  ASSERT_NE(runA, nullptr) << message.toStdString();
  runA->setName(QStringLiteral("run A — depth"));
  ASSERT_GE(stack.addLayer(runA), 0);

  DataItemLayer *runB = DataItemLayer::create(second.get(), message).release();
  ASSERT_NE(runB, nullptr) << message.toStdString();
  runB->setName(QStringLiteral("run B — depth"));
  ASSERT_GE(stack.addLayer(runB), 0);

  // One feature, on one layer. The stack clears every other selection.
  stack.selectOnly(runA, QSet<int>{1});
  ASSERT_TRUE(runB->selection().isEmpty())
    << "the stack let two layers hold a selection at once";

  ASSERT_EQ(panel.seriesCount(), 2)
    << "one run was plotted where two stand on the same mesh";
  ASSERT_EQ(panel.plottedLayers().size(), 2);

  // The values are each run's own, at the feature that was picked.
  const QVector<double> mine = panel.seriesValues(0);
  const QVector<double> theirs = panel.seriesValues(1);

  ASSERT_EQ(mine.size(), 4);
  ASSERT_EQ(theirs.size(), 4);

  for (int level = 0; level < 4; ++level)
  {
    EXPECT_DOUBLE_EQ(mine.at(level), level * 10.0 + 1.0);
    EXPECT_DOUBLE_EQ(theirs.at(level), level * 10.0 + 1.0 + 1000.0);
  }
}

TEST_F(SeriesPlotTest, AnOverlaidSeriesSaysWhichRunItCameFrom)
{
  // The plan's gate. Two curves that do not say which run each belongs to
  // are two curves nobody can act on, and the layer name is where the run's
  // provenance already lives.
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> first = makeItem(3);
  const std::unique_ptr<Testing::StubTimeGeometryItem> second = makeItem(3);

  QString message;
  DataItemLayer *runA = DataItemLayer::create(first.get(), message).release();
  runA->setName(QStringLiteral("baseline 2019 — hydro — depth"));
  ASSERT_GE(stack.addLayer(runA), 0);

  DataItemLayer *runB = DataItemLayer::create(second.get(), message).release();
  runB->setName(QStringLiteral("scenario 2050 — hydro — depth"));
  ASSERT_GE(stack.addLayer(runB), 0);

  stack.selectOnly(runA, QSet<int>{2});

  ASSERT_EQ(panel.seriesCount(), 2);

  const QVector<ExportSeries> &written = panel.exportSeries();
  ASSERT_EQ(written.size(), 2);

  EXPECT_TRUE(written.at(0).name.contains(QStringLiteral("baseline 2019")))
    << written.at(0).name.toStdString();
  EXPECT_TRUE(written.at(1).name.contains(QStringLiteral("scenario 2050")))
    << written.at(1).name.toStdString();

  // Both name the feature too: the run alone does not say where.
  EXPECT_TRUE(written.at(0).name.contains(QStringLiteral("2")));

  // And the chart carries no single title, because there is no one run it
  // is of — a title naming one of two runs is worse than none.
  auto *chart = panel.findChild<QChartView *>(QStringLiteral("seriesPlotView"))
                  ->chart();
  EXPECT_TRUE(chart->title().isEmpty()) << chart->title().toStdString();
}

TEST_F(SeriesPlotTest, ASingleRunKeepsItsPlainFeatureNamesAndItsTitle)
{
  // The other half of the rule: with one run there is nothing to tell apart,
  // and prefixing every series with a layer name the title already carries
  // is noise.
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> item = makeItem(3);

  QString message;
  DataItemLayer *layer = DataItemLayer::create(item.get(), message).release();
  layer->setName(QStringLiteral("only run — depth"));
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{0});

  ASSERT_EQ(panel.seriesCount(), 1);
  EXPECT_EQ(panel.exportSeries().first().name, QStringLiteral("Feature 0"));

  auto *chart = panel.findChild<QChartView *>(QStringLiteral("seriesPlotView"))
                  ->chart();
  EXPECT_EQ(chart->title(), QStringLiteral("only run — depth"));
}

TEST_F(SeriesPlotTest, ARunOnDifferentGroundIsNotOverlaid)
{
  // The check that keeps the overlay honest. Two layers of the same size
  // over different places would overlay a curve from somewhere else
  // entirely, and a matching feature count is exactly what makes that look
  // reasonable.
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> here = makeItem(3);

  const std::unique_ptr<Testing::StubPoint> far0 =
    Testing::makePoint(90.0, 40.0, 0, m_crs.get());
  const std::unique_ptr<Testing::StubPoint> far1 =
    Testing::makePoint(91.0, 41.0, 1, m_crs.get());
  const std::unique_ptr<Testing::StubPoint> far2 =
    Testing::makePoint(92.0, 42.0, 2, m_crs.get());

  auto elsewhere = std::make_unique<Testing::StubTimeGeometryItem>(
    "depth", 3,
    std::vector<HydroCouple::Spatial::IGeometry *>{
      static_cast<HydroCouple::Spatial::IGeometry *>(far0.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(far1.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(far2.get())},
    kEpoch, 1.0);

  for (int step = 0; step < 3; ++step)
  {
    for (int feature = 0; feature < 3; ++feature)
    {
      elsewhere->setValue(step, feature, 5.0);
    }
  }

  QString message;
  DataItemLayer *mine = DataItemLayer::create(here.get(), message).release();
  ASSERT_GE(stack.addLayer(mine), 0);

  DataItemLayer *theirs =
    DataItemLayer::create(elsewhere.get(), message).release();
  ASSERT_GE(stack.addLayer(theirs), 0);

  ASSERT_EQ(mine->featureCount(), theirs->featureCount())
    << "the fixture cannot show that a matching count proves nothing";

  stack.selectOnly(mine, QSet<int>{0});

  EXPECT_EQ(panel.seriesCount(), 1)
    << "a run recorded somewhere else was overlaid on this one";
  ASSERT_EQ(panel.plottedLayers().size(), 1);
  EXPECT_EQ(panel.plottedLayers().first(), mine);
}

TEST_F(SeriesPlotTest, TheOverlayCanBeTurnedOff)
{
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> first = makeItem(3);
  const std::unique_ptr<Testing::StubTimeGeometryItem> second = makeItem(3);

  QString message;
  DataItemLayer *runA = DataItemLayer::create(first.get(), message).release();
  ASSERT_GE(stack.addLayer(runA), 0);
  DataItemLayer *runB = DataItemLayer::create(second.get(), message).release();
  ASSERT_GE(stack.addLayer(runB), 0);

  stack.selectOnly(runA, QSet<int>{0});
  ASSERT_EQ(panel.seriesCount(), 2);

  auto *check =
    panel.findChild<QCheckBox *>(QStringLiteral("overlayRunsCheck"));
  ASSERT_NE(check, nullptr);
  EXPECT_TRUE(check->isChecked()) << "the overlay is off by default";

  check->setChecked(false);

  EXPECT_EQ(panel.seriesCount(), 1)
    << "turning the overlay off left the other run on the chart";
}

TEST_F(SeriesPlotTest, TheTimeAxisSpansEveryRunNotJustTheSelectedOne)
{
  // A longer run overlaid on a shorter one's axis would be cut off at the
  // instant the shorter one stopped — with no sign that anything was
  // missing, since the curve simply ends.
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> shortRun = makeItem(2);
  const std::unique_ptr<Testing::StubTimeGeometryItem> longRun = makeItem(6);

  QString message;
  DataItemLayer *runA =
    DataItemLayer::create(shortRun.get(), message).release();
  ASSERT_GE(stack.addLayer(runA), 0);
  DataItemLayer *runB = DataItemLayer::create(longRun.get(), message).release();
  ASSERT_GE(stack.addLayer(runB), 0);

  stack.selectOnly(runA, QSet<int>{0});
  ASSERT_EQ(panel.seriesCount(), 2);

  auto *chart = panel.findChild<QChartView *>(QStringLiteral("seriesPlotView"))
                  ->chart();
  const QList<QAbstractAxis *> axes = chart->axes(Qt::Horizontal);
  ASSERT_EQ(axes.size(), 1);

  auto *time = qobject_cast<QDateTimeAxis *>(axes.first());
  ASSERT_NE(time, nullptr);

  // The long run ends five days after the epoch; the short one after one.
  const QDateTime last = dateTimeFromJulianDay(kEpoch + 5.0);
  EXPECT_EQ(time->max().date(), last.date())
    << "the axis stopped where the shorter run did";
}

TEST_F(SeriesPlotTest, ADifferenceIsPlottedLikeAnyOtherRecordedLayer)
{
  // Through the capability, not the class. A difference layer is not a
  // DataItemLayer, and a plot that asked for one by name could never draw
  // the very comparison the phase exists to make.
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  const std::unique_ptr<Testing::StubTimeGeometryItem> first = makeItem(3);
  const std::unique_ptr<Testing::StubTimeGeometryItem> second = makeItem(3);

  for (int step = 0; step < 3; ++step)
  {
    for (int feature = 0; feature < 3; ++feature)
    {
      second->setValue(step, feature, step * 10.0 + feature - 4.0);
    }
  }

  QString message;
  DifferenceLayer *gap =
    DifferenceLayer::create(first.get(), second.get(), message).release();
  ASSERT_NE(gap, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(gap), 0);

  stack.selectOnly(gap, QSet<int>{1});

  ASSERT_EQ(panel.seriesCount(), 1);
  ASSERT_EQ(panel.plottedLayers().size(), 1);
  EXPECT_EQ(panel.plottedLayers().first(), gap);

  const QVector<double> plotted = panel.seriesValues(0);
  ASSERT_EQ(plotted.size(), 3);

  for (const double value : plotted)
  {
    EXPECT_DOUBLE_EQ(value, 4.0);
  }
}

TEST_F(SeriesPlotTest, TwoDifferentQuantitiesDoNotShareOneAxisLabel)
{
  // A run and the difference taken from it stand on the same ground, so both
  // are drawn — but one is a depth and the other is a change in depth. One
  // label for both would put a name on the axis that is wrong for half of
  // what is on it.
  LayerStackModel stack;
  SeriesPlotPanel panel;
  panel.setModel(&stack);

  auto first = std::make_unique<Testing::StubTimeGeometryItem>(
    "depth", 3,
    std::vector<HydroCouple::Spatial::IGeometry *>{
      static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_c.get())},
    kEpoch, 1.0, "Water depth (m)");

  auto second = std::make_unique<Testing::StubTimeGeometryItem>(
    "depth", 3,
    std::vector<HydroCouple::Spatial::IGeometry *>{
      static_cast<HydroCouple::Spatial::IGeometry *>(m_a.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_b.get()),
      static_cast<HydroCouple::Spatial::IGeometry *>(m_c.get())},
    kEpoch, 1.0, "Water depth (m)");

  for (int step = 0; step < 3; ++step)
  {
    for (int feature = 0; feature < 3; ++feature)
    {
      first->setValue(step, feature, step + feature);
      second->setValue(step, feature, step + feature - 2.0);
    }
  }

  QString message;
  DataItemLayer *run = DataItemLayer::create(first.get(), message).release();
  ASSERT_GE(stack.addLayer(run), 0);

  DifferenceLayer *gap =
    DifferenceLayer::create(first.get(), second.get(), message).release();
  ASSERT_NE(gap, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(gap), 0);

  stack.selectOnly(run, QSet<int>{0});
  ASSERT_EQ(panel.plottedLayers().size(), 2);

  auto *chart = panel.findChild<QChartView *>(QStringLiteral("seriesPlotView"))
                  ->chart();
  const QList<QAbstractAxis *> axes = chart->axes(Qt::Vertical);
  ASSERT_EQ(axes.size(), 1);

  EXPECT_EQ(axes.first()->titleText(), QStringLiteral("Value"))
    << "one quantity's name was put on an axis carrying two";

  // And with only the run on the chart, its own caption is used.
  auto *check =
    panel.findChild<QCheckBox *>(QStringLiteral("overlayRunsCheck"));
  ASSERT_NE(check, nullptr);
  check->setChecked(false);

  ASSERT_EQ(panel.plottedLayers().size(), 1);
  EXPECT_EQ(chart->axes(Qt::Vertical).first()->titleText(),
            QStringLiteral("Water depth (m)"));
}
