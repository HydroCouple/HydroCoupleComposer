/*!
 * \file   test_meshgenerate.cpp
 * \brief  E2a verification — the domain is meshed by the SDK, not by us.
 *
 * The gate the plan asks for is a delegation gate: a mesh built through the
 * composer and one built by calling the SDK directly, with the same domain
 * and the same parameters, must be the *same mesh* — node for node and face
 * for face. Anything less lets the composer quietly improve the geometry on
 * its way past, and a model run against the meshes would then disagree with
 * a model run against the SDK's.
 *
 * The rest is what the composer is actually responsible for: that every
 * part of the domain reaches the triangulator, that the options are passed
 * through rather than decided here, that a domain which is not one is
 * refused in the words it already has, and that the counts reported
 * describe the mesh that came back.
 *
 * No widgets and no application: this is data in and data out.
 */

#include "mesh/meshgenerator.h"

#include "hydrocouplesdk/tools/quadmesher.h"
#include "hydrocouplesdk/tools/triangulator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace HydroCouple::Composer;

namespace IO = HydroCouple::SDK::IO;
namespace Tools = HydroCouple::SDK::Tools;

namespace
{
  //! A ring, first point not repeated, as a domain holds one.
  QPolygonF ring(double left, double bottom, double right, double top)
  {
    return QPolygonF({QPointF(left, bottom), QPointF(right, bottom),
                      QPointF(right, top), QPointF(left, top)});
  }

  /*!
   * \brief A domain with one of everything.
   *
   * Every part is populated on purpose: the delegation gate below compares
   * whole meshes, so a part left out of the fixture is a part nobody checks
   * ever reaches the triangulator.
   */
  MeshDomain richDomain()
  {
    MeshDomain domain;
    domain.boundary = ring(0.0, 0.0, 100.0, 100.0);
    domain.holes.append(ring(20.0, 20.0, 40.0, 40.0));
    domain.constraintLines.append(
      QPolygonF({QPointF(60.0, 10.0), QPointF(60.0, 90.0)}));
    domain.points.append(QPointF(80.0, 50.0));

    return domain;
  }

  //! Whether two meshes are the same mesh.
  void expectSameMesh(const IO::MeshDefinition &left,
                      const IO::MeshDefinition &right)
  {
    ASSERT_EQ(left.nodeX, right.nodeX);
    ASSERT_EQ(left.nodeY, right.nodeY);
    ASSERT_EQ(left.faceNodeOffsets, right.faceNodeOffsets);
    ASSERT_EQ(left.faceNodes, right.faceNodes);
  }
}

// ── the delegation gate ─────────────────────────────────────────────────────

TEST(MeshGenerateTest, AMeshIsTheTriangulatorsOwnAnswerNodeForNode)
{
  const MeshDomain domain = richDomain();

  const MeshGenerationResult result = generateMesh(domain);
  ASSERT_TRUE(result.ok) << result.message.toStdString();

  IO::MeshDefinition direct;
  std::string message;
  ASSERT_TRUE(Tools::Triangulator::triangulate(domain.toTriangulationInput(),
                                               direct, message))
    << message;

  // Not "the same number of triangles": a composer that reordered, welded or
  // nudged anything would still count right.
  expectSameMesh(result.mesh, direct);
  EXPECT_GT(direct.faceCount(), 0);
}

TEST(MeshGenerateTest, TheQuadMeshIsTheQuadMeshersOwnAnswer)
{
  const MeshDomain domain = richDomain();

  MeshGenerationOptions options;
  options.quadDominant = true;
  options.minQuadQuality = 0.4;

  const MeshGenerationResult result = generateMesh(domain, options);
  ASSERT_TRUE(result.ok) << result.message.toStdString();

  IO::MeshDefinition triangles;
  IO::MeshDefinition merged;
  std::string message;
  ASSERT_TRUE(Tools::Triangulator::triangulate(domain.toTriangulationInput(),
                                               triangles, message))
    << message;
  ASSERT_TRUE(Tools::QuadMesher::triToQuadDominant(triangles, 0.4, merged,
                                                   message))
    << message;

  expectSameMesh(result.mesh, merged);
}

// ── what reaches the SDK ────────────────────────────────────────────────────

TEST(MeshGenerateTest, TheDomainsEdgeLengthLimitReachesTheTriangulator)
{
  MeshDomain coarse;
  coarse.boundary = ring(0.0, 0.0, 100.0, 100.0);

  MeshDomain fine = coarse;
  fine.maxEdgeLength = 10.0;

  const MeshGenerationResult loose = generateMesh(coarse);
  const MeshGenerationResult tight = generateMesh(fine);

  ASSERT_TRUE(loose.ok) << loose.message.toStdString();
  ASSERT_TRUE(tight.ok) << tight.message.toStdString();

  // The limit is the practical refinement control, and it lives on the
  // domain: a generator that built its own input would drop it and produce
  // four nodes whatever the user asked for.
  EXPECT_GT(tight.mesh.nodeCount(), loose.mesh.nodeCount());
  EXPECT_GT(tight.triangles, loose.triangles);
}

TEST(MeshGenerateTest, AForcedPointIsAMeshNode)
{
  const MeshDomain domain = richDomain();

  const MeshGenerationResult result = generateMesh(domain);
  ASSERT_TRUE(result.ok) << result.message.toStdString();

  bool found = false;

  for (int64_t node = 0; node < result.mesh.nodeCount(); ++node)
  {
    if (result.mesh.nodeX[size_t(node)] == 80.0
        && result.mesh.nodeY[size_t(node)] == 50.0)
    {
      found = true;
      break;
    }
  }

  // The one part of the domain whose arrival can be read straight off the
  // output: a point the user forced is a node or it was ignored.
  EXPECT_TRUE(found) << "the forced interior point never reached the mesh";
}

// ── the options are passed through, not decided here ────────────────────────

TEST(MeshGenerateTest, WithoutTheQuadOptionEveryFaceIsATriangle)
{
  const MeshGenerationResult result = generateMesh(richDomain());

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_GT(result.triangles, 0);
  EXPECT_EQ(result.quads, 0);
  EXPECT_EQ(result.mesh.maxNodesPerFace(), 3);
}

TEST(MeshGenerateTest, TheQuadOptionMergesPairsIntoQuads)
{
  MeshGenerationOptions options;
  options.quadDominant = true;
  options.minQuadQuality = 0.4;

  const MeshGenerationResult result = generateMesh(richDomain(), options);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_GT(result.quads, 0) << "quad-dominant produced no quads at all";
  EXPECT_EQ(result.mesh.maxNodesPerFace(), 4);
}

TEST(MeshGenerateTest, RaisingTheQualityThresholdLeavesMoreTrianglesAlone)
{
  MeshGenerationOptions lenient;
  lenient.quadDominant = true;
  lenient.minQuadQuality = 0.0;

  MeshGenerationOptions strict = lenient;
  strict.minQuadQuality = 0.95;

  const MeshGenerationResult loose = generateMesh(richDomain(), lenient);
  const MeshGenerationResult tight = generateMesh(richDomain(), strict);

  ASSERT_TRUE(loose.ok) << loose.message.toStdString();
  ASSERT_TRUE(tight.ok) << tight.message.toStdString();

  // The threshold is the only control over how quad-dominant the result is,
  // so a generator that hard-coded one would answer both the same.
  EXPECT_LT(tight.quads, loose.quads);
}

// ── what is reported ────────────────────────────────────────────────────────

TEST(MeshGenerateTest, TheCountsDescribeTheMeshThatCameBack)
{
  MeshGenerationOptions options;
  options.quadDominant = true;
  options.minQuadQuality = 0.4;

  const MeshGenerationResult result = generateMesh(richDomain(), options);
  ASSERT_TRUE(result.ok) << result.message.toStdString();

  // Counted from the faces rather than tracked alongside them, so the two
  // cannot describe different meshes.
  EXPECT_EQ(result.triangles + result.quads, result.mesh.faceCount());
  EXPECT_TRUE(result.message.contains(QString::number(result.quads)))
    << result.message.toStdString();
}

// ── refusals ────────────────────────────────────────────────────────────────

TEST(MeshGenerateTest, ADomainThatIsNotOneIsRefusedInItsOwnWords)
{
  const MeshDomain empty;

  QString why;
  ASSERT_FALSE(empty.isValid(why));

  const MeshGenerationResult result = generateMesh(empty);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.mesh.nodeCount(), 0);

  // The domain's own sentence, not a second one written here that would
  // drift away from it.
  EXPECT_EQ(result.message, why);
}

TEST(MeshGenerateTest, AHoleOutsideTheBoundaryIsRefusedBeforeTheSdkIsTroubled)
{
  MeshDomain domain;
  domain.boundary = ring(0.0, 0.0, 10.0, 10.0);
  domain.holes.append(ring(50.0, 50.0, 60.0, 60.0));

  const MeshGenerationResult result = generateMesh(domain);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.mesh.nodeCount(), 0);
  EXPECT_TRUE(result.message.contains(QStringLiteral("outside")))
    << result.message.toStdString();
}

// ── watching a run, and stopping one ────────────────────────────────────────

TEST(MeshGenerateTest, WatchingARunReportsTheSdksOwnPhases)
{
  std::vector<std::string> stages;

  const MeshGenerationResult result =
    generateMesh(richDomain(), {},
                 [&stages](double, const char *stage)
                 {
                   stages.emplace_back(stage);
                   return true;
                 });

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_FALSE(result.cancelled);

  // Passed through, not invented here: the stage names are the SDK's, and a
  // dialog that showed its own would drift from what is actually running.
  ASSERT_FALSE(stages.empty());
  EXPECT_NE(std::find(stages.begin(), stages.end(), "inserting edges"),
            stages.end());
  EXPECT_EQ(stages.back(), "done");
}

TEST(MeshGenerateTest, WatchingARunDoesNotChangeTheMesh)
{
  const MeshGenerationResult unwatched = generateMesh(richDomain());
  const MeshGenerationResult watched =
    generateMesh(richDomain(), {}, [](double, const char *) { return true; });

  ASSERT_TRUE(unwatched.ok) << unwatched.message.toStdString();
  ASSERT_TRUE(watched.ok) << watched.message.toStdString();

  expectSameMesh(watched.mesh, unwatched.mesh);
}

TEST(MeshGenerateTest, StoppingARunCarriesNoMeshAndDoesNotCallItAFailure)
{
  const MeshGenerationResult result =
    generateMesh(richDomain(), {}, [](double, const char *) { return false; });

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.mesh.nodeCount(), 0)
    << "the mesh from a run the user abandoned was handed back anyway";

  // A run the user stopped is not a run that failed. Telling them the
  // triangulation failed after they pressed Cancel is a lie about their own
  // model, and one they would reasonably go looking for the cause of.
  EXPECT_TRUE(result.cancelled);
  EXPECT_FALSE(result.message.contains(QStringLiteral("failed")))
    << result.message.toStdString();
}

TEST(MeshGenerateTest, StoppingDuringTheQuadMergeIsAlsoACancellation)
{
  MeshGenerationOptions options;
  options.quadDominant = true;
  options.minQuadQuality = 0.4;

  // Let the triangulation finish and refuse only once the merge starts, so
  // the gate is on the second call and not on the first.
  const MeshGenerationResult result =
    generateMesh(richDomain(), options,
                 [](double, const char *stage)
                 { return std::string(stage) != "merging"; });

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(result.cancelled);
  EXPECT_EQ(result.mesh.faceCount(), 0)
    << "the triangulation was kept when the merge was abandoned";
}

/*
 * There is no gate on the branch that carries a triangulator failure out,
 * and that is a statement about the SDK rather than an omission. CDT was
 * offered a duplicate vertex, a hole all but filling its boundary, a
 * breakline running far outside it, and a boundary that crosses itself with
 * area to spare: it triangulated all four without complaint. Every failure
 * the SDK does document — a boundary or hole ring of fewer than three
 * points, and a triangulation that produced nothing — is refused by
 * MeshDomain::isValid() before the SDK is reached. The branch stays because
 * triangulate() returns a [[nodiscard]] bool and dropping it on the floor
 * would be worse than carrying an untested path, but nothing here proves it
 * works.
 *
 * The fourth of those probes is worth its own line: a self-intersecting
 * boundary with non-zero area is meshed silently, so E1a's note that "the
 * triangulator already refuses one loudly" does not hold, and a user who
 * draws a bow-tie gets a mesh over a shape that crosses itself with nothing
 * said. That belongs to the domain validator, not here.
 */
