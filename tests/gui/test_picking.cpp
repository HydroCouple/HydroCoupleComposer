/*!
 * \file   test_picking.cpp
 * \brief  C4 — identifying what is under a click.
 *
 * The plan asks for "pick tests at known coordinates return the seeded
 * features", and the coordinates here are chosen so that every case has a
 * right answer that can be worked out by hand: a click on a vertex, a click a
 * known distance off a line, a click inside a ring, a click between two
 * features and nearer to one of them.
 *
 * The cases that matter most are the ones where the *index* can be wrong
 * rather than the hit test: a centroid says only roughly where a feature is,
 * so on a graded layer the feature under the point is routinely not the
 * nearest centroid's.
 */

#include "core/preferencesmanager.h"
#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "settingsredirect.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "pick/terrainray.h"
#include "scene/camera.h"
#include "scene/sceneview.h"
#include "vectorprobe.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
using HydroCouple::Composer::Testing::VectorProbe;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  //! A 4x3 grid of unit quads over [0,4]x[0,3].
  MeshDefinition grid()
  {
    MeshDefinition mesh;
    mesh.meshName = "grid";

    for (int row = 0; row <= 3; ++row)
    {
      for (int column = 0; column <= 4; ++column)
      {
        mesh.nodeX.push_back(double(column));
        mesh.nodeY.push_back(double(row));
      }
    }

    mesh.faceNodeOffsets.push_back(0);

    for (int row = 0; row < 3; ++row)
    {
      for (int column = 0; column < 4; ++column)
      {
        const int64_t corner = int64_t(row) * 5 + column;

        mesh.faceNodes.push_back(corner);
        mesh.faceNodes.push_back(corner + 1);
        mesh.faceNodes.push_back(corner + 6);
        mesh.faceNodes.push_back(corner + 5);
        mesh.faceNodeOffsets.push_back(int64_t(mesh.faceNodes.size()));
      }
    }

    return mesh;
  }

  int countMatching(const QImage &image, const QColor &color, int tolerance)
  {
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);

    int matches = 0;

    for (int y = 0; y < rgb.height(); ++y)
    {
      for (int x = 0; x < rgb.width(); ++x)
      {
        const QRgb pixel = rgb.pixel(x, y);

        if (std::abs(qRed(pixel) - color.red()) <= tolerance &&
            std::abs(qGreen(pixel) - color.green()) <= tolerance &&
            std::abs(qBlue(pixel) - color.blue()) <= tolerance)
        {
          ++matches;
        }
      }
    }

    return matches;
  }

  class PickTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!QApplication::instance())
        {
          // Picking reads the application-wide preferences, which a test
          // must not write into the developer's own configuration.
          HydroCouple::Composer::Testing::redirectSettingsTo(
            QStringLiteral(COMPOSER_PREFERENCES_FIXTURE_DIR)
            + QStringLiteral("/test_picking"));

          static int argc = 1;
          static char name[] = "test_picking";
          static char *argv[] = { name, nullptr };
          s_app = new QApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static QApplication *s_app;
  };

  QApplication *PickTest::s_app = nullptr;

  //! The same cases, run at more than one vertical exaggeration.
  class ExaggeratedPickTest : public PickTest,
                              public ::testing::WithParamInterface<double>
  {
  };

  // ── the hit test ────────────────────────────────────────────────────────

  TEST_F(PickTest, PicksAPointAtItsPosition)
  {
    VectorProbe probe(QStringLiteral("nodes"));
    probe.addPoint({ 10.0, 20.0 }, QStringLiteral("J1"));
    probe.addPoint({ 50.0, 20.0 }, QStringLiteral("J2"));

    EXPECT_EQ(probe.pickAt(QPointF(10.0, 20.0), 1.0), 0);
    EXPECT_EQ(probe.pickAt(QPointF(50.4, 20.3), 1.0), 1);
    EXPECT_EQ(probe.pickAt(QPointF(30.0, 20.0), 1.0), -1)
      << "a click between two points is on neither";
  }

  TEST_F(PickTest, PicksALineWithinToleranceAndNotBeyondIt)
  {
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 0.0, 0.0 }, { 100.0, 0.0 } });

    // A line has no inside, so the tolerance is the whole hit test.
    EXPECT_EQ(probe.pickAt(QPointF(50.0, 0.9), 1.0), 0);
    EXPECT_EQ(probe.pickAt(QPointF(50.0, 1.1), 1.0), -1);

    // And it ends where it ends: beyond the last vertex the distance is to
    // that vertex, not to the infinite line through it.
    EXPECT_EQ(probe.pickAt(QPointF(100.5, 0.0), 1.0), 0);
    EXPECT_EQ(probe.pickAt(QPointF(101.5, 0.0), 1.0), -1);
  }

  TEST_F(PickTest, PicksAPolygonFromInsideOrFromItsEdge)
  {
    VectorProbe probe(QStringLiteral("catchments"));
    probe.addRing({ { 0.0, 0.0 },
                    { 100.0, 0.0 },
                    { 100.0, 100.0 },
                    { 0.0, 100.0 },
                    { 0.0, 0.0 } });

    EXPECT_EQ(probe.pickAt(QPointF(50.0, 50.0), 1.0), 0) << "inside";
    EXPECT_EQ(probe.pickAt(QPointF(-0.5, 50.0), 1.0), 0) << "just outside";
    EXPECT_EQ(probe.pickAt(QPointF(-5.0, 50.0), 1.0), -1) << "well outside";
  }

  TEST_F(PickTest, PicksTheNearerOfTwoFeatures)
  {
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 0.0, 0.0 }, { 100.0, 0.0 } });
    probe.addLine({ { 0.0, 10.0 }, { 100.0, 10.0 } });

    // Both are within tolerance; the answer is the one clicked nearer to.
    EXPECT_EQ(probe.pickAt(QPointF(50.0, 4.0), 20.0), 0);
    EXPECT_EQ(probe.pickAt(QPointF(50.0, 6.0), 20.0), 1);
  }

  TEST_F(PickTest, TheWidenedSearchStillAnswersWithTheNearest)
  {
    // Nearest-first is easy to get right in the fast path and easy to lose in
    // the widened one, where the candidates come back in whatever order the
    // tree walked them. Reaching that path takes a decoy: a feature whose
    // *centroid* is nearest and whose geometry is nowhere near.
    VectorProbe probe(QStringLiteral("conduits"));

    // A crowd, all comfortably within tolerance, added first so that tree
    // order and distance order are not the same order.
    for (int i = 0; i < 10; ++i)
    {
      const double y = 60.0 + double(i) * 2.0;
      probe.addLine({ { 40.0, y }, { 60.0, y } });
    }

    // Three sides of a large square: its bounding box is centred exactly on
    // the click and every part of it is fifty units away.
    probe.addLine({ { 0.0, 0.0 }, { 100.0, 0.0 }, { 100.0, 100.0 },
                    { 0.0, 100.0 } });

    // And the right answer, nearest of everything that is actually close.
    probe.addLine({ { 40.0, 44.0 }, { 60.0, 44.0 } });

    EXPECT_EQ(probe.pickAt(QPointF(50.0, 50.0), 30.0), 11)
      << "the widened search did not return the nearest match";
  }

  TEST_F(PickTest, FindsAFeatureWhenCentroidsMislead)
  {
    // One large ring beside a row of small ones. Near their shared boundary
    // the nearest centroid belongs to a small feature that does not contain
    // the point, and answering from that alone picks the wrong one or none.
    VectorProbe probe(QStringLiteral("catchments"));
    probe.addRing({ { 0.0, 0.0 },
                    { 100.0, 0.0 },
                    { 100.0, 100.0 },
                    { 0.0, 100.0 },
                    { 0.0, 0.0 } },
                  QStringLiteral("big"));

    for (int row = 0; row < 10; ++row)
    {
      const double y = double(row) * 10.0;

      probe.addRing({ { 100.0, y },
                      { 110.0, y },
                      { 110.0, y + 10.0 },
                      { 100.0, y + 10.0 },
                      { 100.0, y } },
                    QStringLiteral("small%1").arg(row));
    }

    // Deep inside the large ring by area, but seven units from a small
    // centroid and forty-nine from its own.
    EXPECT_EQ(probe.pickAt(QPointF(99.0, 50.0), 0.5), 0)
      << "the containing feature was not found behind the nearer centroids";

    // And the small ones still answer for themselves.
    EXPECT_EQ(probe.pickAt(QPointF(105.0, 55.0), 0.5), 6);
  }

  TEST_F(PickTest, PicksNothingWhereThereIsNothing)
  {
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });

    EXPECT_EQ(probe.pickAt(QPointF(500.0, 500.0), 1.0), -1);
  }

  TEST_F(PickTest, AnEmptyLayerPicksNothing)
  {
    const VectorProbe probe(QStringLiteral("empty"));

    EXPECT_EQ(probe.pickAt(QPointF(0.0, 0.0), 1.0), -1);
  }

  TEST_F(PickTest, AFeatureAddedAfterAPickIsStillFound)
  {
    // The index is built on demand and cached, so a layer that is picked
    // while it is still loading must not answer from a stale one.
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });

    ASSERT_EQ(probe.pickAt(QPointF(5.0, 0.0), 1.0), 0);

    probe.addLine({ { 0.0, 50.0 }, { 10.0, 50.0 } });

    EXPECT_EQ(probe.pickAt(QPointF(5.0, 50.0), 1.0), 1);
  }

  TEST_F(PickTest, AMeshCellIsPickedByItsFace)
  {
    QString message;
    const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
      QStringLiteral("grid"), grid(), MeshEntity::Face, message);
    ASSERT_TRUE(layer) << message.toStdString();

    // A mesh is a feature layer whose features are its cells, so picking one
    // needs no separate path — cell (2, 1) is the seventh face.
    EXPECT_EQ(layer->pickAt(QPointF(2.5, 1.5), 0.1), 6);
    EXPECT_EQ(layer->pickAt(QPointF(0.5, 0.5), 0.1), 0);
    EXPECT_EQ(layer->pickAt(QPointF(3.5, 2.5), 0.1), 11);
    EXPECT_EQ(layer->pickAt(QPointF(9.0, 9.0), 0.1), -1);
  }

  // ── the selection ───────────────────────────────────────────────────────

  TEST_F(PickTest, SelectingReplacesWhatWasSelected)
  {
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });
    probe.addLine({ { 0.0, 5.0 }, { 10.0, 5.0 } });

    probe.setSelection({ 0 });
    EXPECT_EQ(probe.selection(), QSet<int>({ 0 }));

    probe.setSelection({ 1 });
    EXPECT_EQ(probe.selection(), QSet<int>({ 1 }));

    probe.clearSelection();
    EXPECT_TRUE(probe.selection().isEmpty());
  }

  TEST_F(PickTest, AFeatureThatDoesNotExistIsNotSelected)
  {
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });

    // -1 is what a miss returns, and it must not become a selection of the
    // last feature or of anything else.
    probe.setSelection({ -1, 7 });

    EXPECT_TRUE(probe.selection().isEmpty());
  }

  TEST_F(PickTest, SelectingAnnouncesThatTheLayerMustBeRedrawn)
  {
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });

    QSignalSpy spy(&probe, &MapLayer::appearanceChanged);

    probe.setSelection({ 0 });
    EXPECT_EQ(spy.count(), 1);

    // Selecting what is already selected is not a change.
    probe.setSelection({ 0 });
    EXPECT_EQ(spy.count(), 1);
  }

  TEST_F(PickTest, ASelectedFeatureIsDrawnHighlighted)
  {
    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 10.0, 50.0 }, { 90.0, 50.0 } });

    QImage image(200, 200, QImage::Format_ARGB32);
    image.fill(Qt::white);

    const MapTransform transform(QRectF(0.0, 0.0, 100.0, 100.0),
                                 QSizeF(image.size()));

    {
      QPainter painter(&image);
      probe.render(painter, transform);
    }

    ASSERT_EQ(countMatching(image, QColor(0, 200, 255), 20), 0)
      << "nothing is selected, so nothing should be highlighted";

    probe.setSelection({ 0 });
    image.fill(Qt::white);

    {
      QPainter painter(&image);
      probe.render(painter, transform);
    }

    EXPECT_GT(countMatching(image, QColor(0, 200, 255), 20), 100)
      << "the selected feature is not visibly selected";
  }

  TEST_F(PickTest, TheSelectionColourIsAPreferenceReadAsTheLayerPaints)
  {
    PreferencesManager *prefs = PreferencesManager::instance();
    prefs->resetToDefaults();

    VectorProbe probe(QStringLiteral("conduits"));
    probe.addLine({ { 10.0, 50.0 }, { 90.0, 50.0 } });
    probe.setSelection({ 0 });

    QImage image(200, 200, QImage::Format_ARGB32);
    const MapTransform transform(QRectF(0.0, 0.0, 100.0, 100.0),
                                 QSizeF(image.size()));

    prefs->setSelectionColor(QColor(255, 0, 0));
    image.fill(Qt::white);

    {
      QPainter painter(&image);
      probe.render(painter, transform);
    }

    // Read as it paints, not captured when the layer was made: the same
    // probe, with nothing but the preference changed between two renders.
    EXPECT_GT(countMatching(image, QColor(255, 0, 0), 20), 100)
      << "the selection is not drawn in the preferred colour";
    EXPECT_EQ(countMatching(image, QColor(0, 200, 255), 20), 0)
      << "the old default colour is still being used";

    prefs->resetToDefaults();
  }

  // ── through the canvas ──────────────────────────────────────────────────

  TEST_F(PickTest, ThePickToleranceIsAPreferenceReadOnEveryClick)
  {
    PreferencesManager *prefs = PreferencesManager::instance();
    prefs->resetToDefaults();

    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(probe), 0);

    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(&stack);
    canvas.setVisibleExtent(QRectF(0.0, 0.0, 100.0, 100.0));
    QApplication::processEvents();

    // Four pixels a unit, so thirty pixels below the line is well outside
    // the six the default allows and inside a widened forty.
    int feature = -1;
    EXPECT_EQ(canvas.pickAt(QPoint(200, 230), feature), nullptr)
      << "a click thirty pixels off a line hit it at the default tolerance";

    prefs->setPickTolerancePixels(40.0);
    EXPECT_EQ(canvas.pickAt(QPoint(200, 230), feature), probe)
      << "widening the tolerance did not reach the next pick";
    EXPECT_EQ(feature, 0);

    prefs->resetToDefaults();
    EXPECT_EQ(canvas.pickAt(QPoint(200, 230), feature), nullptr)
      << "narrowing it back did not either";
  }

  TEST_F(PickTest, TheCanvasPicksTheTopmostLayer)
  {
    LayerStackModel stack;

    auto *lower = new VectorProbe(QStringLiteral("lower"));
    lower->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(lower), 0);

    auto *upper = new VectorProbe(QStringLiteral("upper"));
    upper->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(upper), 0);

    // The newest goes on top, and the top is what the user can see.
    ASSERT_EQ(stack.layerAt(0), upper);

    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(&stack);
    canvas.setVisibleExtent(QRectF(0.0, 0.0, 100.0, 100.0));
    QApplication::processEvents();

    int feature = -1;

    EXPECT_EQ(canvas.pickAt(QPoint(200, 200), feature), upper);
    EXPECT_EQ(feature, 0);

    upper->setVisible(false);

    EXPECT_EQ(canvas.pickAt(QPoint(200, 200), feature), lower)
      << "a hidden layer is not on screen, so it cannot be clicked";
  }

  TEST_F(PickTest, AClickSelectsAndADragDoesNot)
  {
    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(probe), 0);

    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(&stack);
    canvas.setVisibleExtent(QRectF(0.0, 0.0, 100.0, 100.0));
    QApplication::processEvents();

    QSignalSpy spy(&canvas, &MapCanvas::featurePicked);

    QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(200, 200));

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(probe->selection(), QSet<int>({ 0 }));

    probe->clearSelection();

    // A drag that happens to start on a feature is a pan, not a click.
    QTest::mousePress(&canvas, Qt::LeftButton, {}, QPoint(200, 200));
    QTest::mouseMove(&canvas, QPoint(260, 240));
    QTest::mouseRelease(&canvas, Qt::LeftButton, {}, QPoint(260, 240));

    EXPECT_EQ(spy.count(), 1) << "panning the map selected something";
    EXPECT_TRUE(probe->selection().isEmpty());
  }

  TEST_F(PickTest, ClickingEmptyMapClearsTheSelection)
  {
    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(probe), 0);

    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(&stack);
    canvas.setVisibleExtent(QRectF(0.0, 0.0, 100.0, 100.0));
    QApplication::processEvents();

    QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(200, 200));
    ASSERT_FALSE(probe->selection().isEmpty());

    QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(200, 40));

    EXPECT_TRUE(probe->selection().isEmpty())
      << "a selection left standing describes somewhere the user has "
         "navigated away from";
  }

  TEST_F(PickTest, SelectingInOneLayerDeselectsTheOthers)
  {
    LayerStackModel stack;

    auto *first = new VectorProbe(QStringLiteral("first"));
    first->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(first), 0);

    auto *second = new VectorProbe(QStringLiteral("second"));
    second->addLine({ { 0.0, 20.0 }, { 100.0, 20.0 } });
    ASSERT_GE(stack.addLayer(second), 0);

    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(&stack);
    canvas.setVisibleExtent(QRectF(0.0, 0.0, 100.0, 100.0));
    QApplication::processEvents();

    QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(200, 320));
    ASSERT_EQ(second->selection(), QSet<int>({ 0 }));

    QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(200, 200));

    EXPECT_EQ(first->selection(), QSet<int>({ 0 }));
    EXPECT_TRUE(second->selection().isEmpty())
      << "one selection, held across the stack, or the table beside it has "
         "to choose which layer it is describing";
  }

  // ── through the 3D view ─────────────────────────────────────────────────

  namespace
  {
    //! A ridge along y, crested at x = 50 and 40 high, over [0,100]^2.
    MeshDefinition ridge()
    {
      MeshDefinition mesh;
      mesh.meshName = "ridge";
      mesh.nodeX = { 0.0, 50.0, 100.0, 0.0, 50.0, 100.0 };
      mesh.nodeY = { 0.0, 0.0, 0.0, 100.0, 100.0, 100.0 };
      mesh.nodeZ = { 0.0, 40.0, 0.0, 0.0, 40.0, 0.0 };
      mesh.faceNodeOffsets = { 0, 4, 8 };
      mesh.faceNodes = { 0, 1, 4, 3, 1, 2, 5, 4 };

      return mesh;
    }

    //! Where a ray meets the z = 0 plane.
    QPointF flatGroundHit(const QVector3D &origin, const QVector3D &direction)
    {
      const double travel = -double(origin.z()) / double(direction.z());
      const QVector3D at = origin + direction * float(travel);

      return QPointF(at.x(), at.y());
    }
  }

  TEST_F(PickTest, TheSceneAndTheMapAgreeAboutWhatIsWhere)
  {
    // The plan's own check, and the reason the scene turns its ray into a
    // place on the ground rather than intersecting its own triangles: at the
    // same world coordinate the two views must name the same feature, or the
    // table beside them has to choose which one to believe.
    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 10.0, 20.0 }, { 90.0, 20.0 } }, QStringLiteral("A"));
    probe->addLine({ { 10.0, 80.0 }, { 90.0, 80.0 } }, QStringLiteral("B"));
    ASSERT_GE(stack.addLayer(probe), 0);

    const QRectF extent(0.0, 0.0, 100.0, 100.0);

    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(&stack);
    canvas.setVisibleExtent(extent);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, 1.0);
    camera.setElevation(90.0);
    camera.setGroundExtent(extent, 1.0);
    camera.setDistance(1000.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    // Both features, and a miss, addressed by world coordinate and converted
    // into each view's own pixels.
    for (const auto &probePoint : { std::make_pair(QPointF(50.0, 20.0), 0),
                                    std::make_pair(QPointF(50.0, 80.0), 1),
                                    std::make_pair(QPointF(50.0, 50.0), -1) })
    {
      const QPointF world = probePoint.first;
      const int expected = probePoint.second;

      const QPoint mapPixel =
        canvas.transform().toScreen(world).toPoint();

      int mapFeature = -1;
      FeatureLayer *mapLayer = canvas.pickAt(mapPixel, mapFeature);

      // The scene's own pixel for the same ground, found by asking it where
      // its pixels land rather than by assuming the two agree.
      QPoint scenePixel;
      bool found = false;

      for (int y = 0; y < scene.height() && !found; ++y)
      {
        for (int x = 0; x < scene.width() && !found; ++x)
        {
          QPointF ground;

          if (scene.groundUnder(QPoint(x, y), ground) &&
              std::hypot(ground.x() - world.x(), ground.y() - world.y()) < 0.4)
          {
            scenePixel = QPoint(x, y);
            found = true;
          }
        }
      }

      ASSERT_TRUE(found) << "no pixel of the scene lands on ("
                         << world.x() << ", " << world.y() << ")";

      int sceneFeature = -1;
      FeatureLayer *sceneLayer = scene.pickAt(scenePixel, sceneFeature);

      EXPECT_EQ(mapFeature, expected) << "the map disagrees with the fixture";
      EXPECT_EQ(sceneFeature, mapFeature)
        << "the two views name different features at (" << world.x() << ", "
        << world.y() << ")";
      EXPECT_EQ(sceneLayer, mapLayer);
    }
  }

  TEST_P(ExaggeratedPickTest, ARayLandsOnTheTerrainAndNotThroughIt)
  {
    // What separates picking in a 3D view from picking on a flat plane. Seen
    // across it, the near face of a ridge stands between the camera and the
    // ground behind it, and a pick that ignored the relief would name
    // whatever lies on that far ground instead.
    //
    // The pixels are found rather than chosen: which one looks at the ridge
    // depends on the camera, and a hand-picked pixel that happened to land at
    // the ridge's foot would agree with the flat answer and prove nothing.
    LayerStackModel stack;

    QString message;
    auto *terrain = MeshLayer::create(QStringLiteral("ridge"), ridge(),
                                      MeshEntity::Face, message)
                      .release();
    ASSERT_TRUE(terrain) << message.toStdString();
    ASSERT_GE(stack.addLayer(terrain), 0);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setElevation(25.0);

    // Across the ridge, not along it: looking down its length, every ray
    // meets it at the same height and the case cannot arise.
    camera.setAzimuth(90.0);
    camera.setTarget(QVector3D(50.0f, 50.0f, 20.0f));
    camera.setDistance(300.0);

    // With the relief stretched as well as flat. Exaggeration is a display
    // property, so the ray has to be built with the model matrix undone —
    // and a ray that kept it would sit at a fraction of its true height and
    // meet the terrain somewhere else entirely. Nothing about a *ground
    // plane* can show that: scaling z leaves where a ray crosses z = 0
    // exactly where it was.
    camera.setVerticalExaggeration(GetParam());
    scene.setCamera(camera);

    QApplication::processEvents();

    int onRelief = 0;

    for (int y = 40; y < 360; y += 20)
    {
      for (int x = 40; x < 360; x += 20)
      {
        const QPoint pixel(x, y);

        QPointF ground;

        if (!scene.groundUnder(pixel, ground))
        {
          continue;
        }

        double surface = 0.0;

        if (!terrain->elevationAt(ground, surface) || surface < 5.0)
        {
          continue;
        }

        ++onRelief;

        QVector3D origin;
        QVector3D direction;
        ASSERT_TRUE(camera.rayThrough(QPointF(pixel), scene.size(), origin,
                                      direction));

        // The point that came back is on the terrain: the ray's height where
        // it passes over that ground is the ground's own height.
        const QVector3D toGround(float(ground.x()) - origin.x(),
                                 float(ground.y()) - origin.y(), 0.0f);
        const QVector3D flatDirection(direction.x(), direction.y(), 0.0f);
        const double travel =
          double(toGround.length()) / double(flatDirection.length());
        const QVector3D at = origin + direction * float(travel);

        // Tight enough that the march's own step cannot reach it: the
        // crossing has to be bisected, not merely stepped past.
        EXPECT_NEAR(double(at.z()), surface, 0.2)
          << "the ray did not stop at the surface, at pixel (" << x << ", "
          << y << ")";

        // And it is not where the ground plane would have answered.
        const QPointF flat = flatGroundHit(origin, direction);

        EXPECT_GT(std::hypot(flat.x() - ground.x(), flat.y() - ground.y()),
                  1.0)
          << "the terrain and the ground plane answer alike at pixel (" << x
          << ", " << y << "), so relief is not being used";
      }
    }

    EXPECT_GT(onRelief, 10)
      << "no ray in this view landed on the raised part of the ridge, so "
         "nothing above was actually checked";
  }

  INSTANTIATE_TEST_SUITE_P(AtEveryExaggeration, ExaggeratedPickTest,
                           ::testing::Values(1.0, 4.0));

  TEST_F(PickTest, ARayDoesNotStepOverANarrowRidge)
  {
    // A terrain whose mean cell is enormous and whose relief is not: two
    // plains either side of a ten-unit ridge two hundred high. The march's
    // step comes from the terrain's own spacing, which here is hundreds of
    // units — so a step sized on that alone walks straight over the ridge and
    // lands on the plain beyond, and the click selects the wrong side of a
    // hill it is pointing directly at.
    MeshDefinition mesh;
    mesh.meshName = "narrow";
    mesh.nodeX = { 0.0,   0.0,   400.0, 400.0, 405.0, 405.0,
                   410.0, 410.0, 1000.0, 1000.0 };
    mesh.nodeY = { 0.0, 1000.0, 0.0, 1000.0, 0.0, 1000.0,
                   0.0, 1000.0, 0.0, 1000.0 };
    mesh.nodeZ = { 0.0, 0.0, 0.0, 0.0, 200.0, 200.0, 0.0, 0.0, 0.0, 0.0 };
    mesh.faceNodeOffsets = { 0, 4, 8, 12, 16 };
    mesh.faceNodes = { 0, 2, 3, 1, 2, 4, 5, 3, 4, 6, 7, 5, 6, 8, 9, 7 };

    LayerStackModel stack;

    QString message;
    auto *terrain = MeshLayer::create(QStringLiteral("narrow"), mesh,
                                      MeshEntity::Face, message)
                      .release();
    ASSERT_TRUE(terrain) << message.toStdString();
    ASSERT_GE(stack.addLayer(terrain), 0);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setElevation(20.0);
    camera.setAzimuth(90.0);
    camera.setTarget(QVector3D(405.0f, 500.0f, 200.0f));
    camera.setDistance(600.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    // Somewhere in this view the ridge is between the eye and the plain. It
    // is a narrow target, so the pixels are found rather than assumed.
    int onRidge = 0;

    for (int y = 20; y < 380; y += 4)
    {
      for (int x = 20; x < 380; x += 4)
      {
        QPointF ground;

        if (!scene.groundUnder(QPoint(x, y), ground))
        {
          continue;
        }

        double surface = 0.0;

        if (terrain->elevationAt(ground, surface) && surface > 100.0)
        {
          ++onRidge;
        }
      }
    }

    EXPECT_GT(onRidge, 20)
      << "every ray stepped over the ridge and landed on the plain behind it";
  }

  TEST_F(PickTest, AnEyeUnderTheTerrainSeesNoGround)
  {
    // Under the surface looking up, what is in front of the camera is the
    // underside of the terrain and then the sky. Answering with the ground
    // anyway would let a click select whatever happened to be overhead.
    LayerStackModel stack;

    QString message;
    auto *terrain = MeshLayer::create(QStringLiteral("ridge"), ridge(),
                                      MeshEntity::Face, message)
                      .release();
    ASSERT_TRUE(terrain) << message.toStdString();
    ASSERT_GE(stack.addLayer(terrain), 0);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setElevation(-40.0);
    camera.setAzimuth(90.0);
    camera.setTarget(QVector3D(50.0f, 50.0f, 38.0f));
    camera.setDistance(10.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    QPointF ground;

    EXPECT_FALSE(scene.groundUnder(QPoint(200, 200), ground))
      << "a camera inside the hill was told what is under the cursor";
  }

  TEST_F(PickTest, TheSceneAlsoPicksTheTopmostLayer)
  {
    LayerStackModel stack;

    auto *lower = new VectorProbe(QStringLiteral("lower"));
    lower->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(lower), 0);

    auto *upper = new VectorProbe(QStringLiteral("upper"));
    upper->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(upper), 0);

    ASSERT_EQ(stack.layerAt(0), upper);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, 1.0);
    camera.setElevation(90.0);
    camera.setGroundExtent(QRectF(0.0, 0.0, 100.0, 100.0), 1.0);
    camera.setDistance(1000.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    int feature = -1;

    EXPECT_EQ(scene.pickAt(QPoint(200, 200), feature), upper);
    EXPECT_EQ(feature, 0);

    upper->setVisible(false);

    EXPECT_EQ(scene.pickAt(QPoint(200, 200), feature), lower)
      << "a hidden layer is not in the scene, so it cannot be clicked";
  }

  TEST_F(PickTest, WithoutTerrainARayMeetsTheGroundPlane)
  {
    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 10.0, 50.0 }, { 90.0, 50.0 } });
    ASSERT_GE(stack.addLayer(probe), 0);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, 1.0);
    camera.setElevation(90.0);
    camera.setGroundExtent(QRectF(0.0, 0.0, 100.0, 100.0), 1.0);
    camera.setDistance(1000.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    QPointF ground;

    ASSERT_TRUE(scene.groundUnder(QPoint(200, 200), ground));
    EXPECT_NEAR(ground.x(), 50.0, 0.5);
    EXPECT_NEAR(ground.y(), 50.0, 0.5);
  }

  TEST_F(PickTest, AClickInTheSceneSelectsAndAnOrbitDoesNot)
  {
    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(probe), 0);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, 1.0);
    camera.setElevation(90.0);
    camera.setGroundExtent(QRectF(0.0, 0.0, 100.0, 100.0), 1.0);
    camera.setDistance(1000.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    QSignalSpy spy(&scene, &SceneView::featurePicked);

    QTest::mouseClick(&scene, Qt::LeftButton, {}, QPoint(200, 200));

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(probe->selection(), QSet<int>({ 0 }))
      << "clicking the 3D view did not select what it was pointing at";

    probe->clearSelection();

    // A drag turns the scene; it does not select whatever it started on.
    QTest::mousePress(&scene, Qt::LeftButton, {}, QPoint(200, 200));
    QTest::mouseMove(&scene, QPoint(260, 240));
    QTest::mouseRelease(&scene, Qt::LeftButton, {}, QPoint(260, 240));

    EXPECT_EQ(spy.count(), 1) << "orbiting the scene selected something";
    EXPECT_TRUE(probe->selection().isEmpty());
  }

  TEST_F(PickTest, TheSceneAndTheMapShareOneSelection)
  {
    // Selection lives on the layer, so this is not a synchronisation to get
    // right — it is the absence of one. The test is here because that is
    // exactly the kind of claim that stops being true quietly.
    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    probe->addLine({ { 0.0, 20.0 }, { 100.0, 20.0 } });
    ASSERT_GE(stack.addLayer(probe), 0);

    const QRectF extent(0.0, 0.0, 100.0, 100.0);

    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(&stack);
    canvas.setVisibleExtent(extent);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, 1.0);
    camera.setElevation(90.0);
    camera.setGroundExtent(extent, 1.0);
    camera.setDistance(1000.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(200, 200));
    ASSERT_EQ(probe->selection(), QSet<int>({ 0 }));

    // Picking in the scene replaces it, in the same one place.
    QTest::mouseClick(&scene, Qt::LeftButton, {},
                      QPoint(200, 320));

    EXPECT_EQ(probe->selection(), QSet<int>({ 1 }));
  }

  TEST_F(PickTest, ClickingTheSkyClearsTheSelection)
  {
    LayerStackModel stack;

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 0.0, 50.0 }, { 100.0, 50.0 } });
    ASSERT_GE(stack.addLayer(probe), 0);

    SceneView scene;
    scene.resize(400, 400);
    scene.show();
    scene.setModel(&stack);

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, 1.0);
    camera.setElevation(90.0);
    camera.setGroundExtent(QRectF(0.0, 0.0, 100.0, 100.0), 1.0);
    camera.setDistance(1000.0);
    scene.setCamera(camera);

    QApplication::processEvents();

    QTest::mouseClick(&scene, Qt::LeftButton, {}, QPoint(200, 200));
    ASSERT_FALSE(probe->selection().isEmpty());

    QTest::mouseClick(&scene, Qt::LeftButton, {}, QPoint(200, 60));

    EXPECT_TRUE(probe->selection().isEmpty());
  }
}
