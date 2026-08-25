/*!
 * \file   test_groundplane.cpp
 * \brief  C3c-2 — image layers drawn as the ground they are a picture of.
 *
 * The property under test is stated in the plan as "a raster's pixels land
 * where its geo-transform says, on a tilted terrain as well as on a flat
 * one". Both halves matter and fail differently: the mapping can be wrong
 * (flipped, offset, scaled), and it can be *made* wrong by the drape, if the
 * texture were coordinated by anything the terrain moves.
 *
 * The fixture is asymmetric in both axes on purpose. A raster that is a
 * west-to-east ramp cannot tell a flipped V from a correct one, and half the
 * ways this can break are flips.
 */

#include "layers/gdalrasterlayer.h"
#include "layers/meshlayer.h"
#include "layers/tilelayer.h"
#include "map/layerstackmodel.h"
#include "map/maptransform.h"
#include "render/colorramp.h"
#include "scene/camera.h"
#include "scene/groundplane.h"
#include "scene/sceneimage.h"
#include "scene/scenerenderer.h"

#include <gtest/gtest.h>

#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include <QApplication>
#include <QDir>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  constexpr int kRasterSide = 64;
  constexpr double kNoData = -9999.0;

  //! The raster's world extent: (-8, -4) to (8, 4), as its geo-transform says.
  QRectF rasterExtent()
  {
    return QRectF(-8.0, -4.0, 16.0, 8.0);
  }

  QString fixture(const QString &name)
  {
    return QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR)).filePath(name);
  }

  /*!
   * \brief Writes a raster with two stripes of different brightness.
   *
   * A bright stripe down the west and a mid one across the north, plus a
   * block of genuine no-data in the south-east.
   *
   * Stripes rather than a bright quadrant, and two of them rather than one,
   * because a quadrant is *invariant* under transposing the texture axes —
   * the north-west stays the north-west when u and v swap, so a fixture made
   * of quadrants cannot see one of the commonest ways this breaks. Two
   * stripes of unequal value swap with each other instead, which is visible.
   */
  void writeGroundFixture()
  {
    const QString path = fixture(QStringLiteral("generated-ground.tif"));

    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    ASSERT_NE(driver, nullptr) << "GDAL has no GTiff driver";

    GDALDataset *dataset =
      driver->Create(path.toUtf8().constData(), kRasterSide, kRasterSide, 1,
                     GDT_Float32, nullptr);
    ASSERT_NE(dataset, nullptr);

    // North-up, so the Y pixel size is negative as every georeferenced
    // raster's is.
    double geotransform[6] = { -8.0, 16.0 / kRasterSide, 0.0,
                               4.0,  0.0,                -8.0 / kRasterSide };
    dataset->SetGeoTransform(geotransform);

    OGRSpatialReference wgs84;
    wgs84.importFromEPSG(4326);
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    char *wkt = nullptr;
    wgs84.exportToWkt(&wkt);
    dataset->SetProjection(wkt);
    CPLFree(wkt);

    std::vector<float> values(size_t(kRasterSide) * kRasterSide, 50.0f);

    for (int row = 0; row < kRasterSide; ++row)
    {
      for (int column = 0; column < kRasterSide; ++column)
      {
        float &value = values[size_t(row) * kRasterSide + size_t(column)];

        // Row 0 is the northern edge, column 0 the western one.
        if (column < kRasterSide / 8)
        {
          value = 100.0f;
        }
        else if (row < kRasterSide / 8)
        {
          value = 75.0f;
        }
        else if (row >= kRasterSide * 3 / 4 && column >= kRasterSide * 3 / 4)
        {
          value = float(kNoData);
        }
      }
    }

    dataset->GetRasterBand(1)->SetNoDataValue(kNoData);
    ASSERT_EQ(dataset->GetRasterBand(1)->RasterIO(
                GF_Write, 0, 0, kRasterSide, kRasterSide, values.data(),
                kRasterSide, kRasterSide, GDT_Float32, 0, 0),
              CE_None);

    GDALClose(dataset);
  }

  std::unique_ptr<GdalRasterLayer> openGroundRaster()
  {
    QString message;
    std::unique_ptr<GdalRasterLayer> layer = GdalRasterLayer::open(
      fixture(QStringLiteral("generated-ground.tif")), message);

    EXPECT_TRUE(layer) << message.toStdString();

    if (layer)
    {
      // Black at the low end, white at the high: the bright quadrant is then
      // the only near-white thing in the picture.
      layer->setRamp(ColorRamp({ { 0.0, QColor(0, 0, 0) },
                                { 1.0, QColor(255, 255, 255) } }));
    }

    return layer;
  }

  //! A flat sheet over the raster's extent, at zero.
  MeshDefinition sheet(double elevation)
  {
    const QRectF box = rasterExtent();

    MeshDefinition mesh;
    mesh.meshName = "sheet";
    mesh.nodeX = { box.left(), box.right(), box.right(), box.left() };
    mesh.nodeY = { box.top(), box.top(), box.bottom(), box.bottom() };
    mesh.nodeZ = { elevation, elevation, elevation, elevation };
    mesh.faceNodeOffsets = { 0, 4 };
    mesh.faceNodes = { 0, 1, 2, 3 };

    return mesh;
  }

  /*!
   * \brief A grid over the raster's extent, tilted west-to-east.
   *
   * Fine enough that the drape has to subdivide, and steep enough that a
   * texture coordinated by anything the terrain moves would visibly slide.
   */
  MeshDefinition tilted(int divisions, double rise)
  {
    const QRectF box = rasterExtent();

    MeshDefinition mesh;
    mesh.meshName = "tilted";

    for (int row = 0; row <= divisions; ++row)
    {
      for (int column = 0; column <= divisions; ++column)
      {
        const double u = double(column) / double(divisions);
        const double v = double(row) / double(divisions);

        mesh.nodeX.push_back(box.left() + box.width() * u);
        mesh.nodeY.push_back(box.top() + box.height() * v);
        mesh.nodeZ.push_back(rise * u);
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
        mesh.faceNodeOffsets.push_back(int64_t(mesh.faceNodes.size()));
      }
    }

    return mesh;
  }

  std::unique_ptr<MeshLayer> terrainLayer(const MeshDefinition &mesh)
  {
    QString message;
    std::unique_ptr<MeshLayer> layer = MeshLayer::create(
      QStringLiteral("terrain"), mesh, MeshEntity::Face, message);

    EXPECT_TRUE(layer) << message.toStdString();

    return layer;
  }

  //! A tile source that hands back one solid colour for every tile.
  class SolidTiles : public ITileSource
  {
    public:
      explicit SolidTiles(const QColor &color) : m_color(color) {}

      [[nodiscard]] QImage tile(const TileId &) override
      {
        QImage image(256, 256, QImage::Format_ARGB32);
        image.fill(m_color);

        return image;
      }

      void request(const TileId &) override {}

      void cancelPending() override {}

      [[nodiscard]] QString attribution() const override
      {
        return QStringLiteral("test");
      }

      [[nodiscard]] int maximumZoom() const override { return 19; }

    private:
      QColor m_color;
  };

  /*!
   * \brief Mean brightness over a normalised window of \a image.
   *
   * u runs west to east, v north to south — the image's own axes, and the
   * world's, since every picture here spans the raster's extent exactly.
   */
  double windowBrightness(const QImage &image, double u0, double u1, double v0,
                          double v1)
  {
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);

    const int x0 = int(u0 * rgb.width());
    const int x1 = std::max(x0 + 1, int(u1 * rgb.width()));
    const int y0 = int(v0 * rgb.height());
    const int y1 = std::max(y0 + 1, int(v1 * rgb.height()));

    double total = 0.0;
    int counted = 0;

    for (int y = y0; y < std::min(y1, rgb.height()); ++y)
    {
      for (int x = x0; x < std::min(x1, rgb.width()); ++x)
      {
        const QRgb pixel = rgb.pixel(x, y);
        total += (qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3.0;
        ++counted;
      }
    }

    return counted > 0 ? total / counted : 0.0;
  }

  //! The west stripe: bright, and mid-way down so the north stripe is clear.
  double westStripe(const QImage &image)
  {
    return windowBrightness(image, 0.02, 0.10, 0.35, 0.65);
  }

  //! The north stripe: mid, and mid-way across.
  double northStripe(const QImage &image)
  {
    return windowBrightness(image, 0.35, 0.65, 0.02, 0.10);
  }

  //! The middle: the raster's base value, which is the ramp's dark end.
  double middle(const QImage &image)
  {
    return windowBrightness(image, 0.40, 0.60, 0.40, 0.60);
  }

  //! Where the north stripe would land if V were flipped.
  double southStripe(const QImage &image)
  {
    return windowBrightness(image, 0.35, 0.65, 0.90, 0.98);
  }

  //! Where the west stripe would land if U were flipped.
  double eastStripe(const QImage &image)
  {
    return windowBrightness(image, 0.90, 0.98, 0.35, 0.65);
  }

  void writeFixture(const QImage &image, const QString &name)
  {
    image.save(fixture(QStringLiteral("generated-ground-%1.png").arg(name)));
  }

  //! Renders the stack from straight above, framed to the raster's extent.
  QImage renderFromAbove(SceneRenderer &renderer, const QSize &size)
  {
    const double aspect = double(size.width()) / double(size.height());

    Camera camera;
    camera.setProjection(CameraProjection::Orthographic, aspect);
    camera.setAzimuth(0.0);
    camera.setElevation(90.0);
    camera.setGroundExtent(rasterExtent(), aspect);

    // Framing the ground says nothing about how far back to stand, and the
    // near plane is derived from that distance — so relief taller than the
    // camera's default standoff is clipped away rather than drawn. Pushed
    // back explicitly here; the map-to-scene hand-off will have to do the
    // same, and a test that quietly framed only what happened to fit would
    // have hidden exactly the pixels it is checking.
    camera.setDistance(1000.0);

    QString message;
    const QImage image =
      renderSceneToImage(renderer, camera, size, QColor(0, 0, 255), message);

    EXPECT_FALSE(image.isNull()) << message.toStdString();

    return image;
  }

  class GroundPlaneTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!QApplication::instance())
        {
          static int argc = 1;
          static char name[] = "test_groundplane";
          static char *argv[] = { name, nullptr };
          s_app = new QApplication(argc, argv);
        }

        GDALAllRegister();
        writeGroundFixture();
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static QApplication *s_app;
  };

  QApplication *GroundPlaneTest::s_app = nullptr;

  // ── the texture ─────────────────────────────────────────────────────────

  TEST_F(GroundPlaneTest, TheImageCoversTheExtentItReports)
  {
    const std::unique_ptr<GdalRasterLayer> layer = openGroundRaster();
    ASSERT_TRUE(layer);

    const GroundImage ground =
      renderLayerToImage(*layer, rasterExtent(), 256);

    ASSERT_TRUE(ground.isValid());

    // Sized to the extent's own shape, so fitting adds no margin. A margin
    // would be texture covering world the layer was never asked about, and
    // it would be sampled as though it did.
    EXPECT_EQ(ground.image.width(), 256);
    EXPECT_EQ(ground.image.height(), 128);
    EXPECT_NEAR(ground.extent.width(), rasterExtent().width(), 1.0e-6);
    EXPECT_NEAR(ground.extent.height(), rasterExtent().height(), 1.0e-6);
    EXPECT_NEAR(ground.extent.center().x(), 0.0, 1.0e-6);
    EXPECT_NEAR(ground.extent.center().y(), 0.0, 1.0e-6);
  }

  TEST_F(GroundPlaneTest, TheImageIsBrightestWhereTheRasterIs)
  {
    const std::unique_ptr<GdalRasterLayer> layer = openGroundRaster();
    ASSERT_TRUE(layer);

    const GroundImage ground =
      renderLayerToImage(*layer, rasterExtent(), 256);

    ASSERT_TRUE(ground.isValid());
    writeFixture(ground.image, QStringLiteral("texture"));

    // The image's first row is the world's northern edge and its first
    // column the western one, so the bright stripe runs down the left of the
    // picture and the mid one across its top.
    EXPECT_GT(westStripe(ground.image), 200.0);
    EXPECT_NEAR(northStripe(ground.image), 128.0, 30.0);
    EXPECT_LT(middle(ground.image), 40.0);
    EXPECT_LT(southStripe(ground.image), 40.0) << "V is flipped";
    EXPECT_LT(eastStripe(ground.image), 40.0) << "U is flipped";
  }

  TEST_F(GroundPlaneTest, TheImageReportsWhatItShowsNotWhatWasAsked)
  {
    const std::unique_ptr<GdalRasterLayer> layer = openGroundRaster();
    ASSERT_TRUE(layer);

    // An extent whose shape does not survive being rounded to whole pixels:
    // 16 by 5 at 100 pixels wants 31.25 rows, gets 31, and the transform then
    // shows 16.13 of width rather than 16. Mapping the texture by the
    // rectangle that was *asked* for would be off by that everywhere — a
    // fraction of a pixel, which is invisible until two ground planes meet
    // and the seam between them does not line up.
    const QRectF asked(-8.0, -2.5, 16.0, 5.0);
    const GroundImage ground = renderLayerToImage(*layer, asked, 100);

    ASSERT_TRUE(ground.isValid());
    EXPECT_EQ(ground.image.size(), QSize(100, 31));

    const MapTransform transform(asked, QSizeF(ground.image.size()));

    EXPECT_NEAR(ground.extent.width(), transform.visibleExtent().width(),
                1.0e-9);
    EXPECT_NEAR(ground.extent.height(), transform.visibleExtent().height(),
                1.0e-9);
    EXPECT_GT(ground.extent.width(), asked.width())
      << "fitting preserves aspect, so it shows more than it was given";
  }

  // ── the surface ─────────────────────────────────────────────────────────

  TEST_F(GroundPlaneTest, WithoutTerrainItIsOneQuad)
  {
    GroundImage ground;
    ground.image = QImage(4, 4, QImage::Format_ARGB32);
    ground.image.fill(Qt::white);
    ground.extent = rasterExtent();

    const SceneGeometry geometry = buildGroundPlane(ground, nullptr);

    EXPECT_EQ(geometry.vertices.size(), 4)
      << "flat ground needs no vertices to be flat with";
    EXPECT_EQ(geometry.indices.size(), 6);
    EXPECT_EQ(geometry.primitive, ScenePrimitive::Triangles);
    EXPECT_FALSE(geometry.texture.isNull());
    EXPECT_EQ(geometry.textureExtent, rasterExtent().normalized());

    for (const SceneVertex &vertex : geometry.vertices)
    {
      EXPECT_FLOAT_EQ(vertex.z, 0.0f);
    }
  }

  TEST_F(GroundPlaneTest, OnTerrainItFollowsTheSurface)
  {
    const std::unique_ptr<MeshLayer> terrain = terrainLayer(tilted(8, 20.0));
    ASSERT_TRUE(terrain);

    GroundImage ground;
    ground.image = QImage(4, 4, QImage::Format_ARGB32);
    ground.image.fill(Qt::white);
    ground.extent = rasterExtent();

    const SceneGeometry geometry =
      buildGroundPlane(ground, terrain->sceneSource()->terrain());

    ASSERT_GT(geometry.vertices.size(), 4)
      << "a ground plane over relief has to be subdivided to follow it";

    for (const SceneVertex &vertex : geometry.vertices)
    {
      double surface = 0.0;

      ASSERT_TRUE(terrain->elevationAt(QPointF(vertex.x, vertex.y), surface))
        << "at (" << vertex.x << ", " << vertex.y << ")";
      EXPECT_NEAR(double(vertex.z), surface, 1.0e-3);
    }
  }

  TEST_F(GroundPlaneTest, ItsNormalsFollowTheRelief)
  {
    // A plane rising 20 over the extent's 16 of width: the surface normal is
    // perpendicular to that slope, and a straight-up normal would leave a
    // hillside lit exactly like the valley floor beside it.
    const std::unique_ptr<MeshLayer> terrain = terrainLayer(tilted(8, 20.0));
    ASSERT_TRUE(terrain);

    GroundImage ground;
    ground.image = QImage(4, 4, QImage::Format_ARGB32);
    ground.image.fill(Qt::white);
    ground.extent = rasterExtent();

    const SceneGeometry geometry =
      buildGroundPlane(ground, terrain->sceneSource()->terrain());

    const QVector3D slope(float(rasterExtent().width()), 0.0f, 20.0f);

    // The interior, where a central difference has neighbours on both sides.
    const SceneVertex &vertex =
      geometry.vertices.at(geometry.vertices.size() / 2);
    const QVector3D normal(vertex.nx, vertex.ny, vertex.nz);

    EXPECT_NEAR(double(QVector3D::dotProduct(normal.normalized(),
                                             slope.normalized())),
                0.0, 1.0e-5)
      << "the ground plane's normal does not stand on the ground";
  }

  TEST_F(GroundPlaneTest, AnEmptyImageMakesNoGeometry)
  {
    EXPECT_TRUE(buildGroundPlane({}, nullptr).isEmpty());
  }

  // ── on the device ───────────────────────────────────────────────────────

  TEST_F(GroundPlaneTest, TheRastersPixelsLandWhereItsGeoTransformSays)
  {
    LayerStackModel stack;

    GdalRasterLayer *layer = openGroundRaster().release();
    ASSERT_TRUE(layer);
    ASSERT_GE(stack.addLayer(layer), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    const QImage image = renderFromAbove(renderer, QSize(512, 256));
    ASSERT_FALSE(image.isNull());
    writeFixture(image, QStringLiteral("flat"));

    // The background is blue, so ground the picture never reached is
    // obvious rather than merely dark.
    EXPECT_GT(westStripe(image), 200.0)
      << "the bright stripe is not down the west";
    EXPECT_NEAR(northStripe(image), 128.0, 30.0)
      << "the mid stripe is not across the north";
    EXPECT_LT(middle(image), 40.0);
    EXPECT_LT(southStripe(image), 40.0) << "V is flipped";
    EXPECT_LT(eastStripe(image), 40.0) << "U is flipped";
  }

  TEST_F(GroundPlaneTest, TiltingTheTerrainDoesNotSlideTheTexture)
  {
    // The clause in the plan that the flat case cannot check. Seen from
    // straight above, a drape changes only z, so the picture must be the
    // same one — to within the shading the relief now casts. A texture
    // coordinated by vertex index, by screen position, or by anything else
    // the terrain moves would slide, and slide by a quarter of the frame.
    LayerStackModel stack;

    MeshLayer *terrain = terrainLayer(tilted(8, 20.0)).release();
    ASSERT_TRUE(terrain);
    ASSERT_GE(stack.addLayer(terrain), 0);

    GdalRasterLayer *layer = openGroundRaster().release();
    ASSERT_TRUE(layer);
    ASSERT_GE(stack.addLayer(layer), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    const QImage image = renderFromAbove(renderer, QSize(512, 256));
    ASSERT_FALSE(image.isNull());
    writeFixture(image, QStringLiteral("tilted"));

    const double west = westStripe(image);

    EXPECT_GT(west, 140.0)
      << "the bright stripe moved when the ground under it tilted";
    EXPECT_GT(northStripe(image), 60.0)
      << "the mid stripe moved when the ground under it tilted";
    EXPECT_LT(middle(image), 40.0);
    EXPECT_LT(southStripe(image), 40.0);
    EXPECT_LT(eastStripe(image), 40.0);

    // And it is *shaded*, which is the other half of laying an image on
    // relief: the same stripe comes back at a full 255 over flat ground, and
    // a ground plane whose normals were left pointing up would too.
    EXPECT_LT(west, 240.0)
      << "the ground plane is lit as though it were flat";
  }

  TEST_F(GroundPlaneTest, NoDataStaysTransparent)
  {
    LayerStackModel stack;

    GdalRasterLayer *layer = openGroundRaster().release();
    ASSERT_TRUE(layer);
    ASSERT_GE(stack.addLayer(layer), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    const QImage image = renderFromAbove(renderer, QSize(512, 256));
    ASSERT_FALSE(image.isNull());

    // The south-east block is genuine no-data, so the blue behind the scene
    // shows through it. Painting it black would invent ground.
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);
    const QRgb pixel =
      rgb.pixel(rgb.width() * 7 / 8, rgb.height() * 7 / 8);

    EXPECT_GT(qBlue(pixel), 200) << "no-data was painted rather than left";
    EXPECT_LT(qRed(pixel), 60);
  }

  TEST_F(GroundPlaneTest, ARasterShowsAllOfItselfNotJustTheFocus)
  {
    // A raster is a picture of a *place*, so it shows all of itself even when
    // the rest of the scene covers somewhere much larger. Showing only the
    // focus would crop it — and would crop it differently every time another
    // layer was added.
    LayerStackModel stack;

    MeshDefinition wide = sheet(0.0);

    for (size_t node = 0; node < wide.nodeX.size(); ++node)
    {
      wide.nodeX[node] *= 4.0;
      wide.nodeY[node] *= 4.0;
    }

    MeshLayer *terrain = terrainLayer(wide).release();
    ASSERT_TRUE(terrain);
    ASSERT_GE(stack.addLayer(terrain), 0);

    GdalRasterLayer *layer = openGroundRaster().release();
    ASSERT_TRUE(layer);
    ASSERT_GE(stack.addLayer(layer), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    SceneContext context;
    context.terrain = terrain->sceneSource()->terrain();
    context.focus = renderer.sceneBounds().footprint();

    ASSERT_NEAR(context.focus.width(), rasterExtent().width() * 4.0, 1.0e-6)
      << "the fixture must make the focus differ from the raster's extent";

    const QVector<SceneGeometry> geometry = layer->sceneGeometry(context);

    ASSERT_EQ(geometry.size(), 1);
    EXPECT_NEAR(geometry.first().textureExtent.width(),
                rasterExtent().width(), 1.0e-6);
    EXPECT_NEAR(geometry.first().textureExtent.height(),
                rasterExtent().height(), 1.0e-6);
  }

  // ── basemaps ────────────────────────────────────────────────────────────

  TEST_F(GroundPlaneTest, ABasemapFramesNothing)
  {
    LayerStackModel stack;

    TileLayer *tiles = new TileLayer(
      QStringLiteral("basemap"),
      std::make_unique<SolidTiles>(QColor(200, 120, 40)));
    ASSERT_GE(stack.addLayer(tiles), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    EXPECT_FALSE(renderer.sceneBounds().isValid())
      << "a basemap covers everywhere, so framing it frames the planet";
    EXPECT_TRUE(tiles->sceneGeometry({}).isEmpty())
      << "there is nothing for a backdrop to be behind";
  }

  TEST_F(GroundPlaneTest, ABasemapCoversWhatTheSceneIsAbout)
  {
    LayerStackModel stack;

    TileLayer *tiles = new TileLayer(
      QStringLiteral("basemap"),
      std::make_unique<SolidTiles>(QColor(200, 120, 40)));
    ASSERT_GE(stack.addLayer(tiles), 0);

    MeshLayer *terrain = terrainLayer(sheet(0.0)).release();
    ASSERT_TRUE(terrain);
    ASSERT_GE(stack.addLayer(terrain), 0);

    SceneRenderer renderer;
    renderer.setModel(&stack);

    const QRectF focus = renderer.sceneBounds().footprint();
    ASSERT_FALSE(focus.isEmpty());

    SceneContext context;
    context.focus = focus;

    const QVector<SceneGeometry> geometry = tiles->sceneGeometry(context);

    ASSERT_EQ(geometry.size(), 1);
    EXPECT_FALSE(geometry.first().texture.isNull());

    // The scene's own footprint, not the planet.
    EXPECT_NEAR(geometry.first().textureExtent.width(),
                rasterExtent().width(), 1.0e-3);
    EXPECT_NEAR(geometry.first().textureExtent.height(),
                rasterExtent().height(), 1.0e-3);
  }

}
