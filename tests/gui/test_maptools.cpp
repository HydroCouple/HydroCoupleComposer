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
#include "core/preferencesmanager.h"
#include "gis/spatialreference.h"
#include "settingsredirect.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maptool.h"
#include "map/maptransform.h"
#include "layers/featurelayer.h"
#include "probelayer.h"
#include "vectorprobe.h"

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
          // The tools read the application-wide preferences, which a test
          // must not write into the developer's own configuration.
          Testing::redirectSettingsTo(
            QStringLiteral(COMPOSER_PREFERENCES_FIXTURE_DIR)
            + QStringLiteral("/test_maptools"));

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

TEST_F(MapToolsTest, TheDragThresholdIsAPreferenceReadOnEveryGesture)
{
  PreferencesManager *prefs = PreferencesManager::instance();
  prefs->resetToDefaults();

  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  showFixture(canvas);

  auto *layer = new Testing::ProbeLayer(QStringLiteral("probe"),
                                        QRectF(-10.0, -10.0, 20.0, 20.0));
  ASSERT_GE(stack.addLayer(layer), 0);

  QSignalSpy spy(&canvas, &MapCanvas::featurePicked);

  // Two pixels of travel is inside the default slop, so it is a click.
  drag(canvas, QPoint(400, 200), QPoint(402, 202));
  ASSERT_EQ(spy.count(), 1) << "the default slop no longer reads as a click";

  // With no slop at all the same two pixels are a pan — read on this
  // gesture, not at the tool's construction, or the change would wait for
  // the next tool.
  prefs->setDragThresholdPixels(0);
  drag(canvas, QPoint(400, 200), QPoint(402, 202));
  EXPECT_EQ(spy.count(), 1) << "a zero threshold still let a drag pick";

  // And with a generous one, eight pixels are a click.
  prefs->setDragThresholdPixels(10);
  drag(canvas, QPoint(400, 200), QPoint(408, 200));
  EXPECT_EQ(spy.count(), 2) << "a wide threshold did not make a short drag a click";

  prefs->resetToDefaults();
}

TEST_F(MapToolsTest, DraggingARectangleFramesExactlyThatRectangle)
{
  MapCanvas canvas;
  showFixture(canvas);
  canvas.setToolKind(MapToolKind::ZoomIn);

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
  canvas.setToolKind(MapToolKind::ZoomIn);

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
  canvas.setToolKind(MapToolKind::ZoomIn);

  press(canvas, QPoint(200, 100));
  moveTo(canvas, QPoint(400, 300));

  auto *band = canvas.findChild<QRubberBand *>(QStringLiteral("mapRubberBand"));
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
  canvas.setToolKind(MapToolKind::ZoomIn);

  press(canvas, QPoint(200, 100));
  moveTo(canvas, QPoint(400, 300));

  ASSERT_NE(canvas.findChild<QRubberBand *>(QStringLiteral("mapRubberBand")),
            nullptr);

  canvas.setToolKind(MapToolKind::Pan);

  // Looked up again rather than held: the band belongs to the tool, and the
  // tool has just been destroyed — keeping the pointer across the switch
  // reads freed memory, which is how this test first "passed" by crashing.
  EXPECT_EQ(canvas.findChild<QRubberBand *>(QStringLiteral("mapRubberBand")),
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

  canvas.setToolKind(MapToolKind::ZoomIn);

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

TEST_F(MapToolsTest, TheSelectToolTakesEveryFeatureTheBandCrosses)
{
  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  showFixture(canvas);

  auto *layer = new Testing::VectorProbe(QStringLiteral("network"));

  // Three short lines at x = -200, 0 and 200, which in the fixture are
  // screen x = 200, 400 and 600.
  layer->addLine({QPointF(-200.0, -20.0), QPointF(-200.0, 20.0)},
                 QStringLiteral("west"));
  layer->addLine({QPointF(0.0, -20.0), QPointF(0.0, 20.0)},
                 QStringLiteral("middle"));
  layer->addLine({QPointF(200.0, -20.0), QPointF(200.0, 20.0)},
                 QStringLiteral("east"));

  ASSERT_GE(stack.addLayer(layer), 0);

  canvas.setToolKind(MapToolKind::Select);

  // A box over the western two only: screen x 150 to 450 catches the lines
  // at 200 and 400 and leaves the one at 600.
  drag(canvas, QPoint(150, 150), QPoint(450, 250));

  EXPECT_EQ(layer->selection().size(), 2)
    << "the band did not take exactly the features it crossed";
  EXPECT_TRUE(layer->selection().contains(0));
  EXPECT_TRUE(layer->selection().contains(1));
  EXPECT_FALSE(layer->selection().contains(2))
    << "a feature outside the band was selected";

  // A click still selects one, and replaces the band's selection rather
  // than adding to it.
  press(canvas, QPoint(600, 200));
  release(canvas, QPoint(600, 200));

  EXPECT_EQ(layer->selection().size(), 1);
  EXPECT_TRUE(layer->selection().contains(2));

  // A band over empty map clears the selection, which is how a user says
  // "nothing" — the same as a click on empty map.
  drag(canvas, QPoint(700, 320), QPoint(780, 380));

  EXPECT_TRUE(layer->selection().isEmpty());
}

TEST_F(MapToolsTest, ABandTakesALineThatMerelyCrossesIt)
{
  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  showFixture(canvas);

  auto *layer = new Testing::VectorProbe(QStringLiteral("network"));

  // One long conduit right across the view. Neither end is anywhere near
  // the small box below, and it is still something the user dragged over —
  // a band that took only features with a vertex inside would select
  // nothing on a network of long lines.
  layer->addLine({QPointF(-380.0, 0.0), QPointF(380.0, 0.0)},
                 QStringLiteral("trunk"));

  ASSERT_GE(stack.addLayer(layer), 0);

  canvas.setToolKind(MapToolKind::Select);

  drag(canvas, QPoint(390, 180), QPoint(430, 220));

  EXPECT_EQ(layer->selection().size(), 1)
    << "a line crossing the band was missed because neither end was inside";
}

TEST_F(MapToolsTest, ZoomOutFitsTheViewIntoTheBoxItWasGiven)
{
  MapCanvas canvas;
  showFixture(canvas);
  canvas.setToolKind(MapToolKind::ZoomOut);

  const QRectF before = canvas.transform().visibleExtent();

  // A box a quarter of the viewport across: everything on screen has to fit
  // inside it, so the view widens by about four. "About", because a QRect
  // spans both its edges — the box a 300→500 drag makes is 201 pixels wide,
  // not 200 — and the expectation is computed the same way rather than
  // rounded to a number that looks tidier than the arithmetic.
  drag(canvas, QPoint(300, 150), QPoint(500, 250));

  const QRectF after = canvas.transform().visibleExtent();

  const double expected =
    before.width() * 800.0 / double(QRect(QPoint(300, 150),
                                          QPoint(500, 250)).width());

  EXPECT_NEAR(after.width(), expected, 1.0)
    << "the drag's size did not decide how far it zoomed out";

  // And it is the inverse of zooming in on the same box: dragging it again
  // under Zoom In returns to where this started.
  canvas.setToolKind(MapToolKind::ZoomIn);
  drag(canvas, QPoint(300, 150), QPoint(500, 250));

  EXPECT_NEAR(canvas.transform().visibleExtent().width(), before.width(),
              before.width() * 0.02)
    << "zoom in and zoom out on the same box are not inverses";
}

TEST_F(MapToolsTest, ASmallerBoxZoomsOutFurther)
{
  MapCanvas wide;
  showFixture(wide);
  wide.setToolKind(MapToolKind::ZoomOut);

  MapCanvas narrow;
  showFixture(narrow);
  narrow.setToolKind(MapToolKind::ZoomOut);

  const double before = wide.transform().visibleExtent().width();

  // Half the viewport …
  drag(wide, QPoint(200, 100), QPoint(600, 300));

  // … against a quarter of it.
  drag(narrow, QPoint(300, 150), QPoint(500, 250));

  EXPECT_GT(narrow.transform().visibleExtent().width(),
            wide.transform().visibleExtent().width())
    << "both boxes zoomed out by the same amount, so the size was ignored";

  EXPECT_GT(wide.transform().visibleExtent().width(), before);
}

TEST_F(MapToolsTest, AClickUnderTheZoomToolsGoesTheRightWay)
{
  MapCanvas canvas;
  showFixture(canvas);

  const double before = canvas.transform().visibleExtent().width();

  canvas.setToolKind(MapToolKind::ZoomIn);
  press(canvas, QPoint(400, 200));
  release(canvas, QPoint(400, 200));

  const double zoomedIn = canvas.transform().visibleExtent().width();
  EXPECT_LT(zoomedIn, before);

  canvas.setToolKind(MapToolKind::ZoomOut);
  press(canvas, QPoint(400, 200));
  release(canvas, QPoint(400, 200));

  EXPECT_NEAR(canvas.transform().visibleExtent().width(), before, 1.0)
    << "a click out did not undo a click in";
}
