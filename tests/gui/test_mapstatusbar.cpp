/*!
 * \file   test_mapstatusbar.cpp
 * \brief  Phase C5e verification — scale, coordinates and CRS on the bar.
 *
 * The gates here are known answers rather than "a number appeared", because
 * every way this breaks still produces a number. One metre per pixel at 96
 * DPI is 1:3779.5 and nothing else; a foot-based system reads 3.28 times a
 * metre-based one over the same ground; and a scale that ignored units
 * entirely would print the identical figure for both, which is the vacuous
 * pass to rule out.
 */

#include "core/composerapplication.h"
#include "gis/spatialreference.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "probelayer.h"
#include "ui/panels/mapstatusbar.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QToolButton>

#include <cmath>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  //! The viewport every fixture uses, so the arithmetic below is checkable.
  constexpr int kWidth = 800;
  constexpr int kHeight = 400;

  //! Physical metres per screen pixel at 96 DPI.
  constexpr double kMetresPerPixelAt96Dpi = 0.0254 / 96.0;

  class MapStatusBarTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_mapstatusbar";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      /*!
       * \brief A canvas showing exactly \a metres of world across its width.
       *
       * No margin, because the fixtures below are arithmetic and a 5% frame
       * would put a 5% error into every expected value.
       */
      void showExactly(MapCanvas &canvas, double worldAcross)
      {
        canvas.resize(kWidth, kHeight);

        const double halfHeight =
          worldAcross * double(kHeight) / double(kWidth) * 0.5;

        canvas.setVisibleExtent(QRectF(-worldAcross * 0.5, -halfHeight,
                                       worldAcross, halfHeight * 2.0));
      }

      //! The denominator the current screen makes of \a groundMetresPerPixel.
      [[nodiscard]] static double expectedDenominator(
        double groundMetresPerPixel)
      {
        return groundMetresPerPixel / kMetresPerPixelAt96Dpi;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *MapStatusBarTest::s_app = nullptr;
}

TEST_F(MapStatusBarTest, OneMetrePerPixelIsOneToThreeThousandSevenHundred)
{
  MapCanvas canvas;

  // Web Mercator: a projected system whose linear unit is the metre, so a
  // world unit is a metre and the arithmetic is direct.
  canvas.setCrs(SpatialReference::webMercator());
  showExactly(canvas, double(kWidth));

  // Only meaningful against this screen's DPI, so the expectation is
  // computed the same way rather than hard-coded to a machine.
  const double perPixel = 1.0;
  const double expected = expectedDenominator(perPixel);

  EXPECT_NEAR(canvas.scaleDenominator(), expected, expected * 0.05)
    << "one metre per pixel should read about 1:3780 at 96 DPI";
}

TEST_F(MapStatusBarTest, TheScaleKnowsWhatAWorldUnitIsWorth)
{
  // The gate that proves unit-awareness. EPSG:2249 is Massachusetts
  // mainland in US survey feet; EPSG:26986 is the same ground in metres.
  // The same extent of world units is 3.28 times less ground in feet, so
  // the two must not read alike — and a scale that ignored units would
  // print the identical number for both.
  QString message;

  MapCanvas metric;
  metric.setCrs(SpatialReference::fromAuthority(QStringLiteral("EPSG"), 26986,
                                                message));
  ASSERT_NE(metric.crs(), nullptr) << message.toStdString();
  showExactly(metric, 8000.0);

  MapCanvas imperial;
  imperial.setCrs(SpatialReference::fromAuthority(QStringLiteral("EPSG"), 2249,
                                                  message));
  ASSERT_NE(imperial.crs(), nullptr) << message.toStdString();
  showExactly(imperial, 8000.0);

  const double ratio =
    imperial.scaleDenominator() / metric.scaleDenominator();

  // 0.3048006… metres per US survey foot.
  EXPECT_NEAR(ratio, 0.3048, 1.0e-3)
    << "a foot-based system read the same as a metre-based one, so the "
       "scale is ignoring the CRS entirely";
}

TEST_F(MapStatusBarTest, ADegreeIsWorthLessAwayFromTheEquator)
{
  MapCanvas equator;
  equator.setCrs(SpatialReference::wgs84());
  equator.resize(kWidth, kHeight);
  equator.setVisibleExtent(QRectF(-2.0, -1.0, 4.0, 2.0));

  MapCanvas high;
  high.setCrs(SpatialReference::wgs84());
  high.resize(kWidth, kHeight);
  high.setVisibleExtent(QRectF(-2.0, 59.0, 4.0, 2.0));

  // cos 60° is a half, so the same four degrees of longitude is about half
  // the ground and about half the denominator.
  const double ratio = high.scaleDenominator() / equator.scaleDenominator();

  EXPECT_NEAR(ratio, std::cos(qDegreesToRadians(60.0)), 0.02)
    << "a degree was treated as the same distance at every latitude";
}

TEST_F(MapStatusBarTest, SettingAScaleIsTheInverseOfReadingOne)
{
  MapCanvas canvas;
  canvas.setCrs(SpatialReference::webMercator());
  showExactly(canvas, 4000.0);

  const QPointF centreBefore = canvas.transform().visibleExtent().center();

  canvas.setScaleDenominator(25000.0);

  EXPECT_NEAR(canvas.scaleDenominator(), 25000.0, 1.0)
    << "setting a scale and reading it back did not return what was set";

  const QPointF centreAfter = canvas.transform().visibleExtent().center();

  // The centre is held: zooming to a scale is a change of magnification,
  // not a navigation command.
  EXPECT_NEAR(centreAfter.x(), centreBefore.x(), 1.0e-6);
  EXPECT_NEAR(centreAfter.y(), centreBefore.y(), 1.0e-6);
}

TEST_F(MapStatusBarTest, TheBarShowsTheScaleAndFollowsTheView)
{
  MapCanvas canvas;
  canvas.setCrs(SpatialReference::webMercator());
  showExactly(canvas, 4000.0);

  MapStatusBar bar;
  bar.setCanvas(&canvas);
  bar.setLive(true);

  const QString first = bar.scaleText();
  ASSERT_TRUE(first.startsWith(QStringLiteral("1:"))) << first.toStdString();

  canvas.setScaleDenominator(50000.0);

  EXPECT_NE(bar.scaleText(), first) << "the readout did not follow the view";
  EXPECT_TRUE(bar.scaleText().contains(QStringLiteral("50")))
    << bar.scaleText().toStdString();
}

TEST_F(MapStatusBarTest, TypingAScaleZoomsTheMap)
{
  MapCanvas canvas;
  canvas.setCrs(SpatialReference::webMercator());
  showExactly(canvas, 4000.0);

  MapStatusBar bar;
  bar.setCanvas(&canvas);
  bar.setLive(true);

  auto *combo = bar.findChild<QComboBox *>(QStringLiteral("mapScaleCombo"));
  ASSERT_NE(combo, nullptr);

  combo->setEditText(QStringLiteral("1:12,000"));
  Q_EMIT combo->lineEdit()->editingFinished();

  EXPECT_NEAR(canvas.scaleDenominator(), 12000.0, 1.0)
    << "a typed scale did not reach the map";

  // Unreadable text puts the real scale back rather than leaving the field
  // showing something the view does not match.
  combo->setEditText(QStringLiteral("not a scale"));
  Q_EMIT combo->lineEdit()->editingFinished();

  EXPECT_NEAR(canvas.scaleDenominator(), 12000.0, 1.0);
  EXPECT_TRUE(bar.scaleText().contains(QStringLiteral("12")))
    << bar.scaleText().toStdString();
}

TEST_F(MapStatusBarTest, TheCrsButtonNamesTheSystemAndAsksForAChange)
{
  MapCanvas canvas;
  canvas.setCrs(SpatialReference::webMercator());

  MapStatusBar bar;
  bar.setCanvas(&canvas);
  bar.setLive(true);

  auto *button = bar.findChild<QToolButton *>(QStringLiteral("crsButton"));
  ASSERT_NE(button, nullptr);

  EXPECT_EQ(button->text(), QStringLiteral("EPSG:3857"));

  // Follows the map rather than being set once: changing the system with the
  // bar already built has to reach it.
  canvas.setCrs(SpatialReference::wgs84());
  EXPECT_EQ(button->text(), QStringLiteral("EPSG:4326"));

  QSignalSpy spy(&bar, &MapStatusBar::crsRequested);
  button->click();

  EXPECT_EQ(spy.count(), 1)
    << "the button does not ask anyone to change the system";
}

TEST_F(MapStatusBarTest, TheCoordinateReadoutFollowsThePointer)
{
  MapCanvas canvas;
  canvas.setCrs(SpatialReference::webMercator());
  showExactly(canvas, 4000.0);

  MapStatusBar bar;
  bar.setCanvas(&canvas);
  bar.setLive(true);

  auto *label = bar.findChild<QLabel *>(QStringLiteral("coordinateLabel"));
  ASSERT_NE(label, nullptr);

  Q_EMIT canvas.cursorMoved(QPointF(1234.5, -678.25));

  EXPECT_TRUE(label->text().contains(QStringLiteral("1234")))
    << label->text().toStdString();
  EXPECT_TRUE(label->text().contains(QStringLiteral("-678")))
    << label->text().toStdString();
}

TEST_F(MapStatusBarTest, TheControlsGoDeadWhereTheyMeanNothing)
{
  MapCanvas canvas;
  canvas.setCrs(SpatialReference::webMercator());
  showExactly(canvas, 4000.0);

  MapStatusBar bar;
  bar.setCanvas(&canvas);

  auto *combo = bar.findChild<QComboBox *>(QStringLiteral("mapScaleCombo"));
  auto *button = bar.findChild<QToolButton *>(QStringLiteral("crsButton"));
  ASSERT_NE(combo, nullptr);
  ASSERT_NE(button, nullptr);

  // A perspective camera has no single scale, so away from the map the
  // controls are disabled rather than showing a number that means nothing.
  bar.setLive(false);
  EXPECT_FALSE(combo->isEnabled());
  EXPECT_FALSE(button->isEnabled());

  bar.setLive(true);
  EXPECT_TRUE(combo->isEnabled());
  EXPECT_TRUE(button->isEnabled());

  // And with no canvas at all there is nothing to be live about.
  bar.setCanvas(nullptr);
  bar.setLive(true);
  EXPECT_FALSE(combo->isEnabled());
}
