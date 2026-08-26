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

#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"
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

  // ── through the canvas ──────────────────────────────────────────────────

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
}
