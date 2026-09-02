/*!
 * \file   test_drape.cpp
 * \brief  C3c — vector layers composed against a terrain.
 *
 * The property under test is stated in the plan as "a draped line follows the
 * surface it is draped on", and it is checked twice over, because the two
 * halves fail independently: the *sampler* can read the wrong height, and the
 * *drape* can read the right heights at too few places and then cut a straight
 * chord between them. A tilted plane catches the first and is blind to the
 * second — linear interpolation is exact on a plane no matter how coarsely it
 * is sampled — so the curved case carries the densification.
 *
 * The one pixel test is here for a reason nothing else can reach: a line lying
 * exactly on the surface it was sampled from has the same depth as that
 * surface, and which of the two wins is decided per fragment on the device.
 */

#include "layers/featurelayer.h"
#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "render/layerstyle.h"
#include "scene/camera.h"
#include "scene/sceneimage.h"
#include "scene/scenerenderer.h"

#include "vectorprobe.h"

#include <gtest/gtest.h>

#include <QSignalSpy>

#include <QApplication>
#include <QDir>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  //! The tilted plane the exactness cases are checked against.
  double plane(double x, double y)
  {
    return 2.0 * x + 3.0 * y + 5.0;
  }

  //! A bowl, so that a chord between two samples of it is not on it.
  double bowl(double x, double y)
  {
    return 0.004 * ((x - 50.0) * (x - 50.0) + (y - 50.0) * (y - 50.0));
  }

  /*!
   * \brief A quad grid over [0,100]^2, elevated by \a surface.
   * \param divisions Cells per side.
   * \param surface Elevation as a function of position.
   */
  MeshDefinition grid(int divisions, double (*surface)(double, double))
  {
    MeshDefinition mesh;
    mesh.meshName = "terrain";

    const double step = 100.0 / double(divisions);

    for (int row = 0; row <= divisions; ++row)
    {
      for (int column = 0; column <= divisions; ++column)
      {
        const double x = double(column) * step;
        const double y = double(row) * step;

        mesh.nodeX.push_back(x);
        mesh.nodeY.push_back(y);
        mesh.nodeZ.push_back(surface ? surface(x, y) : 0.0);
      }
    }

    mesh.faceNodeOffsets.push_back(0);

    for (int row = 0; row < divisions; ++row)
    {
      for (int column = 0; column < divisions; ++column)
      {
        const int64_t corner = int64_t(row) * (divisions + 1) + column;

        mesh.faceNodes.push_back(corner);
        mesh.faceNodes.push_back(corner + 1);
        mesh.faceNodes.push_back(corner + divisions + 2);
        mesh.faceNodes.push_back(corner + divisions + 1);
        mesh.faceNodeOffsets.push_back(
          int64_t(mesh.faceNodes.size()));
      }
    }

    return mesh;
  }

  //! A grid with its elevations stripped: geometry, but no surface.
  MeshDefinition flatGrid(int divisions)
  {
    MeshDefinition mesh = grid(divisions, nullptr);
    mesh.nodeZ.clear();

    return mesh;
  }

  using Network = Testing::VectorProbe;

  /*!
   * \brief A flat terrain that counts the samples taken from it.
   *
   * Which layer the renderer decided to drape on is not visible in the
   * geometry — two terrains at different heights would be, but a test that
   * reads heights is testing the drape again rather than the choice. Counting
   * the samples makes the choice itself observable, and the layer that was
   * not chosen is the one that was never asked.
   */
  class SheetTerrain : public Network, public ITerrainSource
  {
    public:
      SheetTerrain(const QString &name, double height)
        : Network(name), m_height(height)
      {
        addRing({ { 0.0, 0.0 },
                  { 100.0, 0.0 },
                  { 100.0, 100.0 },
                  { 0.0, 100.0 },
                  { 0.0, 0.0 } });
      }

      [[nodiscard]] const ITerrainSource *terrain() const override
      {
        return this;
      }

      [[nodiscard]] bool elevationAt(const QPointF &,
                                     double &elevation) const override
      {
        ++samples;
        elevation = m_height;

        return true;
      }

      [[nodiscard]] QRectF terrainExtent() const override
      {
        return QRectF(-1.0e6, -1.0e6, 2.0e6, 2.0e6);
      }

      [[nodiscard]] double terrainResolution() const override
      {
        return 25.0;
      }

      mutable int samples = 0;

    private:
      double m_height;
  };

  std::unique_ptr<MeshLayer> terrainLayer(const QString &name,
                                          const MeshDefinition &mesh)
  {
    QString message;
    std::unique_ptr<MeshLayer> layer =
      MeshLayer::create(name, mesh, MeshEntity::Face, message);

    EXPECT_TRUE(layer) << message.toStdString();

    return layer;
  }

  //! The context the renderer would build for a stack holding \a terrain.
  SceneContext contextFor(const MeshLayer *terrain)
  {
    SceneContext context;
    context.terrain = terrain ? terrain->sceneSource()->terrain() : nullptr;

    return context;
  }

  //! Every vertex of every batch of \a geometry, in one list.
  QVector<SceneVertex> allVertices(const QVector<SceneGeometry> &geometry)
  {
    QVector<SceneVertex> vertices;

    for (const SceneGeometry &batch : geometry)
    {
      vertices.append(batch.vertices);
    }

    return vertices;
  }

  /*!
   * \brief Pixels the red network line reached.
   *
   * By dominance rather than by an exact colour: the scene's headlight shades
   * every surface by its angle to the eye, so the same red line comes back at
   * full strength from straight above and at 107 from a grazing view. Matching
   * the literal colour would make a test about occlusion fail for the camera
   * angle instead.
   */
  int countRed(const QImage &image)
  {
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);

    int matches = 0;

    for (int y = 0; y < rgb.height(); ++y)
    {
      for (int x = 0; x < rgb.width(); ++x)
      {
        const QRgb pixel = rgb.pixel(x, y);

        if (qRed(pixel) >= 60 && qGreen(pixel) * 3 <= qRed(pixel) &&
            qBlue(pixel) * 3 <= qRed(pixel))
        {
          ++matches;
        }
      }
    }

    return matches;
  }

  void writeFixture(const QImage &image, const QString &name)
  {
    image.save(QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR))
                 .filePath(QStringLiteral("generated-drape-%1.png").arg(name)));
  }

  class DrapeTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!QApplication::instance())
        {
          static int argc = 1;
          static char name[] = "test_drape";
          static char *argv[] = { name, nullptr };
          s_app = new QApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static QApplication *s_app;
  };

  QApplication *DrapeTest::s_app = nullptr;

  // ── the terrain a mesh offers ───────────────────────────────────────────

  TEST_F(DrapeTest, AMeshWithElevationsIsTerrain)
  {
    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("terrain"), grid(4, plane));

    ASSERT_TRUE(layer->sceneSource());
    EXPECT_TRUE(layer->sceneSource()->terrain())
      << "a mesh carrying node elevations is a surface others can rest on";
  }

  TEST_F(DrapeTest, AMeshWithoutElevationsDeclinesToBeTerrain)
  {
    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("flat"), flatGrid(4));

    ASSERT_TRUE(layer->sceneSource());
    EXPECT_FALSE(layer->sceneSource()->terrain())
      << "a sheet at zero moves nothing, so offering it as terrain would "
         "only make a flat drape look like a considered one";
  }

  TEST_F(DrapeTest, ReportsTheCellSizeAsItsResolution)
  {
    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("terrain"), grid(10, plane));

    EXPECT_NEAR(layer->terrainResolution(), 10.0, 1.0e-9);
  }

  TEST_F(DrapeTest, ReportsItsFootprintAsItsExtent)
  {
    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("terrain"), grid(4, plane));

    const QRectF extent = layer->terrainExtent();

    EXPECT_NEAR(extent.left(), 0.0, 1.0e-9);
    EXPECT_NEAR(extent.right(), 100.0, 1.0e-9);
    EXPECT_NEAR(extent.top(), 0.0, 1.0e-9);
    EXPECT_NEAR(extent.bottom(), 100.0, 1.0e-9);
  }

  // ── the sampler ─────────────────────────────────────────────────────────

  TEST_F(DrapeTest, SamplesATiltedPlaneExactly)
  {
    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    // Interior, cell centres, cell corners and an edge midpoint: a point on a
    // shared edge belongs to two faces and must be answered by one of them,
    // not fall between them.
    const QVector<QPointF> probes = { { 3.0, 7.0 },   { 50.0, 50.0 },
                                      { 12.5, 12.5 }, { 25.0, 37.5 },
                                      { 0.0, 0.0 },   { 100.0, 100.0 },
                                      { 99.9, 0.1 } };

    for (const QPointF &probe : probes)
    {
      double elevation = -1.0e9;

      ASSERT_TRUE(layer->elevationAt(probe, elevation))
        << "no face answered for (" << probe.x() << ", " << probe.y() << ")";
      EXPECT_NEAR(elevation, plane(probe.x(), probe.y()), 1.0e-9)
        << "at (" << probe.x() << ", " << probe.y() << ")";
    }
  }

  TEST_F(DrapeTest, SamplesTheFanTheRendererActuallyDraws)
  {
    // One quad whose four corners are not coplanar. Its elevation at the
    // centre depends entirely on how it is cut into triangles: the fan from
    // corner 0 gives the mean of the diagonal it uses, and any other
    // tessellation gives a different number. The scene draws the fan from
    // corner 0, so that is the only answer that puts a drape on the surface
    // rather than above or below it.
    MeshDefinition mesh;
    mesh.meshName = "warped";
    mesh.nodeX = { 0.0, 100.0, 100.0, 0.0 };
    mesh.nodeY = { 0.0, 0.0, 100.0, 100.0 };
    mesh.nodeZ = { 0.0, 40.0, 0.0, 0.0 };
    mesh.faceNodeOffsets = { 0, 4 };
    mesh.faceNodes = { 0, 1, 2, 3 };

    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("warped"), mesh);

    // Just inside the triangle (0, 1, 2), where the surface is the plane
    // through those three corners: z = 40 * (x - y) / 100.
    double elevation = 0.0;
    ASSERT_TRUE(layer->elevationAt(QPointF(60.0, 40.0), elevation));
    EXPECT_NEAR(elevation, 8.0, 1.0e-9);

    // And in the triangle (0, 2, 3), which is flat at zero.
    ASSERT_TRUE(layer->elevationAt(QPointF(40.0, 60.0), elevation));
    EXPECT_NEAR(elevation, 0.0, 1.0e-9);
  }

  TEST_F(DrapeTest, DeclinesPointsOffTheMesh)
  {
    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("terrain"), grid(4, plane));

    double elevation = 42.0;

    EXPECT_FALSE(layer->elevationAt(QPointF(-10.0, 50.0), elevation));
    EXPECT_FALSE(layer->elevationAt(QPointF(50.0, 250.0), elevation));
    EXPECT_DOUBLE_EQ(elevation, 42.0)
      << "a refused sample must leave the caller's value alone";
  }

  TEST_F(DrapeTest, FindsTheFaceWhenCentroidsMislead)
  {
    // One coarse cell beside a column of fine ones — a graded mesh, which is
    // every mesh generated to a channel. Near their shared boundary the
    // nearest centroid belongs to a fine cell that does not contain the
    // point, and the coarse cell that does is the tenth-nearest. Answering
    // from the nearest centroid alone puts the drape on the wrong cell, or on
    // no cell at all.
    MeshDefinition mesh;
    mesh.meshName = "graded";

    mesh.nodeX = { 0.0, 100.0, 100.0, 0.0 };
    mesh.nodeY = { 0.0, 0.0, 100.0, 100.0 };
    mesh.nodeZ = { 7.0, 7.0, 7.0, 7.0 };
    mesh.faceNodeOffsets = { 0, 4 };
    mesh.faceNodes = { 0, 1, 2, 3 };

    for (int row = 0; row <= 10; ++row)
    {
      mesh.nodeX.push_back(100.0);
      mesh.nodeY.push_back(double(row) * 10.0);
      mesh.nodeZ.push_back(99.0);
      mesh.nodeX.push_back(110.0);
      mesh.nodeY.push_back(double(row) * 10.0);
      mesh.nodeZ.push_back(99.0);
    }

    for (int row = 0; row < 10; ++row)
    {
      const int64_t corner = 4 + int64_t(row) * 2;

      mesh.faceNodes.push_back(corner);
      mesh.faceNodes.push_back(corner + 1);
      mesh.faceNodes.push_back(corner + 3);
      mesh.faceNodes.push_back(corner + 2);
      mesh.faceNodeOffsets.push_back(int64_t(mesh.faceNodes.size()));
    }

    const std::unique_ptr<MeshLayer> layer =
      terrainLayer(QStringLiteral("graded"), mesh);

    double elevation = 0.0;

    // Deep inside the coarse cell by area, but 7 units from a fine centroid
    // and 49 from its own.
    ASSERT_TRUE(layer->elevationAt(QPointF(99.0, 50.0), elevation))
      << "no face answered for a point the coarse cell plainly contains";
    EXPECT_NEAR(elevation, 7.0, 1.0e-9);

    // And the fine cells still answer for themselves.
    ASSERT_TRUE(layer->elevationAt(QPointF(105.0, 55.0), elevation));
    EXPECT_NEAR(elevation, 99.0, 1.0e-9);
  }

  // ── the drape ───────────────────────────────────────────────────────────

  TEST_F(DrapeTest, ALineLayerContributesToTheScene)
  {
    Network network(QStringLiteral("conduits"));
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    ASSERT_TRUE(network.sceneSource());
    EXPECT_FALSE(network.sceneGeometry({}).isEmpty());
  }

  TEST_F(DrapeTest, APointLayerContributesNothing)
  {
    Network network(QStringLiteral("junctions"));
    network.addPoint({ 50.0, 50.0 });

    EXPECT_FALSE(network.sceneSource())
      << "a point cloud has no 3D form that is not invented";
  }

  TEST_F(DrapeTest, ADrapedLineLiesOnAPlane)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("conduits"));
    network.addLine({ { 5.0, 5.0 }, { 95.0, 40.0 }, { 60.0, 95.0 } });

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry(contextFor(terrain.get())));

    ASSERT_FALSE(vertices.isEmpty());

    for (const SceneVertex &vertex : vertices)
    {
      EXPECT_NEAR(double(vertex.z), plane(vertex.x, vertex.y), 1.0e-3)
        << "at (" << vertex.x << ", " << vertex.y << ")";
    }
  }

  TEST_F(DrapeTest, ADrapedLineFollowsACurvedSurface)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("bowl"), grid(20, bowl));

    Network network(QStringLiteral("conduits"));
    network.addLine({ { 2.0, 50.0 }, { 98.0, 50.0 } });

    const QVector<SceneGeometry> geometry =
      network.sceneGeometry(contextFor(terrain.get()));
    const QVector<SceneVertex> vertices = allVertices(geometry);

    ASSERT_GE(vertices.size(), 20)
      << "a single chord across a 96-unit line cannot follow a curve; the "
         "drape must densify to the terrain's own cell size";

    // Every emitted vertex is on the drawn surface — which on this mesh is
    // the mesh's own linear interpolation of the bowl, not the bowl itself,
    // so the tolerance is the mesh's faceting and not the sampler's error.
    for (const SceneVertex &vertex : vertices)
    {
      double surface = 0.0;

      ASSERT_TRUE(
        terrain->elevationAt(QPointF(vertex.x, vertex.y), surface));
      EXPECT_NEAR(double(vertex.z), surface, 1.0e-3);
    }

    // And the polyline as a whole stays on the bowl. A single chord from end
    // to end would sit 2.3 units above the low point, so this bound is only
    // reachable by following the surface.
    double worst = 0.0;

    for (const SceneVertex &vertex : vertices)
    {
      worst = std::max(worst, std::abs(double(vertex.z) -
                                       bowl(vertex.x, vertex.y)));
    }

    EXPECT_LT(worst, 0.05) << "the drape departs from the bowl by " << worst;
  }

  TEST_F(DrapeTest, ALineOffTheEdgeHoldsTheHeightItLeftAt)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("conduits"));
    network.addLine({ { 50.0, 50.0 }, { 250.0, 50.0 } });

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry(contextFor(terrain.get())));

    ASSERT_FALSE(vertices.isEmpty());

    double highest = 0.0;
    double lowest = 1.0e9;

    for (const SceneVertex &vertex : vertices)
    {
      if (vertex.x > 100.0)
      {
        highest = std::max(highest, double(vertex.z));
        lowest = std::min(lowest, double(vertex.z));
      }
    }

    // The plane reaches 350 at the mesh's far edge; every point beyond it
    // holds that, rather than dropping to zero and putting a cliff into the
    // scene where the data merely ran out.
    EXPECT_NEAR(lowest, plane(100.0, 50.0), 1.0e-3);
    EXPECT_NEAR(highest, plane(100.0, 50.0), 1.0e-3);
  }

  TEST_F(DrapeTest, ALineBeforeTheEdgeTakesTheFirstHeightItFinds)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("conduits"));
    network.addLine({ { -80.0, 50.0 }, { 90.0, 50.0 } });

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry(contextFor(terrain.get())));

    ASSERT_FALSE(vertices.isEmpty());

    for (const SceneVertex &vertex : vertices)
    {
      if (vertex.x < 0.0)
      {
        EXPECT_GT(double(vertex.z), 100.0)
          << "the leading run has nothing behind it to hold, so it takes the "
             "first elevation the terrain does answer rather than zero";
      }
    }
  }

  TEST_F(DrapeTest, FlatIgnoresTheTerrainUnderIt)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("conduits"));
    network.setZPolicy({ZMode::Constant, 0.0, {}, 0.0});
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry(contextFor(terrain.get())));

    ASSERT_EQ(vertices.size(), 2) << "flat needs no densification";

    for (const SceneVertex &vertex : vertices)
    {
      EXPECT_FLOAT_EQ(vertex.z, 0.0f);
    }
  }

  TEST_F(DrapeTest, TerrainWithoutOneFallsFlatRatherThanAway)
  {
    Network network(QStringLiteral("conduits"));
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    const QVector<SceneVertex> vertices = allVertices(network.sceneGeometry({}));

    ASSERT_FALSE(vertices.isEmpty())
      << "a network is still data when the mesh beside it is closed";

    for (const SceneVertex &vertex : vertices)
    {
      EXPECT_FLOAT_EQ(vertex.z, 0.0f);
    }
  }

  TEST_F(DrapeTest, PolygonRingsAreDrapedAsOutlines)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("catchments"));
    network.addRing({ { 20.0, 20.0 },
                      { 80.0, 20.0 },
                      { 80.0, 80.0 },
                      { 20.0, 80.0 },
                      { 20.0, 20.0 } });

    const QVector<SceneGeometry> geometry =
      network.sceneGeometry(contextFor(terrain.get()));

    ASSERT_EQ(geometry.size(), 1);
    EXPECT_EQ(geometry.first().primitive, ScenePrimitive::Lines)
      << "filling a polygon against terrain is a constrained triangulation, "
         "and its outline already says what the map cannot";

    for (const SceneVertex &vertex : geometry.first().vertices)
    {
      EXPECT_NEAR(double(vertex.z), plane(vertex.x, vertex.y), 1.0e-3);
    }
  }

  // ── extrusion ───────────────────────────────────────────────────────────

  TEST_F(DrapeTest, ExtrusionStandsACurtainOnTheTerrain)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("conduits"));
        network.setExtrusionHeight(25.0);
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    const QVector<SceneGeometry> geometry =
      network.sceneGeometry(contextFor(terrain.get()));

    ASSERT_EQ(geometry.size(), 2) << "a curtain and the crest that caps it";
    EXPECT_EQ(geometry.at(0).primitive, ScenePrimitive::Triangles);
    EXPECT_EQ(geometry.at(1).primitive, ScenePrimitive::Lines);

    // Each quad is four vertices: two on the ground, two a height above the
    // same two positions.
    const QVector<SceneVertex> &wall = geometry.at(0).vertices;

    ASSERT_EQ(wall.size() % 4, 0);

    for (int quad = 0; quad < wall.size(); quad += 4)
    {
      EXPECT_NEAR(double(wall[quad + 3].z - wall[quad].z), 25.0, 1.0e-3);
      EXPECT_NEAR(double(wall[quad + 2].z - wall[quad + 1].z), 25.0, 1.0e-3);
      EXPECT_FLOAT_EQ(wall[quad + 3].x, wall[quad].x);
      EXPECT_FLOAT_EQ(wall[quad + 3].y, wall[quad].y);
      EXPECT_NEAR(double(wall[quad].z), plane(wall[quad].x, wall[quad].y),
                  1.0e-3)
        << "the curtain stands on the ground, not beside it";
    }
  }

  TEST_F(DrapeTest, ACurtainFacesAcrossItsOwnLine)
  {
    Network network(QStringLiteral("conduits"));
        network.setExtrusionHeight(10.0);

    // Oblique and long, so that neither swapping the two axes nor forgetting
    // to divide by the length leaves the normal correct by accident.
    network.addLine({ { 0.0, 0.0 }, { 70.0, 30.0 } });

    const QVector<SceneGeometry> geometry = network.sceneGeometry({});

    ASSERT_FALSE(geometry.isEmpty());

    const SceneVertex &vertex = geometry.first().vertices.first();
    const QVector3D normal(vertex.nx, vertex.ny, vertex.nz);

    EXPECT_NEAR(double(normal.length()), 1.0, 1.0e-5);
    EXPECT_FLOAT_EQ(normal.z(), 0.0f);
    EXPECT_NEAR(double(QVector3D::dotProduct(
                  normal, QVector3D(70.0f, 30.0f, 0.0f).normalized())),
                0.0, 1.0e-5)
      << "a curtain that does not face across its own line is lit as if it "
         "were somewhere else";
  }

  TEST_F(DrapeTest, ACurtainOfNoHeightIsJustTheLine)
  {
    Network network(QStringLiteral("conduits"));
        network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    const QVector<SceneGeometry> geometry = network.sceneGeometry({});

    ASSERT_EQ(geometry.size(), 1);
    EXPECT_EQ(geometry.first().primitive, ScenePrimitive::Lines)
      << "there is no default height worth inventing, so zero degrades to "
         "the crest rather than to a wall of no size";
  }

  TEST_F(DrapeTest, ExtrusionShowsInTheBounds)
  {
    Network network(QStringLiteral("conduits"));
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    EXPECT_FLOAT_EQ(network.sceneBounds().maximum().z(), 0.0f);

        network.setExtrusionHeight(25.0);

    EXPECT_FLOAT_EQ(network.sceneBounds().maximum().z(), 25.0f);

    network.setExtrusionHeight(-25.0);

    EXPECT_FLOAT_EQ(network.sceneBounds().minimum().z(), -25.0f)
      << "a negative height hangs below the ground rather than being ignored";
  }

  // ── the stack decides what the terrain is ───────────────────────────────

  TEST_F(DrapeTest, TheUppermostTerrainWins)
  {
    LayerStackModel stack;

    SheetTerrain *low = new SheetTerrain(QStringLiteral("low"), 3.0);
    ASSERT_GE(stack.addLayer(low), 0);

    SheetTerrain *high = new SheetTerrain(QStringLiteral("high"), 17.0);
    ASSERT_GE(stack.addLayer(high), 0);

    Network *network = new Network(QStringLiteral("conduits"));
    network->addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });
    ASSERT_GE(stack.addLayer(network), 0);

    // The newest layer goes on top, so `high` is above `low`.
    ASSERT_EQ(stack.layerAt(1), high);
    ASSERT_EQ(stack.layerAt(2), low);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    Camera camera;
    camera.fitTo(renderer.sceneBounds(), 1.0);

    QString message;
    const QImage image = renderSceneToImage(renderer, camera, QSize(128, 128),
                                            QColor(0, 0, 0), message);

    ASSERT_FALSE(image.isNull()) << message.toStdString();

    EXPECT_GT(high->samples, 0)
      << "the uppermost terrain is the one the scene drapes on";
    EXPECT_EQ(low->samples, 0)
      << "a terrain buried under another must not be the one sampled";
  }

  TEST_F(DrapeTest, AHiddenTerrainIsNotDrapedOn)
  {
    LayerStackModel stack;

    SheetTerrain *terrain = new SheetTerrain(QStringLiteral("terrain"), 17.0);
    ASSERT_GE(stack.addLayer(terrain), 0);

    Network *network = new Network(QStringLiteral("conduits"));
    network->addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });
    ASSERT_GE(stack.addLayer(network), 0);

    terrain->setVisible(false);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    Camera camera;
    camera.fitTo(renderer.sceneBounds(), 1.0);

    QString message;
    const QImage image = renderSceneToImage(renderer, camera, QSize(128, 128),
                                            QColor(0, 0, 0), message);

    ASSERT_FALSE(image.isNull()) << message.toStdString();

    EXPECT_EQ(terrain->samples, 0)
      << "a hidden layer is not in the scene, so it is not a surface in it "
         "either — keeping its heights would drape on something invisible";
  }

  // ── on the device ───────────────────────────────────────────────────────

  TEST_F(DrapeTest, ADrapedLineIsVisibleOverTheSurfaceItLiesOn)
  {
    LayerStackModel stack;

    MeshLayer *terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, nullptr)).release();

    Symbol surface = terrain->style()->symbol();
    surface.fill = QColor(40, 40, 40);
    surface.stroke = QColor(40, 40, 40);
    terrain->style()->setSymbol(surface);
    terrain->setOpacity(1.0);
    ASSERT_GE(stack.addLayer(terrain), 0);

    Network *network = new Network(QStringLiteral("conduits"));
    Symbol line = network->style()->symbol();
    line.fill = QColor(255, 0, 0);
    line.stroke = QColor(255, 0, 0);
    network->style()->setSymbol(line);
    network->addLine({ { 5.0, 50.0 }, { 95.0, 50.0 } });
    ASSERT_GE(stack.addLayer(network), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, 1.0);
    camera.setElevation(90.0);
    camera.fitTo(renderer.sceneBounds(), 1.0);

    QString message;
    const QImage image = renderSceneToImage(renderer, camera, QSize(512, 512),
                                            QColor(0, 0, 0), message);

    ASSERT_FALSE(image.isNull()) << message.toStdString();
    writeFixture(image, QStringLiteral("coplanar"));

    // Straight down is the harsh case and the one the map hands over to: the
    // line lies exactly on the surface, so the two differ in depth only by
    // how each interpolator rounded. Measured, without the shader's nudge the
    // line is drawn at 89 degrees and completely gone at 90.
    EXPECT_GT(countRed(image), 200)
      << "the draped line was swallowed by the surface it lies on";

    // And it is a line, not what survived of one. A depth conflict that is
    // only half resolved stipples, which a total count would still pass.
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);

    int first = rgb.width();
    int last = -1;
    int columns = 0;

    for (int x = 0; x < rgb.width(); ++x)
    {
      bool found = false;

      for (int y = 0; y < rgb.height() && !found; ++y)
      {
        const QRgb pixel = rgb.pixel(x, y);
        found = qRed(pixel) >= 60 && qGreen(pixel) * 3 <= qRed(pixel) &&
                qBlue(pixel) * 3 <= qRed(pixel);
      }

      if (found)
      {
        first = std::min(first, x);
        last = x;
        ++columns;
      }
    }

    ASSERT_GT(last, first);
    EXPECT_EQ(columns, last - first + 1)
      << "the line is broken: " << columns << " of " << (last - first + 1)
      << " columns it spans carry it";
  }

  TEST_F(DrapeTest, ALineBehindARidgeStaysHidden)
  {
    // The counterpart of the case above, and what bounds the nudge: a line
    // that is genuinely behind terrain must still lose. A nudge large enough
    // to settle coplanar ties and small enough to leave this alone is the
    // only one that is right.
    LayerStackModel stack;

    MeshDefinition ridge;
    ridge.meshName = "ridge";
    ridge.nodeX = { 0.0, 100.0, 0.0, 100.0, 0.0, 100.0 };
    ridge.nodeY = { 0.0, 0.0, 50.0, 50.0, 100.0, 100.0 };
    ridge.nodeZ = { 0.0, 0.0, 60.0, 60.0, 0.0, 0.0 };
    ridge.faceNodeOffsets = { 0, 4, 8 };
    ridge.faceNodes = { 0, 1, 3, 2, 2, 3, 5, 4 };

    MeshLayer *terrain = terrainLayer(QStringLiteral("ridge"), ridge).release();

    Symbol surface = terrain->style()->symbol();
    surface.fill = QColor(40, 40, 40);
    surface.stroke = QColor(40, 40, 40);
    terrain->style()->setSymbol(surface);
    ASSERT_GE(stack.addLayer(terrain), 0);

    // Along the far edge, on the ground, with a 60-unit ridge between it and
    // the camera.
    Network *network = new Network(QStringLiteral("conduits"));
    Symbol line = network->style()->symbol();
    line.fill = QColor(255, 0, 0);
    line.stroke = QColor(255, 0, 0);
    network->style()->setSymbol(line);
    network->addLine({ { 5.0, 97.0 }, { 95.0, 97.0 } });
    ASSERT_GE(stack.addLayer(network), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    Camera camera;
    camera.setElevation(6.0);
    camera.setAzimuth(0.0);
    camera.fitTo(renderer.sceneBounds(), 1.0);

    QString message;
    const QImage image = renderSceneToImage(renderer, camera, QSize(512, 512),
                                            QColor(0, 0, 0), message);

    ASSERT_FALSE(image.isNull()) << message.toStdString();
    writeFixture(image, QStringLiteral("occluded"));

    EXPECT_LT(countRed(image), 20)
      << "a line behind a 60-unit ridge is showing through it";

    // The control, without which the case above passes for a line that was
    // simply off screen: seen from the other side, the same line is in front
    // of the same ridge and must be drawn.
    camera.setAzimuth(180.0);

    const QImage reverse = renderSceneToImage(renderer, camera, QSize(512, 512),
                                              QColor(0, 0, 0), message);

    ASSERT_FALSE(reverse.isNull()) << message.toStdString();
    writeFixture(reverse, QStringLiteral("occluded-reverse"));

    EXPECT_GT(countRed(reverse), 100)
      << "the line is not visible from either side, so hiding it behind the "
         "ridge proved nothing";
  }

}

  // ── placement (coherence plan Z) ────────────────────────────────────────
  //
  // SceneDrape conflated placement with extrusion -- "Extruded" was a
  // placement that smuggled a height in -- so a network could not be lifted
  // a metre above the terrain, or laid at a datum other than zero. ZPolicy
  // splits them: Constant / FromAttribute / OnTerrain place the base, and
  // extrusion raises a curtain from wherever that put it.

  TEST_F(DrapeTest, AConstantPlacesEveryVertexAtThatElevationTerrainOrNot)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("conduits"));
    network.setZPolicy({ZMode::Constant, 42.0, {}, 0.0});
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry(contextFor(terrain.get())));

    ASSERT_FALSE(vertices.isEmpty());

    for (const SceneVertex &vertex : vertices)
    {
      EXPECT_NEAR(double(vertex.z), 42.0, 1.0e-6)
        << "a constant datum was bent by the terrain beneath it";
    }
  }

  TEST_F(DrapeTest, AnAttributePlacesEachFeatureAtItsOwnValue)
  {
    Network network(QStringLiteral("conduits"));
    network.declareInvert();
    network.addLine({ { 10.0, 10.0 }, { 40.0, 40.0 } },
                    QStringLiteral("shallow"), 10.0);
    network.addLine({ { 60.0, 60.0 }, { 90.0, 90.0 } },
                    QStringLiteral("deep"), 20.0);
    network.setZPolicy(
      {ZMode::FromAttribute, 0.0, QStringLiteral("invert"), 0.0});

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry({}));

    ASSERT_FALSE(vertices.isEmpty());

    int atTen = 0;
    int atTwenty = 0;

    for (const SceneVertex &vertex : vertices)
    {
      atTen += std::abs(double(vertex.z) - 10.0) < 1.0e-6 ? 1 : 0;
      atTwenty += std::abs(double(vertex.z) - 20.0) < 1.0e-6 ? 1 : 0;
    }

    EXPECT_GT(atTen, 0);
    EXPECT_GT(atTwenty, 0);
    EXPECT_EQ(atTen + atTwenty, vertices.size())
      << "a feature sits somewhere neither of the two inverts names";
  }

  TEST_F(DrapeTest, TheOffsetLiftsADrapeUniformly)
  {
    const std::unique_ptr<MeshLayer> terrain =
      terrainLayer(QStringLiteral("terrain"), grid(8, plane));

    Network network(QStringLiteral("conduits"));
    network.setZPolicy({ZMode::OnTerrain, 0.0, {}, 5.0});
    network.addLine({ { 5.0, 5.0 }, { 95.0, 40.0 }, { 60.0, 95.0 } });

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry(contextFor(terrain.get())));

    ASSERT_FALSE(vertices.isEmpty());

    for (const SceneVertex &vertex : vertices)
    {
      EXPECT_NEAR(double(vertex.z), plane(vertex.x, vertex.y) + 5.0, 1.0e-3)
        << "at (" << vertex.x << ", " << vertex.y << ")";
    }
  }

  TEST_F(DrapeTest, AMissingFieldSitsAtTheOffsetAloneNotNowhere)
  {
    Network network(QStringLiteral("conduits"));
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });
    network.setZPolicy(
      {ZMode::FromAttribute, 0.0, QStringLiteral("nosuch"), 7.0});

    const QVector<SceneVertex> vertices =
      allVertices(network.sceneGeometry({}));

    ASSERT_FALSE(vertices.isEmpty())
      << "data with a broken row vanished instead of degrading";

    for (const SceneVertex &vertex : vertices)
    {
      EXPECT_NEAR(double(vertex.z), 7.0, 1.0e-6);
    }
  }

  // The one thing the old model could not say at all: a curtain rising from
  // a constant datum, no terrain involved.
  TEST_F(DrapeTest, ExtrusionRisesFromWhereverThePlacementPutTheBase)
  {
    Network network(QStringLiteral("conduits"));
    network.setZPolicy({ZMode::Constant, 10.0, {}, 0.0});
    network.setExtrusionHeight(25.0);
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    const QVector<SceneGeometry> geometry = network.sceneGeometry({});

    // A curtain batch exists...
    ASSERT_EQ(geometry.size(), 2)
      << "no curtain: extrusion still requires a terrain placement";

    float lowest = 1.0e9f;
    float highest = -1.0e9f;

    for (const SceneVertex &vertex : allVertices(geometry))
    {
      lowest = std::min(lowest, vertex.z);
      highest = std::max(highest, vertex.z);
    }

    // ...from the datum to the datum plus the height.
    EXPECT_NEAR(double(lowest), 10.0, 1.0e-6);
    EXPECT_NEAR(double(highest), 35.0, 1.0e-6);
  }

  // Setting the policy a layer already has must not announce a change, or
  // opening and closing the properties dialog repaints the world.
  TEST_F(DrapeTest, SettingTheSamePolicyAnnouncesNothing)
  {
    Network network(QStringLiteral("conduits"));
    network.addLine({ { 10.0, 10.0 }, { 90.0, 90.0 } });

    const ZPolicy policy{ZMode::Constant, 12.0, {}, 0.0};
    network.setZPolicy(policy);

    QSignalSpy announced(&network, &MapLayer::appearanceChanged);

    network.setZPolicy(policy);
    EXPECT_EQ(announced.count(), 0);

    ZPolicy moved = policy;
    moved.constant = 15.0;
    network.setZPolicy(moved);
    EXPECT_EQ(announced.count(), 1);
  }
