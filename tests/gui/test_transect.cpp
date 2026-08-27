/*!
 * \file   test_transect.cpp
 * \brief  D3d verification — a section line cut through a layered mesh.
 *
 * The gates are on distances and elevations with arithmetic answers, over a
 * fixture of unit squares whose spans can be written down: a line entering
 * the second of a row of unit squares enters it 1.5 from its start, and a
 * section that got that wrong would still draw a plausible picture.
 *
 * The awkward cases have tests of their own because they are where a slicer
 * written by pairing crossings up quietly disagrees with one written by
 * asking what contains the middle: a concave cell entered twice, a line
 * lying wholly inside one cell so that it crosses nothing at all, and a gap
 * in the mesh the line runs across.
 */

#include "core/composerapplication.h"
#include "layers/layeredmesh.h"
#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maptool.h"
#include "render/classification.h"
#include "render/layerstyle.h"
#include "results/transect.h"
#include "ui/panels/transectpanel.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QMouseEvent>
#include <QPixmap>
#include <QSignalSpy>

#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  //! A row of `columns` unit squares, sharing their vertical edges.
  MeshDefinition strip(int columns)
  {
    MeshDefinition mesh;
    mesh.meshName = "strip";

    for (int index = 0; index <= columns; ++index)
    {
      mesh.nodeX.push_back(double(index));
      mesh.nodeY.push_back(0.0);
      mesh.nodeX.push_back(double(index));
      mesh.nodeY.push_back(1.0);
    }

    mesh.faceNodeOffsets.push_back(0);

    for (int index = 0; index < columns; ++index)
    {
      const int64_t left = 2 * index;

      mesh.faceNodes.push_back(left);
      mesh.faceNodes.push_back(left + 2);
      mesh.faceNodes.push_back(left + 3);
      mesh.faceNodes.push_back(left + 1);
      mesh.faceNodeOffsets.push_back(
        static_cast<int64_t>(mesh.faceNodes.size()));
    }

    return mesh;
  }

  /*!
   * \brief `strip(columns)` with an unusable face in front of it.
   *
   * A face whose connectivity points outside the node array, which the layer
   * skips when it builds its features — so from there on feature N is face
   * N + 1, and anything that reads a column by feature index is off by one.
   */
  MeshDefinition strippedStrip(int columns)
  {
    const MeshDefinition usable = strip(columns);

    MeshDefinition mesh;
    mesh.meshName = usable.meshName;
    mesh.nodeX = usable.nodeX;
    mesh.nodeY = usable.nodeY;

    mesh.faceNodes = {999, 998, 997};
    mesh.faceNodeOffsets = {0, 3};

    for (size_t index = 0; index < usable.faceNodes.size(); ++index)
    {
      mesh.faceNodes.push_back(usable.faceNodes[index]);
    }

    for (size_t index = 1; index < usable.faceNodeOffsets.size(); ++index)
    {
      mesh.faceNodeOffsets.push_back(usable.faceNodeOffsets[index] + 3);
    }

    return mesh;
  }

  //! A layered mesh over `horizontal`, flat bed, uniform sigma.
  LayeredMesh flatOver(MeshDefinition horizontal, int layers,
                       double bed = -10.0, double surface = 0.0)
  {
    const int columns = int(horizontal.faceCount());

    LayeredMesh mesh;
    mesh.horizontal = std::move(horizontal);
    mesh.layerCount = layers;
    mesh.interfaceZ.resize(size_t(columns) * size_t(layers + 1));

    for (int column = 0; column < columns; ++column)
    {
      for (int k = 0; k <= layers; ++k)
      {
        const double fraction = double(k) / double(layers);
        mesh.interfaceZ[size_t(mesh.interfaceSlot(column, k))] =
          surface + fraction * (bed - surface);
      }
    }

    return mesh;
  }

  //! A layered mesh over `strip(columns)`.
  LayeredMesh flatStrip(int columns, int layers, double bed = -10.0,
                        double surface = 0.0)
  {
    return flatOver(strip(columns), layers, bed, surface);
  }

  std::unique_ptr<MeshLayer> layeredLayer(const LayeredMesh &layered,
                                          QString &message)
  {
    std::unique_ptr<MeshLayer> layer = MeshLayer::create(
      QStringLiteral("column"), layered.horizontal, MeshEntity::Face,
      message);

    if (!layer || !layer->setLayering(layered, message))
    {
      return nullptr;
    }

    return layer;
  }

  //! A closed square ring with its lower-left corner at (x, y).
  QPolygonF square(double x, double y, double side = 1.0)
  {
    return QPolygonF({QPointF(x, y), QPointF(x + side, y),
                      QPointF(x + side, y + side), QPointF(x, y + side)});
  }

  //! A line through the middle of the strip, from `from` to `to` at y = 0.5.
  QPolygonF alongStrip(double from, double to)
  {
    return QPolygonF({QPointF(from, 0.5), QPointF(to, 0.5)});
  }

  class TransectTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_transect";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
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

  ComposerApplication *TransectTest::s_app = nullptr;
}

// ── the slice itself ────────────────────────────────────────────────────────

TEST_F(TransectTest, ALineAcrossARowOfCellsEntersEachWhereItActuallyDoes)
{
  const QVector<QPolygonF> rings{square(0.0, 0.0), square(1.0, 0.0),
                                 square(2.0, 0.0)};

  // From half a cell before the row to half a cell after it: the answers are
  // arithmetic, not "three spans came back".
  const QVector<TransectSpan> spans =
    spansAlongLine(rings, alongStrip(-0.5, 3.5));

  ASSERT_EQ(spans.size(), 3);

  for (int index = 0; index < 3; ++index)
  {
    EXPECT_EQ(spans.at(index).ring, index);
    EXPECT_NEAR(spans.at(index).start, 0.5 + index, 1e-9);
    EXPECT_NEAR(spans.at(index).end, 1.5 + index, 1e-9);
  }
}

TEST_F(TransectTest, ALineDrawnBackwardsReportsTheCellsBackwards)
{
  // Ordered along the line, not by cell index. A slicer that sorted its
  // answer by ring would pass every count-based check and draw a section
  // that is the mirror image of the line the user dragged.
  const QVector<QPolygonF> rings{square(0.0, 0.0), square(1.0, 0.0),
                                 square(2.0, 0.0)};

  const QVector<TransectSpan> spans =
    spansAlongLine(rings, alongStrip(3.5, -0.5));

  ASSERT_EQ(spans.size(), 3);

  EXPECT_EQ(spans.at(0).ring, 2);
  EXPECT_EQ(spans.at(1).ring, 1);
  EXPECT_EQ(spans.at(2).ring, 0);

  EXPECT_NEAR(spans.at(0).start, 0.5, 1e-9);
  EXPECT_NEAR(spans.at(0).end, 1.5, 1e-9);
}

TEST_F(TransectTest, ALineLyingWhollyInsideOneCellStillCutsIt)
{
  // The case with no crossings at all. A slicer that only ever looks at the
  // cells it crossed an edge of answers "nothing here" for a line drawn
  // inside a single large cell, which is an ordinary thing to draw on a
  // coarse mesh.
  const QVector<QPolygonF> rings{square(0.0, 0.0, 10.0)};

  const QVector<TransectSpan> spans =
    spansAlongLine(rings, QPolygonF({QPointF(2.0, 5.0), QPointF(6.0, 5.0)}));

  ASSERT_EQ(spans.size(), 1);
  EXPECT_EQ(spans.first().ring, 0);
  EXPECT_NEAR(spans.first().start, 0.0, 1e-9);
  EXPECT_NEAR(spans.first().end, 4.0, 1e-9);
}

TEST_F(TransectTest, AConcaveCellEnteredTwiceIsReportedTwice)
{
  // A C-shaped cell, opening to the right. A line through the opening leaves
  // it and comes back, which is two spans with a gap between them. Pairing
  // crossings up — first with second, third with fourth — would report one
  // span straight through the notch and colour ground the cell does not
  // cover.
  const QPolygonF notched({QPointF(0.0, 0.0), QPointF(4.0, 0.0),
                           QPointF(4.0, 1.0), QPointF(1.0, 1.0),
                           QPointF(1.0, 2.0), QPointF(4.0, 2.0),
                           QPointF(4.0, 3.0), QPointF(0.0, 3.0)});

  const QVector<TransectSpan> spans =
    spansAlongLine({notched},
                   QPolygonF({QPointF(-1.0, 0.5), QPointF(-1.0, 2.5)}));

  // Down the left edge, outside: nothing at all, which is the control.
  EXPECT_TRUE(spans.isEmpty());

  // Started *inside* the lower arm, so the line crosses an odd number of
  // edges. That is what tells the two families of slicer apart: pairing
  // crossings up — enter with exit, third with fourth — agrees with the
  // right answer whenever the line starts outside, and on this line it puts
  // a span across the notch and loses the upper arm altogether.
  const QVector<TransectSpan> through =
    spansAlongLine({notched},
                   QPolygonF({QPointF(3.0, 0.5), QPointF(3.0, 4.0)}));

  ASSERT_EQ(through.size(), 2);
  EXPECT_NEAR(through.at(0).start, 0.0, 1e-9);
  EXPECT_NEAR(through.at(0).end, 0.5, 1e-9);
  EXPECT_NEAR(through.at(1).start, 1.5, 1e-9);
  EXPECT_NEAR(through.at(1).end, 2.5, 1e-9);
}

TEST_F(TransectTest, AGapInTheMeshIsAGapInTheSection)
{
  // Two cells with a unit hole between them. The section has to show the
  // hole where it is: a slicer that closed the gap would put the far cell a
  // metre nearer than it is and mislabel every distance after it.
  const QVector<QPolygonF> rings{square(0.0, 0.0), square(2.0, 0.0)};

  const QVector<TransectSpan> spans =
    spansAlongLine(rings, alongStrip(0.0, 3.0));

  ASSERT_EQ(spans.size(), 2);
  EXPECT_NEAR(spans.at(0).end, 1.0, 1e-9);
  EXPECT_NEAR(spans.at(1).start, 2.0, 1e-9);
}

TEST_F(TransectTest, APolylineBendingInsideOneCellIsNotSeamed)
{
  // Two segments, both inside the same cell. Reported as one traversal, not
  // as two abutting ones: a section drawn from the latter has a seam down
  // the middle of a cell, which reads as a cell boundary that is not there.
  const QVector<QPolygonF> rings{square(0.0, 0.0, 10.0)};

  const QVector<TransectSpan> spans =
    spansAlongLine(rings, QPolygonF({QPointF(1.0, 1.0), QPointF(5.0, 1.0),
                                     QPointF(5.0, 4.0)}));

  ASSERT_EQ(spans.size(), 1);
  EXPECT_NEAR(spans.first().start, 0.0, 1e-9);
  EXPECT_NEAR(spans.first().end, 7.0, 1e-9);
}

// ── the layer's section ─────────────────────────────────────────────────────

TEST_F(TransectTest, ASectionCarriesTheColumnsElevationsAndValues)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(3, 2), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Column-major, vertical index fastest: column 0 surface, column 0 bed,
  // column 1 surface, and so on.
  ASSERT_TRUE(layer->setLayeredValues(
    QStringLiteral("temp"),
    QVector<double>{20.0, 4.0, 21.0, 5.0, 22.0, 6.0}));

  TransectSection section;
  ASSERT_TRUE(layer->transect(alongStrip(-0.5, 3.5), section, message))
    << message.toStdString();

  EXPECT_NEAR(section.length, 4.0, 1e-9);
  ASSERT_EQ(section.cells.size(), 6);

  // Surface first within each column, columns in the order the line met them.
  EXPECT_EQ(section.cells.at(0).column, 0);
  EXPECT_EQ(section.cells.at(0).layer, 0);
  EXPECT_NEAR(section.cells.at(0).value, 20.0, 1e-9);
  EXPECT_NEAR(section.cells.at(0).topElevation, 0.0, 1e-9);
  EXPECT_NEAR(section.cells.at(0).bottomElevation, -5.0, 1e-9);
  EXPECT_NEAR(section.cells.at(0).startDistance, 0.5, 1e-9);
  EXPECT_NEAR(section.cells.at(0).endDistance, 1.5, 1e-9);

  EXPECT_EQ(section.cells.at(1).layer, 1);
  EXPECT_NEAR(section.cells.at(1).value, 4.0, 1e-9);
  EXPECT_NEAR(section.cells.at(1).topElevation, -5.0, 1e-9);
  EXPECT_NEAR(section.cells.at(1).bottomElevation, -10.0, 1e-9);

  EXPECT_EQ(section.cells.at(4).column, 2);
  EXPECT_NEAR(section.cells.at(4).value, 22.0, 1e-9);
  EXPECT_NEAR(section.cells.at(4).startDistance, 2.5, 1e-9);

  const auto [lowest, highest] = section.elevationRange();
  EXPECT_NEAR(lowest, -10.0, 1e-9);
  EXPECT_NEAR(highest, 0.0, 1e-9);

  const auto [coldest, warmest] = section.valueRange();
  EXPECT_NEAR(coldest, 4.0, 1e-9);
  EXPECT_NEAR(warmest, 22.0, 1e-9);
}

TEST_F(TransectTest, TheColumnCutIsTheFaceNotTheFeatureThatDrewIt)
{
  // The two sequences diverge on exactly the meshes where guessing would be
  // wrong: a face whose connectivity points outside the node array is
  // skipped when the features are built, so from there on feature N is face
  // N + 1. Reading the column by feature index gives a real value, of the
  // right mesh, at the right elevation, from the wrong column.
  QString message;
  const LayeredMesh layered = flatOver(strippedStrip(3), 1);
  ASSERT_EQ(layered.columnCount(), 4);

  const std::unique_ptr<MeshLayer> layer = layeredLayer(layered, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Column 0 is the unusable face; the three squares are columns 1 to 3.
  ASSERT_TRUE(layer->setLayeredValues(QStringLiteral("temp"),
                                      QVector<double>{99.0, 20.0, 21.0,
                                                      22.0}));

  TransectSection section;
  ASSERT_TRUE(layer->transect(alongStrip(-0.5, 3.5), section, message))
    << message.toStdString();

  ASSERT_EQ(section.cells.size(), 3);

  EXPECT_EQ(section.cells.at(0).column, 1);
  EXPECT_NEAR(section.cells.at(0).value, 20.0, 1e-9);
  EXPECT_EQ(section.cells.at(2).column, 3);
  EXPECT_NEAR(section.cells.at(2).value, 22.0, 1e-9);
}

TEST_F(TransectTest, TheAxisRunsTheWholeLineNotJustTheCells)
{
  // The line starts and ends off the mesh, and the section says so: length
  // is what was drawn, and the first cell starts half a unit in. Trimming
  // the axis to the cells would hide that the line began over nothing.
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(1, 1), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(
    layer->setLayeredValues(QStringLiteral("temp"), QVector<double>{7.0}));

  TransectSection section;
  ASSERT_TRUE(layer->transect(alongStrip(-2.0, 3.0), section, message))
    << message.toStdString();

  EXPECT_NEAR(section.length, 5.0, 1e-9);
  ASSERT_EQ(section.cells.size(), 1);
  EXPECT_NEAR(section.cells.first().startDistance, 2.0, 1e-9);
  EXPECT_NEAR(section.cells.first().endDistance, 3.0, 1e-9);
}

TEST_F(TransectTest, AMeshWithNoValuesIsSaidRatherThanCut)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(2, 2), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  TransectSection section;
  EXPECT_FALSE(layer->transect(alongStrip(-0.5, 2.5), section, message));
  EXPECT_TRUE(message.contains(QStringLiteral("no layered values")))
    << message.toStdString();
  EXPECT_TRUE(section.isEmpty());
}

TEST_F(TransectTest, AFlatMeshHasNoColumnToCut)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("column"), strip(2), MeshEntity::Face, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  TransectSection section;
  EXPECT_FALSE(layer->transect(alongStrip(-0.5, 2.5), section, message));
  EXPECT_TRUE(message.contains(QStringLiteral("no vertical layering")))
    << message.toStdString();
}

TEST_F(TransectTest, ALineThatMissesTheMeshIsSaidRatherThanDrawnEmpty)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(2, 1), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(layer->setLayeredValues(QStringLiteral("temp"),
                                      QVector<double>{1.0, 2.0}));

  TransectSection section;
  EXPECT_FALSE(layer->transect(
    QPolygonF({QPointF(-5.0, 8.0), QPointF(5.0, 8.0)}), section, message));
  EXPECT_TRUE(message.contains(QStringLiteral("does not cross")))
    << message.toStdString();
}

TEST_F(TransectTest, ALineOfOnePointIsNoLine)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(1, 1), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(
    layer->setLayeredValues(QStringLiteral("temp"), QVector<double>{1.0}));

  TransectSection section;
  EXPECT_FALSE(layer->transect(QPolygonF({QPointF(0.5, 0.5)}), section,
                               message));
  EXPECT_TRUE(message.contains(QStringLiteral("at least two")))
    << message.toStdString();

  // Two points in the same place is the same non-line, arrived at the other
  // way: the gesture that produces it is a click, not a drag.
  EXPECT_FALSE(layer->transect(
    QPolygonF({QPointF(0.5, 0.5), QPointF(0.5, 0.5)}), section, message));
  EXPECT_TRUE(message.contains(QStringLiteral("zero length")))
    << message.toStdString();
}

// ── the panel ───────────────────────────────────────────────────────────────

TEST_F(TransectTest, ThePanelSaysWhatItIsWaitingFor)
{
  LayerStackModel stack;
  TransectPanel panel;

  panel.setModel(&stack);
  EXPECT_TRUE(panel.statusText().contains(QStringLiteral("No layered mesh")))
    << panel.statusText().toStdString();

  QString message;
  std::unique_ptr<MeshLayer> layer = layeredLayer(flatStrip(2, 1), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(layer->setLayeredValues(QStringLiteral("temp"),
                                      QVector<double>{1.0, 2.0}));

  stack.addLayer(layer.release());

  // A mesh but no line: a different thing to be waiting for, and said
  // differently, because telling someone to load a mesh they have loaded
  // explains nothing.
  EXPECT_TRUE(panel.statusText().contains(QStringLiteral("Draw a section")))
    << panel.statusText().toStdString();
  EXPECT_EQ(panel.cellCount(), 0);
}

TEST_F(TransectTest, ALineGivenToThePanelCutsTheSection)
{
  LayerStackModel stack;
  TransectPanel panel;
  panel.setModel(&stack);

  QString message;
  std::unique_ptr<MeshLayer> layer = layeredLayer(flatStrip(3, 2), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(layer->setLayeredValues(
    QStringLiteral("temp"),
    QVector<double>{20.0, 4.0, 21.0, 5.0, 22.0, 6.0}));

  MeshLayer *kept = layer.get();
  stack.addLayer(layer.release());

  panel.setLine(alongStrip(-0.5, 3.5));

  EXPECT_TRUE(panel.statusText().isEmpty())
    << panel.statusText().toStdString();
  EXPECT_EQ(panel.cellCount(), 6);
  EXPECT_EQ(panel.layer(), kept);
  EXPECT_NEAR(panel.section().length, 4.0, 1e-9);

  // Clearing the line puts the panel back where it started rather than
  // leaving the last section standing over ground nobody is looking at.
  panel.setLine({});
  EXPECT_EQ(panel.cellCount(), 0);
  EXPECT_EQ(panel.layer(), nullptr);
  EXPECT_FALSE(panel.statusText().isEmpty());
}

TEST_F(TransectTest, TheSectionIsRecutWhenTheValuesChange)
{
  // A run stepping is a renderChanged on the stack. The line stays where the
  // user drew it and the section under it follows: re-drawing the line every
  // step would be asking them to hold still through the animation.
  LayerStackModel stack;
  TransectPanel panel;
  panel.setModel(&stack);

  QString message;
  std::unique_ptr<MeshLayer> layer = layeredLayer(flatStrip(1, 1), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(
    layer->setLayeredValues(QStringLiteral("temp"), QVector<double>{3.0}));

  MeshLayer *kept = layer.get();
  stack.addLayer(layer.release());

  panel.setLine(alongStrip(-0.5, 1.5));
  ASSERT_EQ(panel.cellCount(), 1);
  ASSERT_NEAR(panel.section().cells.first().value, 3.0, 1e-9);

  ASSERT_TRUE(
    kept->setLayeredValues(QStringLiteral("temp"), QVector<double>{9.0}));

  EXPECT_EQ(panel.cellCount(), 1);
  EXPECT_NEAR(panel.section().cells.first().value, 9.0, 1e-9)
    << "the section kept the values it was first cut with";
}

TEST_F(TransectTest, ACellIsPaintedInTheColourTheMapGivesIt)
{
  // The gate is a pixel, because "the section is coloured by the layer's
  // classification" is only observable where it is drawn — a section that
  // built the right cells and filled them from its own ramp would satisfy
  // every check made on the section itself.
  LayerStackModel stack;
  TransectPanel panel;
  panel.setModel(&stack);

  QString message;
  std::unique_ptr<MeshLayer> layer = layeredLayer(flatStrip(1, 2), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(layer->setLayeredValues(QStringLiteral("temp"),
                                      QVector<double>{20.0, 4.0}));

  LayerStyle *style = layer->style();
  style->setMode(StyleMode::Graduated);
  ASSERT_TRUE(style->classification().setManualBreaks({0.0, 12.0, 30.0}));
  style->classification().setClassColor(0, QColor(0, 0, 255));
  style->classification().setClassColor(1, QColor(255, 0, 0));

  stack.addLayer(layer.release());

  panel.resize(400, 300);
  panel.setLine(alongStrip(-0.5, 1.5));
  ASSERT_EQ(panel.cellCount(), 2);

  TransectView *view = panel.view();
  ASSERT_NE(view, nullptr);

  // Painted before measured. A panel that has never been shown has not run
  // its layout, so the view still stands at its minimum size — and grab()
  // lays it out on the way past. Measuring first would describe a widget
  // half the size of the image the pixels are then read from, and the check
  // would fail on ground that is drawn perfectly correctly.
  const QImage painted = view->grab().toImage();

  const QRectF surface = view->cellRect(panel.section().cells.at(0));
  const QRectF bed = view->cellRect(panel.section().cells.at(1));

  ASSERT_FALSE(surface.isEmpty());
  ASSERT_FALSE(bed.isEmpty());
  ASSERT_EQ(painted.size(), view->size())
    << "the rectangles were measured against a different widget size";

  // The surface layer is the warm one and is drawn above the cold bed: an
  // upside-down section is the mistake this fixture exists to catch.
  EXPECT_LT(surface.center().y(), bed.center().y());

  EXPECT_EQ(painted.pixelColor(surface.center().toPoint()), QColor(255, 0, 0));
  EXPECT_EQ(painted.pixelColor(bed.center().toPoint()), QColor(0, 0, 255));
}

// ── the tool ────────────────────────────────────────────────────────────────

TEST_F(TransectTest, DraggingUnderTheSectionToolPublishesTheLineItDrew)
{
  MapCanvas canvas;
  canvas.resize(800, 400);
  canvas.setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));
  canvas.setToolKind(MapToolKind::Transect);

  QSignalSpy drawn(&canvas, &MapCanvas::transectDrawn);

  press(canvas, QPoint(100, 100));
  moveTo(canvas, QPoint(300, 300));
  release(canvas, QPoint(300, 300));

  ASSERT_EQ(canvas.transectLine().size(), 2);

  // In world coordinates, not pixels: a line kept in pixels would slide off
  // the ground it was cut across the moment the view panned.
  EXPECT_NEAR(canvas.transectLine().first().x(), -300.0, 1e-6);
  EXPECT_NEAR(canvas.transectLine().first().y(), 100.0, 1e-6);
  EXPECT_NEAR(canvas.transectLine().last().x(), -100.0, 1e-6);
  EXPECT_NEAR(canvas.transectLine().last().y(), -100.0, 1e-6);

  EXPECT_GE(drawn.count(), 1);
}

TEST_F(TransectTest, ASectionDrawnStraightDownACanyonHasNoWidthAtAll)
{
  // A vertical drag: zero width, and the drag someone cutting across a
  // channel actually makes. A guard written on the bounding rectangle would
  // throw it away as degenerate — the trap the band tools already pay for.
  MapCanvas canvas;
  canvas.resize(800, 400);
  canvas.setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));
  canvas.setToolKind(MapToolKind::Transect);

  press(canvas, QPoint(200, 50));
  moveTo(canvas, QPoint(200, 350));
  release(canvas, QPoint(200, 350));

  ASSERT_EQ(canvas.transectLine().size(), 2);
  EXPECT_NEAR(canvas.transectLine().first().x(),
              canvas.transectLine().last().x(), 1e-6);
  EXPECT_NEAR(canvas.transectLine().first().y(), 150.0, 1e-6);
  EXPECT_NEAR(canvas.transectLine().last().y(), -150.0, 1e-6);
}

TEST_F(TransectTest, AClickUnderTheSectionToolClearsTheLine)
{
  MapCanvas canvas;
  canvas.resize(800, 400);
  canvas.setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));
  canvas.setToolKind(MapToolKind::Transect);

  press(canvas, QPoint(100, 100));
  moveTo(canvas, QPoint(300, 300));
  release(canvas, QPoint(300, 300));
  ASSERT_EQ(canvas.transectLine().size(), 2);

  // Inside the click slop: a click, the way clicking empty map clears a
  // selection rather than leaving the last one standing.
  press(canvas, QPoint(400, 200));
  release(canvas, QPoint(401, 201));

  EXPECT_TRUE(canvas.transectLine().isEmpty());
}

TEST_F(TransectTest, SwitchingToolsMidDragLeavesNoHalfDrawnLine)
{
  // The preview lives on the canvas, not in a child widget, so it does not
  // go away with the tool the way a rubber band does. Switching mid-drag
  // would otherwise leave a line belonging to a gesture nobody is making.
  MapCanvas canvas;
  canvas.resize(800, 400);
  canvas.setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));
  canvas.setToolKind(MapToolKind::Transect);

  press(canvas, QPoint(100, 100));
  moveTo(canvas, QPoint(300, 300));
  ASSERT_EQ(canvas.transectLine().size(), 2);

  canvas.setToolKind(MapToolKind::Pan);

  EXPECT_TRUE(canvas.transectLine().isEmpty());
}

TEST_F(TransectTest, AFinishedSectionSurvivesSwitchingBackToPan)
{
  // The other half of the rule above: a line the user finished drawing is a
  // thing on the map, and it stays there when they go back to panning. A
  // teardown that cleared unconditionally would take it with them.
  MapCanvas canvas;
  canvas.resize(800, 400);
  canvas.setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));
  canvas.setToolKind(MapToolKind::Transect);

  press(canvas, QPoint(100, 100));
  moveTo(canvas, QPoint(300, 300));
  release(canvas, QPoint(300, 300));
  ASSERT_EQ(canvas.transectLine().size(), 2);

  canvas.setToolKind(MapToolKind::Pan);

  EXPECT_EQ(canvas.transectLine().size(), 2);
}
