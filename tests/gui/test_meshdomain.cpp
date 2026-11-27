/*!
 * \file   test_meshdomain.cpp
 * \brief  E1a verification — the ground a mesh will be generated over.
 *
 * The gates are on arithmetic with known answers — signed areas, vertex
 * counts, segment counts, orientations — because every way a domain can be
 * wrong still looks like a domain. A boundary of three collinear points has
 * a bounding box, a vertex count and no ground inside it. A boundary drawn
 * clockwise is a perfectly good ring that triangulates to nothing. A hole
 * outside the boundary cuts nothing and leaves a mesh that looks whole
 * because it is.
 *
 * The round-trip tests write real files next to the other fixtures, so what
 * was checked can be opened and read rather than taken on trust.
 */

#include "core/composerapplication.h"
#include "mesh/meshdomain.h"
#include "project/presentation.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include <cmath>
#include <limits>

using namespace HydroCouple::Composer;
namespace Tools = HydroCouple::SDK::Tools;

namespace
{
  //! A unit square, counter-clockwise, first point not repeated.
  QPolygonF square(double x = 0.0, double y = 0.0, double side = 1.0)
  {
    return QPolygonF({QPointF(x, y), QPointF(x + side, y),
                      QPointF(x + side, y + side), QPointF(x, y + side)});
  }

  //! A domain that passes every check, for mutating one field at a time.
  MeshDomain goodDomain()
  {
    MeshDomain domain;
    domain.boundary = square(0.0, 0.0, 10.0);
    domain.holes.append(square(2.0, 2.0, 1.0));
    domain.constraintLines.append(
      QPolygonF({QPointF(1.0, 5.0), QPointF(5.0, 5.0), QPointF(9.0, 6.0)}));
    domain.points.append(QPointF(7.0, 7.0));
    domain.maxEdgeLength = 0.5;

    return domain;
  }

  class MeshDomainTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_meshdomain";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *MeshDomainTest::s_app = nullptr;
}

// ── orientation and area ────────────────────────────────────────────────────

TEST_F(MeshDomainTest, SignedAreaKnowsWhichWayARingWasDrawn)
{
  // Twice the area, signed: a unit square is 2, and the same square walked
  // the other way is -2. Both are the same ground.
  // Offset from the origin on purpose. For a unit square at (0,0) the
  // closing edge contributes exactly nothing to the shoelace sum, so a
  // version that forgot to close the ring gets the right answer there and
  // is wrong everywhere else. This rectangle is 4 by 3 at (1,1): area 12,
  // twice-area 24, and the closing edge contributes -3 of it.
  const QPolygonF counterClockwise({QPointF(1.0, 1.0), QPointF(5.0, 1.0),
                                    QPointF(5.0, 4.0), QPointF(1.0, 4.0)});
  QPolygonF clockwise = counterClockwise;
  std::reverse(clockwise.begin(), clockwise.end());

  EXPECT_DOUBLE_EQ(signedDoubleArea(counterClockwise), 24.0);
  EXPECT_DOUBLE_EQ(signedDoubleArea(clockwise), -24.0);

  // Three collinear points: a real bounding box, three vertices, no area.
  const QPolygonF collinear(
    {QPointF(0.0, 0.0), QPointF(1.0, 1.0), QPointF(2.0, 2.0)});
  EXPECT_DOUBLE_EQ(signedDoubleArea(collinear), 0.0);
}

TEST_F(MeshDomainTest, ABoundaryDrawnClockwiseIsHandedOverCounterClockwise)
{
  // The triangulator requires counter-clockwise. Drawing the other way round
  // is not an error worth telling anyone about — it is the same ground,
  // walked the other way — but handing it over unturned produces no mesh at
  // all, from a domain that passed every check.
  MeshDomain domain;
  domain.boundary = square(0.0, 0.0, 4.0);
  std::reverse(domain.boundary.begin(), domain.boundary.end());

  ASSERT_LT(signedDoubleArea(domain.boundary), 0.0)
    << "the fixture is not actually clockwise";

  const Tools::TriangulationInput input = domain.toTriangulationInput();

  ASSERT_EQ(input.boundary.size(), 4u);

  QPolygonF handedOver;

  for (const auto &point : input.boundary)
  {
    handedOver.append(QPointF(point.x, point.y));
  }

  EXPECT_GT(signedDoubleArea(handedOver), 0.0)
    << "a clockwise boundary was handed to the triangulator unturned";

  // And it is the same ground, not a different ring: same area, opposite sign.
  EXPECT_DOUBLE_EQ(std::abs(signedDoubleArea(handedOver)),
                   std::abs(signedDoubleArea(domain.boundary)));
}

TEST_F(MeshDomainTest, ABoundaryAlreadyCounterClockwiseIsLeftAlone)
{
  // The other half, and the half a clockwise fixture cannot test: a
  // converter that reversed unconditionally turns every correctly-drawn
  // boundary into a clockwise one, and satisfies "the output is
  // counter-clockwise" for exactly the input that was drawn backwards.
  MeshDomain domain;
  domain.boundary = square(0.0, 0.0, 4.0);

  ASSERT_GT(signedDoubleArea(domain.boundary), 0.0)
    << "the fixture is not actually counter-clockwise";

  const Tools::TriangulationInput input = domain.toTriangulationInput();
  ASSERT_EQ(input.boundary.size(), 4u);

  QPolygonF handedOver;

  for (const auto &point : input.boundary)
  {
    handedOver.append(QPointF(point.x, point.y));
  }

  EXPECT_GT(signedDoubleArea(handedOver), 0.0)
    << "a counter-clockwise boundary was turned around";

  // Vertex for vertex, in the order drawn: reversing and then reversing back
  // would satisfy the sign check while renumbering every corner.
  for (int index = 0; index < domain.boundary.size(); ++index)
  {
    EXPECT_DOUBLE_EQ(handedOver.at(index).x(), domain.boundary.at(index).x());
    EXPECT_DOUBLE_EQ(handedOver.at(index).y(), domain.boundary.at(index).y());
  }
}

// ── what is refused ─────────────────────────────────────────────────────────

TEST_F(MeshDomainTest, ABoundaryWithNoAreaIsRefused)
{
  MeshDomain domain;
  domain.boundary =
    QPolygonF({QPointF(0.0, 0.0), QPointF(1.0, 1.0), QPointF(2.0, 2.0)});

  QString message;
  EXPECT_FALSE(domain.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("no area")))
    << message.toStdString();

  // Distinct from too few points, which is a different thing to tell a user.
  MeshDomain sparse;
  sparse.boundary = QPolygonF({QPointF(0.0, 0.0), QPointF(1.0, 0.0)});
  EXPECT_FALSE(sparse.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("three points")))
    << message.toStdString();
}

TEST_F(MeshDomainTest, AHoleOutsideTheBoundaryIsRefused)
{
  // Not degenerate — a perfectly good ring, in the wrong place. It cuts
  // nothing, so the mesh comes out whole and looks entirely correct.
  MeshDomain domain = goodDomain();
  domain.holes.clear();
  domain.holes.append(square(50.0, 50.0, 1.0));

  QString message;
  EXPECT_FALSE(domain.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("outside the boundary")))
    << message.toStdString();
}

TEST_F(MeshDomainTest, ADegenerateHoleOrBreaklineIsRefused)
{
  QString message;

  MeshDomain flatHole = goodDomain();
  flatHole.holes.clear();
  flatHole.holes.append(
    QPolygonF({QPointF(1.0, 1.0), QPointF(2.0, 2.0), QPointF(3.0, 3.0)}));
  EXPECT_FALSE(flatHole.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("no area")))
    << message.toStdString();

  MeshDomain shortLine = goodDomain();
  shortLine.constraintLines.clear();
  shortLine.constraintLines.append(QPolygonF({QPointF(1.0, 1.0)}));
  EXPECT_FALSE(shortLine.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("two points")))
    << message.toStdString();

  // Two points in the same place: the count a line needs, and no direction
  // to constrain anything along.
  MeshDomain pointLine = goodDomain();
  pointLine.constraintLines.clear();
  pointLine.constraintLines.append(
    QPolygonF({QPointF(3.0, 3.0), QPointF(3.0, 3.0)}));
  EXPECT_FALSE(pointLine.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("same point")))
    << message.toStdString();
}

TEST_F(MeshDomainTest, ACoordinateThatIsNotANumberIsRefused)
{
  QString message;

  MeshDomain domain = goodDomain();
  domain.boundary[1] =
    QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0);
  EXPECT_FALSE(domain.isValid(message));

  MeshDomain infinite = goodDomain();
  infinite.points.append(
    QPointF(std::numeric_limits<double>::infinity(), 1.0));
  EXPECT_FALSE(infinite.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("not a number")))
    << message.toStdString();

  MeshDomain negative = goodDomain();
  negative.maxEdgeLength = -1.0;
  EXPECT_FALSE(negative.isValid(message));
}

TEST_F(MeshDomainTest, AGoodDomainPasses)
{
  // The control. Without it every refusal above is satisfied by a validator
  // that refuses everything.
  QString message;
  EXPECT_TRUE(goodDomain().isValid(message)) << message.toStdString();
  EXPECT_TRUE(message.isEmpty());
}

// ── conversion to the triangulator's input ──────────────────────────────────

TEST_F(MeshDomainTest, ABreaklineBecomesItsSegmentsNotItsPoints)
{
  // Three points is two segments. A polyline handed over as three
  // independent segments, or as one, constrains different ground.
  MeshDomain domain = goodDomain();

  const Tools::TriangulationInput input = domain.toTriangulationInput();

  ASSERT_EQ(input.constraintSegments.size(), 2u);

  EXPECT_DOUBLE_EQ(input.constraintSegments[0][0].x, 1.0);
  EXPECT_DOUBLE_EQ(input.constraintSegments[0][1].x, 5.0);
  EXPECT_DOUBLE_EQ(input.constraintSegments[1][0].x, 5.0);
  EXPECT_DOUBLE_EQ(input.constraintSegments[1][1].x, 9.0);

  // The segments are consecutive: the end of one is the start of the next.
  EXPECT_DOUBLE_EQ(input.constraintSegments[0][1].y,
                   input.constraintSegments[1][0].y);

  EXPECT_EQ(input.holes.size(), 1u);
  EXPECT_EQ(input.interiorPoints.size(), 1u);
  EXPECT_DOUBLE_EQ(input.maxEdgeLength, 0.5);
}

TEST_F(MeshDomainTest, TwoBreaklinesThatTouchStayTwoBreaklines)
{
  // Stored as polylines rather than as a pool of segments, so an editor can
  // tell one line from another. Two three-point lines are four segments,
  // not one six-point line's five.
  MeshDomain domain;
  domain.boundary = square(0.0, 0.0, 10.0);
  domain.constraintLines.append(
    QPolygonF({QPointF(1.0, 1.0), QPointF(4.0, 4.0), QPointF(5.0, 5.0)}));
  domain.constraintLines.append(
    QPolygonF({QPointF(5.0, 5.0), QPointF(6.0, 6.0), QPointF(9.0, 9.0)}));

  EXPECT_EQ(domain.toTriangulationInput().constraintSegments.size(), 4u);
}

// ── persistence ─────────────────────────────────────────────────────────────

TEST_F(MeshDomainTest, ADomainSurvivesTheRoundTrip)
{
  const MeshDomain before = goodDomain();

  MeshDomain after;
  QString message;
  ASSERT_TRUE(MeshDomain::fromJson(before.toJson(), after, message))
    << message.toStdString();

  ASSERT_EQ(after.boundary.size(), before.boundary.size());
  ASSERT_EQ(after.holes.size(), before.holes.size());
  ASSERT_EQ(after.constraintLines.size(), before.constraintLines.size());
  ASSERT_EQ(after.points.size(), before.points.size());
  EXPECT_DOUBLE_EQ(after.maxEdgeLength, before.maxEdgeLength);

  // Coordinates, not just counts: a round trip that kept the shape of the
  // domain and moved its corners is the failure worth catching.
  for (int index = 0; index < before.boundary.size(); ++index)
  {
    EXPECT_DOUBLE_EQ(after.boundary.at(index).x(),
                     before.boundary.at(index).x());
    EXPECT_DOUBLE_EQ(after.boundary.at(index).y(),
                     before.boundary.at(index).y());
  }

  EXPECT_DOUBLE_EQ(after.holes.first().at(2).x(),
                   before.holes.first().at(2).x());
  EXPECT_DOUBLE_EQ(after.constraintLines.first().at(1).y(),
                   before.constraintLines.first().at(1).y());
  EXPECT_DOUBLE_EQ(after.points.first().x(), before.points.first().x());

  // And the orientation survived, so a boundary does not flip every save.
  EXPECT_DOUBLE_EQ(signedDoubleArea(after.boundary),
                   signedDoubleArea(before.boundary));
}

TEST_F(MeshDomainTest, AMalformedDomainIsRefusedRatherThanHalfRead)
{
  QJsonObject broken;
  broken.insert(QStringLiteral("boundary"),
                QJsonArray({QJsonArray({0.0, 0.0}), QJsonArray({1.0})}));

  MeshDomain domain;
  QString message;
  EXPECT_FALSE(MeshDomain::fromJson(broken, domain, message));
  EXPECT_TRUE(domain.isEmpty())
    << "a domain that failed to parse kept part of what it read";

  // A failure *after* something was read successfully, which is the case a
  // broken boundary cannot reach: the boundary here is perfectly good, and
  // a truncated file must not load as a smaller domain that opens fine.
  QJsonObject lateFailure;
  lateFailure.insert(QStringLiteral("boundary"),
                     QJsonArray({QJsonArray({0.0, 0.0}),
                                 QJsonArray({4.0, 0.0}),
                                 QJsonArray({4.0, 4.0})}));
  lateFailure.insert(QStringLiteral("holes"),
                     QJsonArray({QJsonArray({QJsonArray({1.0, 1.0}),
                                             QJsonArray({2.0})})}));

  MeshDomain partial;
  EXPECT_FALSE(MeshDomain::fromJson(lateFailure, partial, message));
  EXPECT_TRUE(partial.isEmpty())
    << "a domain whose holes failed kept the boundary it had already read";
  EXPECT_TRUE(partial.boundary.isEmpty());
}

TEST_F(MeshDomainTest, TheSidecarCarriesTheDomainBesideTheCanvasPositions)
{
  Presentation before;
  before.setComponent(QStringLiteral("solver"),
                      ComponentPresentation{QPointF(12.0, 34.0)});
  before.setMeshDomain(goodDomain());

  const QByteArray json = before.toJson();

  Presentation after;
  ASSERT_TRUE(after.fromJson(json));

  EXPECT_TRUE(after.hasComponent(QStringLiteral("solver")));
  EXPECT_DOUBLE_EQ(after.component(QStringLiteral("solver")).position.x(),
                   12.0);

  ASSERT_TRUE(after.hasMeshDomain());
  EXPECT_EQ(after.meshDomain().boundary.size(), 4);
  EXPECT_EQ(after.meshDomain().holes.size(), 1);
  EXPECT_DOUBLE_EQ(after.meshDomain().maxEdgeLength, 0.5);

  // Written to a real file, so what this checked can be opened and read.
  const QString path = QStringLiteral(COMPOSER_MESH_FIXTURE_DIR)
                       + QStringLiteral("/domain.composer.json");
  QDir().mkpath(QStringLiteral(COMPOSER_MESH_FIXTURE_DIR));

  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly)) << path.toStdString();
  file.write(json);
  file.close();

  EXPECT_GT(QFileInfo(path).size(), 0);
}

TEST_F(MeshDomainTest, ASidecarWithOnlyADomainIsStillWorthWriting)
{
  // Saving is gated on isEmpty(), so a domain drawn over a composition that
  // has no components yet — which is the order anyone building a model
  // actually works in — would be written nowhere and gone at the next open.
  Presentation presentation;
  EXPECT_TRUE(presentation.isEmpty());

  presentation.setMeshDomain(goodDomain());

  EXPECT_FALSE(presentation.isEmpty())
    << "a sidecar holding only a mesh domain reported itself empty, so it "
       "would never be written";
}

TEST_F(MeshDomainTest, ASidecarWithNoDomainSaysNothingAboutMeshes)
{
  // Absent, not empty: a section written unconditionally reads as a domain
  // somebody drew and then cleared.
  Presentation presentation;
  presentation.setComponent(QStringLiteral("solver"),
                            ComponentPresentation{QPointF(1.0, 2.0)});

  EXPECT_FALSE(presentation.toJson().contains(QByteArray("mesh_domain")));

  Presentation reopened;
  ASSERT_TRUE(reopened.fromJson(presentation.toJson()));
  EXPECT_FALSE(reopened.hasMeshDomain());
}

TEST_F(MeshDomainTest, ASidecarWhoseDomainWillNotParseKeepsItsPositions)
{
  // The canvas positions beside a broken mesh section are still good, and
  // throwing them away over a domain would cost more than it saved.
  const QByteArray json = QByteArrayLiteral(
    "{\"sidecar_version\":1,"
    "\"components\":{\"solver\":{\"x\":5.0,\"y\":6.0}},"
    "\"mesh_domain\":{\"boundary\":[[0.0],[1.0,1.0]]}}");

  Presentation presentation;
  ASSERT_TRUE(presentation.fromJson(json));

  EXPECT_TRUE(presentation.hasComponent(QStringLiteral("solver")));
  EXPECT_DOUBLE_EQ(presentation.component(QStringLiteral("solver")).position.y(),
                   6.0);
  EXPECT_FALSE(presentation.hasMeshDomain());
}
