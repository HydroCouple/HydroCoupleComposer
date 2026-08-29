/*!
 * \file   test_domaindraw.cpp
 * \brief  E1b-2 verification — drawing a domain's parts on the map.
 *
 * Driven with real mouse events through the canvas, because what is under
 * test is the gesture: which click puts a vertex down, which one finishes,
 * and what reaches the model when it does. A test that called finish()
 * directly would pass over a tool that never bound the button.
 *
 * The gates are on world coordinates in the model, not on click counts. A
 * tool that stored screen pixels, or dropped the last vertex, or appended
 * the cursor position on every mouse move, all produce a shape with a
 * plausible number of vertices in the wrong place.
 */

#include "core/composerapplication.h"
#include "layers/domainlayer.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "mesh/domaindrawtool.h"
#include "mesh/meshdomainmodel.h"

#include <gtest/gtest.h>

#include <QMouseEvent>
#include <QSignalSpy>

#include <memory>

using namespace HydroCouple::Composer;

namespace
{
  class DomainDrawTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_domaindraw";
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
        m_canvas = std::make_unique<MapCanvas>();
        m_canvas->resize(800, 400);

        // Exactly (-400,-200) to (400,200), so a pixel maps to a world unit
        // and every expected coordinate below is arithmetic.
        m_canvas->setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));

        m_model = std::make_unique<MeshDomainModel>();
      }

      void TearDown() override
      {
        m_canvas.reset();
        m_model.reset();
      }

      //! Installs a drawing tool for \a part and returns it.
      DomainDrawTool *useTool(DomainPart part)
      {
        auto tool = std::make_unique<DomainDrawTool>(m_canvas.get(),
                                                     m_model.get(), part);
        DomainDrawTool *raw = tool.get();
        m_canvas->setTool(std::move(tool));

        return raw;
      }

      void click(const QPoint &at, Qt::MouseButton button = Qt::LeftButton)
      {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(at),
                          m_canvas->mapToGlobal(QPointF(at)), button, button,
                          Qt::NoModifier);
        QApplication::sendEvent(m_canvas.get(), &press);

        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at),
                            m_canvas->mapToGlobal(QPointF(at)), button,
                            button, Qt::NoModifier);
        QApplication::sendEvent(m_canvas.get(), &release);
      }

      void moveTo(const QPoint &at)
      {
        QMouseEvent event(QEvent::MouseMove, QPointF(at),
                          m_canvas->mapToGlobal(QPointF(at)), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(m_canvas.get(), &event);
      }

      std::unique_ptr<MapCanvas> m_canvas;
      std::unique_ptr<MeshDomainModel> m_model;

      static ComposerApplication *s_app;
  };

  ComposerApplication *DomainDrawTest::s_app = nullptr;
}

// ── the gesture ─────────────────────────────────────────────────────────────

TEST_F(DomainDrawTest, ClicksPlaceVerticesAndARightClickFinishes)
{
  useTool(DomainPart::Boundary);

  click(QPoint(100, 100));
  click(QPoint(300, 100));
  click(QPoint(300, 300));

  // Nothing reaches the domain until the gesture is finished: a boundary
  // that appeared after three clicks could never be given a fourth corner.
  EXPECT_TRUE(m_model->domain().boundary.isEmpty());

  click(QPoint(400, 200), Qt::RightButton);

  const QPolygonF &ring = m_model->domain().boundary;
  ASSERT_EQ(ring.size(), 3)
    << "the right-click that finished the shape also left a vertex behind";

  // World coordinates, not pixels. The viewport is 800x400 over
  // (-400,-200)-(400,200), so screen (100,100) is world (-300, 100) — y
  // flipped, because the map runs up and the widget runs down.
  EXPECT_DOUBLE_EQ(ring.at(0).x(), -300.0);
  EXPECT_DOUBLE_EQ(ring.at(0).y(), 100.0);
  EXPECT_DOUBLE_EQ(ring.at(1).x(), -100.0);
  EXPECT_DOUBLE_EQ(ring.at(2).y(), -100.0);
}

TEST_F(DomainDrawTest, APointIsFinishedByItsOwnFirstClick)
{
  // Nothing to accumulate, so asking for a right-click as well would be one
  // gesture for a thing that has only one.
  useTool(DomainPart::ForcedPoints);

  click(QPoint(500, 100));

  ASSERT_EQ(m_model->domain().points.size(), 1);
  EXPECT_DOUBLE_EQ(m_model->domain().points.first().x(), 100.0);
  EXPECT_DOUBLE_EQ(m_model->domain().points.first().y(), 100.0);

  // And a second click is a second point, not a second vertex of the first.
  click(QPoint(600, 200));
  EXPECT_EQ(m_model->domain().points.size(), 2);
}

TEST_F(DomainDrawTest, MovingTheMouseShowsWhereTheNextVertexWouldGo)
{
  // The rubber segment is drawn and never stored. A version that appended it
  // would grow the shape by one vertex per mouse move, which looks right on
  // screen and produces a ring of hundreds of points.
  DomainDrawTool *tool = useTool(DomainPart::Boundary);

  click(QPoint(100, 100));
  click(QPoint(300, 100));

  moveTo(QPoint(320, 120));
  moveTo(QPoint(340, 140));
  moveTo(QPoint(360, 160));

  EXPECT_EQ(tool->pending().size(), 2)
    << "the cursor position was stored as a vertex";

  // Shown, though: the sketch carries the placed vertices plus the cursor.
  EXPECT_EQ(m_canvas->sketch().size(), 3);

  click(QPoint(300, 300));
  click(QPoint(0, 0), Qt::RightButton);

  EXPECT_EQ(m_model->domain().boundary.size(), 3);
}

TEST_F(DomainDrawTest, TheSketchIsClearedWhenTheShapeIsCommitted)
{
  useTool(DomainPart::Breaklines);

  click(QPoint(100, 100));
  moveTo(QPoint(200, 200));
  ASSERT_FALSE(m_canvas->sketch().isEmpty());

  click(QPoint(300, 300));
  click(QPoint(0, 0), Qt::RightButton);

  EXPECT_EQ(m_model->domain().constraintLines.size(), 1);
  EXPECT_TRUE(m_canvas->sketch().isEmpty())
    << "the finished shape is drawn twice: once by the layer and once by a "
       "sketch nobody cleared";
}

// ── what is refused ─────────────────────────────────────────────────────────

TEST_F(DomainDrawTest, AShapeWithTooFewVerticesIsDiscardedNotCommitted)
{
  // Two clicks is not a ring. Committing it would leave the user to discover
  // a degenerate boundary they never finished drawing.
  useTool(DomainPart::Boundary);

  click(QPoint(100, 100));
  click(QPoint(200, 200));
  click(QPoint(0, 0), Qt::RightButton);

  EXPECT_TRUE(m_model->domain().boundary.isEmpty());

  // A breakline needs only two, so the same two clicks are a valid line.
  useTool(DomainPart::Breaklines);

  click(QPoint(100, 100));
  click(QPoint(200, 200));
  click(QPoint(0, 0), Qt::RightButton);

  EXPECT_EQ(m_model->domain().constraintLines.size(), 1);
}

TEST_F(DomainDrawTest, EachPartKnowsHowManyVerticesItNeeds)
{
  EXPECT_EQ(minimumVertices(DomainPart::Boundary), 3);
  EXPECT_EQ(minimumVertices(DomainPart::Holes), 3);
  EXPECT_EQ(minimumVertices(DomainPart::Breaklines), 2);
  EXPECT_EQ(minimumVertices(DomainPart::ForcedPoints), 1);
}

TEST_F(DomainDrawTest, SwitchingToolsMidShapeCommitsNothingAndLeavesNoSketch)
{
  useTool(DomainPart::Boundary);

  click(QPoint(100, 100));
  click(QPoint(300, 100));
  click(QPoint(300, 300));
  ASSERT_FALSE(m_canvas->sketch().isEmpty());

  // Three vertices is enough to commit, which is exactly why abandoning has
  // to discard rather than finish: the user switched away, they did not
  // right-click.
  m_canvas->setToolKind(MapToolKind::Pan);

  EXPECT_TRUE(m_model->domain().boundary.isEmpty())
    << "abandoning a shape committed it anyway";
  EXPECT_TRUE(m_canvas->sketch().isEmpty())
    << "a half-drawn shape was left on the map after its tool went away";
}

// ── the model is the only route to the map ──────────────────────────────────

TEST_F(DomainDrawTest, AShapeDrawnReachesTheLayerThroughTheModel)
{
  // The whole point of the tool writing to the model and nowhere else: a
  // domain drawn here and a domain loaded from a file arrive on screen by
  // exactly the same path.
  const std::unique_ptr<DomainLayer> holes =
    DomainLayer::create(m_model.get(), DomainPart::Holes);

  ASSERT_EQ(holes->featureCount(), 0);

  useTool(DomainPart::Holes);

  QSignalSpy changed(m_model.get(), &MeshDomainModel::domainChanged);

  click(QPoint(100, 100));
  click(QPoint(200, 100));
  click(QPoint(200, 200));
  click(QPoint(0, 0), Qt::RightButton);

  EXPECT_EQ(changed.count(), 1)
    << "the domain was announced once per click rather than once per shape";
  EXPECT_EQ(holes->featureCount(), 1);

  // A second hole is a second feature, not a replacement.
  click(QPoint(300, 100));
  click(QPoint(400, 100));
  click(QPoint(400, 200));
  click(QPoint(0, 0), Qt::RightButton);

  EXPECT_EQ(holes->featureCount(), 2);
}

TEST_F(DomainDrawTest, DrawingABoundaryTwiceReplacesItRatherThanAddingOne)
{
  // There is exactly one boundary, and drawing another is how the first is
  // corrected. Appending would leave two rings and a domain that means
  // nothing.
  useTool(DomainPart::Boundary);

  click(QPoint(100, 100));
  click(QPoint(300, 100));
  click(QPoint(300, 300));
  click(QPoint(0, 0), Qt::RightButton);

  ASSERT_EQ(m_model->domain().boundary.size(), 3);

  click(QPoint(500, 100));
  click(QPoint(700, 100));
  click(QPoint(700, 300));
  click(QPoint(600, 350));
  click(QPoint(0, 0), Qt::RightButton);

  EXPECT_EQ(m_model->domain().boundary.size(), 4);
  EXPECT_DOUBLE_EQ(m_model->domain().boundary.first().x(), 100.0)
    << "the second boundary was added beside the first rather than "
       "replacing it";
}
