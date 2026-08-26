/*!
 * \file   test_maptools.cpp
 * \brief  Phase C5c verification — the map's gesture sets.
 *
 * The gates are on where the view ends up, in world coordinates, rather than
 * on which handler ran: a zoom tool that showed a rubber band and framed the
 * wrong rectangle would satisfy every softer check.
 *
 * The degenerate drag has a test of its own because it is where this breaks
 * silently. A zero-area QRect reports itself null, so the obvious guard is
 * true for a rectangle that is merely thin — the same trap C4a paid for with
 * QRectF and a point layer's bounds.
 */

#include "core/composerapplication.h"
#include "gis/spatialreference.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maptool.h"
#include "map/maptransform.h"
#include "probelayer.h"

#include <gtest/gtest.h>

#include <QMouseEvent>
#include <QRubberBand>
#include <QSignalSpy>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  constexpr int kWidth = 800;
  constexpr int kHeight = 400;

  class MapToolsTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_maptools";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      //! A canvas showing exactly (-400, -200) to (400, 200).
      void showFixture(MapCanvas &canvas)
      {
        canvas.resize(kWidth, kHeight);
        canvas.setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));
      }

      //! Sends a press, a move and a release, as a drag does.
      static void drag(MapCanvas &canvas, const QPoint &from,
                       const QPoint &to)
      {
        press(canvas, from);
        moveTo(canvas, to);
        release(canvas, to);
      }

      static void press(MapCanvas &canvas, const QPoint &at)
      {
        QMouseEvent event(QEvent::MouseButtonPress, QPointF(at),
                          canvas.mapToGlobal(QPointF(at)), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &event);
      }

      static void moveTo(MapCanvas &canvas, const QPoint &at)
      {
        QMouseEvent event(QEvent::MouseMove, QPointF(at),
                          canvas.mapToGlobal(QPointF(at)), Qt::NoButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &event);
      }

      static void release(MapCanvas &canvas, const QPoint &at)
      {
        QMouseEvent event(QEvent::MouseButtonRelease, QPointF(at),
                          canvas.mapToGlobal(QPointF(at)), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &event);
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *MapToolsTest::s_app = nullptr;
}

TEST_F(MapToolsTest, ThePanToolIsWhatTheMapStartsUnder)
{
  MapCanvas canvas;

  EXPECT_EQ(canvas.toolKind(), MapToolKind::Pan)
    << "the map opened under a gesture set nobody chose";
}

TEST_F(MapToolsTest, DraggingUnderThePanToolMovesTheView)
{
  MapCanvas canvas;
  showFixture(canvas);

  const QRectF before = canvas.transform().visibleExtent();

  // Dragging the content right by 100 pixels moves the view 100 world units
  // left, since the fixture is one world unit per pixel.
  drag(canvas, QPoint(400, 200), QPoint(500, 200));

  const QRectF after = canvas.transform().visibleExtent();

  EXPECT_NEAR(after.center().x(), before.center().x() - 100.0, 1.0e-6);
  EXPECT_NEAR(after.center().y(), before.center().y(), 1.0e-6);
  EXPECT_NEAR(after.width(), before.width(), 1.0e-6)
    << "a pan changed the scale";
}

TEST_F(MapToolsTest, AClickUnderThePanToolIdentifiesWhatIsUnderIt)
{
  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  showFixture(canvas);

  auto *layer = new Testing::ProbeLayer(QStringLiteral("probe"),
                                        QRectF(-10.0, -10.0, 20.0, 20.0));
  ASSERT_GE(stack.addLayer(layer), 0);

  QSignalSpy spy(&canvas, &MapCanvas::featurePicked);

  // A press and release at the same point is a click, not a pan.
  press(canvas, QPoint(400, 200));
  release(canvas, QPoint(400, 200));

  EXPECT_EQ(spy.count(), 1)
    << "a click under the pan tool no longer identifies";

  // A drag that happens to end over a feature is not a click, and must not
  // select it — otherwise every pan that finishes on top of something
  // silently changes the selection.
  drag(canvas, QPoint(300, 150), QPoint(400, 200));

  EXPECT_EQ(spy.count(), 1)
    << "panning selected whatever the drag happened to end on";
}

TEST_F(MapToolsTest, DraggingARectangleFramesExactlyThatRectangle)
{
  MapCanvas canvas;
  showFixture(canvas);
  canvas.setToolKind(MapToolKind::Zoom);

  // Screen (500, 250) to (700, 350) is world (100, -50) to (300, -150) in
  // the fixture: one unit per pixel, y downward on screen and upward in the
  // world.
  drag(canvas, QPoint(500, 250), QPoint(700, 350));

  const QRectF shown = canvas.transform().visibleExtent();

  // The width is what was dragged. The height follows the viewport's aspect
  // ratio rather than the drag's, since the view cannot show a rectangle of
  // a different shape than itself — so it is the wider of the two that is
  // honoured, and nothing dragged is ever cropped out.
  EXPECT_NEAR(shown.center().x(), 200.0, 1.0)
    << "the framed rectangle is not centred on what was dragged";
  EXPECT_NEAR(shown.center().y(), -100.0, 1.0);

  EXPECT_GE(shown.width(), 200.0 - 1.0);
  EXPECT_LE(shown.width(), 210.0)
    << "the view is far wider than the rectangle that was dragged";
}

TEST_F(MapToolsTest, ADragTooSmallToBeARectangleZoomsAboutThePoint)
{
  MapCanvas canvas;
  showFixture(canvas);
  canvas.setToolKind(MapToolKind::Zoom);

  const QRectF before = canvas.transform().visibleExtent();

  // Two pixels: inside the click slop, and a rectangle with area. Framing it
  // would zoom to a two-metre view; the tool must read it as a click.
  drag(canvas, QPoint(400, 200), QPoint(402, 202));

  const QRectF after = canvas.transform().visibleExtent();

  EXPECT_NEAR(after.width(), before.width() * 0.5, 1.0)
    << "a click under the zoom tool did not zoom in by half";

  // And a drag of no extent at all — the case a QRect::isNull() guard would
  // have caught and a width/height guard has to catch instead.
  const QRectF beforeClick = canvas.transform().visibleExtent();

  press(canvas, QPoint(400, 200));
  release(canvas, QPoint(400, 200));

  EXPECT_NEAR(canvas.transform().visibleExtent().width(),
              beforeClick.width() * 0.5, 1.0);
}

TEST_F(MapToolsTest, TheZoomToolShowsABandWhileDragging)
{
  MapCanvas canvas;
  showFixture(canvas);
  canvas.setToolKind(MapToolKind::Zoom);

  press(canvas, QPoint(200, 100));
  moveTo(canvas, QPoint(400, 300));

  auto *band = canvas.findChild<QRubberBand *>(QStringLiteral("zoomRubberBand"));
  ASSERT_NE(band, nullptr) << "no rubber band, so the drag is invisible";

  // isVisibleTo(), not isVisible(): the canvas here is never shown, so
  // every child reports itself invisible whatever the tool did. This asks
  // the question actually being tested — would it be on screen if the
  // canvas were.
  EXPECT_TRUE(band->isVisibleTo(&canvas));

  // Corner to corner, which is a pixel wider than the same rectangle given
  // as an origin and a size — QRect counts both edges.
  EXPECT_EQ(band->geometry(), QRect(QPoint(200, 100), QPoint(400, 300)));

  release(canvas, QPoint(400, 300));

  EXPECT_FALSE(band->isVisibleTo(&canvas))
    << "the band outlived the gesture that drew it";
}

TEST_F(MapToolsTest, SwitchingToolsMidDragLeavesNoBandBehind)
{
  MapCanvas canvas;
  showFixture(canvas);
  canvas.setToolKind(MapToolKind::Zoom);

  press(canvas, QPoint(200, 100));
  moveTo(canvas, QPoint(400, 300));

  ASSERT_NE(canvas.findChild<QRubberBand *>(QStringLiteral("zoomRubberBand")),
            nullptr);

  canvas.setToolKind(MapToolKind::Pan);

  // Looked up again rather than held: the band belongs to the tool, and the
  // tool has just been destroyed — keeping the pointer across the switch
  // reads freed memory, which is how this test first "passed" by crashing.
  EXPECT_EQ(canvas.findChild<QRubberBand *>(QStringLiteral("zoomRubberBand")),
            nullptr)
    << "a band belonging to a tool that is no longer active is still on "
       "screen, with nothing left to finish it";
}

TEST_F(MapToolsTest, ThePanToolDoesNotZoomAndTheZoomToolDoesNotPan)
{
  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  showFixture(canvas);

  auto *layer = new Testing::ProbeLayer(QStringLiteral("probe"),
                                        QRectF(-10.0, -10.0, 20.0, 20.0));
  ASSERT_GE(stack.addLayer(layer), 0);

  QSignalSpy spy(&canvas, &MapCanvas::featurePicked);

  canvas.setToolKind(MapToolKind::Zoom);

  const QRectF before = canvas.transform().visibleExtent();

  // The same gesture that pans under the other tool. Under this one it must
  // zoom instead — a tool switch that changed nothing would leave both of
  // these looking identical.
  drag(canvas, QPoint(400, 200), QPoint(500, 250));

  const QRectF after = canvas.transform().visibleExtent();

  EXPECT_LT(after.width(), before.width())
    << "dragging under the zoom tool panned instead of zooming";

  EXPECT_EQ(spy.count(), 0)
    << "the zoom tool identified a feature, which is the pan tool's job";
}
