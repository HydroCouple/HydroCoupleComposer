/*!
 * \file   test_gdallayers.cpp
 * \brief  Phase C1d verification — vector and raster datasets through GDAL.
 *
 * Reads real files rather than mocking OGR: the questions worth asking here —
 * does a hole in a polygon survive, is an unset field distinguishable from
 * zero, does a raster land in the right place — are questions about what GDAL
 * actually hands back, and a mock would answer them the way the author
 * assumed rather than the way the library behaves.
 *
 * The raster fixture is generated into the source tree beside the vector ones
 * so a failure can be opened and looked at.
 */

#include "core/composerapplication.h"
#include "gis/spatialreference.h"
#include "layers/gdalrasterlayer.h"
#include "layers/featurelayer.h"
#include "layers/gdalvectorlayer.h"
#include "map/layerstackmodel.h"
#include "map/extentmath.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "render/layerstyle.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLineF>
#include <QPainter>

#include <gdal_priv.h>

#include <vector>

using namespace HydroCouple::Composer;

namespace
{
  class GdalLayerTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_gdallayers";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }

        GDALAllRegister();
        writeRasterFixture();
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static QString fixture(const QString &name)
      {
        return QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR)).filePath(name);
      }

      //! Fixture raster size. Deliberately larger than the viewports the
      //! tests render into, so a layer that read the whole band instead of
      //! the part on screen is detectable.
      static constexpr int kRasterWidth = 256;
      static constexpr int kRasterHeight = 128;

      //! Rows at the top of the fixture that hold no data.
      static constexpr int kNoDataRows = 32;

      static constexpr double kNoData = -9999.0;

      /*!
       * \brief Writes a georeferenced raster: a ramp from west to east, with
       *        a band of no-data across the top.
       *
       * Generated rather than committed, and written where the vector
       * fixtures live so it can be inspected with any GIS tool when a test
       * fails.
       */
      static void writeRasterFixture()
      {
        const QString path = fixture(QStringLiteral("generated-terrain.tif"));

        GDALDriver *driver =
          GetGDALDriverManager()->GetDriverByName("GTiff");
        ASSERT_NE(driver, nullptr) << "GDAL has no GTiff driver";

        GDALDataset *dataset =
          driver->Create(path.toUtf8().constData(), kRasterWidth,
                         kRasterHeight, 1, GDT_Float32, nullptr);
        ASSERT_NE(dataset, nullptr);

        // Spanning (-8, -4) to (8, 4) — north-up, so the Y pixel size is
        // negative as every georeferenced raster's is.
        double geotransform[6] = {-8.0, 16.0 / kRasterWidth, 0.0, 4.0, 0.0,
                                  -8.0 / kRasterHeight};
        dataset->SetGeoTransform(geotransform);

        OGRSpatialReference wgs84;
        wgs84.importFromEPSG(4326);
        wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        char *wkt = nullptr;
        wgs84.exportToWkt(&wkt);
        dataset->SetProjection(wkt);
        CPLFree(wkt);

        std::vector<float> values(static_cast<size_t>(kRasterWidth)
                                  * kRasterHeight);

        for (int y = 0; y < kRasterHeight; ++y)
        {
          for (int x = 0; x < kRasterWidth; ++x)
          {
            // A real band of no-data, not merely a declared value nothing
            // uses: a test whose fixture holds none cannot tell whether
            // no-data is honoured.
            values[static_cast<size_t>(y) * kRasterWidth + x] =
              y < kNoDataRows ? static_cast<float>(kNoData)
                              : static_cast<float>(x);
          }
        }

        dataset->GetRasterBand(1)->SetNoDataValue(kNoData);
        ASSERT_EQ(dataset->GetRasterBand(1)->RasterIO(
                    GF_Write, 0, 0, kRasterWidth, kRasterHeight, values.data(),
                    kRasterWidth, kRasterHeight, GDT_Float32, 0, 0),
                  CE_None);

        GDALClose(dataset);
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *GdalLayerTest::s_app = nullptr;
}

// ── Vector ──────────────────────────────────────────────────────────────────

TEST_F(GdalLayerTest, ReadsPointsWithTheirAttributesAndCrs)
{
  QString message;
  const std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("network.geojson")), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->featureCount(), 4);
  EXPECT_EQ(layer->geometryKind(), GeometryKind::Point);

  // Fields come from the dataset, not from a guess.
  const QVector<AttributeField> fields = layer->attributeFields();
  ASSERT_EQ(fields.size(), 3);

  EXPECT_EQ(layer->attributeValue(0, QStringLiteral("name")).toString(),
            QStringLiteral("J1"));
  EXPECT_NEAR(layer->attributeValue(2, QStringLiteral("depth")).toDouble(), 9.0,
              1e-9);

  ASSERT_NE(layer->crs(), nullptr) << "the dataset's CRS was not read";
  EXPECT_TRUE(layer->crs()->isGeographic());

  // The extent is the data's, in the data's own CRS.
  EXPECT_NEAR(layer->extent().left(), -1.0, 1e-9);
  EXPECT_NEAR(layer->extent().right(), 1.0, 1e-9);
}

TEST_F(GdalLayerTest, KeepsAnUnsetFieldDistinctFromZero)
{
  QString message;
  const std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("network.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Feature N1 has no depth. Read as 0.0 it would join the lowest class and
  // look like the shallowest node in the network.
  const QVariant missing = layer->attributeValue(3, QStringLiteral("depth"));
  EXPECT_FALSE(missing.isValid());

  const QVector<double> values = layer->numericValues(QStringLiteral("depth"));
  EXPECT_EQ(values.size(), 3);
}

TEST_F(GdalLayerTest, ReadsLinesAndPolygons)
{
  QString message;

  const std::unique_ptr<GdalVectorLayer> lines = GdalVectorLayer::open(
    fixture(QStringLiteral("conduits.geojson")), message);
  ASSERT_NE(lines, nullptr) << message.toStdString();

  EXPECT_EQ(lines->geometryKind(), GeometryKind::Line);
  EXPECT_EQ(lines->featureCount(), 2);
  EXPECT_EQ(lines->features().first().parts.first().size(), 2);

  const std::unique_ptr<GdalVectorLayer> polygons = GdalVectorLayer::open(
    fixture(QStringLiteral("catchments.geojson")), message);
  ASSERT_NE(polygons, nullptr) << message.toStdString();

  EXPECT_EQ(polygons->geometryKind(), GeometryKind::Polygon);
  EXPECT_EQ(polygons->featureCount(), 2);

  // A polygon read as a closed ring, not as an open line.
  const QPolygonF ring = polygons->features().first().parts.first();
  ASSERT_GE(ring.size(), 4);
  EXPECT_EQ(ring.first(), ring.last());
}

TEST_F(GdalLayerTest, ListsTheSublayersADatasetHolds)
{
  const QStringList names = GdalVectorLayer::sublayerNames(
    fixture(QStringLiteral("network.geojson")));

  ASSERT_FALSE(names.isEmpty());
  EXPECT_EQ(names.first(), QStringLiteral("network"));
}

TEST_F(GdalLayerTest, ReportsWhatItCannotOpenRatherThanReturningNothing)
{
  QString message;

  EXPECT_EQ(GdalVectorLayer::open(fixture(QStringLiteral("no-such-file.shp")),
                                  message),
            nullptr);
  EXPECT_FALSE(message.isEmpty()) << "a failure with no explanation";

  message.clear();
  EXPECT_EQ(
    GdalRasterLayer::open(fixture(QStringLiteral("no-such-raster.tif")),
                          message),
    nullptr);
  EXPECT_FALSE(message.isEmpty());
}

TEST_F(GdalLayerTest, StylesItselfFromItsOwnAttributes)
{
  QString message;
  const std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("network.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_NE(layer->style(), nullptr);

  layer->style()->setMode(StyleMode::Categorized);
  layer->style()->setAttribute(QStringLiteral("kind"));

  ASSERT_TRUE(layer->restyle());

  // Two kinds in the fixture, so two legend rows and two colours.
  ASSERT_EQ(layer->style()->legendItems().size(), 2);
  EXPECT_NE(layer->style()->colorFor(*layer, 0),
            layer->style()->colorFor(*layer, 2));
}

TEST_F(GdalLayerTest, DrawsItsFeaturesWhereTheMapPutsThem)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("catchments.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  Symbol symbol = layer->style()->symbol();
  symbol.fill = QColor(Qt::red);
  symbol.fill.setAlpha(255);
  layer->style()->setSymbol(symbol);

  MapTransform transform;
  transform.fit(QRectF(-2.0, -2.0, 4.0, 4.0), QSizeF(400.0, 400.0), 0.0);

  QImage canvas(400, 400, QImage::Format_ARGB32);
  canvas.fill(Qt::white);

  QPainter painter(&canvas);
  layer->render(painter, transform);
  painter.end();

  // S1 covers the south-west quadrant of the extent, which is the bottom
  // left of the screen; the north-west quadrant holds nothing.
  EXPECT_EQ(canvas.pixelColor(100, 300), QColor(Qt::red));
  EXPECT_EQ(canvas.pixelColor(100, 100), QColor(Qt::white))
    << "geometry was drawn in the wrong quadrant — check the Y flip";
}

TEST_F(GdalLayerTest, KeepsPolygonsTranslucentWhenTheyAreClassified)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("catchments.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  ASSERT_LT(layer->style()->symbol().fill.alpha(), 255)
    << "polygon layers should open translucent";

  layer->style()->setMode(StyleMode::Graduated);
  layer->style()->setAttribute(QStringLiteral("area"));
  layer->style()->classification().setClassCount(2);
  ASSERT_TRUE(layer->restyle());

  MapTransform transform;
  transform.fit(QRectF(-2.0, -2.0, 4.0, 4.0), QSizeF(400.0, 400.0), 0.0);

  // Drawn twice over different backgrounds. A translucent fill blends with
  // whatever is beneath and lands on two different colours; an opaque one
  // gives the same colour both times, whatever its brightness happens to be
  // — which is why this does not test the pixel's lightness.
  const auto drawOver = [&](const QColor &background)
  {
    QImage canvas(400, 400, QImage::Format_ARGB32);
    canvas.fill(background);

    QPainter painter(&canvas);
    layer->render(painter, transform);
    painter.end();

    return canvas.pixelColor(100, 300);
  };

  const QColor overWhite = drawOver(Qt::white);
  const QColor overBlack = drawOver(Qt::black);

  EXPECT_NE(overWhite, QColor(Qt::white)) << "the polygon was not drawn";
  EXPECT_NE(overWhite, overBlack)
    << "the classified polygon is opaque — nothing beneath it can show";
}

TEST_F(GdalLayerTest, ReprojectsItsGeometryIntoTheMapsCrs)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("network.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  GdalVectorLayer *raw = layer.get();

  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  canvas.setCrs(SpatialReference::webMercator());
  canvas.resize(400, 400);

  stack.addLayer(layer.release());

  // The layer's own extent stays in degrees; the map's view of it does not.
  EXPECT_NEAR(raw->extent().right(), 1.0, 1e-9);

  const QRectF inMapCrs = canvas.fullExtent();
  EXPECT_NEAR(inMapCrs.right(), 111319.49, 1.0)
    << "the layer's extent was not reprojected into the map's CRS";

  // And the geometry itself follows. Node O1 sits at longitude and latitude
  // 1, which in Web Mercator is 111 km from the origin — a third of the way
  // to the edge of this view. Unreprojected it would be one metre from the
  // origin, which at this scale is the same pixel, so asserting only that
  // *something* is drawn at the origin would pass either way.
  canvas.zoomToFullExtent();

  QImage image(400, 400, QImage::Format_ARGB32);
  image.fill(Qt::white);
  canvas.render(&image);

  const auto drawnNear = [&image](const QPointF &at)
  {
    for (int dy = -6; dy <= 6; ++dy)
    {
      for (int dx = -6; dx <= 6; ++dx)
      {
        const QPoint probe(qRound(at.x()) + dx, qRound(at.y()) + dy);

        if (image.rect().contains(probe)
            && image.pixelColor(probe) != QColor(Qt::white))
        {
          return true;
        }
      }
    }

    return false;
  };

  const QPointF outfall = canvas.transform().toScreen(
    QPointF(111319.49, 111325.14));

  EXPECT_TRUE(drawnNear(outfall))
    << "the feature at 1°E 1°N was not drawn where Web Mercator puts it";

  // Its neighbour at the CRS origin is drawn too, and they are far apart —
  // unreprojected, all four features would pile onto one pixel.
  const QPointF origin = canvas.transform().toScreen(QPointF(0.0, 0.0));

  EXPECT_TRUE(drawnNear(origin));
  EXPECT_GT(QLineF(origin, outfall).length(), 50.0)
    << "the features were not spread out — the degrees were never converted";
}

// A CRS chosen after the data is loaded must move the data that is already
// there; only a change made before anything loaded would be picked up by the
// layer-arrival path alone.
TEST_F(GdalLayerTest, ChangingTheMapCrsMovesLayersAlreadyLoaded)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("network.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  canvas.resize(400, 400);

  // Loaded while the map has no CRS at all, so the extent is still degrees.
  stack.addLayer(layer.release());
  EXPECT_NEAR(canvas.fullExtent().right(), 1.0, 1e-9);

  canvas.setCrs(SpatialReference::webMercator());
  canvas.zoomToFullExtent();

  // The canvas reprojects layer *extents* itself, so the extent alone cannot
  // tell whether the layer was told anything. The geometry can: a layer that
  // never heard about the new CRS still holds degrees, and drawn into a view
  // framed on metres its four features collapse onto a single pixel.
  QImage image(400, 400, QImage::Format_ARGB32);
  image.fill(Qt::white);
  canvas.render(&image);

  QRectF painted;
  bool any = false;

  for (int y = 0; y < image.height(); ++y)
  {
    for (int x = 0; x < image.width(); ++x)
    {
      if (image.pixelColor(x, y) != QColor(Qt::white))
      {
        expandTo(painted, any, QPointF(x, y));
      }
    }
  }

  ASSERT_TRUE(any) << "nothing was drawn at all";
  EXPECT_GT(painted.width(), 100.0)
    << "the CRS change never reached the layer that was already loaded";
}

// ── Raster ──────────────────────────────────────────────────────────────────

TEST_F(GdalLayerTest, ReadsARasterWithItsGeoreferencing)
{
  QString message;
  const std::unique_ptr<GdalRasterLayer> layer = GdalRasterLayer::open(
    fixture(QStringLiteral("generated-terrain.tif")), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  EXPECT_EQ(layer->rasterSize(), QSize(kRasterWidth, kRasterHeight));
  EXPECT_EQ(layer->bandCount(), 1);
  EXPECT_FALSE(layer->isColorImage());

  // Top-left (-8, 4), one degree per pixel, 16 by 8 pixels.
  EXPECT_NEAR(layer->extent().left(), -8.0, 1e-9);
  EXPECT_NEAR(layer->extent().right(), 8.0, 1e-9);
  EXPECT_NEAR(layer->extent().top(), -4.0, 1e-9);
  EXPECT_NEAR(layer->extent().bottom(), 4.0, 1e-9);

  double minimum = 0.0;
  double maximum = 0.0;
  layer->valueRange(minimum, maximum);

  // No-data excluded from the range: included, the minimum would be -9999
  // and every real value would crowd into the ramp's top sliver.
  EXPECT_NEAR(minimum, 0.0, 1e-6);
  EXPECT_NEAR(maximum, kRasterWidth - 1, 1e-6);
}

TEST_F(GdalLayerTest, ShadesASingleBandRasterAlongItsRamp)
{
  QString message;
  std::unique_ptr<GdalRasterLayer> layer = GdalRasterLayer::open(
    fixture(QStringLiteral("generated-terrain.tif")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  layer->setRamp(
    ColorRamp({{0.0, QColor(Qt::black)}, {1.0, QColor(Qt::white)}}));

  MapTransform transform;
  transform.fit(layer->extent(), QSizeF(320.0, 160.0), 0.0);

  QImage canvas(320, 160, QImage::Format_ARGB32);
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  layer->render(painter, transform);
  painter.end();

  ASSERT_FALSE(layer->lastImage().isNull());

  // The fixture ramps west to east, so the drawn image must too. Reading the
  // rendered canvas rather than the source proves the whole path.
  // Sampled below the no-data band, which is transparent by design.
  const int left = canvas.pixelColor(8, 130).lightness();
  const int right = canvas.pixelColor(311, 130).lightness();

  EXPECT_LT(left, 60) << "the western edge is not the ramp's low end";
  EXPECT_GT(right, 200) << "the eastern edge is not the ramp's high end";
}

TEST_F(GdalLayerTest, LeavesNoDataUnpaintedRatherThanShadingIt)
{
  QString message;
  std::unique_ptr<GdalRasterLayer> layer = GdalRasterLayer::open(
    fixture(QStringLiteral("generated-terrain.tif")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  MapTransform transform;
  transform.fit(layer->extent(), QSizeF(256.0, 128.0), 0.0);

  QImage canvas(256, 128, QImage::Format_ARGB32);
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  layer->render(painter, transform);
  painter.end();

  // The top quarter of the fixture holds no data. Shaded with the ramp's
  // lowest colour it would look like ground; it must stay transparent.
  const int noDataRow =
    static_cast<int>(128.0 * kNoDataRows / kRasterHeight / 2.0);

  EXPECT_EQ(canvas.pixelColor(128, noDataRow).alpha(), 0)
    << "no-data was painted as though it were a measurement";

  // And real data below it is painted.
  EXPECT_GT(canvas.pixelColor(128, 100).alpha(), 0);
}

TEST_F(GdalLayerTest, ReadsNoMorePixelsThanTheScreenCanShow)
{
  QString message;
  std::unique_ptr<GdalRasterLayer> layer = GdalRasterLayer::open(
    fixture(QStringLiteral("generated-terrain.tif")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  // A 40-pixel viewport over a 256-pixel-wide raster: decoding all of it to
  // fill 40 pixels is the difference between a map that pans and one that
  // does not.
  MapTransform transform;
  transform.fit(layer->extent(), QSizeF(40.0, 20.0), 0.0);

  QImage canvas(40, 20, QImage::Format_ARGB32);
  QPainter painter(&canvas);
  layer->render(painter, transform);
  painter.end();

  ASSERT_FALSE(layer->lastImage().isNull());
  EXPECT_LE(layer->lastImage().width(), 40);
  EXPECT_LE(layer->lastImage().height(), 20);
  EXPECT_LT(layer->lastImage().width(), kRasterWidth)
    << "the whole band was decoded to fill a 40-pixel viewport";
}

TEST_F(GdalLayerTest, DrawsNothingWhenTheRasterIsOffScreen)
{
  QString message;
  std::unique_ptr<GdalRasterLayer> layer = GdalRasterLayer::open(
    fixture(QStringLiteral("generated-terrain.tif")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  MapTransform transform;
  transform.fit(QRectF(1000.0, 1000.0, 10.0, 10.0), QSizeF(200.0, 200.0), 0.0);

  QImage canvas(200, 200, QImage::Format_ARGB32);
  canvas.fill(Qt::white);

  QPainter painter(&canvas);
  layer->render(painter, transform);
  painter.end();

  for (int y = 0; y < 200; y += 20)
  {
    for (int x = 0; x < 200; x += 20)
    {
      ASSERT_EQ(canvas.pixelColor(x, y), QColor(Qt::white));
    }
  }
}
