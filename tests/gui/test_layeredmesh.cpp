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
#include "scene/camera.h"
#include "scene/sceneimage.h"
#include "scene/scenerenderer.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>

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
  EXPECT_EQ(triangleCount(thin->sceneSource()->sceneGeometry()),
            thinCaps + 4 * 2);
  EXPECT_EQ(triangleCount(thick->sceneSource()->sceneGeometry()),
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

  const int single = triangleCount(one->sceneSource()->sceneGeometry());
  const int pair = triangleCount(two->sceneSource()->sceneGeometry());

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
    layer->sceneSource()->sceneGeometry();

  // The shared wall is exposed over the 30 m the second column extends past
  // the first, so it is built — as part of column 1's own wall.
  const LayeredMesh flat = flatStrip(2, 1, -10.0, 0.0);
  QString flatMessage;
  const std::unique_ptr<MeshLayer> flatLayer =
    layeredLayer(flat, flatMessage);
  ASSERT_NE(flatLayer, nullptr);

  EXPECT_GT(triangleCount(batches),
            triangleCount(flatLayer->sceneSource()->sceneGeometry()))
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
    layer->sceneSource()->sceneGeometry();
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

  const int whole = triangleCount(layer->sceneSource()->sceneGeometry());

  layer->setVisibleLayers(2, 3);
  const int slab = triangleCount(layer->sceneSource()->sceneGeometry());

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
    layer->sceneSource()->sceneGeometry();
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

  const int both = triangleCount(layer->sceneSource()->sceneGeometry());
  ASSERT_GT(both, 0);

  // Hide the cold class: the bed layer goes, the surface layer stays.
  style->classification().setClassVisible(0, false);

  const int remaining = triangleCount(layer->sceneSource()->sceneGeometry());

  EXPECT_LT(remaining, both) << "the hidden class was built anyway";
  EXPECT_GT(remaining, 0) << "hiding one class removed both";

  // And it is the bed layer that went: nothing reaches the bottom any more.
  double lowest = 0.0;

  for (const SceneGeometry &geometry : layer->sceneSource()->sceneGeometry())
  {
    lowest = std::min(lowest, double(geometry.bounds.minimum().z()));
  }

  EXPECT_NEAR(lowest, -5.0, 1.0e-6)
    << "the hidden bed layer still contributed geometry";
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
