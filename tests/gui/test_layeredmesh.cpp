/*!
 * \file   test_layeredmesh.cpp
 * \brief  C3b — sigma-layered meshes as prismatic cells, and peeling.
 *
 * Two things are being pinned. One is FVQual's conventions: which end of the
 * sigma range is the surface, and which index runs fastest. Both are the kind
 * of mistake that produces a picture — an upside-down reservoir, a transposed
 * water column — rather than an error, so they are asserted on numbers rather
 * than looked at.
 *
 * The other is that only the outside of the mesh is built. Drawing every
 * cell's six faces is correct and unaffordable: at the phase's budget it puts
 * three quarters of the triangles where nobody can see them.
 */

#include "layers/layeredmesh.h"
#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "render/classification.h"
#include "render/layerstyle.h"
#include "results/seriesexport.h"
#include "scene/camera.h"
#include "ui/panels/profileplotpanel.h"
#include "scene/sceneimage.h"
#include "scene/scenerenderer.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QChartView>
#include <QLineSeries>
#include <QDir>
#include <QFile>
#include <QValueAxis>

#include "hydrocouplesdk/io/netcdfugridwriter.h"

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

  //! A layered mesh over `strip(columns)`, flat bed, uniform sigma.
  LayeredMesh flatStrip(int columns, int layers, double bed = -10.0,
                        double surface = 0.0)
  {
    LayeredMesh mesh;
    mesh.horizontal = strip(columns);
    mesh.layerCount = layers;
    mesh.interfaceZ.resize(size_t(columns) * size_t(layers + 1));

    for (int column = 0; column < columns; ++column)
    {
      for (int k = 0; k <= layers; ++k)
      {
        // Interface 0 is the surface, layerCount the bed.
        const double fraction = double(k) / double(layers);
        mesh.interfaceZ[size_t(mesh.interfaceSlot(column, k))] =
          surface + fraction * (bed - surface);
      }
    }

    return mesh;
  }

  std::unique_ptr<MeshLayer> layeredLayer(const LayeredMesh &layered,
                                          QString &message)
  {
    std::unique_ptr<MeshLayer> layer = MeshLayer::create(
      QStringLiteral("layered"), layered.horizontal, MeshEntity::Face,
      message);

    if (!layer || !layer->setLayering(layered, message))
    {
      return nullptr;
    }

    return layer;
  }

  int triangleCount(const QVector<SceneGeometry> &batches)
  {
    int triangles = 0;

    for (const SceneGeometry &geometry : batches)
    {
      triangles += geometry.indices.size() / 3;
    }

    return triangles;
  }

  class LayeredMeshTest : public ::testing::Test
  {
    protected:
      void SetUp() override
      {
        if (!QApplication::instance())
        {
          static int argc = 1;
          static char name[] = "test_layeredmesh";
          static char *argv[] = { name, nullptr };
          m_app = std::make_unique<QApplication>(argc, argv);
        }
      }

    private:
      std::unique_ptr<QApplication> m_app;
  };

}

// ── FVQual's conventions ────────────────────────────────────────────────────

TEST_F(LayeredMeshTest, InterfaceZeroIsTheSurfaceAndTheLastIsTheBed)
{
  // Getting this backwards draws every reservoir upside down, and looks
  // entirely plausible while doing it.
  const LayeredMesh mesh = flatStrip(1, 4, -20.0, 5.0);

  EXPECT_NEAR(mesh.z(0, 0), 5.0, 1.0e-9);
  EXPECT_NEAR(mesh.z(0, mesh.layerCount), -20.0, 1.0e-9);

  for (int k = 1; k <= mesh.layerCount; ++k)
  {
    EXPECT_LT(mesh.z(0, k), mesh.z(0, k - 1))
      << "interface " << k << " is not below interface " << k - 1;
  }
}

TEST_F(LayeredMeshTest, IndexingMatchesFVQualsSoAFieldIsNotTransposed)
{
  // column * layerCount + k for cells, column * (layerCount + 1) + k for
  // interfaces. A field read in the other order still fills every cell, so
  // nothing downstream notices; the water column is simply wrong.
  const LayeredMesh mesh = flatStrip(3, 5);

  EXPECT_EQ(mesh.cell(0, 0), 0);
  EXPECT_EQ(mesh.cell(0, 4), 4);
  EXPECT_EQ(mesh.cell(1, 0), 5);
  EXPECT_EQ(mesh.cell(2, 3), 13);

  EXPECT_EQ(mesh.interfaceSlot(0, 0), 0);
  EXPECT_EQ(mesh.interfaceSlot(1, 0), 6);
  EXPECT_EQ(mesh.cellCount(), 15);
  EXPECT_EQ(mesh.columnCount(), 3);
}

TEST_F(LayeredMeshTest, CfSigmaIsConvertedToElevations)
{
  // The form FVQual writes: sigma 0 at the surface to -1 at the bed, with
  // depth positive down. Both sign flips have to happen, and getting one of
  // the two right produces a mesh that is merely somewhere else.
  QString message;
  const LayeredMesh mesh = LayeredMesh::fromCfSigma(
    strip(2), { 0.0, -0.5, -1.0 }, { 10.0, 20.0 }, { 2.0, 2.0 }, message);

  ASSERT_EQ(mesh.layerCount, 2) << message.toStdString();

  // Column 0: surface 2, depth 10 -> bed at -10, midpoint at -4.
  EXPECT_NEAR(mesh.z(0, 0), 2.0, 1.0e-9);
  EXPECT_NEAR(mesh.z(0, 1), -4.0, 1.0e-9);
  EXPECT_NEAR(mesh.z(0, 2), -10.0, 1.0e-9);

  // Column 1 is deeper, and its surface is the same.
  EXPECT_NEAR(mesh.z(1, 0), 2.0, 1.0e-9);
  EXPECT_NEAR(mesh.z(1, 2), -20.0, 1.0e-9);
}

TEST_F(LayeredMeshTest, AMismatchedElevationArrayIsRefused)
{
  LayeredMesh mesh;
  mesh.horizontal = strip(3);
  mesh.layerCount = 4;
  mesh.interfaceZ.assign(5, 0.0);  // needs 3 * 5

  QString message;
  EXPECT_FALSE(mesh.isValid(message));
  EXPECT_TRUE(message.contains(QStringLiteral("15")))
    << message.toStdString();
}

TEST_F(LayeredMeshTest, ALayeringForADifferentMeshIsRefused)
{
  QString message;
  std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("two"), strip(2), MeshEntity::Face, message);
  ASSERT_NE(layer, nullptr);

  EXPECT_FALSE(layer->setLayering(flatStrip(5, 3), message));
  EXPECT_FALSE(layer->isLayered());
}

// ── Only the outside is built ───────────────────────────────────────────────

TEST_F(LayeredMeshTest, InteriorFacesBetweenStackedLayersAreNotBuilt)
{
  // A single column of N layers has one top and one bottom, not N of each.
  // The interfaces between visible layers are inside the water.
  QString message;

  const std::unique_ptr<MeshLayer> thin =
    layeredLayer(flatStrip(1, 1), message);
  const std::unique_ptr<MeshLayer> thick =
    layeredLayer(flatStrip(1, 8), message);

  ASSERT_NE(thin, nullptr) << message.toStdString();
  ASSERT_NE(thick, nullptr) << message.toStdString();

  const int thinCaps = 2 * 2;  // two quads, two triangles each

  // Walls scale with layer count; caps must not.
  EXPECT_EQ(triangleCount(thin->sceneSource()->sceneGeometry({})),
            thinCaps + 4 * 2);
  EXPECT_EQ(triangleCount(thick->sceneSource()->sceneGeometry({})),
            thinCaps + 8 * 4 * 2)
    << "the caps were rebuilt once per layer";
}

TEST_F(LayeredMeshTest, WallsBetweenNeighbouringColumnsAreNotBuilt)
{
  // Two columns side by side share one wall, which is inside the water. The
  // pair must therefore cost less than twice a single column, not more.
  QString message;

  const std::unique_ptr<MeshLayer> one =
    layeredLayer(flatStrip(1, 2), message);
  const std::unique_ptr<MeshLayer> two =
    layeredLayer(flatStrip(2, 2), message);

  ASSERT_NE(one, nullptr) << message.toStdString();
  ASSERT_NE(two, nullptr) << message.toStdString();

  const int single = triangleCount(one->sceneSource()->sceneGeometry({}));
  const int pair = triangleCount(two->sceneSource()->sceneGeometry({}));

  // Two columns, two layers: 2 caps each (8 triangles), and 6 outer walls
  // per layer rather than 8 — the shared edge contributes none. One column
  // alone has 4 walls per layer, so sharing is what keeps the pair under
  // twice the single.
  EXPECT_EQ(pair, 2 * 2 * 2 + 6 * 2 * 2);
  EXPECT_LT(pair, 2 * single) << "the shared wall was built twice";
}

TEST_F(LayeredMeshTest, AWallStandingProudOfItsNeighbourIsStillBuilt)
{
  // Sigma layers follow the bed, so adjacent columns' slabs rarely line up.
  // Dropping a whole wall because its edge is interior punches a hole into
  // the mesh wherever the bed steps — and a step is the normal case, not an
  // exotic one.
  LayeredMesh stepped = flatStrip(2, 1, -10.0, 0.0);

  // Deepen the second column: its slab now reaches below the first's.
  stepped.interfaceZ[size_t(stepped.interfaceSlot(1, 1))] = -40.0;

  QString message;
  const std::unique_ptr<MeshLayer> layer = layeredLayer(stepped, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  const QVector<SceneGeometry> batches =
    layer->sceneSource()->sceneGeometry({});

  // The shared wall is exposed over the 30 m the second column extends past
  // the first, so it is built — as part of column 1's own wall.
  const LayeredMesh flat = flatStrip(2, 1, -10.0, 0.0);
  QString flatMessage;
  const std::unique_ptr<MeshLayer> flatLayer =
    layeredLayer(flat, flatMessage);
  ASSERT_NE(flatLayer, nullptr);

  EXPECT_GT(triangleCount(batches),
            triangleCount(flatLayer->sceneSource()->sceneGeometry({})))
    << "the exposed part of the shared wall was dropped with the rest of it";

  // And it really is where the step is: the geometry must reach the deeper
  // column's bed.
  double lowest = 0.0;

  for (const SceneGeometry &geometry : batches)
  {
    lowest = std::min(lowest, double(geometry.bounds.minimum().z()));
  }

  EXPECT_NEAR(lowest, -40.0, 1.0e-6);
}

TEST_F(LayeredMeshTest, TheGeometryBoundsAreTheWholeBoxItOccupies)
{
  // Framing reads these, so a bounds that is short on any axis frames the
  // scene wrongly — and does it quietly, since the geometry itself is fine.
  // Asserted on all three axes: the caps carry the vertical extremes, so a
  // check that only looked at z would pass with the footprint half missing.
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(2, 3, -12.0, 3.0), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  const QVector<SceneGeometry> batches =
    layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);

  const Bounds3D &bounds = batches.first().bounds;
  ASSERT_TRUE(bounds.isValid());

  EXPECT_NEAR(double(bounds.minimum().x()), 0.0, 1.0e-6);
  EXPECT_NEAR(double(bounds.maximum().x()), 2.0, 1.0e-6);
  EXPECT_NEAR(double(bounds.minimum().y()), 0.0, 1.0e-6);
  EXPECT_NEAR(double(bounds.maximum().y()), 1.0, 1.0e-6);
  EXPECT_NEAR(double(bounds.minimum().z()), -12.0, 1.0e-6);
  EXPECT_NEAR(double(bounds.maximum().z()), 3.0, 1.0e-6);
}

TEST_F(LayeredMeshTest, WallNormalsAreUnitAndPerpendicularToTheirEdge)
{
  // The material lights by these. Left unnormalised they scale the shading
  // term; pointing along the edge rather than across it, they shade a wall
  // as though it faced somewhere else. Both look like lighting choices.
  //
  // The column is deliberately neither square nor unit-sized: on a 1x1 cell
  // dividing by the edge length changes nothing, and swapping "across" for
  // "along" still yields an axis-aligned unit vector, so either fault would
  // pass unnoticed.
  HydroCouple::SDK::IO::MeshDefinition mesh;
  mesh.meshName = "oblong";
  mesh.nodeX = { 0.0, 7.0, 7.0, 0.0 };
  mesh.nodeY = { 0.0, 0.0, 3.0, 3.0 };
  mesh.faceNodeOffsets = { 0, 4 };
  mesh.faceNodes = { 0, 1, 2, 3 };

  LayeredMesh layered;
  layered.horizontal = mesh;
  layered.layerCount = 1;
  layered.interfaceZ = { 0.0, -4.0 };

  QString message;
  const std::unique_ptr<MeshLayer> layer = layeredLayer(layered, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  const QVector<SceneGeometry> batches =
    layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);

  const QVector<SceneVertex> &vertices = batches.first().vertices;

  int walls = 0;

  for (int index = 0; index + 3 < vertices.size();)
  {
    if (!qFuzzyIsNull(vertices[index].nz))
    {
      // A cap vertex; its normal is vertical.
      EXPECT_NEAR(std::abs(double(vertices[index].nz)), 1.0, 1.0e-5)
        << "a cap normal is not vertical";
      ++index;

      continue;
    }

    // Walls are emitted four vertices at a time: the edge's two ends at the
    // bottom, then the same two at the top.
    const SceneVertex &a = vertices[index];
    const SceneVertex &b = vertices[index + 1];

    const QVector3D normal(a.nx, a.ny, a.nz);
    const QVector3D along(b.x - a.x, b.y - a.y, 0.0f);

    EXPECT_NEAR(double(normal.length()), 1.0, 1.0e-5)
      << "wall normal (" << a.nx << ", " << a.ny << ") is not unit length";
    ASSERT_GT(double(along.length()), 1.0e-6) << "a wall has no edge";

    EXPECT_NEAR(double(QVector3D::dotProduct(normal, along.normalized())),
                0.0, 1.0e-5)
      << "wall normal (" << a.nx << ", " << a.ny << ") is not across its "
      << "edge (" << along.x() << ", " << along.y() << ")";

    ++walls;
    index += 4;
  }

  EXPECT_EQ(walls, 4) << "a single column should have four walls";
}

// ── Peeling ─────────────────────────────────────────────────────────────────

TEST_F(LayeredMeshTest, PeelingToOneLayerShowsThatLayersElevations)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(1, 4, -20.0, 0.0), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  layer->setVisibleLayers(1, 1);

  ASSERT_EQ(layer->firstVisibleLayer(), 1);
  ASSERT_EQ(layer->lastVisibleLayer(), 1);

  const QVector<SceneGeometry> batches =
    layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);

  // Layer 1 of four spans -5 to -10 in a 0..-20 column.
  EXPECT_NEAR(double(batches.first().bounds.maximum().z()), -5.0, 1.0e-6);
  EXPECT_NEAR(double(batches.first().bounds.minimum().z()), -10.0, 1.0e-6);
}

TEST_F(LayeredMeshTest, PeelingExposesTheCapsOfTheRangeItLeaves)
{
  // The whole point of peeling is to look inside, which means the newly cut
  // surfaces have to be closed. A range's caps cost the same as the whole
  // stack's — one top and one bottom — no matter where it is cut.
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(1, 6), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  const int whole = triangleCount(layer->sceneSource()->sceneGeometry({}));

  layer->setVisibleLayers(2, 3);
  const int slab = triangleCount(layer->sceneSource()->sceneGeometry({}));

  EXPECT_EQ(slab, 2 * 2 + 2 * 4 * 2);
  EXPECT_LT(slab, whole);
}

TEST_F(LayeredMeshTest, AVisibleRangeIsClampedRatherThanReadPastTheEnd)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(1, 3), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  layer->setVisibleLayers(-5, 99);
  EXPECT_EQ(layer->firstVisibleLayer(), 0);
  EXPECT_EQ(layer->lastVisibleLayer(), 2);

  // An inverted range is one layer, not an empty mesh.
  layer->setVisibleLayers(2, 0);
  EXPECT_EQ(layer->firstVisibleLayer(), 2);
  EXPECT_EQ(layer->lastVisibleLayer(), 2);
}

// ── Values ──────────────────────────────────────────────────────────────────

TEST_F(LayeredMeshTest, CellValuesColourTheLayerTheyBelongTo)
{
  // One value per cell, not per column: a layered field's whole purpose is
  // that it varies down the water column.
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(1, 2), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_FALSE(layer->setLayeredValues(QStringLiteral("temp"),
                                       QVector<double>{ 1.0 }))
    << "a per-column field was accepted for a layered mesh";

  ASSERT_TRUE(layer->setLayeredValues(QStringLiteral("temp"),
                                      QVector<double>{ 20.0, 4.0 }));

  LayerStyle *style = layer->style();
  style->setMode(StyleMode::Graduated);

  // Boundaries set outright, so the two cells land in known classes without
  // depending on how a method would have divided them.
  ASSERT_TRUE(style->classification().setManualBreaks({ 0.0, 12.0, 30.0 }));
  ASSERT_EQ(style->classification().breaks().size(), 2);

  style->classification().setClassColor(0, QColor(0, 0, 255));
  style->classification().setClassColor(1, QColor(255, 0, 0));

  const QVector<SceneGeometry> batches =
    layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);

  bool sawWarm = false;
  bool sawCold = false;

  for (const SceneVertex &vertex : batches.first().vertices)
  {
    sawWarm = sawWarm || (vertex.r > 0.9f && vertex.b < 0.1f);
    sawCold = sawCold || (vertex.b > 0.9f && vertex.r < 0.1f);
  }

  EXPECT_TRUE(sawWarm) << "the surface layer's value did not colour it";
  EXPECT_TRUE(sawCold) << "the bed layer's value did not colour it";
}

TEST_F(LayeredMeshTest, ACellWhoseClassIsHiddenIsNotBuilt)
{
  // The prisms resolve colour through their own path — cell values do not
  // belong to a feature, so the feature-indexed colorFor() cannot serve them
  // — which means the legend's class visibility has to be honoured here too,
  // and separately. Hiding a class must remove those cells' geometry, not
  // merely blend them away: a hidden class on a large mesh is exactly how a
  // user makes one worth looking at.
  QString message;
  const std::unique_ptr<MeshLayer> layer =
    layeredLayer(flatStrip(1, 2), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(layer->setLayeredValues(QStringLiteral("temp"),
                                      QVector<double>{ 20.0, 4.0 }));

  LayerStyle *style = layer->style();
  style->setMode(StyleMode::Graduated);
  ASSERT_TRUE(style->classification().setManualBreaks({ 0.0, 12.0, 30.0 }));

  const int both = triangleCount(layer->sceneSource()->sceneGeometry({}));
  ASSERT_GT(both, 0);

  // Hide the cold class: the bed layer goes, the surface layer stays.
  style->classification().setClassVisible(0, false);

  const int remaining = triangleCount(layer->sceneSource()->sceneGeometry({}));

  EXPECT_LT(remaining, both) << "the hidden class was built anyway";
  EXPECT_GT(remaining, 0) << "hiding one class removed both";

  // And it is the bed layer that went: nothing reaches the bottom any more.
  double lowest = 0.0;

  for (const SceneGeometry &geometry : layer->sceneSource()->sceneGeometry({}))
  {
    lowest = std::min(lowest, double(geometry.bounds.minimum().z()));
  }

  EXPECT_NEAR(lowest, -5.0, 1.0e-6)
    << "the hidden bed layer still contributed geometry";
}

// ── Reading a layered file ──────────────────────────────────────────────────

TEST_F(LayeredMeshTest, ReadsALayeredFileTheSdkWrote)
{
  if (!MeshLayer::ugridSupported())
  {
    GTEST_SKIP() << "the SDK this build links has no NetCDF support";
  }

  // Written by the SDK's own writer and read back through the SDK's reader,
  // so this proves the whole chain agrees rather than proving Composer
  // agrees with a file it invented. Composer links no NetCDF of its own.
  const QString path = QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR))
                         .filePath(QStringLiteral("generated-layered.nc"));
  QFile::remove(path);

  HydroCouple::SDK::IO::MeshDefinition mesh;
  mesh.meshName = "lake";
  mesh.nodeX = { 0.0, 10.0, 10.0, 0.0, 20.0, 20.0 };
  mesh.nodeY = { 0.0, 0.0, 10.0, 10.0, 0.0, 10.0 };
  mesh.faceNodeOffsets = { 0, 4, 8 };
  mesh.faceNodes = { 0, 1, 2, 3, 1, 4, 5, 2 };

  {
    HydroCouple::SDK::IO::NetCDFUGRIDWriter writer(path.toStdString(), mesh);

    // Two columns of different depth, so the layering is not uniform and a
    // reader that ignored per-face depth would still produce something.
    writer.setVerticalCoordinate({ 0.0, -0.25, -1.0 }, { 8.0, 20.0 },
                                 { 2.0, 2.0 });

    ASSERT_EQ(writer.initialize(), 0) << writer.errorMessage();
    ASSERT_EQ(writer.finalize(), 0) << writer.errorMessage();
  }

  ASSERT_TRUE(QFile::exists(path));
  EXPECT_TRUE(MeshLayer::isLayeredUGRIDFile(path, QString()));

  QString message;
  const std::unique_ptr<MeshLayer> layer =
    MeshLayer::fromLayeredUGRIDFile(path, QString(), 0, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_TRUE(layer->isLayered())
    << "the file's water column did not reach the layer: "
    << message.toStdString();

  const LayeredMesh &layered = layer->layering();

  EXPECT_EQ(layered.layerCount, 2);
  ASSERT_EQ(layered.columnCount(), 2);

  // Column 0: surface 2, depth 8 -> bed -8, a 10 m column with an interface
  // a quarter of the way down at -0.5.
  EXPECT_NEAR(layered.z(0, 0), 2.0, 1.0e-9);
  EXPECT_NEAR(layered.z(0, 1), -0.5, 1.0e-9);
  EXPECT_NEAR(layered.z(0, 2), -8.0, 1.0e-9);

  // Column 1 is deeper, which is what proves depth is read per face.
  EXPECT_NEAR(layered.z(1, 2), -20.0, 1.0e-9)
    << "every column took the same depth";
}

TEST_F(LayeredMeshTest, LoadsTheWaterColumnOfTheTimeAsked)
{
  if (!MeshLayer::ugridSupported())
  {
    GTEST_SKIP() << "the SDK this build links has no NetCDF support";
  }

  // A layered mesh moves: the surface is what turns sigma into elevations,
  // so stepping through a run has to reload the column, not only the values
  // on it. Reading time 0 for every step freezes the mesh in place.
  const QString path = QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR))
                         .filePath(QStringLiteral("generated-layered-times.nc"));
  QFile::remove(path);

  HydroCouple::SDK::IO::MeshDefinition mesh;
  mesh.meshName = "lake";
  mesh.nodeX = { 0.0, 10.0, 10.0, 0.0 };
  mesh.nodeY = { 0.0, 0.0, 10.0, 10.0 };
  mesh.faceNodeOffsets = { 0, 4 };
  mesh.faceNodes = { 0, 1, 2, 3 };

  {
    HydroCouple::SDK::IO::NetCDFUGRIDWriter writer(path.toStdString(), mesh);

    // One face, three time levels: the reservoir fills.
    writer.setVerticalCoordinate({ 0.0, -1.0 }, { 10.0 },
                                 { 0.0, 2.0, 5.0 });

    ASSERT_EQ(writer.initialize(), 0) << writer.errorMessage();
    ASSERT_EQ(writer.finalize(), 0) << writer.errorMessage();
  }

  EXPECT_EQ(MeshLayer::ugridTimeCount(path, QString()), 3);

  QString message;

  for (const auto &[timeIndex, surface] :
       { std::pair<int, double>{ 0, 0.0 }, { 1, 2.0 }, { 2, 5.0 } })
  {
    const std::unique_ptr<MeshLayer> layer =
      MeshLayer::fromLayeredUGRIDFile(path, QString(), timeIndex, message);

    ASSERT_NE(layer, nullptr) << message.toStdString();
    ASSERT_TRUE(layer->isLayered()) << message.toStdString();

    EXPECT_NEAR(layer->layering().z(0, 0), surface, 1.0e-9)
      << "time " << timeIndex << " did not take its own water surface";

    // The bed does not move with it: depth is measured from the datum.
    EXPECT_NEAR(layer->layering().z(0, 1), -10.0, 1.0e-9);
  }
}

TEST_F(LayeredMeshTest, APlainTwoDimensionalFileStillLoadsAsASurface)
{
  if (!MeshLayer::ugridSupported())
  {
    GTEST_SKIP() << "the SDK this build links has no NetCDF support";
  }

  // Most UGRID meshes have no water column, and asking for the layered
  // treatment must degrade to the flat one rather than fail.
  const QString path = QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR))
                         .filePath(QStringLiteral("generated-flat.nc"));
  QFile::remove(path);

  HydroCouple::SDK::IO::MeshDefinition mesh;
  mesh.meshName = "flat";
  mesh.nodeX = { 0.0, 1.0, 1.0, 0.0 };
  mesh.nodeY = { 0.0, 0.0, 1.0, 1.0 };
  mesh.faceNodeOffsets = { 0, 4 };
  mesh.faceNodes = { 0, 1, 2, 3 };

  {
    HydroCouple::SDK::IO::NetCDFUGRIDWriter writer(path.toStdString(), mesh);
    ASSERT_EQ(writer.initialize(), 0) << writer.errorMessage();
    ASSERT_EQ(writer.finalize(), 0) << writer.errorMessage();
  }

  EXPECT_FALSE(MeshLayer::isLayeredUGRIDFile(path, QString()));

  QString message;
  const std::unique_ptr<MeshLayer> layer =
    MeshLayer::fromLayeredUGRIDFile(path, QString(), 0, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  EXPECT_FALSE(layer->isLayered());
  EXPECT_FALSE(layer->sceneSource()->sceneGeometry({}).isEmpty())
    << "a two-dimensional mesh lost its surface as well as its column";
}

// ── It renders ──────────────────────────────────────────────────────────────

TEST_F(LayeredMeshTest, ALayeredMeshRendersAndPeelingChangesThePicture)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  QString message;
  std::unique_ptr<MeshLayer> owned =
    layeredLayer(flatStrip(6, 5, -20.0, 0.0), message);
  ASSERT_NE(owned, nullptr) << message.toStdString();

  MeshLayer *layer = owned.release();
  ASSERT_GE(stack.addLayer(layer), 0);

  Camera camera;
  camera.setTarget(QVector3D(3.0f, 0.5f, -10.0f));
  camera.setElevation(25.0);
  camera.setAzimuth(30.0);
  camera.setDistance(40.0);

  const QSize size(200, 150);
  const QColor background(0, 0, 0);

  const QImage whole =
    renderSceneToImage(renderer, camera, size, background, message);
  ASSERT_FALSE(whole.isNull()) << message.toStdString();

  layer->setVisibleLayers(0, 0);

  const QImage peeled =
    renderSceneToImage(renderer, camera, size, background, message);
  ASSERT_FALSE(peeled.isNull()) << message.toStdString();

  const QDir fixtures(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR));
  whole.save(fixtures.filePath(QStringLiteral("generated-layered-whole.png")));
  peeled.save(
    fixtures.filePath(QStringLiteral("generated-layered-peeled.png")));

  const auto drawn = [](const QImage &image)
  {
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);
    int count = 0;

    for (int y = 0; y < rgb.height(); ++y)
    {
      for (int x = 0; x < rgb.width(); ++x)
      {
        if (qRed(rgb.pixel(x, y)) > 6 || qGreen(rgb.pixel(x, y)) > 6 ||
            qBlue(rgb.pixel(x, y)) > 6)
        {
          ++count;
        }
      }
    }

    return count;
  };

  const int wholeDrawn = drawn(whole);

  ASSERT_GT(wholeDrawn, size.width() * size.height() / 20);

  // A single surface layer is a thin sheet where the stack was a solid body.
  EXPECT_LT(drawn(peeled), wholeDrawn)
    << "peeling to one layer drew as much as the whole stack";
}

// ── D3c: the water column under a picked face ───────────────────────────────
//
// A profile answers what a column holds from top to bottom. The gate is that
// the values are the ones attached to that column's cells and the elevations
// are the ones the layering puts them at -- both compared against the mesh
// the test built, not against a second reading of the layer.

namespace
{
  //! A layered strip whose cell values are column*100 + layer.
  std::unique_ptr<MeshLayer> profiledLayer(int columns, int layers,
                                           double bed, double surface,
                                           QString &message)
  {
    const LayeredMesh mesh = flatStrip(columns, layers, bed, surface);

    std::unique_ptr<MeshLayer> layer = layeredLayer(mesh, message);

    if (!layer)
    {
      return nullptr;
    }

    QVector<double> values(static_cast<int>(mesh.cellCount()), 0.0);

    for (int column = 0; column < columns; ++column)
    {
      for (int layer_ = 0; layer_ < layers; ++layer_)
      {
        values[static_cast<int>(mesh.cell(column, layer_))] =
          column * 100.0 + layer_;
      }
    }

    if (!layer->setLayeredValues(QStringLiteral("temperature"), values))
    {
      message = QStringLiteral("the layered values were refused");
      return nullptr;
    }

    return layer;
  }
}

TEST_F(LayeredMeshTest, AColumnProfileIsItsOwnCellsAtItsOwnElevations)
{
  QString message;

  // Three columns so a profile of the wrong one is visible in the values,
  // and four layers over a 20 m column so the elevations are not all equal.
  const std::unique_ptr<MeshLayer> layer =
    profiledLayer(3, 4, -20.0, 0.0, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  for (int column = 0; column < 3; ++column)
  {
    SCOPED_TRACE("column " + std::to_string(column));

    QVector<double> values;
    QVector<double> elevations;

    ASSERT_TRUE(layer->columnProfile(column, values, elevations, message))
      << message.toStdString();

    ASSERT_EQ(values.size(), 4);
    ASSERT_EQ(elevations.size(), 4);

    for (int level = 0; level < 4; ++level)
    {
      // Surface first, as the layering indexes them: reading the column the
      // other way up transposes it into something that still looks like a
      // profile.
      EXPECT_NEAR(values.at(level), column * 100.0 + level, 1.0e-9)
        << "layer " << level;

      // Each layer is 5 m thick over a 20 m column, so its centre sits at
      // -2.5, -7.5, -12.5, -17.5 -- the midpoint of its interfaces, not
      // either boundary it shares with a neighbour.
      EXPECT_NEAR(elevations.at(level), -2.5 - 5.0 * level, 1.0e-9)
        << "layer " << level;
    }
  }
}

TEST_F(LayeredMeshTest, AProfileFollowsTheSurfaceItHangsUnder)
{
  QString message;

  // The same layering, lifted: a sigma column stretches with its surface, so
  // the elevations move and the values do not.
  const std::unique_ptr<MeshLayer> layer =
    profiledLayer(1, 2, -10.0, 6.0, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  QVector<double> values;
  QVector<double> elevations;

  ASSERT_TRUE(layer->columnProfile(0, values, elevations, message))
    << message.toStdString();

  ASSERT_EQ(elevations.size(), 2);

  // Surface 6, bed -10: two 8 m layers centred at 2 and -6.
  EXPECT_NEAR(elevations.at(0), 2.0, 1.0e-9);
  EXPECT_NEAR(elevations.at(1), -6.0, 1.0e-9);
}

TEST_F(LayeredMeshTest, AFlatMeshHasNoColumnToProfile)
{
  QString message;

  const std::unique_ptr<MeshLayer> flat = MeshLayer::create(
    QStringLiteral("flat"), strip(2), MeshEntity::Face, message);
  ASSERT_NE(flat, nullptr) << message.toStdString();

  QVector<double> values;
  QVector<double> elevations;

  EXPECT_FALSE(flat->columnProfile(0, values, elevations, message));
  EXPECT_TRUE(message.contains(QStringLiteral("flat"))) << message.toStdString();
}

TEST_F(LayeredMeshTest, ALayeringWithoutValuesIsSaidRatherThanProfiled)
{
  QString message;

  const LayeredMesh mesh = flatStrip(2, 3);
  const std::unique_ptr<MeshLayer> layer = layeredLayer(mesh, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  QVector<double> values;
  QVector<double> elevations;

  // A mesh carrying its shape and not its values would otherwise profile as
  // a column of zeros, which reads as a model that computed them.
  EXPECT_FALSE(layer->columnProfile(0, values, elevations, message));
  EXPECT_FALSE(message.isEmpty());
  EXPECT_TRUE(values.isEmpty());
  EXPECT_TRUE(elevations.isEmpty());
}

TEST_F(LayeredMeshTest, AColumnOutsideTheMeshIsRefused)
{
  QString message;

  const std::unique_ptr<MeshLayer> layer =
    profiledLayer(2, 3, -10.0, 0.0, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  QVector<double> values;
  QVector<double> elevations;

  EXPECT_FALSE(layer->columnProfile(2, values, elevations, message));
  EXPECT_TRUE(message.contains(QStringLiteral("2"))) << message.toStdString();

  EXPECT_FALSE(layer->columnProfile(-1, values, elevations, message));
}

TEST_F(LayeredMeshTest, TheProfilePanelFollowsTheSelection)
{
  LayerStackModel stack;
  ProfilePlotPanel panel;
  panel.setModel(&stack);

  EXPECT_EQ(panel.profileCount(), 0);
  EXPECT_FALSE(panel.statusText().isEmpty());

  QString message;
  MeshLayer *layer = profiledLayer(3, 4, -20.0, 0.0, message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{0, 2});

  ASSERT_EQ(panel.profileCount(), 2);
  EXPECT_EQ(panel.layer(), layer);
  EXPECT_TRUE(panel.statusText().isEmpty());

  // Sorted by column, and each profile carries its own column's values
  // against its own elevations -- not the first column's twice.
  const QVector<ExportSeries> &profiles = panel.profiles();
  ASSERT_EQ(profiles.size(), 2);

  EXPECT_EQ(profiles.at(0).name, QStringLiteral("Column 0"));
  EXPECT_EQ(profiles.at(1).name, QStringLiteral("Column 2"));

  ASSERT_EQ(profiles.at(0).values.size(), 4);
  EXPECT_NEAR(profiles.at(0).values.first(), 0.0, 1.0e-9);
  EXPECT_NEAR(profiles.at(1).values.first(), 200.0, 1.0e-9);

  // The elevations ride along, because a value without the height it was
  // measured at is not a profile.
  EXPECT_NEAR(profiles.at(0).julianDays.first(), -2.5, 1.0e-9);

  auto *view = panel.findChild<QChartView *>(QStringLiteral("profilePlotView"));
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->chart()->series().size(), 2);

  stack.selectOnly(nullptr, QSet<int>{});
  EXPECT_EQ(panel.profileCount(), 0);
  EXPECT_FALSE(panel.statusText().isEmpty());

  // The chart is emptied too, not just the record of what it held: series
  // left attached accumulate on every refresh and are drawn over the next
  // selection's.
  EXPECT_EQ(view->chart()->series().size(), 0);
}

TEST_F(LayeredMeshTest, TheProfilePanelSaysWhenAColumnIsNotWhatIsSelected)
{
  LayerStackModel stack;
  ProfilePlotPanel panel;
  panel.setModel(&stack);

  QString message;

  // A layered mesh drawn by its edges. An edge has no water column hanging
  // under it, and profiling the face of the same index would answer with a
  // column that is not the one picked.
  //
  // Named "sides" rather than "edges" on purpose: the layer's name goes into
  // every message this panel produces, so a layer called "edges" would make
  // the assertion below pass on any of them.
  LayeredMesh mesh = flatStrip(3, 2);

  // The strip carries faces and no edge list, and a layer asked to draw
  // edges it does not have falls back to nodes -- so the edges are given
  // here, or this test would be about the fallback instead.
  mesh.horizontal.edgeNodes = {{0, 1}, {1, 2}};

  MeshLayer *sides = MeshLayer::create(QStringLiteral("sides"),
                                       mesh.horizontal, MeshEntity::Edge,
                                       message)
                       .release();
  ASSERT_NE(sides, nullptr) << message.toStdString();
  ASSERT_EQ(sides->entity(), MeshEntity::Edge)
    << "the layer did not draw the entity this test is about";
  ASSERT_TRUE(sides->setLayering(mesh, message)) << message.toStdString();

  // And given values, so refusing to profile is a decision about *what* is
  // selected rather than a layer that had nothing to draw anyway.
  QVector<double> values(static_cast<int>(mesh.cellCount()), 1.0);
  ASSERT_TRUE(sides->setLayeredValues(QStringLiteral("temperature"), values));

  ASSERT_GE(stack.addLayer(sides), 0);

  stack.selectOnly(sides, QSet<int>{0});

  EXPECT_EQ(panel.profileCount(), 0);
  EXPECT_TRUE(panel.statusText().contains(QStringLiteral("edge")))
    << panel.statusText().toStdString();
}

TEST_F(LayeredMeshTest, TheProfileAxesAreValueAcrossAndElevationUp)
{
  LayerStackModel stack;
  ProfilePlotPanel panel;
  panel.setModel(&stack);

  QString message;
  MeshLayer *layer = profiledLayer(1, 4, -20.0, 0.0, message).release();
  ASSERT_NE(layer, nullptr) << message.toStdString();
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.selectOnly(layer, QSet<int>{0});
  ASSERT_EQ(panel.profileCount(), 1);

  auto *view = panel.findChild<QChartView *>(QStringLiteral("profilePlotView"));
  ASSERT_NE(view, nullptr);

  const QList<QAbstractAxis *> vertical = view->chart()->axes(Qt::Vertical);
  const QList<QAbstractAxis *> horizontal = view->chart()->axes(Qt::Horizontal);
  ASSERT_FALSE(vertical.isEmpty());
  ASSERT_FALSE(horizontal.isEmpty());

  auto *elevation = qobject_cast<QValueAxis *>(vertical.first());
  auto *value = qobject_cast<QValueAxis *>(horizontal.first());
  ASSERT_NE(elevation, nullptr);
  ASSERT_NE(value, nullptr);

  // Elevation up. A profile drawn with the axes the other way round is a
  // time series' shape, and reads as one.
  EXPECT_NEAR(elevation->min(), -17.5, 1.0e-9);
  EXPECT_NEAR(elevation->max(), -2.5, 1.0e-9);

  EXPECT_NEAR(value->min(), 0.0, 1.0e-9);
  EXPECT_NEAR(value->max(), 3.0, 1.0e-9);

  // The points themselves, not only the ranges the axes were given: the two
  // are computed separately, so a plot appending them the other way round
  // still gets its axes right and draws the profile on its side.
  ASSERT_EQ(view->chart()->series().size(), 1);

  const auto *line =
    qobject_cast<const QLineSeries *>(view->chart()->series().first());
  ASSERT_NE(line, nullptr);
  ASSERT_EQ(line->count(), 4);

  for (int layer = 0; layer < 4; ++layer)
  {
    const QPointF point = line->at(layer);

    EXPECT_NEAR(point.x(), layer, 1.0e-9) << "layer " << layer;
    EXPECT_NEAR(point.y(), -2.5 - 5.0 * layer, 1.0e-9) << "layer " << layer;
  }

  // And the axis carries the name the values were attached under.
  EXPECT_EQ(value->titleText(), QStringLiteral("temperature"));
}
