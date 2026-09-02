/*!
 * \file   test_dataitemlayers.cpp
 * \brief  Phase C2 verification — component data items and meshes on the map.
 */

#include "core/composerapplication.h"
#include "gis/spatialreference.h"
#include "layers/dataitemlayer.h"
#include "layers/rasterdataitemlayer.h"
#include "scene/scenegeometry.h"
#include "scene/scenesource.h"
#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "render/layerstyle.h"

#include "hydrocouplesdk/io/netcdfugridwriter.h"
#include "spatialstubs.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  class DataItemLayerTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_dataitemlayers";
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

  ComposerApplication *DataItemLayerTest::s_app = nullptr;

  //! A four-node square, split into two triangles sharing a diagonal.
  MeshDefinition twoTriangleMesh()
  {
    MeshDefinition mesh;
    mesh.meshName = "test";
    mesh.nodeX = {0.0, 10.0, 10.0, 0.0};
    mesh.nodeY = {0.0, 0.0, 10.0, 10.0};
    mesh.faceNodeOffsets = {0, 3, 6};
    mesh.faceNodes = {0, 1, 2, 0, 2, 3};
    mesh.edgeNodes = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {0, 2}};

    return mesh;
  }
}

// ── Geometry data items ─────────────────────────────────────────────────────

TEST_F(DataItemLayerTest, TurnsAGeometryItemIntoFeaturesWithItsValues)
{
  Testing::StubCrs crs(4326);

  const auto a = Testing::makePoint(0.0, 0.0, 0, &crs);
  const auto b = Testing::makePoint(1.0, 2.0, 1, &crs);
  const auto c = Testing::makePoint(3.0, 4.0, 2, &crs);

  Testing::StubGeometryItem item("depths", {a.get(), b.get(), c.get()});
  item.setValue(0, 1.0);
  item.setValue(1, 5.0);
  item.setValue(2, 9.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->featureCount(), 3);
  EXPECT_EQ(layer->geometryKind(), GeometryKind::Point);

  // The item's values arrive as an ordinary attribute, which is what lets a
  // component's output be classified with the same controls as a file column.
  ASSERT_FALSE(layer->valueAttribute().isEmpty());
  EXPECT_NEAR(layer->attributeValue(2, layer->valueAttribute()).toDouble(), 9.0,
              1e-9);

  // The extent is the geometry's, in the item's own CRS.
  EXPECT_NEAR(layer->extent().right(), 3.0, 1e-9);
  ASSERT_NE(layer->crs(), nullptr) << "the item's CRS was not read";
  EXPECT_TRUE(layer->crs()->isGeographic());
}

TEST_F(DataItemLayerTest, ReadsPolygonGeometryThroughWellKnownBinary)
{
  Testing::StubCrs crs(4326);

  const auto square = Testing::makePolygon(
    {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}, 0, &crs);
  Testing::StubGeometryItem item("areas", {square.get()});
  item.setValue(0, 16.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->geometryKind(), GeometryKind::Polygon);
  ASSERT_EQ(layer->featureCount(), 1);

  const QPolygonF ring = layer->features().first().parts.first();
  ASSERT_GE(ring.size(), 4);
  EXPECT_EQ(ring.first(), ring.last()) << "the ring did not come back closed";

  // Polygons open translucent, so what lies beneath them stays visible.
  EXPECT_LT(layer->style()->symbol().fill.alpha(), 255);
}

TEST_F(DataItemLayerTest, RefusesItemsThatCarryNoGeometry)
{
  Testing::StubGeometryItem empty("nothing", {});

  QString message;

  EXPECT_EQ(DataItemLayer::create(&empty, message), nullptr);
  EXPECT_FALSE(message.isEmpty()) << "a refusal with no explanation";

  EXPECT_FALSE(DataItemLayer::isSpatial(nullptr));
  EXPECT_TRUE(DataItemLayer::isSpatial(&empty));
}

TEST_F(DataItemLayerTest, RereadsValuesWithoutRebuildingGeometry)
{
  Testing::StubCrs crs(4326);
  const auto a = Testing::makePoint(0.0, 0.0, 0, &crs);
  const auto b = Testing::makePoint(1.0, 1.0, 1, &crs);

  Testing::StubGeometryItem item("depths", {a.get(), b.get()});
  item.setValue(0, 1.0);
  item.setValue(1, 2.0);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&item, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  const int readsAfterLoad = item.geometryReads;
  ASSERT_GT(readsAfterLoad, 0) << "geometry was never read at all";

  // What a running model does between steps: the values move, the mesh does
  // not.
  item.setValue(0, 40.0);
  item.setValue(1, 80.0);

  ASSERT_TRUE(layer->refreshValues());

  EXPECT_NEAR(layer->attributeValue(1, layer->valueAttribute()).toDouble(),
              80.0, 1e-9);
  EXPECT_EQ(layer->featureCount(), 2);

  // Counted at the source rather than by comparing buffer addresses: a
  // rebuilt vector of the same size frequently lands on the same address.
  EXPECT_EQ(item.geometryReads, readsAfterLoad)
    << "the geometry was re-read to pick up new values";
}

TEST_F(DataItemLayerTest, ClassifiesAComponentsValuesLikeAnyOtherAttribute)
{
  Testing::StubCrs crs(4326);
  const auto a = Testing::makePoint(0.0, 0.0, 0, &crs);
  const auto b = Testing::makePoint(1.0, 1.0, 1, &crs);
  const auto c = Testing::makePoint(2.0, 2.0, 2, &crs);
  const auto d = Testing::makePoint(3.0, 3.0, 3, &crs);

  Testing::StubGeometryItem item("depths",
                                 {a.get(), b.get(), c.get(), d.get()});

  for (int i = 0; i < 4; ++i)
  {
    item.setValue(i, i * 10.0);
  }

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&item, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  layer->style()->setMode(StyleMode::Graduated);
  layer->style()->setAttribute(layer->valueAttribute());
  layer->style()->classification().setClassCount(2);

  ASSERT_TRUE(layer->restyle());

  ASSERT_EQ(layer->style()->legendItems().size(), 2);
  EXPECT_NE(layer->style()->colorFor(*layer, 0),
            layer->style()->colorFor(*layer, 3));
}

// The SDK's spatiotemporal items declare {time, geometries} — time first. A
// viewer that assumed the entity axis was dimension 0 would read one value
// per time step and colour every feature by a time index.
TEST_F(DataItemLayerTest, FindsTheEntityAxisWhenTimeComesFirst)
{
  Testing::StubCrs crs(4326);
  const auto a = Testing::makePoint(0.0, 0.0, 0, &crs);
  const auto b = Testing::makePoint(1.0, 1.0, 1, &crs);
  const auto c = Testing::makePoint(2.0, 2.0, 2, &crs);

  constexpr int kSteps = 4;
  Testing::StubTimeGeometryItem item("depths", kSteps,
                                     {a.get(), b.get(), c.get()});

  for (int step = 0; step < kSteps; ++step)
  {
    for (int geometry = 0; geometry < 3; ++geometry)
    {
      // Distinct per geometry and per step, so reading the wrong axis — or
      // the wrong step — produces a different set of numbers.
      item.setValue(step, geometry, step * 100.0 + geometry);
    }
  }

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&item, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_EQ(layer->featureCount(), 3);

  // One value per geometry, from the most recent step — which is what a map
  // of a running model should show.
  const QString field = layer->valueAttribute();

  EXPECT_NEAR(layer->attributeValue(0, field).toDouble(), 300.0, 1e-9);
  EXPECT_NEAR(layer->attributeValue(1, field).toDouble(), 301.0, 1e-9);
  EXPECT_NEAR(layer->attributeValue(2, field).toDouble(), 302.0, 1e-9);
}

// ── Meshes ──────────────────────────────────────────────────────────────────

TEST_F(DataItemLayerTest, DrawsMeshFacesAsClosedPolygons)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("mesh"), twoTriangleMesh(), MeshEntity::Face, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->entity(), MeshEntity::Face);
  ASSERT_EQ(layer->featureCount(), 2);
  EXPECT_EQ(layer->geometryKind(), GeometryKind::Polygon);

  // UGRID connectivity does not repeat the first node, so the ring has to be
  // closed here or each face draws with a side missing.
  const QPolygonF ring = layer->features().first().parts.first();
  ASSERT_EQ(ring.size(), 4);
  EXPECT_EQ(ring.first(), ring.last());

  EXPECT_NEAR(layer->extent().width(), 10.0, 1e-9);
  EXPECT_NEAR(layer->extent().height(), 10.0, 1e-9);
}

TEST_F(DataItemLayerTest, DrawsMeshEdgesAndNodesWhenAsked)
{
  QString message;

  const std::unique_ptr<MeshLayer> edges = MeshLayer::create(
    QStringLiteral("edges"), twoTriangleMesh(), MeshEntity::Edge, message);
  ASSERT_NE(edges, nullptr) << message.toStdString();
  EXPECT_EQ(edges->featureCount(), 5);
  EXPECT_EQ(edges->geometryKind(), GeometryKind::Line);

  const std::unique_ptr<MeshLayer> nodes = MeshLayer::create(
    QStringLiteral("nodes"), twoTriangleMesh(), MeshEntity::Node, message);
  ASSERT_NE(nodes, nullptr) << message.toStdString();
  EXPECT_EQ(nodes->featureCount(), 4);
  EXPECT_EQ(nodes->geometryKind(), GeometryKind::Point);
}

TEST_F(DataItemLayerTest, FallsBackWhenAMeshHasNoFaces)
{
  MeshDefinition cloud;
  cloud.nodeX = {0.0, 1.0, 2.0};
  cloud.nodeY = {0.0, 1.0, 0.0};

  QString message;

  // A node-only mesh is valid — a point cloud, or the nodes of a 1-D network
  // — so asking for faces it does not have must not fail.
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("cloud"), cloud, MeshEntity::Face, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  EXPECT_EQ(layer->entity(), MeshEntity::Node);
  EXPECT_EQ(layer->featureCount(), 3);
}

TEST_F(DataItemLayerTest, RefusesAMeshWithNoNodes)
{
  QString message;

  EXPECT_EQ(MeshLayer::create(QStringLiteral("empty"), MeshDefinition(),
                              MeshEntity::Face, message),
            nullptr);
  EXPECT_FALSE(message.isEmpty());
}

TEST_F(DataItemLayerTest, SkipsFacesWhoseConnectivityPointsOutsideTheMesh)
{
  MeshDefinition broken = twoTriangleMesh();

  // A differently-based or truncated connectivity array. Drawing it would
  // read past the end of the node coordinates.
  broken.faceNodes = {0, 1, 2, 0, 2, 99};

  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("mesh"), broken, MeshEntity::Face, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  EXPECT_EQ(layer->featureCount(), 1) << "the corrupt face was drawn anyway";
}

TEST_F(DataItemLayerTest, AttachesOneValuePerDrawnEntity)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("mesh"), twoTriangleMesh(), MeshEntity::Face, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Values belonging to a different entity — nodes, here — must be refused: a
  // mesh coloured by the wrong values is worse than an uncoloured one,
  // because it looks like an answer.
  EXPECT_FALSE(layer->setValues(QStringLiteral("depth"),
                                {1.0, 2.0, 3.0, 4.0}));
  EXPECT_TRUE(layer->valueAttribute().isEmpty());

  ASSERT_TRUE(layer->setValues(QStringLiteral("depth"), {1.0, 2.0}));
  EXPECT_EQ(layer->valueAttribute(), QStringLiteral("depth"));
  EXPECT_NEAR(layer->attributeValue(1, QStringLiteral("depth")).toDouble(), 2.0,
              1e-9);
}

TEST_F(DataItemLayerTest, DrawsAMeshOnTheMap)
{
  QString message;
  std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("mesh"), twoTriangleMesh(), MeshEntity::Face, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_TRUE(layer->setValues(QStringLiteral("depth"), {1.0, 9.0}));

  layer->style()->setMode(StyleMode::Graduated);
  layer->style()->setAttribute(QStringLiteral("depth"));
  layer->style()->classification().setClassCount(2);
  ASSERT_TRUE(layer->restyle());

  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  canvas.setBackgroundColor(Qt::white);
  canvas.resize(200, 200);

  MeshLayer *raw = layer.get();
  stack.addLayer(layer.release());
  canvas.zoomToFullExtent();

  QImage image(200, 200, QImage::Format_ARGB32);
  image.fill(Qt::white);
  canvas.render(&image);

  EXPECT_EQ(raw->featureCount(), 2);

  // The two triangles meet along the diagonal from bottom-left to top-right,
  // so the halves either side of it carry different class colours.
  const QColor lower = image.pixelColor(150, 150);
  const QColor upper = image.pixelColor(50, 50);

  EXPECT_NE(lower, QColor(Qt::white)) << "the mesh was not drawn";
  EXPECT_NE(upper, QColor(Qt::white));
  EXPECT_NE(lower, upper) << "both faces were drawn the same colour";
}

// ── Regular grids (C2) ──────────────────────────────────────────────────────

TEST_F(DataItemLayerTest, DrawsARegularGridAsAMeshOfQuads)
{
  Testing::StubCrs crs(4326);
  Testing::StubGrid grid(4, 3, 5.0, &crs);
  Testing::StubGridItem item("depths", &grid);

  for (int cell = 0; cell < 6; ++cell)
  {
    item.setValue(cell, cell * 2.0);
  }

  EXPECT_TRUE(MeshLayer::isRegularGrid(&item));

  QString message;
  const std::unique_ptr<MeshLayer> layer =
    MeshLayer::fromRegularGrid(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Three by two cells from four by three nodes — a grid is a mesh whose
  // faces happen to be quads.
  EXPECT_EQ(layer->featureCount(), 6);
  EXPECT_EQ(layer->geometryKind(), GeometryKind::Polygon);
  EXPECT_EQ(layer->features().first().parts.first().size(), 5)
    << "a quad face should come back as a closed four-sided ring";

  EXPECT_NEAR(layer->extent().width(), 15.0, 1e-9);
  EXPECT_NEAR(layer->extent().height(), 10.0, 1e-9);

  ASSERT_NE(layer->crs(), nullptr);
  EXPECT_TRUE(layer->crs()->isGeographic());
}

TEST_F(DataItemLayerTest, LeavesInactiveGridCellsOutAndKeepsValuesAligned)
{
  Testing::StubCrs crs(4326);
  Testing::StubGrid grid(4, 3, 5.0, &crs);

  // A hole in the domain: cell (0, 0). Every later cell's value would land on
  // the wrong face if the faces were matched to values by position.
  grid.deactivate(0, 0);

  Testing::StubGridItem item("depths", &grid);

  for (int cell = 0; cell < 6; ++cell)
  {
    item.setValue(cell, cell * 2.0);
  }

  QString message;
  const std::unique_ptr<MeshLayer> layer =
    MeshLayer::fromRegularGrid(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->featureCount(), 5)
    << "an inactive cell was drawn as though the model solved on it";

  ASSERT_FALSE(layer->valueAttribute().isEmpty());

  // The first drawn face is grid cell 1, whose value is 2 — not cell 0's.
  EXPECT_NEAR(layer->attributeValue(0, layer->valueAttribute()).toDouble(), 2.0,
              1e-9);
  EXPECT_NEAR(layer->attributeValue(4, layer->valueAttribute()).toDouble(),
              10.0, 1e-9);
}

// ── UGRID files (C2, via the SDK's reader) ──────────────────────────────────

TEST_F(DataItemLayerTest, LoadsAMeshFromAUgridFileTheSdkWrote)
{
  if (!MeshLayer::ugridSupported())
  {
    GTEST_SKIP() << "the SDK this build links has no NetCDF support";
  }

  // Written by the SDK's own writer, so this proves the pair agree end to
  // end rather than proving the reader agrees with a file this test invented.
  const QString path =
    QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR))
      .filePath(QStringLiteral("generated-mesh.nc"));

  QFile::remove(path);

  {
    HydroCouple::SDK::IO::NetCDFUGRIDWriter writer(path.toStdString(),
                                                   twoTriangleMesh());
    ASSERT_EQ(writer.initialize(), 0);
    writer.finalize();
  }

  ASSERT_TRUE(QFile::exists(path)) << "the SDK writer produced no file";

  const QStringList meshes = MeshLayer::ugridMeshNames(path);
  ASSERT_EQ(meshes.size(), 1);
  EXPECT_EQ(meshes.first(), QStringLiteral("test"));

  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::fromUGRIDFile(
    path, QString(), MeshEntity::Face, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->name(), QStringLiteral("test"));
  EXPECT_EQ(layer->featureCount(), 2);
  EXPECT_EQ(layer->geometryKind(), GeometryKind::Polygon);

  // The geometry survived the file: same extent, same closed rings.
  EXPECT_NEAR(layer->extent().width(), 10.0, 1e-9);
  EXPECT_NEAR(layer->extent().height(), 10.0, 1e-9);

  const QPolygonF ring = layer->features().first().parts.first();
  ASSERT_EQ(ring.size(), 4);
  EXPECT_EQ(ring.first(), ring.last());
}

TEST_F(DataItemLayerTest, ExplainsAUgridFileItCannotRead)
{
  if (!MeshLayer::ugridSupported())
  {
    GTEST_SKIP() << "the SDK this build links has no NetCDF support";
  }

  QString message;

  EXPECT_EQ(MeshLayer::fromUGRIDFile(QStringLiteral("/no/such/mesh.nc"),
                                     QString(), MeshEntity::Face, message),
            nullptr);
  EXPECT_FALSE(message.isEmpty()) << "a failure with no explanation";

  EXPECT_TRUE(MeshLayer::ugridMeshNames(QStringLiteral("/no/such/mesh.nc"))
                .isEmpty());
}

// ── Polyhedral surface data items ───────────────────────────────────────────
//
// A component's mesh item attaches its values to one of three entities, and
// the mesh below has a different number of each -- 4 nodes, 5 edges, 2 faces.
// Drawing the wrong one is therefore visible in the count alone, which is the
// point: a triangle has as many edges as vertices, so a fixture where the
// counts coincide would pass whatever was drawn.

namespace
{
  //! A mesh item of \a steps levels with value = step*100 + entity.
  std::unique_ptr<Testing::StubTimeSurfaceItem> surfaceItem(
    HydroCouple::Spatial::MeshDataObjectType attachedTo, int steps = 3)
  {
    auto item = std::make_unique<Testing::StubTimeSurfaceItem>(
      "depth", twoTriangleMesh(), attachedTo, steps);

    const int entities = static_cast<int>(
      Testing::StubTimeSurfaceItem::entityCount(twoTriangleMesh(), attachedTo));

    for (int step = 0; step < steps; ++step)
    {
      for (int entity = 0; entity < entities; ++entity)
      {
        item->setValue(step, entity, step * 100.0 + entity);
      }
    }

    return item;
  }
}

TEST_F(DataItemLayerTest, ASurfaceItemIsDrawnOnTheEntityItsValuesBelongTo)
{
  struct Expectation
  {
      HydroCouple::Spatial::MeshDataObjectType attachedTo;
      int features;
      GeometryKind kind;
      const char *what;
  };

  const Expectation cases[] = {
    {HydroCouple::Spatial::MeshDataObjectType::Vertex, 4, GeometryKind::Point, "vertex"},
    {HydroCouple::Spatial::MeshDataObjectType::Edge, 5, GeometryKind::Line, "edge"},
    {HydroCouple::Spatial::MeshDataObjectType::Cell, 2, GeometryKind::Polygon, "cell"},
  };

  for (const Expectation &expected : cases)
  {
    SCOPED_TRACE(expected.what);

    const std::unique_ptr<Testing::StubTimeSurfaceItem> item =
      surfaceItem(expected.attachedTo);

    QString message;
    const std::unique_ptr<DataItemLayer> layer =
      DataItemLayer::create(item.get(), message);
    ASSERT_NE(layer, nullptr) << message.toStdString();

    // One feature per entity the values are on. An edge item drawn as faces
    // would carry five values on two features -- three of them lost, and the
    // two shown attached to the wrong shapes.
    EXPECT_EQ(layer->featureCount(), expected.features);
    EXPECT_EQ(layer->geometryKind(), expected.kind);

    // And the values follow the same order the entities do.
    EXPECT_NEAR(
      layer->attributeValue(0, layer->valueAttribute()).toDouble(),
      200.0, 1.0e-9);
    EXPECT_NEAR(
      layer->attributeValue(expected.features - 1, layer->valueAttribute())
        .toDouble(),
      200.0 + expected.features - 1, 1.0e-9);
  }
}

TEST_F(DataItemLayerTest, AnEdgeItemsFeaturesAreTheMeshsEdges)
{
  const std::unique_ptr<Testing::StubTimeSurfaceItem> item =
    surfaceItem(HydroCouple::Spatial::MeshDataObjectType::Edge);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(item.get(), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  const MeshDefinition mesh = twoTriangleMesh();
  ASSERT_EQ(layer->featureCount(), static_cast<int>(mesh.edgeCount()));

  // Endpoints, not just counts: five lines of the right length in the wrong
  // places would satisfy every count assertion above.
  for (int64_t edge = 0; edge < mesh.edgeCount(); ++edge)
  {
    SCOPED_TRACE("edge " + std::to_string(edge));

    const QVector<QPolygonF> &parts =
      layer->features().at(static_cast<int>(edge)).parts;
    ASSERT_EQ(parts.size(), 1);
    ASSERT_EQ(parts.first().size(), 2);

    const size_t origin =
      static_cast<size_t>(mesh.edgeNodes[static_cast<size_t>(edge)][0]);
    const size_t destination =
      static_cast<size_t>(mesh.edgeNodes[static_cast<size_t>(edge)][1]);

    EXPECT_NEAR(parts.first().first().x(), mesh.nodeX[origin], 1.0e-9);
    EXPECT_NEAR(parts.first().first().y(), mesh.nodeY[origin], 1.0e-9);
    EXPECT_NEAR(parts.first().last().x(), mesh.nodeX[destination], 1.0e-9);
    EXPECT_NEAR(parts.first().last().y(), mesh.nodeY[destination], 1.0e-9);
  }
}

TEST_F(DataItemLayerTest, AnEdgeItemOnASurfaceWithNoEdgesIsRefused)
{
  MeshDefinition faceless = twoTriangleMesh();
  faceless.edgeNodes.clear();

  Testing::StubTimeSurfaceItem item("depth", faceless,
                                    HydroCouple::Spatial::MeshDataObjectType::Edge, 3);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&item, message);

  // Refused rather than quietly drawn as faces. Two polygons for five edge
  // values is a map that looks right and is not, and the entity a value
  // belongs to is not something a viewer gets to substitute.
  EXPECT_EQ(layer, nullptr)
    << "an edge item was drawn on a surface that has no edges";
  EXPECT_TRUE(message.contains(QStringLiteral("edge")))
    << message.toStdString();
}

TEST_F(DataItemLayerTest, SkipsEdgesWhoseEndpointsPointOutsideTheMesh)
{
  MeshDefinition malformed = twoTriangleMesh();

  // One edge naming a node the mesh does not have. Dereferencing it would
  // read past the coordinate arrays; clamping it would draw a line somewhere
  // definite and wrong.
  malformed.edgeNodes.push_back({0, 99});

  Testing::StubTimeSurfaceItem item("depth", malformed,
                                    HydroCouple::Spatial::MeshDataObjectType::Edge,
                                    2);

  QString message;
  const std::unique_ptr<DataItemLayer> layer =
    DataItemLayer::create(&item, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  // The five good edges are drawn and the sixth is not.
  EXPECT_EQ(layer->featureCount(), 5);
}

// ── Raster data items ───────────────────────────────────────────────────────
//
// A raster is neither a feature nor a mesh: millions of cells as polygons is
// a mesh nobody can draw, so it is shaded into an image the way a GeoTIFF is.
// This is what lets a coverage be a layer and model data at once.

TEST_F(DataItemLayerTest, ARasterItemIsNotSomethingDataItemLayerClaims)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(4, 3, 0.0, 30.0, 10.0, &crs);
  Testing::StubRasterItem item("depths", &raster);

  // DataItemLayer draws what is features once read; saying yes here and then
  // returning nullptr from create() would be worse than saying no.
  EXPECT_FALSE(DataItemLayer::isSpatial(&item));
  EXPECT_TRUE(RasterDataItemLayer::isRaster(&item));
}

TEST_F(DataItemLayerTest, DrawsARasterItemOverTheGroundItCovers)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(4, 3, 0.0, 30.0, 10.0, &crs);
  Testing::StubRasterItem item("depths", &raster);

  for (int row = 0; row < 3; ++row)
  {
    for (int column = 0; column < 4; ++column)
    {
      item.setValue(0, row, column, row * 4.0 + column);
    }
  }

  QString message;
  const std::unique_ptr<RasterDataItemLayer> layer =
    RasterDataItemLayer::create(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Four columns and three rows of ten-unit cells, hung from an upper-left
  // origin: rows run down, which is what the negative y step means.
  EXPECT_NEAR(layer->extent().width(), 40.0, 1e-9);
  EXPECT_NEAR(layer->extent().height(), 30.0, 1e-9);
  EXPECT_NEAR(layer->extent().left(), 0.0, 1e-9);
  EXPECT_NEAR(layer->extent().bottom(), 30.0, 1e-9);

  EXPECT_EQ(layer->valueRange().first, 0.0);
  EXPECT_EQ(layer->valueRange().second, 11.0);

  ASSERT_NE(layer->crs(), nullptr);
  EXPECT_TRUE(layer->crs()->isGeographic());
}

TEST_F(DataItemLayerTest, ARasterIsShadedOverItsOwnRangeAndNotSomeFixedOne)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(2, 1, 0.0, 10.0, 10.0, &crs);
  Testing::StubRasterItem item("depths", &raster);

  item.setValue(0, 0, 0, 100.0);
  item.setValue(0, 0, 1, 200.0);

  QString message;
  const std::unique_ptr<RasterDataItemLayer> layer =
    RasterDataItemLayer::create(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  QImage canvas(64, 64, QImage::Format_ARGB32);
  canvas.fill(Qt::white);

  {
    QPainter painter(&canvas);
    MapTransform transform(layer->extent(), QSize(64, 64));
    layer->render(painter, transform);
  }

  ASSERT_EQ(layer->lastImage().size(), QSize(2, 1));
  EXPECT_NE(layer->lastImage().pixelColor(0, 0),
            layer->lastImage().pixelColor(1, 0))
    << "both cells were shaded the same colour";
}

// No-data is nothing, not a colour at one end of the ramp -- a hole in a
// survey shaded deep blue reads as a lake.
TEST_F(DataItemLayerTest, ARastersHolesAreDrawnAsHolesAndLeftOutOfItsRange)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(2, 1, 0.0, 10.0, 10.0, &crs);
  Testing::StubRasterItem item("depths", &raster);

  item.setValue(0, 0, 0, 5.0);
  item.setValue(0, 0, 1, std::numeric_limits<double>::quiet_NaN());

  QString message;
  const std::unique_ptr<RasterDataItemLayer> layer =
    RasterDataItemLayer::create(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->valueRange().first, 5.0);
  EXPECT_EQ(layer->valueRange().second, 5.0)
    << "a hole was counted as a value";

  QImage canvas(64, 64, QImage::Format_ARGB32);

  {
    QPainter painter(&canvas);
    MapTransform transform(layer->extent(), QSize(64, 64));
    layer->render(painter, transform);
  }

  EXPECT_EQ(layer->lastImage().pixelColor(1, 0).alpha(), 0)
    << "a hole in the survey was given a colour";
  EXPECT_NE(layer->lastImage().pixelColor(0, 0).alpha(), 0);
}

TEST_F(DataItemLayerTest, ASecondBandIsADifferentPictureOfTheSameGround)
{
  Testing::StubCrs crs(4326);

  // Two rows as well as two bands, so reading the band axis as the row axis
  // is a different answer rather than the same numbers by coincidence.
  Testing::StubRaster raster(2, 2, 0.0, 20.0, 10.0, &crs, 2);
  Testing::StubRasterItem item("depths", &raster);

  item.setValue(0, 0, 0, 1.0);
  item.setValue(0, 0, 1, 2.0);
  item.setValue(0, 1, 0, 3.0);
  item.setValue(0, 1, 1, 4.0);
  item.setValue(1, 0, 0, 30.0);
  item.setValue(1, 0, 1, 40.0);
  item.setValue(1, 1, 0, 50.0);
  item.setValue(1, 1, 1, 60.0);

  QString message;
  const std::unique_ptr<RasterDataItemLayer> layer =
    RasterDataItemLayer::create(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  EXPECT_EQ(layer->valueRange().second, 4.0);

  layer->setBand(1);

  EXPECT_EQ(layer->band(), 1);
  EXPECT_EQ(layer->valueRange().first, 30.0);
  EXPECT_EQ(layer->valueRange().second, 60.0)
    << "the second band was read from the first band's rows";

  // The ground did not move.
  EXPECT_NEAR(layer->extent().width(), 20.0, 1e-9);
  EXPECT_NEAR(layer->extent().height(), 20.0, 1e-9);
}

TEST_F(DataItemLayerTest, AnItemThatIsNotARasterIsRefusedWithAReason)
{
  Testing::StubCrs crs(4326);
  const auto point = Testing::makePoint(0.0, 0.0, 0, &crs);
  Testing::StubGeometryItem item("depths", {point.get()});

  EXPECT_FALSE(RasterDataItemLayer::isRaster(&item));
  EXPECT_FALSE(RasterDataItemLayer::isRaster(nullptr));

  QString message;
  EXPECT_EQ(RasterDataItemLayer::create(&item, message), nullptr);
  EXPECT_FALSE(message.isEmpty());
}

// The SDK's own RasterComponentDataItem is one band with shape {y, x}. An
// item shaped that way must be refused and told why, not read as though its
// rows were its bands.
TEST_F(DataItemLayerTest, ARasterItemWithNoBandAxisIsRefusedAndToldWhy)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(2, 2, 0.0, 20.0, 10.0, &crs);
  Testing::StubRasterItem item("depths", &raster);
  item.reportShapeWithoutABandAxis();

  QString message;
  EXPECT_EQ(RasterDataItemLayer::create(&item, message), nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("band"))) << message.toStdString();
}

// Every cell a hole is a drawable, empty raster -- not a range of positive
// and negative infinity that shades into nonsense.
TEST_F(DataItemLayerTest, ARasterThatIsAllHolesIsEmptyRatherThanWild)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(2, 1, 0.0, 10.0, 10.0, &crs);
  Testing::StubRasterItem item("depths", &raster);

  const double hole = std::numeric_limits<double>::quiet_NaN();
  item.setValue(0, 0, 0, hole);
  item.setValue(0, 0, 1, hole);

  QString message;
  const std::unique_ptr<RasterDataItemLayer> layer =
    RasterDataItemLayer::create(&item, message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  EXPECT_EQ(layer->valueRange().first, 0.0);
  EXPECT_EQ(layer->valueRange().second, 0.0);

  QImage canvas(64, 64, QImage::Format_ARGB32);

  {
    QPainter painter(&canvas);
    MapTransform transform(layer->extent(), QSize(64, 64));
    layer->render(painter, transform);
  }

  EXPECT_EQ(layer->lastImage().pixelColor(0, 0).alpha(), 0);
  EXPECT_EQ(layer->lastImage().pixelColor(1, 0).alpha(), 0);
}

// ── The raster item joins the 3D scene (coherence plan V8) ────────────────
//
// Run results were the one whole class of layer silently absent from 3D:
// RasterDataItemLayer derived from MapLayer alone, so a simulated depth
// field could be browsed on the map and never draped over the terrain it
// was computed on.

TEST_F(DataItemLayerTest, ARasterItemHasA3dFormAndItIsItsShadedBand)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(2, 2, 0.0, 20.0, 10.0, &crs);
  Testing::StubRasterItem item("depths", &raster);

  item.setValue(0, 0, 0, 1.0);
  item.setValue(0, 0, 1, 2.0);
  item.setValue(0, 1, 0, 3.0);
  item.setValue(0, 1, 1, 4.0);

  QString message;
  const std::unique_ptr<RasterDataItemLayer> layer =
    RasterDataItemLayer::create(&item, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_NE(layer->sceneSource(), nullptr)
    << "raster results are still absent from the scene";

  // Flat, no terrain: one textured quad over the raster's footprint.
  const QVector<SceneGeometry> batches =
    layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);
  EXPECT_FALSE(batches.first().texture.isNull());
  EXPECT_EQ(batches.first().textureExtent.normalized(),
            QRectF(0.0, 0.0, 20.0, 20.0).normalized());

  const Bounds3D bounds = layer->sceneSource()->sceneBounds();
  ASSERT_TRUE(bounds.isValid());
  EXPECT_NEAR(bounds.maximum().x(), 20.0, 1e-6);
}

TEST_F(DataItemLayerTest, ANewBandInvalidatesTheDrapedTexture)
{
  Testing::StubCrs crs(4326);
  Testing::StubRaster raster(2, 1, 0.0, 10.0, 10.0, &crs, 2);
  Testing::StubRasterItem item("depths", &raster);

  // The second band runs the other way, because each band is shaded over
  // its own range: two ascending bands would normalise to the same picture
  // and the gate would compare a texture with itself.
  item.setValue(0, 0, 0, 1.0);
  item.setValue(0, 0, 1, 2.0);
  item.setValue(1, 0, 0, 40.0);
  item.setValue(1, 0, 1, 30.0);

  QString message;
  const std::unique_ptr<RasterDataItemLayer> layer =
    RasterDataItemLayer::create(&item, message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  const QImage first =
    layer->sceneSource()->sceneGeometry({}).first().texture;

  layer->setBand(1);

  const QImage second =
    layer->sceneSource()->sceneGeometry({}).first().texture;

  EXPECT_NE(first, second)
    << "the scene kept draping the old band's picture";
}
