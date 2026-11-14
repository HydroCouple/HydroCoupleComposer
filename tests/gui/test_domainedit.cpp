/*!
 * \file   test_domainedit.cpp
 * \brief  E1b-3 verification — moving, adding and removing domain vertices.
 *
 * Two things are under test and they fail differently. The reach — what
 * counts as being *on* a vertex or an edge — is arithmetic, and its failures
 * are quiet: a tolerance fixed in world units works at the zoom it was
 * written at, an unclamped projection puts a corner a long way off the edge
 * that was clicked, and a snap that does not exclude the vertex being
 * dragged pins every drag to where it started. None of those look wrong on
 * screen.
 *
 * The gestures are driven through real mouse events on the canvas, as the
 * drawing suite drives its own, because what is under test is which button
 * does what. A test that called moveVertex() directly would pass over a tool
 * that never bound the drag.
 */

#include "core/composerapplication.h"
#include "core/preferencesmanager.h"
#include "map/mapcanvas.h"
#include "settingsredirect.h"
#include "map/maptool.h"
#include "map/maptransform.h"
#include "mesh/domainedittool.h"
#include "mesh/domainsnap.h"
#include "mesh/meshdomainmodel.h"

#include <gtest/gtest.h>

#include <QMouseEvent>
#include <QSignalSpy>

#include <limits>
#include <memory>

using namespace HydroCouple::Composer;

namespace
{
  //! The domain every gesture test edits.
  MeshDomain fixtureDomain()
  {
    MeshDomain domain;

    // A rectangle rather than a triangle: a vertex has to be removable from
    // the boundary without the boundary ceasing to be one, and on a triangle
    // every removal is the whole-shape case.
    domain.boundary = QPolygonF({QPointF(-300.0, -150.0),
                                 QPointF(-100.0, -150.0),
                                 QPointF(-100.0, 150.0),
                                 QPointF(-300.0, 150.0)});

    domain.holes.append(QPolygonF({QPointF(-250.0, -50.0),
                                   QPointF(-150.0, -50.0),
                                   QPointF(-200.0, 50.0)}));

    // Bent, not straight: a collinear polyline's phantom closing edge lies
    // on top of its real ones, so the gate that a line is not a ring would
    // pass on a tool that closed it.
    domain.constraintLines.append(QPolygonF({QPointF(0.0, -100.0),
                                             QPointF(100.0, 0.0),
                                             QPointF(0.0, 100.0)}));

    domain.points.append(QPointF(300.0, -150.0));

    return domain;
  }

  class DomainEditTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          // The editor reads the application-wide snap preference, which a
          // test must not write into the developer's own configuration.
          Testing::redirectSettingsTo(
            QStringLiteral(COMPOSER_PREFERENCES_FIXTURE_DIR)
            + QStringLiteral("/test_domainedit"));

          static int argc = 1;
          static char arg0[] = "test_domainedit";
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

        // 800x400 pixels over 800x400 world units, so a pixel is a unit and
        // the ten-pixel reach is ten units — every distance below is
        // arithmetic anyone can check.
        m_canvas->setVisibleExtent(QRectF(-400.0, -200.0, 800.0, 400.0));

        m_model = std::make_unique<MeshDomainModel>();
        m_model->setDomain(fixtureDomain());
      }

      void TearDown() override
      {
        m_canvas.reset();
        m_model.reset();
      }

      //! Installs the editor and returns it.
      DomainEditTool *useTool()
      {
        auto tool = std::make_unique<DomainEditTool>(m_canvas.get(),
                                                     m_model.get());
        DomainEditTool *raw = tool.get();
        m_canvas->setTool(std::move(tool));

        return raw;
      }

      //! \returns Where \a world sits on the widget, y flipped.
      static QPoint screenFor(const QPointF &world)
      {
        return QPoint(int(world.x() + 400.0), int(200.0 - world.y()));
      }

      //! \returns Whether the canvas took the press.
      bool press(const QPoint &at, Qt::MouseButton button = Qt::LeftButton)
      {
        QMouseEvent event(QEvent::MouseButtonPress, QPointF(at),
                          m_canvas->mapToGlobal(QPointF(at)), button, button,
                          Qt::NoModifier);
        QApplication::sendEvent(m_canvas.get(), &event);

        return event.isAccepted();
      }

      void moveTo(const QPoint &at,
                  Qt::MouseButtons held = Qt::NoButton)
      {
        QMouseEvent event(QEvent::MouseMove, QPointF(at),
                          m_canvas->mapToGlobal(QPointF(at)), Qt::NoButton,
                          held, Qt::NoModifier);
        QApplication::sendEvent(m_canvas.get(), &event);
      }

      void release(const QPoint &at,
                   Qt::MouseButton button = Qt::LeftButton)
      {
        QMouseEvent event(QEvent::MouseButtonRelease, QPointF(at),
                          m_canvas->mapToGlobal(QPointF(at)), button, button,
                          Qt::NoModifier);
        QApplication::sendEvent(m_canvas.get(), &event);
      }

      //! Press, drag through \a via, release — one uninterrupted gesture.
      void drag(const QPoint &from, const QPoint &to)
      {
        press(from);
        moveTo(to, Qt::LeftButton);
        release(to);
      }

      std::unique_ptr<MapCanvas> m_canvas;
      std::unique_ptr<MeshDomainModel> m_model;

      static ComposerApplication *s_app;
  };

  ComposerApplication *DomainEditTest::s_app = nullptr;
}

// ── the reach ───────────────────────────────────────────────────────────────

TEST_F(DomainEditTest, TheReachIsMeasuredInPixelsAndSpentInWorldUnits)
{
  // Ten pixels is what a hand can aim at. Ten metres is unusable at one zoom
  // and grabs half the map at another, so the conversion is the whole point.
  EXPECT_DOUBLE_EQ(worldTolerance(10.0, 1.0), 10.0);
  EXPECT_DOUBLE_EQ(worldTolerance(10.0, 4.0), 2.5);

  // An unusable scale reaches nothing rather than everything: a zero here
  // divides, and an infinite tolerance snaps the first vertex in the domain
  // onto every click.
  EXPECT_DOUBLE_EQ(worldTolerance(10.0, 0.0), 0.0);
  EXPECT_DOUBLE_EQ(worldTolerance(10.0, -2.0), 0.0);
  EXPECT_DOUBLE_EQ(
    worldTolerance(10.0, std::numeric_limits<double>::quiet_NaN()), 0.0);
}

TEST_F(DomainEditTest, EveryVertexIsReportedWithTheAddressItLivesAt)
{
  QVector<DomainVertex> addresses;
  const QVector<QPointF> vertices = domainVertices(m_model->domain(),
                                                   addresses);

  ASSERT_EQ(vertices.size(), 11) << "4 corners, 3 hole, 3 breakline, 1 point";
  ASSERT_EQ(addresses.size(), vertices.size())
    << "the handles the editor draws and the addresses it edits would drift";

  for (int index = 0; index < vertices.size(); ++index)
  {
    QPointF position;
    ASSERT_TRUE(vertexPosition(m_model->domain(), addresses.at(index),
                               position))
      << "address " << index << " points at nothing";
    EXPECT_EQ(position, vertices.at(index))
      << "the point at index " << index << " is not where its address says";
  }

  // The order is the order the parts are named in, which is the order the
  // canvas is handed and the order an index into it means anything in.
  EXPECT_EQ(addresses.first().part, DomainPart::Boundary);
  EXPECT_EQ(addresses.last().part, DomainPart::ForcedPoints);
  EXPECT_EQ(addresses.last().shape, 0);
}

TEST_F(DomainEditTest, TheNearestVertexWinsNotTheFirstOrLastWithinReach)
{
  MeshDomain domain;
  domain.boundary = QPolygonF({QPointF(-6.0, 0.0), QPointF(1.0, 0.0),
                               QPointF(4.0, 30.0), QPointF(-6.0, 30.0)});

  // Two corners are within reach of the origin and the nearer is neither the
  // first nor the last of them, so neither a search that stops at the first
  // hit nor one that keeps the last can pass.
  const DomainSnap hit = nearestVertex(domain, QPointF(0.0, 0.0), 10.0);

  ASSERT_TRUE(hit.hit);
  EXPECT_EQ(hit.at.vertex, 1);
  EXPECT_EQ(hit.point, QPointF(1.0, 0.0));

  // And nothing at all when the reach is shorter than the nearest corner.
  EXPECT_FALSE(nearestVertex(domain, QPointF(0.0, 0.0), 0.5).hit);
}

TEST_F(DomainEditTest, TheVertexBeingDraggedIsNotWithinReachOfItself)
{
  MeshDomain domain;
  domain.boundary = QPolygonF({QPointF(-6.0, 0.0), QPointF(1.0, 0.0),
                               QPointF(4.0, 30.0), QPointF(-6.0, 30.0)});

  const DomainVertex dragged{DomainPart::Boundary, 0, 0};
  const DomainSnap hit =
    nearestVertex(domain, QPointF(-6.0, 0.0), 10.0, dragged);

  // Standing exactly on the dragged vertex, the answer is the *other* one
  // within reach — a snap that returned the vertex itself would hold every
  // drag at its starting point for as long as the pointer stayed near it.
  ASSERT_TRUE(hit.hit);
  EXPECT_EQ(hit.at.vertex, 1);
}

TEST_F(DomainEditTest, AClickPastTheEndOfAnEdgeIsNotOnIt)
{
  // (0,-150) is on the infinite line through the boundary's bottom edge and
  // 100 units past the end of the edge that was actually drawn. Projecting
  // without clamping puts it at distance zero and inserts a corner where
  // nobody clicked.
  EXPECT_FALSE(nearestEdge(m_model->domain(), QPointF(0.0, -150.0), 10.0).hit);

  // The same edge, hit where it exists.
  const DomainSnap on =
    nearestEdge(m_model->domain(), QPointF(-200.0, -152.0), 10.0);
  ASSERT_TRUE(on.hit);
  EXPECT_EQ(on.point, QPointF(-200.0, -150.0));
}

TEST_F(DomainEditTest, ARingIsSearchedRoundItsClosingEdgeAndALineIsNot)
{
  // The closing edge is drawn, so a user who can see it can click it, and
  // the corner it takes is appended to the end of the ring.
  const DomainSnap closing =
    nearestEdge(m_model->domain(), QPointF(-305.0, 0.0), 10.0);

  ASSERT_TRUE(closing.hit);
  EXPECT_EQ(closing.at.part, DomainPart::Boundary);
  EXPECT_EQ(closing.at.vertex, 4) << "the closing edge appends, it does not "
                                     "insert at the front";
  EXPECT_EQ(closing.point, QPointF(-300.0, 0.0));

  // A breakline has two ends and no edge between them. (5,0) is five units
  // from the segment a closed version would draw and sixty-seven from either
  // real one.
  EXPECT_FALSE(nearestEdge(m_model->domain(), QPointF(5.0, 0.0), 10.0).hit);
}

TEST_F(DomainEditTest, AnEdgeHitAddressesTheCornerItWouldBecome)
{
  // Between vertices 1 and 2, so the new corner is vertex 2 and the old one
  // moves along. An off-by-one here inserts the corner on the neighbouring
  // edge, which is a shape nobody drew.
  const DomainSnap hit =
    nearestEdge(m_model->domain(), QPointF(-98.0, 0.0), 10.0);

  ASSERT_TRUE(hit.hit);
  EXPECT_EQ(hit.at.part, DomainPart::Boundary);
  EXPECT_EQ(hit.at.vertex, 2);
}

TEST_F(DomainEditTest, AForcedPointHasNoEdgeToPutACornerOn)
{
  MeshDomain domain;
  domain.points.append(QPointF(10.0, 10.0));
  domain.points.append(QPointF(20.0, 10.0));

  // Standing on one of them: a search that treated the point list as a ring
  // would report an edge between two points that are not joined by anything.
  EXPECT_FALSE(nearestEdge(domain, QPointF(10.0, 10.0), 10.0).hit);
  EXPECT_FALSE(nearestEdge(domain, QPointF(15.0, 10.0), 10.0).hit);
}

// ── the model's vertex operations ───────────────────────────────────────────

TEST_F(DomainEditTest, MovingAVertexMovesItAndSaysSoOnce)
{
  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  ASSERT_TRUE(m_model->moveVertex(DomainVertex{DomainPart::Holes, 0, 2},
                                  QPointF(-210.0, 60.0)));

  EXPECT_EQ(m_model->domain().holes.first().at(2), QPointF(-210.0, 60.0));
  EXPECT_EQ(spy.count(), 1);

  // The neighbours are where they were: a move that rewrote the ring would
  // still put the dragged corner right.
  EXPECT_EQ(m_model->domain().holes.first().at(0), QPointF(-250.0, -50.0));
  EXPECT_EQ(m_model->domain().holes.first().size(), 3);
}

TEST_F(DomainEditTest, MovingAVertexNowhereIsNotAChange)
{
  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  // A drag writes a position on every mouse move, and the pointer spends
  // most of a slow drag inside one pixel: announcing those would repaint the
  // map for nothing.
  EXPECT_FALSE(m_model->moveVertex(DomainVertex{DomainPart::Boundary, 0, 0},
                                   QPointF(-300.0, -150.0)));
  EXPECT_EQ(spy.count(), 0);

  // And an address that names nothing is refused rather than growing a shape.
  EXPECT_FALSE(m_model->moveVertex(DomainVertex{DomainPart::Boundary, 0, 9},
                                   QPointF(0.0, 0.0)));
  EXPECT_FALSE(m_model->moveVertex(DomainVertex{}, QPointF(0.0, 0.0)));
  EXPECT_EQ(spy.count(), 0);
  EXPECT_EQ(m_model->domain().boundary.size(), 4);
}

TEST_F(DomainEditTest, AForcedPointIsMovedByItsOwnAddress)
{
  // A point is a shape of one vertex, not a vertex of one shape: folding it
  // in with the rings would move points[0]'s *vertex* 0 by writing into a
  // ring that does not exist.
  ASSERT_TRUE(m_model->moveVertex(DomainVertex{DomainPart::ForcedPoints, 0, 0},
                                  QPointF(320.0, -120.0)));

  EXPECT_EQ(m_model->domain().points.first(), QPointF(320.0, -120.0));
  EXPECT_EQ(m_model->domain().points.size(), 1);
}

TEST_F(DomainEditTest, InsertingOnePastTheLastAppends)
{
  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  ASSERT_TRUE(m_model->insertVertex(DomainVertex{DomainPart::Boundary, 0, 4},
                                    QPointF(-300.0, 0.0)));

  const QPolygonF &ring = m_model->domain().boundary;
  ASSERT_EQ(ring.size(), 5);
  EXPECT_EQ(ring.last(), QPointF(-300.0, 0.0));
  EXPECT_EQ(ring.first(), QPointF(-300.0, -150.0));
  EXPECT_EQ(spy.count(), 1);
}

TEST_F(DomainEditTest, AnInsertOutsideTheShapeIsRefused)
{
  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  // Two past the last is not an append, and a shape that is not there is not
  // a shape.
  EXPECT_FALSE(m_model->insertVertex(DomainVertex{DomainPart::Boundary, 0, 6},
                                     QPointF(0.0, 0.0)));
  EXPECT_FALSE(m_model->insertVertex(DomainVertex{DomainPart::Holes, 7, 0},
                                     QPointF(0.0, 0.0)));

  EXPECT_EQ(spy.count(), 0);
  EXPECT_EQ(m_model->domain().boundary.size(), 4);
  EXPECT_EQ(m_model->domain().holes.size(), 1);
}

TEST_F(DomainEditTest, ACornerComesOutOfAShapeThatCanSpareOne)
{
  ASSERT_TRUE(m_model->removeVertex(DomainVertex{DomainPart::Boundary, 0, 1}));

  const QPolygonF &ring = m_model->domain().boundary;
  ASSERT_EQ(ring.size(), 3);

  // The survivors keep their order: a removal that rebuilt the ring would
  // count right and describe different ground.
  EXPECT_EQ(ring.at(0), QPointF(-300.0, -150.0));
  EXPECT_EQ(ring.at(1), QPointF(-100.0, 150.0));
  EXPECT_EQ(ring.at(2), QPointF(-300.0, 150.0));
}

TEST_F(DomainEditTest, AShapeLeftBelowItsMinimumGoesRatherThanStayBroken)
{
  // A second hole, so the gate can tell "the right hole went" from "a hole
  // went".
  m_model->addHole(QPolygonF({QPointF(-280.0, 100.0), QPointF(-260.0, 100.0),
                              QPointF(-270.0, 120.0)}));
  ASSERT_EQ(m_model->domain().holes.size(), 2);

  ASSERT_TRUE(m_model->removeVertex(DomainVertex{DomainPart::Holes, 0, 1}));

  ASSERT_EQ(m_model->domain().holes.size(), 1)
    << "two corners is not a hole, and it cannot be seen to be broken";
  EXPECT_EQ(m_model->domain().holes.first().first(), QPointF(-280.0, 100.0))
    << "the hole that went is not the one the address named";

  // The rest of the domain is untouched: removing a hole is not a redraw.
  EXPECT_EQ(m_model->domain().boundary.size(), 4);
  EXPECT_EQ(m_model->domain().constraintLines.size(), 1);
}

TEST_F(DomainEditTest, TheBoundaryIsClearedRatherThanLeftTwoCornered)
{
  ASSERT_TRUE(m_model->removeVertex(DomainVertex{DomainPart::Boundary, 0, 0}));
  ASSERT_EQ(m_model->domain().boundary.size(), 3);

  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);
  ASSERT_TRUE(m_model->removeVertex(DomainVertex{DomainPart::Boundary, 0, 0}));

  EXPECT_TRUE(m_model->domain().boundary.isEmpty());
  EXPECT_EQ(spy.count(), 1);

  // And the boundary being gone is not the domain being gone.
  EXPECT_EQ(m_model->domain().holes.size(), 1);
}

TEST_F(DomainEditTest, ALineCanLoseACornerAndStillBeALine)
{
  // Two is a line, so the count a shape has to keep is per part and not the
  // ring's three: a table that demanded three everywhere would delete this
  // breakline instead of straightening it.
  ASSERT_TRUE(m_model->removeVertex(DomainVertex{DomainPart::Breaklines, 0,
                                                 1}));

  ASSERT_EQ(m_model->domain().constraintLines.size(), 1);
  ASSERT_EQ(m_model->domain().constraintLines.first().size(), 2);
  EXPECT_EQ(m_model->domain().constraintLines.first().at(1),
            QPointF(0.0, 100.0));

  // One is not, so the next removal takes the line.
  ASSERT_TRUE(m_model->removeVertex(DomainVertex{DomainPart::Breaklines, 0,
                                                 0}));
  EXPECT_TRUE(m_model->domain().constraintLines.isEmpty());
}

TEST_F(DomainEditTest, AForcedPointIsRemovedByItsOwnAddress)
{
  m_model->addPoint(QPointF(320.0, 100.0));
  ASSERT_EQ(m_model->domain().points.size(), 2);

  ASSERT_TRUE(m_model->removeVertex(DomainVertex{DomainPart::ForcedPoints, 0,
                                                 0}));

  ASSERT_EQ(m_model->domain().points.size(), 1);
  EXPECT_EQ(m_model->domain().points.first(), QPointF(320.0, 100.0))
    << "the point that went is not the one the address named";
}

TEST_F(DomainEditTest, RemovingAVertexThatIsNotThereIsRefusedQuietly)
{
  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  EXPECT_FALSE(m_model->removeVertex(DomainVertex{DomainPart::Holes, 0, 5}));
  EXPECT_FALSE(m_model->removeVertex(DomainVertex{DomainPart::ForcedPoints, 3,
                                                  0}));
  EXPECT_FALSE(m_model->removeVertex(DomainVertex{}));

  EXPECT_EQ(spy.count(), 0);
  EXPECT_EQ(m_model->domain().holes.first().size(), 3);
  EXPECT_EQ(m_model->domain().points.size(), 1);
}

// ── the gesture ─────────────────────────────────────────────────────────────

TEST_F(DomainEditTest, DraggingAVertexMovesIt)
{
  useTool();

  drag(screenFor(QPointF(-300.0, -150.0)), screenFor(QPointF(-250.0, -100.0)));

  const QPolygonF &ring = m_model->domain().boundary;
  ASSERT_EQ(ring.size(), 4) << "the drag added or dropped a corner";
  EXPECT_EQ(ring.at(0), QPointF(-250.0, -100.0));
  EXPECT_EQ(ring.at(1), QPointF(-100.0, -150.0));
}

TEST_F(DomainEditTest, TheDomainMovesDuringTheDragNotACopyCommittedAtTheEnd)
{
  useTool();

  press(screenFor(QPointF(-300.0, -150.0)));
  moveTo(screenFor(QPointF(-280.0, -130.0)), Qt::LeftButton);

  // Mid-gesture, before any release: what is on the map is the domain, so a
  // layer that re-reads the model is already right and cannot disagree with
  // a preview drawn beside it.
  EXPECT_EQ(m_model->domain().boundary.at(0), QPointF(-280.0, -130.0));

  moveTo(screenFor(QPointF(-260.0, -110.0)), Qt::LeftButton);
  EXPECT_EQ(m_model->domain().boundary.at(0), QPointF(-260.0, -110.0));

  release(screenFor(QPointF(-260.0, -110.0)));
  EXPECT_EQ(m_model->domain().boundary.at(0), QPointF(-260.0, -110.0));
}

TEST_F(DomainEditTest, AShortDragIsNotPinnedByTheVertexsOwnSnap)
{
  useTool();

  // Three units, well inside the ten-unit reach. Without the exclusion the
  // dragged corner is always the nearest vertex to the pointer, so it snaps
  // back onto itself and small corrections — the commonest edit there is —
  // are impossible.
  drag(screenFor(QPointF(-300.0, -150.0)), screenFor(QPointF(-297.0, -147.0)));

  EXPECT_EQ(m_model->domain().boundary.at(0), QPointF(-297.0, -147.0));
}

TEST_F(DomainEditTest, ADragOntoAnotherVertexLandsExactlyOnIt)
{
  useTool();

  // Released four and a half units from the hole's first corner: on the map
  // that gap is invisible, and it is a gap the triangulator meshes through.
  drag(screenFor(QPointF(-100.0, -150.0)), screenFor(QPointF(-154.0, -52.0)));

  EXPECT_EQ(m_model->domain().boundary.at(1), QPointF(-150.0, -50.0))
    << "the drag landed where the pointer was, not on the vertex it reached";
}

TEST_F(DomainEditTest, TheSnapReachIsAPreferenceReadOnEveryDrag)
{
  PreferencesManager *prefs = PreferencesManager::instance();
  prefs->resetToDefaults();
  useTool();

  // Twelve units short of the hole's corner: beyond the default ten, so
  // the drag lands where the pointer was...
  drag(screenFor(QPointF(-100.0, -150.0)), screenFor(QPointF(-162.0, -50.0)));
  EXPECT_EQ(m_model->domain().boundary.at(1), QPointF(-162.0, -50.0))
    << "twelve units snapped at a ten-pixel reach";

  // ...and within a widened reach, the same release snaps onto it.
  prefs->setSnapTolerancePixels(20.0);
  drag(screenFor(QPointF(-162.0, -50.0)), screenFor(QPointF(-162.0, -50.0) + QPointF(0.0, 1.0)));
  EXPECT_EQ(m_model->domain().boundary.at(1), QPointF(-150.0, -50.0))
    << "widening the reach did not apply to the next drag";

  prefs->resetToDefaults();
}

TEST_F(DomainEditTest, ClickingAnEdgeInsertsACornerAndTheSameDragMovesIt)
{
  useTool();

  // Two units below the bottom edge, a hundred from either of its corners.
  press(screenFor(QPointF(-200.0, -152.0)));

  ASSERT_EQ(m_model->domain().boundary.size(), 5);
  EXPECT_EQ(m_model->domain().boundary.at(1), QPointF(-200.0, -150.0))
    << "the corner landed under the pointer rather than on the edge";

  moveTo(screenFor(QPointF(-200.0, -100.0)), Qt::LeftButton);
  release(screenFor(QPointF(-200.0, -100.0)));

  const QPolygonF &ring = m_model->domain().boundary;
  ASSERT_EQ(ring.size(), 5) << "the drag inserted a second corner";
  EXPECT_EQ(ring.at(1), QPointF(-200.0, -100.0))
    << "the new corner was not the one the same gesture went on to drag";
  EXPECT_EQ(ring.at(0), QPointF(-300.0, -150.0));
  EXPECT_EQ(ring.at(2), QPointF(-100.0, -150.0));
}

TEST_F(DomainEditTest, RightClickingAVertexTakesItOut)
{
  useTool();

  press(screenFor(QPointF(-100.0, 150.0)), Qt::RightButton);
  release(screenFor(QPointF(-100.0, 150.0)), Qt::RightButton);

  const QPolygonF &ring = m_model->domain().boundary;
  ASSERT_EQ(ring.size(), 3);
  EXPECT_EQ(ring.at(2), QPointF(-300.0, 150.0));
}

TEST_F(DomainEditTest, ARightClickThatMissedIsTakenAndChangesNothing)
{
  useTool();

  // Taken so it cannot fall through to whatever the canvas would otherwise
  // make of it — a right-click that reached the widget while an editor is
  // running is a context menu nobody asked for.
  EXPECT_TRUE(press(QPoint(750, 50), Qt::RightButton));
  release(QPoint(750, 50), Qt::RightButton);

  EXPECT_EQ(m_model->domain().boundary.size(), 4);
  EXPECT_EQ(m_model->domain().holes.size(), 1);
}

TEST_F(DomainEditTest, AClickOnNothingStartsNoDrag)
{
  useTool();

  EXPECT_FALSE(press(QPoint(750, 50)));

  moveTo(screenFor(QPointF(-300.0, -150.0)), Qt::LeftButton);
  release(screenFor(QPointF(-300.0, -150.0)));

  // Nothing was grabbed, so the pointer passing over a corner on its way is
  // not a drag of that corner.
  EXPECT_EQ(m_model->domain().boundary.at(0), QPointF(-300.0, -150.0));
  EXPECT_EQ(m_model->domain().boundary.size(), 4);
}

TEST_F(DomainEditTest, AnInterruptedDragPutsTheVertexBack)
{
  useTool();

  press(screenFor(QPointF(-300.0, -150.0)));
  moveTo(screenFor(QPointF(-250.0, -100.0)), Qt::LeftButton);
  ASSERT_EQ(m_model->domain().boundary.at(0), QPointF(-250.0, -100.0));

  // Reaching for the ribbon mid-drag is not an edit: the vertex following
  // the pointer to wherever it happened to be is a change nobody made.
  m_canvas->setToolKind(MapToolKind::Pan);

  EXPECT_EQ(m_model->domain().boundary.at(0), QPointF(-300.0, -150.0));
  EXPECT_EQ(m_model->domain().boundary.size(), 4);
}

// ── the handles ─────────────────────────────────────────────────────────────

TEST_F(DomainEditTest, PickingUpTheEditorOffersEveryVertexAsAHandle)
{
  useTool();

  // Published by picking the tool up, not by the first click: the handles
  // are how the user is told there is anything to grab.
  // Asserted, not expected: the two reads below index this list, and a test
  // that crashes on a tool publishing nothing reports the defect as a
  // segfault rather than as the gate it is.
  ASSERT_EQ(m_canvas->vertexHandles().size(), 11);
  EXPECT_EQ(m_canvas->activeVertexHandle(), -1);
  EXPECT_EQ(m_canvas->vertexHandles().at(0), QPointF(-300.0, -150.0));
  EXPECT_EQ(m_canvas->vertexHandles().at(10), QPointF(300.0, -150.0));
}

TEST_F(DomainEditTest, TheHandleUnderThePointerIsTheOneMarked)
{
  useTool();

  // The hole's second corner: index 5 of the walk, so a marker taken from
  // the shape's own index would point at the boundary instead.
  moveTo(screenFor(QPointF(-150.0, -50.0)));
  EXPECT_EQ(m_canvas->activeVertexHandle(), 5);

  moveTo(screenFor(QPointF(300.0, -150.0)));
  EXPECT_EQ(m_canvas->activeVertexHandle(), 10);

  moveTo(QPoint(750, 50));
  EXPECT_EQ(m_canvas->activeVertexHandle(), -1)
    << "the mark stayed on a vertex the pointer had left";
}

TEST_F(DomainEditTest, TheHandlesFollowTheDomainAsItIsEdited)
{
  useTool();

  drag(screenFor(QPointF(-300.0, -150.0)), screenFor(QPointF(-250.0, -100.0)));

  ASSERT_EQ(m_canvas->vertexHandles().size(), 11);
  EXPECT_EQ(m_canvas->vertexHandles().at(0), QPointF(-250.0, -100.0))
    << "the handle stayed where the vertex used to be";

  press(screenFor(QPointF(-250.0, -100.0)), Qt::RightButton);
  release(screenFor(QPointF(-250.0, -100.0)), Qt::RightButton);

  EXPECT_EQ(m_canvas->vertexHandles().size(), 10)
    << "a removed vertex left a handle behind for the user to grab";
}

TEST_F(DomainEditTest, PuttingTheEditorDownTakesItsHandlesWithIt)
{
  useTool();
  ASSERT_FALSE(m_canvas->vertexHandles().isEmpty());

  m_canvas->setToolKind(MapToolKind::Pan);

  // Handles belong to the tool, not to the data: leaving them on the map
  // offers the user something to grab that nothing is listening for.
  EXPECT_TRUE(m_canvas->vertexHandles().isEmpty());
  EXPECT_EQ(m_canvas->activeVertexHandle(), -1);
}
