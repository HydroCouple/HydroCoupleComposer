/*!
 * \file test_terrainsampler.cpp
 * \brief Elevations from a coverage, onto a mesh's vertices.
 *
 * The step that turns a fetched raster from a picture behind a model into
 * data inside one.
 *
 * The fixture is a real GetCoverage response from PDOK's Actueel
 * Hoogtebestand Nederland: 64 by 64 cells at half-metre resolution, in the
 * Dutch national grid, covering x 120000–120032 and y 486000–486032. Every
 * expectation below is anchored to that ground rather than to a raster
 * written to suit the test.
 */

#include "gis/spatialreference.h"
#include "layers/meshlayer.h"
#include "layers/wcscoveragelayer.h"
#include "mesh/terrainsampler.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QSignalSpy>
#include <QString>

#include <cmath>
#include <iostream>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  std::unique_ptr<WcsCoverageLayer> terrain()
  {
    QFile file(QStringLiteral(COMPOSER_OGC_FIXTURE_DIR
                              "/wcs-coverage-ahn-dtm.tif"));

    if (!file.open(QIODevice::ReadOnly))
    {
      return nullptr;
    }

    QString message;

    return WcsCoverageLayer::fromResponse(file.readAll(),
                                          QStringLiteral("AHN DTM"), message);
  }

  /*!
   * \brief A mesh of \a n by \a n vertices over ground the fixture has
   *        values for.
   *
   * Not over the whole coverage, because most of this one is holes. It is a
   * DTM -- a terrain model -- so buildings and open water are removed at the
   * source, and roughly two thirds of these 64 by 64 cells carry no value at
   * all. The block below is a patch of continuously surveyed ground on the
   * eastern edge, found by reading the fixture at its own half-metre
   * resolution rather than by assuming: sampled every two metres it looks
   * solid over a far larger area than it is.
   */
  MeshDefinition gridOverData(int n)
  {
    MeshDefinition mesh;

    for (int row = 0; row < n; ++row)
    {
      for (int column = 0; column < n; ++column)
      {
        mesh.nodeX.push_back(120027.5 + column * (3.0 / (n - 1)));
        mesh.nodeY.push_back(486016.0 + row * (6.0 / (n - 1)));
      }
    }

    return mesh;
  }
} // namespace

TEST(TerrainSampler, everyVertexOverSurveyedGroundGetsAnElevation)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  MeshDefinition mesh = gridOverData(6);
  const size_t vertices = mesh.nodeX.size();

  const TerrainSampleResult result =
    sampleTerrain(*raster, raster->crs(), mesh);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.sampled, static_cast<int>(vertices));
  EXPECT_EQ(result.missed, 0);
  ASSERT_EQ(mesh.nodeZ.size(), vertices);

  // Metres above NAP. The service bounds this coverage at -8 to 322, and
  // this is one Dutch field, so anything outside that envelope means the
  // band was read as something it is not.
  for (double z : mesh.nodeZ)
  {
    EXPECT_FALSE(std::isnan(z));
    EXPECT_GE(z, -8.0);
    EXPECT_LE(z, 322.0);
  }
}

TEST(TerrainSampler, theElevationsVaryAcrossTheGround)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  MeshDefinition mesh = gridOverData(6);

  ASSERT_TRUE(sampleTerrain(*raster, raster->crs(), mesh).ok);

  double lowest = mesh.nodeZ.front();
  double highest = mesh.nodeZ.front();

  for (double z : mesh.nodeZ)
  {
    lowest = std::min(lowest, z);
    highest = std::max(highest, z);
  }

  // A sampler that read one cell and copied it everywhere, or that fell
  // back to a constant, would satisfy every other gate here.
  EXPECT_GT(highest - lowest, 0.01)
    << "every vertex got the same elevation: " << lowest;
}

TEST(TerrainSampler, aVertexBetweenCellsIsInterpolatedNotSnapped)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  // Three points INSIDE one cell -- it spans x [120028.75, 120029.25) --
  // so a nearest-neighbour sampler returns the same value three times and a
  // bilinear one returns three different ones. Straddling the boundary
  // instead, as this first did, lets nearest pass: the points then sit in
  // different cells and differ for the wrong reason.
  MeshDefinition mesh;
  mesh.nodeX = {120028.85, 120029.00, 120029.15};
  mesh.nodeY = {486015.85, 486016.00, 486016.15};

  ASSERT_TRUE(sampleTerrain(*raster, raster->crs(), mesh).ok);
  ASSERT_EQ(mesh.nodeZ.size(), 3u);

  // Terrain is a continuous surface, and sampling it in steps gives a mesh
  // visible half-cell terraces.
  EXPECT_NE(mesh.nodeZ[0], mesh.nodeZ[1]);
  EXPECT_NE(mesh.nodeZ[1], mesh.nodeZ[2]);
}

TEST(TerrainSampler, aMeshInDegreesIsBroughtIntoTheCoveragesOwnSystem)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  QString message;

  const std::unique_ptr<SpatialReference> wgs84 =
    SpatialReference::fromAuthority(QStringLiteral("EPSG"), 4326, message);
  ASSERT_NE(wgs84, nullptr) << message.toStdString();

  const std::unique_ptr<SpatialReference> dutch =
    SpatialReference::fromAuthority(QStringLiteral("EPSG"), 28992, message);
  ASSERT_NE(dutch, nullptr) << message.toStdString();

  // The middle of the coverage, expressed in degrees.
  const std::unique_ptr<CoordinateTransform> toGeographic =
    CoordinateTransform::between(*dutch, *wgs84, message);
  ASSERT_NE(toGeographic, nullptr) << message.toStdString();

  bool ok = true;
  const QPointF centre =
    toGeographic->transform(QPointF(120029.0, 486016.0), &ok);
  ASSERT_TRUE(ok);

  MeshDefinition mesh;
  mesh.nodeX = {centre.x()};
  mesh.nodeY = {centre.y()};

  const TerrainSampleResult result = sampleTerrain(*raster, wgs84.get(), mesh);

  // Without the reprojection this does not fail loudly: the degrees land
  // far outside a raster measured in metres, every vertex misses, and the
  // result is a mesh with no elevations and nothing said about why.
  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.sampled, 1);
  EXPECT_FALSE(std::isnan(mesh.nodeZ.front()));
}

TEST(TerrainSampler, aMeshSomewhereElseEntirelyIsToldSoRatherThanZeroed)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  MeshDefinition mesh;
  mesh.nodeX = {0.0, 10.0};
  mesh.nodeY = {0.0, 10.0};

  const TerrainSampleResult result =
    sampleTerrain(*raster, raster->crs(), mesh);

  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.message.isEmpty());

  // Not zeroed. A vertex quietly set to zero is a hole punched to sea level
  // in the middle of a catchment, and it looks exactly like data.
  EXPECT_TRUE(mesh.nodeZ.empty());
}

TEST(TerrainSampler, aMeshHalfOffTheCoverageKeepsWhatItCanAndCountsTheRest)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  MeshDefinition mesh;
  mesh.nodeX = {120029.0, 120029.0, 500000.0};
  mesh.nodeY = {486016.0, 486020.0, 486016.0};

  const TerrainSampleResult result =
    sampleTerrain(*raster, raster->crs(), mesh);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.sampled, 2);
  EXPECT_EQ(result.missed, 1);

  // The one that missed is NaN, which is distinguishable from a reading.
  // Zero is not: it is a perfectly ordinary elevation in the Netherlands.
  EXPECT_TRUE(std::isnan(mesh.nodeZ.at(2)));
}

TEST(TerrainSampler, anEmptyMeshIsRefusedRatherThanReportedAsSuccess)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  MeshDefinition mesh;

  const TerrainSampleResult result =
    sampleTerrain(*raster, raster->crs(), mesh);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.sampled, 0);

  // And says which of the two failures it is. Without its own guard this
  // still fails -- nothing was sampled, so the other branch catches it --
  // but reports "none of the mesh's 0 vertices fall on those elevations",
  // which sends the reader looking for a coverage that is somewhere else.
  EXPECT_TRUE(result.message.contains(QStringLiteral("no vertices")))
    << result.message.toStdString();
}

TEST(TerrainSampler, aHoleInTheSurveyIsCountedRatherThanInvented)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  // The middle of this coverage carries no value: a DTM has buildings and
  // open water removed at the source, and roughly two thirds of these cells
  // are holes. A vertex over one must come back with nothing rather than
  // with a number.
  MeshDefinition mesh;
  mesh.nodeX = {120016.0, 120029.0};
  mesh.nodeY = {486016.0, 486016.0};

  const TerrainSampleResult result =
    sampleTerrain(*raster, raster->crs(), mesh);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.sampled, 1);
  EXPECT_EQ(result.missed, 1);

  EXPECT_TRUE(std::isnan(mesh.nodeZ.at(0)));
  EXPECT_FALSE(std::isnan(mesh.nodeZ.at(1)));
}

TEST(TerrainSampler, aVertexBesideAHoleIsNotPulledTowardsIt)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  MeshDefinition mesh = gridOverData(6);

  ASSERT_TRUE(sampleTerrain(*raster, raster->crs(), mesh).ok);

  // No-data is the largest float there is, so interpolating across one
  // would not give a slightly wrong elevation -- it would give 1e38. Every
  // reading here is a Dutch polder height, within a couple of metres of
  // NAP.
  for (double z : mesh.nodeZ)
  {
    if (!std::isnan(z))
    {
      EXPECT_LT(std::abs(z), 100.0) << z;
    }
  }
}

TEST(TerrainSampler, aVertexReadsTheCellsItActuallySitsBetween)
{
  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  // x = 120026.6 sits between the cell centred on 120026.25 and the one
  // centred on 120026.75. At this latitude the first of those is a hole and
  // the second is surveyed, so the answer turns on which pair is read --
  // which is what the half-cell offset from GDAL's pixel corners to its
  // pixel centres decides.
  //
  // Read correctly, the hole is one of the two and the vertex gets nothing.
  // Read half a cell across, both neighbours are surveyed and the vertex
  // gets a plausible elevation from the wrong ground. Interpolating across
  // the hole instead gives 1e38, which is no-data pretending to be terrain.
  MeshDefinition mesh;
  mesh.nodeX = {120026.6, 120027.6};
  mesh.nodeY = {486017.75, 486017.75};

  const TerrainSampleResult result =
    sampleTerrain(*raster, raster->crs(), mesh);

  ASSERT_TRUE(result.ok) << result.message.toStdString();

  EXPECT_TRUE(std::isnan(mesh.nodeZ.at(0))) << mesh.nodeZ.at(0);
  EXPECT_FALSE(std::isnan(mesh.nodeZ.at(1)));
  EXPECT_LT(std::abs(mesh.nodeZ.at(1)), 100.0) << mesh.nodeZ.at(1);
}

// ---------------------------------------------------------------------------
// What sampling does to the mesh itself
// ---------------------------------------------------------------------------

TEST(TerrainSampler, aMeshWithoutElevationsIsNotASurfaceUntilItHasThem)
{
  MeshDefinition definition;

  // Two triangles over ground the fixture has values for.
  definition.nodeX = {120027.5, 120030.5, 120030.5, 120027.5};
  definition.nodeY = {486016.0, 486016.0, 486022.0, 486022.0};
  definition.faceNodeOffsets = {0, 3, 6};
  definition.faceNodes = {0, 1, 2, 0, 2, 3};

  QString message;

  std::unique_ptr<MeshLayer> mesh = MeshLayer::create(
    QStringLiteral("mesh"), definition, MeshEntity::Face, message);
  ASSERT_NE(mesh, nullptr) << message.toStdString();

  // A mesh without elevations declines to be a terrain rather than offering
  // a sheet at zero, which is a ground indistinguishable from none.
  EXPECT_EQ(mesh->terrain(), nullptr);

  std::unique_ptr<WcsCoverageLayer> raster = terrain();
  ASSERT_NE(raster, nullptr);

  MeshDefinition sampled = mesh->mesh();
  const TerrainSampleResult result =
    sampleTerrain(*raster, mesh->crs(), sampled);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  ASSERT_TRUE(mesh->setNodeElevations(sampled.nodeZ));

  // And is one afterwards, which is what lets anything drape onto it.
  EXPECT_NE(mesh->terrain(), nullptr);
}

TEST(TerrainSampler, aMeshRefusesElevationsThatAreNotItsOwn)
{
  MeshDefinition definition;
  definition.nodeX = {120027.5, 120030.5, 120030.5};
  definition.nodeY = {486016.0, 486016.0, 486022.0};
  definition.faceNodeOffsets = {0, 3};
  definition.faceNodes = {0, 1, 2};

  QString message;

  std::unique_ptr<MeshLayer> mesh = MeshLayer::create(
    QStringLiteral("mesh"), definition, MeshEntity::Face, message);
  ASSERT_NE(mesh, nullptr) << message.toStdString();

  // A mismatched array would pair each vertex with a stranger's height, and
  // every one of them would look like a plausible elevation.
  EXPECT_FALSE(mesh->setNodeElevations({1.0, 2.0}));
  EXPECT_TRUE(mesh->mesh().nodeZ.empty());
}

TEST(TerrainSampler, newElevationsReplaceTheSurfaceTheMeshAlreadyAnswered)
{
  MeshDefinition definition;
  definition.nodeX = {120027.5, 120030.5, 120030.5, 120027.5};
  definition.nodeY = {486016.0, 486016.0, 486022.0, 486022.0};
  definition.faceNodeOffsets = {0, 3, 6};
  definition.faceNodes = {0, 1, 2, 0, 2, 3};

  QString message;

  std::unique_ptr<MeshLayer> mesh =
    MeshLayer::create(QStringLiteral("mesh"), definition, MeshEntity::Face,
                      message);
  ASSERT_NE(mesh, nullptr) << message.toStdString();

  ASSERT_TRUE(mesh->setNodeElevations({1.0, 1.0, 1.0, 1.0}));

  const ITerrainSource *ground = mesh->terrain();
  ASSERT_NE(ground, nullptr);

  const QPointF inside(120029.5, 486018.0);
  double elevation = 0.0;

  // Asking once is what builds the index behind this — which is the whole
  // point of the gate: re-sampling a mesh that has already been asked for
  // its ground must not go on answering from the old surface.
  ASSERT_TRUE(ground->elevationAt(inside, elevation));
  EXPECT_NEAR(elevation, 1.0, 1e-6);

  ASSERT_TRUE(mesh->setNodeElevations({5.0, 5.0, 5.0, 5.0}));

  ASSERT_TRUE(mesh->terrain()->elevationAt(inside, elevation));
  EXPECT_NEAR(elevation, 5.0, 1e-6);
}

TEST(TerrainSampler, aMeshThatGainedElevationsSaysSoSoTheMapRedraws)
{
  MeshDefinition definition;
  definition.nodeX = {120027.5, 120030.5, 120030.5};
  definition.nodeY = {486016.0, 486016.0, 486022.0};
  definition.faceNodeOffsets = {0, 3};
  definition.faceNodes = {0, 1, 2};

  QString message;

  std::unique_ptr<MeshLayer> mesh =
    MeshLayer::create(QStringLiteral("mesh"), definition, MeshEntity::Face,
                      message);
  ASSERT_NE(mesh, nullptr) << message.toStdString();

  QSignalSpy repainted(mesh.get(), &MapLayer::appearanceChanged);

  ASSERT_TRUE(mesh->setNodeElevations({1.0, 2.0, 3.0}));

  // Without this the elevations are there and nothing on screen shows it
  // until some unrelated thing happens to force a repaint.
  EXPECT_EQ(repainted.count(), 1);
}
