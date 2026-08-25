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

#include <gtest/gtest.h>

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
