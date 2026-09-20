/*!
 * \file   test_scenemarkers.cpp
 * \brief  What a point layer and a filled ring contribute to the 3D view.
 *
 * Asserted on geometry — vertex positions and counts — rather than on
 * pixels, because that is what the layer decides. The renderer's job of
 * turning it into an image is C3a's, and QRhiWidget cannot make a device
 * under the offscreen platform anyway (D31).
 */

#include "core/composerapplication.h"
#include "map/layerstackmodel.h"
#include "render/layerstyle.h"
#include "scene/scenesource.h"

#include "vectorprobe.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  //! A terrain that reports a constant height, so a drape is checkable.
  class FlatTerrain : public ITerrainSource
  {
    public:
      explicit FlatTerrain(double height) : m_height(height) {}

      [[nodiscard]] bool elevationAt(const QPointF &, double &elevation) const override
      {
        elevation = m_height;

        return true;
      }

      [[nodiscard]] QRectF terrainExtent() const override
      {
        return QRectF(-1000.0, -1000.0, 2000.0, 2000.0);
      }

      [[nodiscard]] double terrainResolution() const override { return 0.0; }

    private:
      double m_height = 0.0;
  };

  class MarkerTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_scenemarkers";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static inline ComposerApplication *s_app = nullptr;
  };
}

TEST_F(MarkerTest, APointLayerContributesToTheSceneAtAll)
{
  Testing::VectorProbe points(QStringLiteral("gauges"));
  points.addPoint({ 0.0, 0.0 });
  points.addPoint({ 100.0, 0.0 });

  // The whole point of U3a: before it, a point layer answered nullptr here
  // and a gauge network was invisible in the view showing the ground it
  // stands on.
  ASSERT_NE(points.sceneSource(), nullptr);

  SceneContext context;
  const QVector<SceneGeometry> batches = points.sceneGeometry(context);

  ASSERT_EQ(batches.size(), 1);
  EXPECT_EQ(batches.first().primitive, ScenePrimitive::Triangles);

  // Six vertices and eight faces per marker: the smallest solid that reads
  // from any direction.
  EXPECT_EQ(batches.first().vertices.size(), 2 * 6);
  EXPECT_EQ(batches.first().indices.size(), 2 * 8 * 3);
}

TEST_F(MarkerTest, AMarkerIsCentredOnItsPointAndSizedFromTheExtent)
{
  Testing::VectorProbe points(QStringLiteral("gauges"));
  points.addPoint({ 0.0, 0.0 });
  points.addPoint({ 300.0, 400.0 });

  // The extent's diagonal is 500, so the automatic size is 10.
  EXPECT_DOUBLE_EQ(points.resolvedMarkerSize(), 10.0);

  SceneContext context;
  const SceneGeometry markers = points.sceneGeometry(context).first();

  // The first marker's six vertices straddle its point by half the size.
  double minX = 1e9;
  double maxX = -1e9;

  for (int i = 0; i < 6; ++i)
  {
    minX = std::min(minX, double(markers.vertices.at(i).x));
    maxX = std::max(maxX, double(markers.vertices.at(i).x));
  }

  EXPECT_NEAR(minX, -5.0, 1e-4);
  EXPECT_NEAR(maxX, 5.0, 1e-4);
  EXPECT_NEAR((minX + maxX) / 2.0, 0.0, 1e-4) << "not centred on its point";

  // An explicit size overrides the automatic one.
  points.setMarkerSize(2.0);
  EXPECT_DOUBLE_EQ(points.resolvedMarkerSize(), 2.0);
}

TEST_F(MarkerTest, PointsInARowStillHaveAnExtentToSizeFrom)
{
  // A gauge network along a river is collinear, and a collinear extent has
  // a height of exactly zero — which QRectF::isEmpty() calls empty. Sizing
  // off that made every such layer invisible; the diagonal is what is
  // actually being asked for. The same rectangle trap as D26 and C4a.
  Testing::VectorProbe row(QStringLiteral("gauges"));
  row.addPoint({ 0.0, 0.0 });
  row.addPoint({ 100.0, 0.0 });

  ASSERT_TRUE(row.extent().isEmpty()) << "the fixture is not degenerate";
  EXPECT_DOUBLE_EQ(row.resolvedMarkerSize(), 2.0);

  SceneContext context;
  ASSERT_EQ(row.sceneGeometry(context).size(), 1);
  EXPECT_EQ(row.sceneGeometry(context).first().vertices.size(), 2 * 6);
}

TEST_F(MarkerTest, ASinglePointHasNoExtentToSizeFromAndDrawsNothing)
{
  Testing::VectorProbe one(QStringLiteral("gauge"));
  one.addPoint({ 5.0, 5.0 });

  // Guessing a size in map units would be guessing between metres and
  // degrees — a factor of a hundred thousand. Nothing is drawn until a
  // size is given, and then it is.
  EXPECT_DOUBLE_EQ(one.resolvedMarkerSize(), 0.0);

  SceneContext context;
  EXPECT_TRUE(one.sceneGeometry(context).isEmpty());

  one.setMarkerSize(4.0);
  ASSERT_EQ(one.sceneGeometry(context).size(), 1);
  EXPECT_EQ(one.sceneGeometry(context).first().vertices.size(), 6);
}

TEST_F(MarkerTest, AMarkerSitsOnTheTerrainWhenItIsDrapedAndFlatWhenNot)
{
  Testing::VectorProbe points(QStringLiteral("gauges"));
  points.addPoint({ 0.0, 0.0 });
  points.addPoint({ 100.0, 0.0 });
  points.setMarkerSize(2.0);

  FlatTerrain terrain(250.0);
  SceneContext context;
  context.terrain = &terrain;

  ZPolicy draped;
  draped.mode = ZMode::OnTerrain;
  points.setZPolicy(draped);

  const SceneGeometry on = points.sceneGeometry(context).first();

  // Centred on the sampled height: half the solid stands above the ground,
  // which is why a marker needs no coplanar nudge to be seen on one.
  double sum = 0.0;

  for (int i = 0; i < 6; ++i)
  {
    sum += double(on.vertices.at(i).z);
  }

  EXPECT_NEAR(sum / 6.0, 250.0, 1e-4);

  ZPolicy flat;
  flat.mode = ZMode::Constant;
  flat.constant = 7.0;
  points.setZPolicy(flat);

  const SceneGeometry off = points.sceneGeometry(context).first();
  double flatSum = 0.0;

  for (int i = 0; i < 6; ++i)
  {
    flatSum += double(off.vertices.at(i).z);
  }

  EXPECT_NEAR(flatSum / 6.0, 7.0, 1e-4)
    << "a flat marker followed the terrain anyway";
}

// ── filled rings ────────────────────────────────────────────────────────

TEST_F(MarkerTest, AConvexRingFillsAndOnlyWhenAsked)
{
  Testing::VectorProbe faces(QStringLiteral("cells"));
  faces.addRing({ { 0.0, 0.0 }, { 10.0, 0.0 }, { 10.0, 10.0 },
                     { 0.0, 10.0 } });

  SceneContext context;

  // Off by default: an outline that follows the ground already says what
  // the map cannot, and filling an arbitrary ring is a triangulation.
  EXPECT_FALSE(faces.fillsRings());
  EXPECT_TRUE(faces.supportsRingFill());

  QVector<SceneGeometry> batches = faces.sceneGeometry(context);
  ASSERT_EQ(batches.size(), 1);
  EXPECT_EQ(batches.first().primitive, ScenePrimitive::Lines);

  faces.setFillsRings(true);
  batches = faces.sceneGeometry(context);

  // The fill comes first, so the crest reads as the edge of its own face.
  ASSERT_EQ(batches.size(), 2);
  EXPECT_EQ(batches.first().primitive, ScenePrimitive::Triangles);
  EXPECT_EQ(batches.last().primitive, ScenePrimitive::Lines);

  // A quad is two triangles from a fan of four corners.
  EXPECT_EQ(batches.first().indices.size(), 2 * 3);
}

TEST_F(MarkerTest, AConcaveRingIsLeftAsAnOutlineRatherThanFilledWrongly)
{
  // An L: a fan from its first corner spans the notch, which would put a
  // face over ground the ring excludes.
  Testing::VectorProbe shape(QStringLiteral("catchment"));
  shape.addRing({ { 0.0, 0.0 }, { 10.0, 0.0 }, { 10.0, 4.0 },
                     { 4.0, 4.0 }, { 4.0, 10.0 }, { 0.0, 10.0 } });
  shape.setFillsRings(true);

  SceneContext context;
  const QVector<SceneGeometry> batches = shape.sceneGeometry(context);

  ASSERT_EQ(batches.size(), 1) << "a concave ring was filled";
  EXPECT_EQ(batches.first().primitive, ScenePrimitive::Lines);
}

TEST_F(MarkerTest, ALineLayerIsNeverOfferedRingFilling)
{
  Testing::VectorProbe lines(QStringLiteral("reaches"));
  lines.addLine({ { 0.0, 0.0 }, { 10.0, 0.0 }, { 10.0, 10.0 } });

  // Offering the control on a layer with no rings would be offering one
  // that does nothing.
  EXPECT_FALSE(lines.supportsRingFill());

  lines.setFillsRings(true);
  SceneContext context;
  const QVector<SceneGeometry> batches = lines.sceneGeometry(context);

  ASSERT_EQ(batches.size(), 1);
  EXPECT_EQ(batches.first().primitive, ScenePrimitive::Lines);
}
