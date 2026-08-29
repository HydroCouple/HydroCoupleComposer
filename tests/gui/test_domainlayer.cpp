/*!
 * \file   test_domainlayer.cpp
 * \brief  E1b-1 verification — a mesh domain drawn on the map.
 *
 * Two things are pinned. One is that the layers are *views*: the domain
 * lives in one model, and four layers re-read it rather than keeping four
 * copies that drift apart the moment one is edited.
 *
 * The other is that each layer is homogeneous. FeatureLayer reports the kind
 * of the last feature added and branches its hit-testing on that, so a
 * single layer holding rings, lines and points renders perfectly and picks
 * wrongly — which would only surface once vertices became draggable, and
 * would look like a bug in the dragging.
 */

#include "core/composerapplication.h"
#include "layers/domainlayer.h"
#include "mesh/meshdomainmodel.h"
#include "render/layerstyle.h"

#include <gtest/gtest.h>

#include <QSignalSpy>

#include <memory>

using namespace HydroCouple::Composer;

namespace
{
  QPolygonF square(double x, double y, double side)
  {
    return QPolygonF({QPointF(x, y), QPointF(x + side, y),
                      QPointF(x + side, y + side), QPointF(x, y + side)});
  }

  //! A domain with one of everything, each a different size.
  MeshDomain populated()
  {
    MeshDomain domain;
    domain.boundary = square(0.0, 0.0, 10.0);
    domain.holes.append(square(2.0, 2.0, 1.0));
    domain.holes.append(square(6.0, 6.0, 2.0));
    domain.constraintLines.append(
      QPolygonF({QPointF(1.0, 5.0), QPointF(9.0, 5.0)}));
    domain.points.append(QPointF(4.0, 4.0));
    domain.points.append(QPointF(7.0, 3.0));
    domain.points.append(QPointF(8.0, 8.0));

    return domain;
  }

  class DomainLayerTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_domainlayer";
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

  ComposerApplication *DomainLayerTest::s_app = nullptr;
}

// ── the layers are views of one model ───────────────────────────────────────

TEST_F(DomainLayerTest, EachLayerDrawsItsOwnPartAndNothingElse)
{
  MeshDomainModel model;
  model.setDomain(populated());

  const std::unique_ptr<DomainLayer> boundary =
    DomainLayer::create(&model, DomainPart::Boundary);
  const std::unique_ptr<DomainLayer> holes =
    DomainLayer::create(&model, DomainPart::Holes);
  const std::unique_ptr<DomainLayer> lines =
    DomainLayer::create(&model, DomainPart::Breaklines);
  const std::unique_ptr<DomainLayer> points =
    DomainLayer::create(&model, DomainPart::ForcedPoints);

  // Counts that differ from each other, so a layer drawing the wrong part
  // cannot pass by drawing the right number of the wrong things.
  EXPECT_EQ(boundary->featureCount(), 1);
  EXPECT_EQ(holes->featureCount(), 2);
  EXPECT_EQ(lines->featureCount(), 1);
  EXPECT_EQ(points->featureCount(), 3);
}

TEST_F(DomainLayerTest, EveryLayerIsOneGeometryKind)
{
  // The whole reason there are four layers. FeatureLayer takes its kind from
  // the last feature added and branches its picking on it, so a mixed layer
  // draws correctly and picks as whatever happened to be added last.
  MeshDomainModel model;
  model.setDomain(populated());

  EXPECT_EQ(DomainLayer::create(&model, DomainPart::Boundary)->geometryKind(),
            GeometryKind::Polygon);
  EXPECT_EQ(DomainLayer::create(&model, DomainPart::Holes)->geometryKind(),
            GeometryKind::Polygon);
  EXPECT_EQ(DomainLayer::create(&model, DomainPart::Breaklines)->geometryKind(),
            GeometryKind::Line);
  EXPECT_EQ(
    DomainLayer::create(&model, DomainPart::ForcedPoints)->geometryKind(),
    GeometryKind::Point);
}

TEST_F(DomainLayerTest, TheFeaturesCarryTheDomainsOwnCoordinates)
{
  // Counts alone would be satisfied by a layer that drew the right number of
  // features somewhere else entirely.
  MeshDomainModel model;
  model.setDomain(populated());

  const std::unique_ptr<DomainLayer> boundary =
    DomainLayer::create(&model, DomainPart::Boundary);

  ASSERT_EQ(boundary->featureCount(), 1);
  ASSERT_EQ(boundary->features().first().parts.size(), 1);

  const QPolygonF &ring = boundary->features().first().parts.first();
  ASSERT_EQ(ring.size(), 4);
  EXPECT_DOUBLE_EQ(ring.at(2).x(), 10.0);
  EXPECT_DOUBLE_EQ(ring.at(2).y(), 10.0);

  const std::unique_ptr<DomainLayer> points =
    DomainLayer::create(&model, DomainPart::ForcedPoints);

  ASSERT_EQ(points->featureCount(), 3);
  EXPECT_DOUBLE_EQ(points->features().at(1).parts.first().first().x(), 7.0);
  EXPECT_DOUBLE_EQ(points->features().at(1).parts.first().first().y(), 3.0);

  // The second hole, not the first: an index that always read zero would
  // still produce a ring of the right shape.
  const std::unique_ptr<DomainLayer> holes =
    DomainLayer::create(&model, DomainPart::Holes);
  EXPECT_DOUBLE_EQ(holes->features().at(1).parts.first().first().x(), 6.0);
}

TEST_F(DomainLayerTest, AllFourLayersFollowOneEdit)
{
  // The MVC gate. One model, four views: editing the domain updates every
  // layer showing it, without any layer knowing another exists.
  MeshDomainModel model;

  const std::unique_ptr<DomainLayer> boundary =
    DomainLayer::create(&model, DomainPart::Boundary);
  const std::unique_ptr<DomainLayer> holes =
    DomainLayer::create(&model, DomainPart::Holes);
  const std::unique_ptr<DomainLayer> points =
    DomainLayer::create(&model, DomainPart::ForcedPoints);

  ASSERT_EQ(boundary->featureCount(), 0);
  ASSERT_EQ(holes->featureCount(), 0);
  ASSERT_EQ(points->featureCount(), 0);

  model.setDomain(populated());

  EXPECT_EQ(boundary->featureCount(), 1);
  EXPECT_EQ(holes->featureCount(), 2);
  EXPECT_EQ(points->featureCount(), 3);

  // And back again: a cleared domain leaves no features standing.
  model.setDomain(MeshDomain{});

  EXPECT_EQ(boundary->featureCount(), 0);
  EXPECT_EQ(holes->featureCount(), 0);
  EXPECT_EQ(points->featureCount(), 0);
}

TEST_F(DomainLayerTest, AnEditRedrawsTheMapWithoutTheLayerBeingAsked)
{
  // Views listen to renderChanged(); the layer must actually raise it, or
  // the features are right and the canvas still shows the old domain.
  MeshDomainModel model;

  const std::unique_ptr<DomainLayer> boundary =
    DomainLayer::create(&model, DomainPart::Boundary);

  QSignalSpy redrawn(boundary.get(), &MapLayer::appearanceChanged);
  QSignalSpy reframed(boundary.get(), &MapLayer::extentChanged);

  model.setDomain(populated());

  EXPECT_GE(redrawn.count(), 1)
    << "the domain changed and the layer never asked to be redrawn";

  // And the ground it covers changed too, which is a separate claim: a
  // view that framed this layer would otherwise keep framing the domain as
  // it was drawn three edits ago.
  EXPECT_GE(reframed.count(), 1)
    << "the domain grew and the layer never said its extent had moved";

  // The extent is the domain's own, not a stale or default rectangle.
  EXPECT_DOUBLE_EQ(boundary->extent().width(), 10.0);
  EXPECT_DOUBLE_EQ(boundary->extent().height(), 10.0);
}

// ── the model ───────────────────────────────────────────────────────────────

TEST_F(DomainLayerTest, AnUnchangedDomainIsNotAnnounced)
{
  // Saving a file re-sets the domain it already holds. Announcing that would
  // repaint every view for nothing, on every save.
  MeshDomainModel model;
  model.setDomain(populated());

  QSignalSpy announced(&model, &MeshDomainModel::domainChanged);

  model.setDomain(populated());
  EXPECT_EQ(announced.count(), 0)
    << "an identical domain was announced as a change";

  // A change of any single part is a change, including one the counts do
  // not show: the same number of points, one of them moved.
  MeshDomain moved = populated();
  moved.points[1] = QPointF(7.5, 3.0);
  model.setDomain(moved);
  EXPECT_EQ(announced.count(), 1);

  // And a change to the refinement control alone, which has no geometry.
  MeshDomain refined = moved;
  refined.maxEdgeLength = 2.0;
  model.setDomain(refined);
  EXPECT_EQ(announced.count(), 2);
}

TEST_F(DomainLayerTest, TheModelAnswersForTheDomainsValidity)
{
  MeshDomainModel model;

  QString message;
  EXPECT_FALSE(model.isValid(message));

  model.setDomain(populated());
  EXPECT_TRUE(model.isValid(message)) << message.toStdString();
}

TEST_F(DomainLayerTest, ALayerOutlivingItsModelEmptiesRatherThanDangles)
{
  // Layers are owned by the stack and the model by the document, so the
  // model can go first. Reading a freed domain is the failure worth
  // preventing; an empty layer is the honest alternative.
  auto model = std::make_unique<MeshDomainModel>();
  model->setDomain(populated());

  const std::unique_ptr<DomainLayer> holes =
    DomainLayer::create(model.get(), DomainPart::Holes);

  ASSERT_EQ(holes->featureCount(), 2);

  model.reset();

  EXPECT_EQ(holes->model(), nullptr);
  EXPECT_EQ(holes->featureCount(), 0)
    << "the layer kept features read from a model that no longer exists";
}

// ── how the parts read ──────────────────────────────────────────────────────

TEST_F(DomainLayerTest, AHoleIsFilledAndABoundaryIsNot)
{
  // A hole is an absence, and the fill is what says so. A filled boundary
  // would hide every layer the domain was drawn over — which is the whole
  // reason for drawing a domain over them.
  MeshDomainModel model;
  model.setDomain(populated());

  const std::unique_ptr<DomainLayer> boundary =
    DomainLayer::create(&model, DomainPart::Boundary);
  const std::unique_ptr<DomainLayer> holes =
    DomainLayer::create(&model, DomainPart::Holes);

  EXPECT_EQ(boundary->style()->symbol().fill.alpha(), 0)
    << "the boundary is filled, so it hides what it was drawn over";
  EXPECT_GT(holes->style()->symbol().fill.alpha(), 0)
    << "a hole is not filled, so it does not read as an absence";

  // Distinct outlines too, so the two are told apart where they touch.
  EXPECT_NE(boundary->style()->symbol().stroke,
            holes->style()->symbol().stroke);
}

TEST_F(DomainLayerTest, EachPartIsNamedForWhatItIs)
{
  EXPECT_NE(DomainLayer::nameFor(DomainPart::Boundary),
            DomainLayer::nameFor(DomainPart::Holes));
  EXPECT_NE(DomainLayer::nameFor(DomainPart::Breaklines),
            DomainLayer::nameFor(DomainPart::ForcedPoints));

  MeshDomainModel model;
  EXPECT_EQ(DomainLayer::create(&model, DomainPart::Breaklines)->name(),
            DomainLayer::nameFor(DomainPart::Breaklines));
}
