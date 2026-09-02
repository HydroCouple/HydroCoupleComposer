/*!
 * \file   test_viewhandoff.cpp
 * \brief  C3c-3 — switching between the map and the 3D view.
 *
 * The plan asks for the round trip to be checked "through the UI, not only
 * the camera", and the distinction is the whole point of this file: Camera
 * and MapTransform were shown to agree about a ground rectangle back in C3a,
 * and none of that says the tab change carries it. So every case here drives
 * the real main window and switches tabs on the real tab widget.
 *
 * Nothing renders. It does not need to: what is under test is which
 * rectangle each view is looking at, and SceneRenderer was deliberately kept
 * out of the widget so that question can be asked without a graphics device —
 * which the offscreen platform plugin this suite runs under cannot provide.
 */

#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "scene/camera.h"
#include "scene/sceneview.h"
#include "ui/composermainwindow.h"

#include "vectorprobe.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QMouseEvent>
#include <QRubberBand>
#include <QDoubleSpinBox>

#include <QApplication>
#include <QRectF>
#include <QTabWidget>

#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  //! A mesh with relief, over a known footprint.
  MeshDefinition terrain()
  {
    MeshDefinition mesh;
    mesh.meshName = "terrain";
    mesh.nodeX = { 0.0, 400.0, 400.0, 0.0, 200.0 };
    mesh.nodeY = { 0.0, 0.0, 300.0, 300.0, 150.0 };
    mesh.nodeZ = { 0.0, 0.0, 0.0, 0.0, 45.0 };
    mesh.faceNodeOffsets = { 0, 3, 6, 9, 12 };
    mesh.faceNodes = { 0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4 };

    return mesh;
  }

  /*!
   * \brief Whether \a outer contains \a inner.
   *
   * With slack, and relative rather than absolute: Qt's vector maths is
   * single precision, so a camera solved for containment holds it to about a
   * part in 10^5 of the extent and not to the last bit. An absolute epsilon
   * would pass on a small extent and fail on a large one for no reason to do
   * with the hand-off.
   */
  bool containsWithinRounding(const QRectF &outer, const QRectF &inner)
  {
    const double slack =
      1.0e-5 * std::max({ inner.width(), inner.height(), 1.0 });

    return outer.left() <= inner.left() + slack &&
           outer.right() >= inner.right() - slack &&
           outer.top() <= inner.top() + slack &&
           outer.bottom() >= inner.bottom() - slack;
  }

  //! How far two rectangles differ, relative to their size.
  double relativeDifference(const QRectF &first, const QRectF &second)
  {
    const double scale =
      std::max({ first.width(), first.height(), 1.0e-9 });

    return std::max({ std::abs(first.left() - second.left()),
                      std::abs(first.right() - second.right()),
                      std::abs(first.top() - second.top()),
                      std::abs(first.bottom() - second.bottom()) }) /
           scale;
  }

  class ViewHandoffTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!QApplication::instance())
        {
          static int argc = 1;
          static char name[] = "test_viewhandoff";
          static char *argv[] = { name, nullptr };
          s_app = new QApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        m_window = std::make_unique<ComposerMainWindow>();
        m_window->resize(1000, 700);
        m_window->show();

        m_tabs = m_window->findChild<QTabWidget *>(
          QStringLiteral("workspaceTabs"));
        ASSERT_NE(m_tabs, nullptr) << "the workspace tabs are how a user "
                                     "switches views; without them there is "
                                     "no hand-off to test";

        QApplication::processEvents();
      }

      /*!
       * \brief Loads the terrain.
       *
       * Called by each test rather than by the fixture, because *when* the
       * geometry arrives is itself under test: a view framed on arriving
       * from the map must not be reframed by a layer that loads afterwards.
       */
      void addTerrain()
      {
        QString message;
        std::unique_ptr<MeshLayer> layer = MeshLayer::create(
          QStringLiteral("terrain"), terrain(), MeshEntity::Face, message);
        ASSERT_TRUE(layer) << message.toStdString();

        ASSERT_GE(m_window->layerStack()->addLayer(layer.release()), 0);

        QApplication::processEvents();
      }

      void TearDown() override
      {
        m_window.reset();
      }

      //! Switches to a tab the way a user does.
      void showTab(QWidget *page)
      {
        m_tabs->setCurrentWidget(page);
        QApplication::processEvents();
      }

      static QApplication *s_app;

      std::unique_ptr<ComposerMainWindow> m_window;
      QTabWidget *m_tabs = nullptr;
  };

  QApplication *ViewHandoffTest::s_app = nullptr;

  TEST_F(ViewHandoffTest, BothViewsAreTabsOfOneWorkspace)
  {
    EXPECT_GE(m_tabs->indexOf(m_window->mapCanvas()), 0);
    EXPECT_GE(m_tabs->indexOf(m_window->sceneView()), 0);
  }

  TEST_F(ViewHandoffTest, TheSceneArrivesLookingAtWhatTheMapShowed)
  {
    addTerrain();

    const QRectF asked(100.0, 80.0, 120.0, 90.0);

    showTab(m_window->mapCanvas());
    m_window->mapCanvas()->setVisibleExtent(asked);

    const QRectF shown = m_window->mapCanvas()->transform().visibleExtent();
    ASSERT_FALSE(shown.isEmpty());

    showTab(m_window->sceneView());

    const QRectF ground = m_window->sceneView()->groundExtent();
    ASSERT_FALSE(ground.isEmpty()) << "the 3D view has no ground extent, so "
                                      "the hand-off had nothing to carry";

    // Contains, not equals: framing preserves aspect ratio, and a tilted
    // camera sees a trapezoid whose bounding rectangle is larger still.
    EXPECT_TRUE(containsWithinRounding(ground, shown))
      << "the scene arrived showing less ground than the map had";

    // And it is the *map's* rectangle, not the whole scene: arriving at a
    // view of everything would lose the place the user had chosen.
    EXPECT_LT(ground.width(), 400.0)
      << "the scene framed the whole model rather than what the map showed";
  }

  TEST_F(ViewHandoffTest, TheMapArrivesLookingAtWhatTheSceneShowed)
  {
    addTerrain();
    showTab(m_window->sceneView());

    Camera camera = m_window->sceneView()->camera();
    camera.setElevation(90.0);
    camera.setTarget(QVector3D(250.0f, 120.0f, 0.0f));
    m_window->sceneView()->setCamera(camera);

    const QRectF ground = m_window->sceneView()->groundExtent();
    ASSERT_FALSE(ground.isEmpty());

    showTab(m_window->mapCanvas());

    const QRectF shown = m_window->mapCanvas()->transform().visibleExtent();

    EXPECT_LT(relativeDifference(ground, shown), 1.0e-3)
      << "the map is not showing the ground the scene was looking at";
  }

  TEST_F(ViewHandoffTest, ARoundTripThroughTheUiPreservesTheExtent)
  {
    // The plan's own check. Straight down, because that is the case where
    // preservation is available at all: the camera then sees a rectangle
    // rather than a trapezoid, and the two views are pages of one tab widget
    // so they share an aspect ratio exactly.
    addTerrain();
    showTab(m_window->sceneView());

    Camera camera = m_window->sceneView()->camera();
    camera.setElevation(90.0);
    camera.setTarget(QVector3D(180.0f, 140.0f, 0.0f));
    m_window->sceneView()->setCamera(camera);

    const QRectF before = m_window->sceneView()->groundExtent();
    ASSERT_FALSE(before.isEmpty());

    showTab(m_window->mapCanvas());
    showTab(m_window->sceneView());

    const QRectF after = m_window->sceneView()->groundExtent();

    EXPECT_LT(relativeDifference(before, after), 1.0e-3)
      << "a round trip through the tabs moved the view";
  }

  TEST_F(ViewHandoffTest, ATiltedRoundTripKeepsEverythingItShowed)
  {
    // Tilted, the camera sees a trapezoid and groundExtent() is its bounding
    // rectangle — so a round trip *widens*, necessarily and by construction.
    // What must hold is that nothing is lost: whatever was on screen is
    // still on screen. Saying the extent is preserved here would be saying
    // something that is not true of a bounding box.
    addTerrain();
    showTab(m_window->sceneView());

    Camera camera = m_window->sceneView()->camera();
    camera.setElevation(35.0);
    camera.setAzimuth(20.0);
    camera.setTarget(QVector3D(180.0f, 140.0f, 0.0f));
    m_window->sceneView()->setCamera(camera);

    const QRectF before = m_window->sceneView()->groundExtent();
    ASSERT_FALSE(before.isEmpty());

    showTab(m_window->mapCanvas());
    showTab(m_window->sceneView());

    const QRectF after = m_window->sceneView()->groundExtent();

    EXPECT_TRUE(containsWithinRounding(after, before))
      << "the round trip lost ground that had been on screen";

    // The orientation is the user's and is not the map's to change.
    EXPECT_DOUBLE_EQ(m_window->sceneView()->camera().elevation(), 35.0);
    EXPECT_DOUBLE_EQ(m_window->sceneView()->camera().azimuth(), 20.0);
  }

  TEST_F(ViewHandoffTest, SwitchingProjectionKeepsWhatIsOnScreen)
  {
    // C5d. The toggle changes how the same view is drawn, not where it looks
    // — a projection switch that reframed would read as a navigation command.
    // Straight down, for the same reason the round-trip test is: that is
    // where the two projections describe the same rectangle exactly.
    addTerrain();
    showTab(m_window->sceneView());

    SceneView *view = m_window->sceneView();

    Camera camera = view->camera();
    camera.setElevation(90.0);
    camera.setTarget(QVector3D(180.0f, 140.0f, 0.0f));
    view->setCamera(camera);

    ASSERT_EQ(view->projection(), CameraProjection::Perspective);

    const QRectF before = view->groundExtent();
    ASSERT_FALSE(before.isEmpty());

    view->setProjection(CameraProjection::Orthographic);

    EXPECT_EQ(view->projection(), CameraProjection::Orthographic);
    EXPECT_LT(relativeDifference(before, view->groundExtent()), 1.0e-3)
      << "switching to a parallel projection moved the view";

    view->setProjection(CameraProjection::Perspective);

    EXPECT_EQ(view->projection(), CameraProjection::Perspective);
    EXPECT_LT(relativeDifference(before, view->groundExtent()), 1.0e-3)
      << "switching back moved the view";
  }

  TEST_F(ViewHandoffTest, TheProjectionToggleIsReachableAndExclusive)
  {
    auto *perspective =
      m_window->findChild<QAction *>(QStringLiteral("perspectiveAction"));
    auto *orthographic =
      m_window->findChild<QAction *>(QStringLiteral("orthographicAction"));

    ASSERT_NE(perspective, nullptr);
    ASSERT_NE(orthographic, nullptr);

    EXPECT_TRUE(perspective->isChecked());

    orthographic->trigger();

    EXPECT_EQ(m_window->sceneView()->projection(),
              CameraProjection::Orthographic);
    EXPECT_FALSE(perspective->isChecked())
      << "both projections were checked at once, which the camera cannot be";

    perspective->trigger();

    EXPECT_EQ(m_window->sceneView()->projection(),
              CameraProjection::Perspective);
    EXPECT_FALSE(orthographic->isChecked());
  }

  TEST_F(ViewHandoffTest, TheExaggerationControlReachesTheCamera)
  {
    auto *spin =
      m_window->findChild<QDoubleSpinBox *>(QStringLiteral("exaggerationSpin"));
    ASSERT_NE(spin, nullptr);

    addTerrain();
    showTab(m_window->sceneView());

    spin->setValue(4.0);

    EXPECT_NEAR(m_window->sceneView()->verticalExaggeration(), 4.0, 1.0e-9)
      << "the spin box moved but the camera did not";
  }

  TEST_F(ViewHandoffTest, LaterGeometryDoesNotReframeAChosenView)
  {
    // The order that matters, and the reason the fixture does not load the
    // terrain itself. A user frames a corner of the model, switches to 3D,
    // and a second layer finishes loading a moment later — the view must
    // stay where it was put rather than jump out to show everything.
    addTerrain();

    const QRectF asked(150.0, 100.0, 80.0, 60.0);

    showTab(m_window->mapCanvas());
    m_window->mapCanvas()->setVisibleExtent(asked);
    showTab(m_window->sceneView());

    const QRectF handed = m_window->sceneView()->groundExtent();
    ASSERT_FALSE(handed.isEmpty());
    ASSERT_LT(handed.width(), 200.0)
      << "the hand-off did not narrow the view, so nothing here is being "
         "tested";

    addTerrain();

    EXPECT_LT(relativeDifference(handed,
                                 m_window->sceneView()->groundExtent()),
              1.0e-9)
      << "a layer arriving after the hand-off reframed the scene";
  }

  TEST_F(ViewHandoffTest, AnEmptyMapHandsOverNothingAndTheSceneFramesItself)
  {
    // Opening the 3D tab before loading anything. The map is showing its
    // default view around the origin, which is not a place anybody chose, so
    // handing it over would count as framing the scene deliberately — and the
    // model loaded a moment later would then never be framed at all.
    showTab(m_window->sceneView());

    const QRectF beforeAnything = m_window->sceneView()->groundExtent();

    addTerrain();

    const QRectF ground = m_window->sceneView()->groundExtent();
    ASSERT_FALSE(ground.isEmpty());

    EXPECT_GT(relativeDifference(beforeAnything, ground), 1.0)
      << "the scene never framed the model that arrived";

    // At the model's own scale, and near it — fitTo() frames the bounding
    // sphere and raises the target into the relief, so this is deliberately
    // not a containment check on the footprint.
    const QRectF data = m_window->mapCanvas()->fullExtent();

    ASSERT_FALSE(data.isEmpty());
    EXPECT_GT(ground.width(), data.width() * 0.5);
    EXPECT_LT(std::abs(ground.center().x() - data.center().x()),
              data.width());
  }

  TEST_F(ViewHandoffTest, ASceneWithNothingInItHandsOverNothing)
  {
    // The mirror of the empty-map case, and the one that bites in practice:
    // plenty of layers have no 3D form at all, so leaving the map tab and
    // coming back would otherwise replace whatever the user had framed with
    // the default camera's couple of units around the origin.
    addTerrain();
    showTab(m_window->mapCanvas());
    m_window->mapCanvas()->zoomToFullExtent();

    const QRectF framed =
      m_window->mapCanvas()->transform().visibleExtent();

    // Hide the only layer with 3D geometry: the scene now shows nothing.
    m_window->layerStack()->layerAt(0)->setVisible(false);
    QApplication::processEvents();

    ASSERT_TRUE(m_window->sceneView()->groundExtent().isEmpty())
      << "a view of nothing is reporting a view";

    showTab(m_window->sceneView());
    showTab(m_window->mapCanvas());

    EXPECT_LT(relativeDifference(
                framed, m_window->mapCanvas()->transform().visibleExtent()),
              1.0e-9)
      << "an empty 3D view moved the map";
  }

  TEST_F(ViewHandoffTest, FramingCommandsStillLeaveAirAroundTheData)
  {
    // The margin that used to live in setVisibleExtent moved out to the
    // commands that frame data, so that carrying a view between the tabs is
    // not charged for it. What must not have moved is the air itself:
    // "zoom to everything" with the data touching all four edges is worse
    // than the drift this fixed.
    addTerrain();
    showTab(m_window->mapCanvas());
    m_window->mapCanvas()->zoomToFullExtent();

    const QRectF data = m_window->mapCanvas()->fullExtent();
    const QRectF shown =
      m_window->mapCanvas()->transform().visibleExtent();

    ASSERT_FALSE(data.isEmpty());

    // On the tighter axis: fitting expands the other one to the viewport's
    // shape whatever the margin is, so measuring that axis would pass with
    // no margin at all.
    EXPECT_GT(std::min(shown.width() / data.width(),
                       shown.height() / data.height()),
              1.05)
      << "zooming to the data left it touching the frame";

    // And showing a rectangle still shows exactly that rectangle.
    m_window->mapCanvas()->setVisibleExtent(data);

    const QRectF exact =
      m_window->mapCanvas()->transform().visibleExtent();

    EXPECT_LT(std::abs(exact.height() - data.height()) / data.height(), 1.0e-6)
      << "showing a rectangle padded it";
  }

}

namespace
{
  //! Sends one mouse event of \a type at \a at to \a widget.
  void sendMouse(QWidget *widget, QEvent::Type type, const QPoint &at,
                 Qt::MouseButton button)
  {
    QMouseEvent event(type, QPointF(at), widget->mapToGlobal(QPointF(at)),
                      button,
                      type == QEvent::MouseMove ? Qt::LeftButton : button,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
  }

  void dragOn(QWidget *widget, const QPoint &from, const QPoint &to)
  {
    sendMouse(widget, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(widget, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(widget, QEvent::MouseButtonRelease, to, Qt::LeftButton);
  }
}

TEST_F(ViewHandoffTest, TheSceneStartsUnderOrbitAndDraggingTurnsIt)
{
  addTerrain();
  showTab(m_window->sceneView());

  SceneView *view = m_window->sceneView();

  EXPECT_EQ(view->toolKind(), SceneToolKind::Orbit)
    << "the scene opened under a gesture set nobody chose";

  const double azimuth = view->camera().azimuth();

  dragOn(view, QPoint(200, 150), QPoint(300, 150));

  EXPECT_NE(view->camera().azimuth(), azimuth)
    << "dragging under Orbit did not turn the scene";
}

TEST_F(ViewHandoffTest, ABandInTheSceneFramesTheGroundUnderIt)
{
  addTerrain();
  showTab(m_window->sceneView());

  SceneView *view = m_window->sceneView();

  // Straight down, where a screen rectangle covers a ground rectangle
  // exactly rather than a trapezoid.
  Camera camera = view->camera();
  camera.setElevation(90.0);
  view->setCamera(camera);

  view->setToolKind(SceneToolKind::ZoomIn);
  EXPECT_EQ(view->toolKind(), SceneToolKind::ZoomIn);

  const QRectF before = view->groundExtent();
  ASSERT_FALSE(before.isEmpty());

  const QRect band(view->width() / 4, view->height() / 4,
                   view->width() / 4, view->height() / 4);

  QRectF ground;
  ASSERT_TRUE(view->groundRectUnder(band, ground))
    << "the band's corners did not land on the terrain";

  dragOn(view, band.topLeft(), band.bottomRight());

  const QRectF after = view->groundExtent();

  EXPECT_LT(after.width(), before.width())
    << "framing a box did not move closer";

  // It framed what was dragged, not merely something smaller: the band's
  // own ground rectangle has to be inside what is now shown.
  EXPECT_LT(std::abs(after.center().x() - ground.center().x()),
            before.width() * 0.25)
    << "it zoomed somewhere other than the box that was dragged";
}

TEST_F(ViewHandoffTest, ABandUnderZoomOutWidensTheScene)
{
  addTerrain();
  showTab(m_window->sceneView());

  SceneView *view = m_window->sceneView();

  Camera camera = view->camera();
  camera.setElevation(90.0);
  view->setCamera(camera);

  view->setToolKind(SceneToolKind::ZoomOut);

  const QRectF before = view->groundExtent();
  ASSERT_FALSE(before.isEmpty());

  // A quarter of the viewport: everything on screen has to fit inside it,
  // so the view widens by about four. Asserted as about four rather than as
  // "wider", because a fixed step of two also widens and is exactly the
  // mistake this is here to catch.
  dragOn(view, QPoint(view->width() / 4, view->height() / 4),
         QPoint(view->width() / 2, view->height() / 2));

  const QRectF after = view->groundExtent();
  const double ratio = after.width() / before.width();

  EXPECT_GT(ratio, 3.0)
    << "the band widened by " << ratio << ", so its size was ignored";
  EXPECT_LT(ratio, 5.5) << "it widened by " << ratio;
}

TEST_F(ViewHandoffTest, ABandInTheSceneSelectsWhatItCovers)
{
  addTerrain();

  auto *network = new Testing::VectorProbe(QStringLiteral("network"));
  // One line, placed below once the band's ground rectangle has been
  // measured. Nothing else is added: a second feature put somewhere guessed
  // would end up under whichever band this test later claims is empty, and
  // a perspective camera makes where that is a matter of field of view
  // rather than of arithmetic anyone should do by hand.
  network->addLine({QPointF(-9000.0, -9000.0), QPointF(-9000.0, -8990.0)},
                   QStringLiteral("far away"));

  ASSERT_GE(m_window->layerStack()->addLayer(network), 0);

  showTab(m_window->sceneView());

  SceneView *view = m_window->sceneView();

  // Pointed at the network explicitly, through the same entry point the map
  // hands its extent over by. The automatic first framing does not run in
  // these offscreen tests, and a camera left on its default sits two world
  // units across, covering none of the network below.
  view->showGroundExtent(
    network->extent().adjusted(-60.0, -60.0, 60.0, 60.0));
  QApplication::processEvents();

  view->setToolKind(SceneToolKind::Select);

  // A band across the middle of the view, not out to its corners. A corner
  // ray of a wide, shallow viewport leaves the eye at better than fifty
  // degrees off axis and grazes the terrain rather than meeting it, so a
  // band dragged right to the edges is testing the marcher rather than the
  // tool. Noted rather than papered over — see the commit.
  const QRect band(view->width() / 4, view->height() / 4,
                   view->width() / 2, view->height() / 2);

  QRectF ground;
  ASSERT_TRUE(view->groundRectUnder(band, ground))
    << "the band's corners did not land";

  // The features go where the band actually looks, measured rather than
  // assumed. A perspective camera compresses ground toward the edges, so
  // how much of the world the middle half of the screen covers depends on
  // the field of view — and a fixture that guessed would be testing the
  // guess.
  network->addLine({ground.center() + QPointF(0.0, -ground.height() * 0.2),
                    ground.center() + QPointF(0.0, ground.height() * 0.2)},
                   QStringLiteral("centre"));

  // What the band covers, asked of the layer directly. The test is not that
  // these two agree — they share pickIn — but that dragging reaches it at
  // all, through the view, the stack and the layer, and that it takes more
  // than one.
  const QSet<int> expected = network->pickIn(ground);
  ASSERT_FALSE(expected.isEmpty())
    << "the band covers no part of the network, so it proves nothing";

  dragOn(view, band.topLeft(), band.bottomRight());

  EXPECT_EQ(network->selection(), expected)
    << "dragging a band in 3D did not reach the layer's selection";

  // And a band over ground with nothing on it clears the selection, exactly
  // as the map's does. The band is moved off the features and checked
  // against them first, so "empty" is a fact about this fixture rather than
  // an assumption about where the camera happens to be looking.
  network->setSelection({0});
  ASSERT_FALSE(network->selection().isEmpty());

  const QRect elsewhere = band.translated(0, -band.height());

  QRectF emptyGround;
  ASSERT_TRUE(view->groundRectUnder(elsewhere, emptyGround));
  ASSERT_TRUE(network->pickIn(emptyGround).isEmpty())
    << "the band moved off the features still covers some of them";

  view->selectIn(elsewhere);

  EXPECT_TRUE(network->selection().isEmpty())
    << "a band over empty ground left the previous selection standing";
}

TEST_F(ViewHandoffTest, SwitchingSceneToolsMidDragLeavesNoBandBehind)
{
  addTerrain();
  showTab(m_window->sceneView());

  SceneView *view = m_window->sceneView();
  view->setToolKind(SceneToolKind::ZoomIn);

  sendMouse(view, QEvent::MouseButtonPress, QPoint(60, 40), Qt::LeftButton);
  sendMouse(view, QEvent::MouseMove, QPoint(160, 120), Qt::LeftButton);

  auto *band =
    view->findChild<QRubberBand *>(QStringLiteral("sceneRubberBand"));
  ASSERT_NE(band, nullptr) << "no band, so the drag is invisible";
  ASSERT_TRUE(band->isVisibleTo(view));

  // To the *other* band tool, not to Orbit: a stale gesture released under
  // Orbit does nothing anyway, so switching there would hide whether the
  // gesture was abandoned or merely harmless.
  view->setToolKind(SceneToolKind::ZoomOut);

  EXPECT_FALSE(band->isVisibleTo(view))
    << "a band belonging to a tool that is no longer active is still on "
       "screen, with nothing left to finish it";

  // And the abandoned gesture does not finish when the button comes up: a
  // release that still acted would zoom by a box the user had already left
  // behind, under a tool they had just left.
  const QRectF before = view->groundExtent();

  sendMouse(view, QEvent::MouseButtonRelease, QPoint(160, 120),
            Qt::LeftButton);

  EXPECT_EQ(view->groundExtent(), before)
    << "the abandoned band still acted on release";
}

TEST_F(ViewHandoffTest, TheSceneToolButtonsReachTheView)
{
  const struct
  {
      const char *name;
      SceneToolKind kind;
  } tools[] = {
    {"sceneSelectToolAction", SceneToolKind::Select},
    {"sceneZoomInToolAction", SceneToolKind::ZoomIn},
    {"sceneZoomOutToolAction", SceneToolKind::ZoomOut},
    {"orbitToolAction", SceneToolKind::Orbit},
  };

  for (const auto &tool : tools)
  {
    auto *action =
      m_window->findChild<QAction *>(QLatin1String(tool.name));

    ASSERT_NE(action, nullptr) << tool.name;

    action->trigger();

    EXPECT_EQ(m_window->sceneView()->toolKind(), tool.kind) << tool.name;
  }

  // Exclusive, so the UI cannot show a state the view cannot be in.
  auto *orbit = m_window->findChild<QAction *>(QStringLiteral("orbitToolAction"));
  auto *select =
    m_window->findChild<QAction *>(QStringLiteral("sceneSelectToolAction"));

  ASSERT_NE(orbit, nullptr);
  ASSERT_NE(select, nullptr);
  EXPECT_TRUE(orbit->isChecked());
  EXPECT_FALSE(select->isChecked());
}

// ── The zoom shortcuts (V1) ───────────────────────────────────────────────
//
// Ctrl+= and Ctrl+- used to target the map unconditionally, so pressing them
// on the 3D tab zoomed a hidden canvas: the screen did not move, and the map
// silently lost the framing it had. The shortcut acts on the view in front,
// the way Zoom to Full Extent always has.

TEST_F(ViewHandoffTest, ZoomInOnTheSceneTabMovesTheCameraNotTheMap)
{
  addTerrain();

  showTab(m_window->sceneView());

  const QRectF mapBefore = m_window->mapCanvas()->transform().visibleExtent();
  const double distanceBefore = m_window->sceneView()->camera().distance();

  auto *zoomIn =
    m_window->findChild<QAction *>(QStringLiteral("zoomInAction"));
  ASSERT_NE(zoomIn, nullptr);
  zoomIn->trigger();

  EXPECT_LT(m_window->sceneView()->camera().distance(), distanceBefore)
    << "the shortcut did not reach the scene camera";
  EXPECT_EQ(m_window->mapCanvas()->transform().visibleExtent(), mapBefore)
    << "the hidden map was zoomed";
}

TEST_F(ViewHandoffTest, ZoomOutOnTheSceneTabBacksTheCameraAway)
{
  addTerrain();

  showTab(m_window->sceneView());

  const double distanceBefore = m_window->sceneView()->camera().distance();

  auto *zoomOut =
    m_window->findChild<QAction *>(QStringLiteral("zoomOutAction"));
  ASSERT_NE(zoomOut, nullptr);
  zoomOut->trigger();

  EXPECT_GT(m_window->sceneView()->camera().distance(), distanceBefore);
}

TEST_F(ViewHandoffTest, ZoomShortcutsStillDriveTheMapWhenItIsInFront)
{
  addTerrain();

  showTab(m_window->mapCanvas());

  const QRectF before = m_window->mapCanvas()->transform().visibleExtent();
  const double distanceBefore = m_window->sceneView()->camera().distance();

  auto *zoomIn =
    m_window->findChild<QAction *>(QStringLiteral("zoomInAction"));
  ASSERT_NE(zoomIn, nullptr);
  zoomIn->trigger();

  EXPECT_LT(m_window->mapCanvas()->transform().visibleExtent().width(),
            before.width());
  EXPECT_EQ(m_window->sceneView()->camera().distance(), distanceBefore)
    << "the hidden scene camera moved";
}

// A zoom that lost the target would be a pan wearing a zoom's name.
TEST_F(ViewHandoffTest, SceneZoomHoldsWhatIsBeingLookedAt)
{
  addTerrain();

  showTab(m_window->sceneView());

  const QVector3D targetBefore = m_window->sceneView()->camera().target();

  auto *zoomIn =
    m_window->findChild<QAction *>(QStringLiteral("zoomInAction"));
  ASSERT_NE(zoomIn, nullptr);
  zoomIn->trigger();
  zoomIn->trigger();

  EXPECT_EQ(m_window->sceneView()->camera().target(), targetBefore);
}
