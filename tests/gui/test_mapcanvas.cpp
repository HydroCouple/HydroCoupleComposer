/*!
 * \file   test_mapcanvas.cpp
 * \brief  Phase C1b verification — the map canvas.
 *
 * These tests render for real: the canvas is asked to paint into an image and
 * the layers report what they were handed. Asserting only that a method was
 * called would pass for a canvas that painted nothing, which is the failure
 * this suite exists to catch.
 *
 * Pixels are read back where the question is "did this end up on top", since
 * draw order is exactly what a call-counting test cannot see.
 */

#include "core/composerapplication.h"
#include "gis/spatialreference.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maplayer.h"
#include "probelayer.h"
#include "render/attributeprovider.h"
#include "render/layerstyle.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QWheelEvent>

using namespace HydroCouple::Composer;
using HydroCouple::Composer::Testing::ProbeLayer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  constexpr int kWidth = 400;
  constexpr int kHeight = 400;

  class MapCanvasTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_mapcanvas";
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
        canvas.setModel(&stack);
        canvas.resize(kWidth, kHeight);
      }

      //! Paints the canvas into an image and returns it.
      QImage paint()
      {
        QImage image(canvas.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        canvas.render(&image);

        return image;
      }

      ProbeLayer *addProbe(const QString &name, const QRectF &extent,
                           const QColor &color = Qt::black)
      {
        auto *layer = new ProbeLayer(name, extent, color);
        stack.addLayer(layer);

        return layer;
      }

      LayerStackModel stack;
      MapCanvas canvas;

      static ComposerApplication *s_app;
  };

  ComposerApplication *MapCanvasTest::s_app = nullptr;
}

TEST_F(MapCanvasTest, PaintsAnEmptyStackWithoutComplaint)
{
  canvas.setBackgroundColor(Qt::white);

  const QImage image = paint();

  ASSERT_FALSE(image.isNull());
  EXPECT_EQ(image.pixelColor(kWidth / 2, kHeight / 2), QColor(Qt::white));
}

TEST_F(MapCanvasTest, DrawsTheStackFromTheBottomUp)
{
  QVector<QString> order;

  // Both probes cover the same ground, so whichever is drawn last owns the
  // pixel — which is how the assertion below distinguishes them.
  ProbeLayer *bottom =
    addProbe(QStringLiteral("bottom"), QRectF(0.0, 0.0, 100.0, 100.0),
             Qt::red);
  ProbeLayer *top = addProbe(QStringLiteral("top"),
                             QRectF(0.0, 0.0, 100.0, 100.0), Qt::blue);

  bottom->order = &order;
  top->order = &order;

  const QImage image = paint();

  ASSERT_EQ(order.size(), 2);
  EXPECT_EQ(order.at(0), QStringLiteral("bottom"));
  EXPECT_EQ(order.at(1), QStringLiteral("top"));

  EXPECT_EQ(image.pixelColor(kWidth / 2, kHeight / 2), QColor(Qt::blue))
    << "the top of the layer tree must be the top of the map";
}

TEST_F(MapCanvasTest, LeavesHiddenAndFullyTransparentLayersUndrawn)
{
  ProbeLayer *hidden =
    addProbe(QStringLiteral("hidden"), QRectF(0.0, 0.0, 100.0, 100.0));
  ProbeLayer *transparent =
    addProbe(QStringLiteral("transparent"), QRectF(0.0, 0.0, 100.0, 100.0));

  hidden->setVisible(false);
  transparent->setOpacity(0.0);

  paint();

  EXPECT_EQ(hidden->renderCount, 0);
  EXPECT_EQ(transparent->renderCount, 0);
}

TEST_F(MapCanvasTest, AppliesPerLayerOpacityToThePainter)
{
  ProbeLayer *layer =
    addProbe(QStringLiteral("faded"), QRectF(0.0, 0.0, 100.0, 100.0));
  layer->setOpacity(0.25);

  const QImage image = paint();

  ASSERT_EQ(layer->renderCount, 1);
  EXPECT_NEAR(layer->lastOpacity, 0.25, 1e-9);

  // And the opacity reached the output, not just the painter: a quarter-alpha
  // black over white is a light grey, nowhere near black.
  const QColor pixel = image.pixelColor(kWidth / 2, kHeight / 2);
  EXPECT_GT(pixel.red(), 150);
  EXPECT_LT(pixel.red(), 220);
}

TEST_F(MapCanvasTest, DoesNotLetOneLayerLeakPainterStateIntoTheNext)
{
  ProbeLayer *first =
    addProbe(QStringLiteral("first"), QRectF(0.0, 0.0, 100.0, 100.0));
  ProbeLayer *second =
    addProbe(QStringLiteral("second"), QRectF(0.0, 0.0, 100.0, 100.0));

  first->setOpacity(0.2);
  second->setOpacity(1.0);

  paint();

  EXPECT_NEAR(first->lastOpacity, 0.2, 1e-9);
  EXPECT_NEAR(second->lastOpacity, 1.0, 1e-9);
}

TEST_F(MapCanvasTest, FramesTheFirstLayerToArrive)
{
  QSignalSpy transformSpy(&canvas, &MapCanvas::transformChanged);

  addProbe(QStringLiteral("probe"), QRectF(1000.0, 2000.0, 100.0, 100.0));

  EXPECT_GE(transformSpy.count(), 1);

  const QRectF visible = canvas.transform().visibleExtent();

  EXPECT_TRUE(visible.contains(QPointF(1050.0, 2050.0)))
    << "the view must be looking at the data, not at the origin";
  EXPECT_FALSE(visible.contains(QPointF(0.0, 0.0)));
}

TEST_F(MapCanvasTest, FullExtentCoversEveryVisibleLayerAndNothingHidden)
{
  addProbe(QStringLiteral("west"), QRectF(0.0, 0.0, 10.0, 10.0));
  addProbe(QStringLiteral("east"), QRectF(90.0, 0.0, 10.0, 10.0));

  ProbeLayer *far =
    addProbe(QStringLiteral("far"), QRectF(9000.0, 0.0, 10.0, 10.0));
  far->setVisible(false);

  const QRectF extent = canvas.fullExtent();

  EXPECT_DOUBLE_EQ(extent.left(), 0.0);
  EXPECT_DOUBLE_EQ(extent.right(), 100.0);
  EXPECT_LT(extent.right(), 9000.0)
    << "a hidden layer must not drag the view across the world";
}

TEST_F(MapCanvasTest, ZoomsToOneLayer)
{
  addProbe(QStringLiteral("here"), QRectF(0.0, 0.0, 10.0, 10.0));
  ProbeLayer *there =
    addProbe(QStringLiteral("there"), QRectF(500.0, 500.0, 10.0, 10.0));

  canvas.zoomToLayer(there);

  const QRectF visible = canvas.transform().visibleExtent();

  EXPECT_TRUE(visible.contains(QPointF(505.0, 505.0)));
  EXPECT_FALSE(visible.contains(QPointF(5.0, 5.0)));
}

TEST_F(MapCanvasTest, WheelZoomKeepsThePointUnderTheCursorStill)
{
  addProbe(QStringLiteral("probe"), QRectF(0.0, 0.0, 100.0, 100.0));

  const QPointF cursor(300.0, 120.0);
  const QPointF worldBefore = canvas.transform().toWorld(cursor);
  const double scaleBefore = canvas.transform().scale();

  QWheelEvent wheel(cursor, canvas.mapToGlobal(cursor.toPoint()), QPoint(),
                    QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QApplication::sendEvent(&canvas, &wheel);

  EXPECT_GT(canvas.transform().scale(), scaleBefore) << "a notch must zoom in";

  const QPointF worldAfter = canvas.transform().toWorld(cursor);

  EXPECT_NEAR(worldAfter.x(), worldBefore.x(), 1e-6);
  EXPECT_NEAR(worldAfter.y(), worldBefore.y(), 1e-6);
}

TEST_F(MapCanvasTest, DraggingPansTheMapWithTheHand)
{
  addProbe(QStringLiteral("probe"), QRectF(0.0, 0.0, 100.0, 100.0));

  const QPointF probe(50.0, 50.0);
  const QPointF screenBefore = canvas.transform().toScreen(probe);

  const QPointF press(200.0, 200.0);
  const QPointF release(240.0, 180.0);

  QMouseEvent pressEvent(QEvent::MouseButtonPress, press,
                         canvas.mapToGlobal(press.toPoint()), Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
  QMouseEvent moveEvent(QEvent::MouseMove, release,
                        canvas.mapToGlobal(release.toPoint()), Qt::NoButton,
                        Qt::LeftButton, Qt::NoModifier);
  QMouseEvent releaseEvent(QEvent::MouseButtonRelease, release,
                           canvas.mapToGlobal(release.toPoint()),
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);

  QApplication::sendEvent(&canvas, &pressEvent);
  QApplication::sendEvent(&canvas, &moveEvent);
  QApplication::sendEvent(&canvas, &releaseEvent);

  const QPointF screenAfter = canvas.transform().toScreen(probe);

  // The map follows the pointer exactly — 40 pixels right, 20 up.
  EXPECT_NEAR(screenAfter.x() - screenBefore.x(), 40.0, 1e-6);
  EXPECT_NEAR(screenAfter.y() - screenBefore.y(), -20.0, 1e-6);

  // And the drag ended: further motion must not keep panning.
  const QPointF drifted(400.0, 400.0);
  QMouseEvent afterRelease(QEvent::MouseMove, drifted,
                           canvas.mapToGlobal(drifted.toPoint()),
                           Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(&canvas, &afterRelease);

  EXPECT_NEAR(canvas.transform().toScreen(probe).x(), screenAfter.x(), 1e-6);
}

TEST_F(MapCanvasTest, ReprojectsALayerExtentIntoTheMapsCrs)
{
  const std::shared_ptr<SpatialReference> wgs84 = SpatialReference::wgs84();
  const std::shared_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();

  ASSERT_NE(wgs84, nullptr);
  ASSERT_NE(mercator, nullptr);

  canvas.setCrs(mercator);

  ProbeLayer *layer =
    addProbe(QStringLiteral("degrees"), QRectF(-1.0, -1.0, 2.0, 2.0));
  layer->setCrs(wgs84);

  const QRectF extent = canvas.fullExtent();

  // Known answers rather than a round-trip: one degree of longitude is
  // 111319.49 m on the Pseudo-Mercator sphere, and one degree of latitude at
  // the equator is 111325.14 m on it.
  EXPECT_NEAR(extent.right(), 111319.49, 1.0);
  EXPECT_NEAR(extent.left(), -111319.49, 1.0);
  EXPECT_NEAR(extent.top(), -111325.14, 1.0);
  EXPECT_NEAR(extent.bottom(), 111325.14, 1.0);
}

TEST_F(MapCanvasTest, LeavesExtentsAloneWhenTheCrsMatches)
{
  const std::shared_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();
  ASSERT_NE(mercator, nullptr);

  canvas.setCrs(mercator);

  ProbeLayer *layer = addProbe(QStringLiteral("metres"),
                               QRectF(1000.0, 2000.0, 500.0, 500.0));
  layer->setCrs(SpatialReference::webMercator());

  const QRectF extent = canvas.fullExtent();

  EXPECT_NEAR(extent.left(), 1000.0, 1e-6);
  EXPECT_NEAR(extent.top(), 2000.0, 1e-6);
  EXPECT_NEAR(extent.width(), 500.0, 1e-6);
}

TEST_F(MapCanvasTest, RepaintsOfItsOwnAccordWhenALayerChanges)
{
  ProbeLayer *layer =
    addProbe(QStringLiteral("probe"), QRectF(0.0, 0.0, 100.0, 100.0));

  // Shown and pumped, so the canvas receives real paint events. Painting on
  // demand instead would prove only that render() works when called — the
  // question here is whether an edit anywhere reaches the map without anyone
  // asking the canvas to redraw.
  canvas.show();
  QApplication::processEvents();

  ASSERT_GT(layer->renderCount, 0) << "the canvas never painted at all";

  const int before = layer->renderCount;

  layer->setOpacity(0.5);
  QApplication::processEvents();

  EXPECT_GT(layer->renderCount, before)
    << "a layer edit did not reach the canvas";
}

TEST_F(MapCanvasTest, FramingSurvivesTheWidgetsFirstRealLayout)
{
  addProbe(QStringLiteral("probe"), QRectF(0.0, 0.0, 1000.0, 700.0));

  // Framed while the widget is still at a placeholder size, which is what
  // happens when a project is opened while another tab is showing.
  canvas.resize(40, 20);
  canvas.zoomToFullExtent();

  canvas.resize(800, 600);

  const QRectF visible = canvas.transform().visibleExtent();

  EXPECT_TRUE(visible.adjusted(-1.0, -1.0, 1.0, 1.0)
                .contains(QRectF(0.0, 0.0, 1000.0, 700.0)))
    << "the framed extent was lost when the widget was laid out";

  // And it fills the viewport rather than sitting in a corner of it: the
  // binding axis must match the requested extent to within the margin.
  EXPECT_LT(visible.width(), 1000.0 * 2.0)
    << "the map is drawn far smaller than the space it has";
}

TEST_F(MapCanvasTest, ResizeKeepsTheScaleOnceTheUserHasMovedTheView)
{
  addProbe(QStringLiteral("probe"), QRectF(0.0, 0.0, 1000.0, 700.0));

  canvas.resize(400, 400);
  canvas.zoomToFullExtent();

  canvas.zoomBy(4.0);
  const double chosenScale = canvas.transform().scale();

  canvas.resize(800, 800);

  EXPECT_NEAR(canvas.transform().scale(), chosenScale, 1e-9)
    << "a resize overrode the scale the user chose";
}

// ── Styled layers (C1c) ─────────────────────────────────────────────────────

TEST_F(MapCanvasTest, DrawsEachClassInItsOwnColour)
{
  auto *layer = new Testing::ProbeFeatureLayer(QStringLiteral("styled"));

  AttributeField kind;
  kind.name = QStringLiteral("kind");
  kind.type = QMetaType::QString;
  layer->declareField(kind);

  layer->addFeature(QPointF(0.0, 0.0),
                    {{QStringLiteral("kind"), QStringLiteral("pipe")}});
  layer->addFeature(QPointF(100.0, 100.0),
                    {{QStringLiteral("kind"), QStringLiteral("weir")}});

  layer->styleRef().setMode(StyleMode::Categorized);
  layer->styleRef().setAttribute(QStringLiteral("kind"));

  stack.addLayer(layer);
  ASSERT_TRUE(layer->restyle());

  paint();

  ASSERT_EQ(layer->drawnColors.size(), 2);
  EXPECT_NE(layer->drawnColors.at(0), layer->drawnColors.at(1))
    << "two categories were drawn the same colour";
}

TEST_F(MapCanvasTest, StopsDrawingAClassSwitchedOffInTheLegend)
{
  auto *layer = new Testing::ProbeFeatureLayer(QStringLiteral("styled"));

  AttributeField kind;
  kind.name = QStringLiteral("kind");
  kind.type = QMetaType::QString;
  layer->declareField(kind);

  layer->addFeature(QPointF(0.0, 0.0),
                    {{QStringLiteral("kind"), QStringLiteral("pipe")}});
  layer->addFeature(QPointF(100.0, 100.0),
                    {{QStringLiteral("kind"), QStringLiteral("weir")}});

  layer->styleRef().setMode(StyleMode::Categorized);
  layer->styleRef().setAttribute(QStringLiteral("kind"));

  stack.addLayer(layer);
  ASSERT_TRUE(layer->restyle());

  paint();
  ASSERT_EQ(layer->drawnColors.size(), 2);

  // Straight through the model, exactly as the layer tree's checkbox does.
  const QModelIndex legendRow = stack.index(0, 0, stack.index(0, 0));
  ASSERT_TRUE(legendRow.isValid());
  ASSERT_TRUE(stack.setData(legendRow, Qt::Unchecked, Qt::CheckStateRole));

  paint();

  EXPECT_EQ(layer->drawnColors.size(), 1)
    << "a class hidden in the legend was still drawn on the map";
}
