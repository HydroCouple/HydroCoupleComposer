/*!
 * \file   test_tilelayer.cpp
 * \brief  Phase C1d verification — the basemap layer and its source.
 *
 * No network: the source is an interface precisely so these tests can supply
 * tiles from memory. A basemap test that needed the internet would fail for
 * reasons having nothing to do with this code, and would be switched off
 * within a week.
 */

#include "core/composerapplication.h"
#include "layers/networktilesource.h"
#include "layers/tilelayer.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QPainter>
#include <QSet>

using namespace HydroCouple::Composer;

namespace
{
  //! A source backed by a table of images, recording what was asked for.
  class FakeTileSource : public ITileSource
  {
    public:
      void give(const TileId &id, const QColor &color)
      {
        QImage image(TileGrid::kTileSize, TileGrid::kTileSize,
                     QImage::Format_ARGB32);
        image.fill(color);

        m_images.insert(key(id), image);
      }

      [[nodiscard]] QImage tile(const TileId &id) override
      {
        return m_images.value(key(id));
      }

      void request(const TileId &id) override
      {
        requested.append(id);
      }

      void cancelPending() override { ++cancelCount; }

      [[nodiscard]] QString attribution() const override
      {
        return QStringLiteral("© test");
      }

      [[nodiscard]] int maximumZoom() const override { return maxZoom; }

      QVector<TileId> requested;
      int cancelCount = 0;
      int maxZoom = 19;

    private:
      static QString key(const TileId &id)
      {
        return QStringLiteral("%1/%2/%3").arg(id.zoom).arg(id.x).arg(id.y);
      }

      QHash<QString, QImage> m_images;
  };

  class TileLayerTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_tilelayer";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      //! A transform showing the whole world in a 512-pixel square.
      static MapTransform worldView()
      {
        MapTransform transform;
        transform.fit(TileGrid::worldExtent(), QSizeF(512.0, 512.0), 0.0);

        return transform;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *TileLayerTest::s_app = nullptr;
}

TEST_F(TileLayerTest, AsksForTheTilesItDoesNotHave)
{
  auto source = std::make_unique<FakeTileSource>();
  FakeTileSource *raw = source.get();

  TileLayer layer(QStringLiteral("basemap"), std::move(source));

  QImage canvas(512, 512, QImage::Format_ARGB32);
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  layer.render(painter, worldView());
  painter.end();

  EXPECT_FALSE(raw->requested.isEmpty())
    << "a basemap with no tiles asked for none";
  EXPECT_EQ(layer.lastDrawnTileCount(), 0);

  // Every request must be a real tile of the chosen zoom.
  for (const TileId &id : raw->requested)
  {
    EXPECT_EQ(id.zoom, layer.lastZoom());
    EXPECT_GE(id.x, 0);
    EXPECT_LT(id.x, TileGrid::tilesAcross(id.zoom));
  }
}

TEST_F(TileLayerTest, DrawsTheTilesItHasWhereTheyBelong)
{
  auto source = std::make_unique<FakeTileSource>();
  FakeTileSource *raw = source.get();

  // A 512-pixel view of the world at zoom 1 shows four tiles of 256 pixels.
  raw->give({1, 0, 0}, Qt::red);     // north-west
  raw->give({1, 1, 0}, Qt::green);   // north-east
  raw->give({1, 0, 1}, Qt::blue);    // south-west
  raw->give({1, 1, 1}, Qt::yellow);  // south-east

  TileLayer layer(QStringLiteral("basemap"), std::move(source));

  QImage canvas(512, 512, QImage::Format_ARGB32);
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  layer.render(painter, worldView());
  painter.end();

  ASSERT_EQ(layer.lastZoom(), 1);
  EXPECT_EQ(layer.lastDrawnTileCount(), 4);

  // North-west of the world is the top-left of the screen. A grid that
  // counted rows from the south would put blue here.
  EXPECT_EQ(canvas.pixelColor(64, 64), QColor(Qt::red));
  EXPECT_EQ(canvas.pixelColor(448, 64), QColor(Qt::green));
  EXPECT_EQ(canvas.pixelColor(64, 448), QColor(Qt::blue));
  EXPECT_EQ(canvas.pixelColor(448, 448), QColor(Qt::yellow));

  EXPECT_TRUE(raw->requested.isEmpty())
    << "tiles already to hand were requested again";
}

TEST_F(TileLayerTest, LeavesNoSeamsBetweenAdjacentTiles)
{
  auto source = std::make_unique<FakeTileSource>();
  FakeTileSource *raw = source.get();

  for (int x = 0; x < 2; ++x)
  {
    for (int y = 0; y < 2; ++y)
    {
      raw->give({1, x, y}, Qt::black);
    }
  }

  // Through the real canvas, not a painter this test configures itself.
  // Seams appear only when the painter antialiases — which MapCanvas turns
  // on — so a test that built its own painter would render under conditions
  // the application never uses and would pass for a renderer that seams.
  //
  // 501 pixels, deliberately: the world then lands on fractional tile
  // boundaries. At a size where tiles fall on exact pixels — 512, say — a
  // missing rounding step is invisible.
  constexpr int kSide = 501;

  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  canvas.setBackgroundColor(Qt::white);
  canvas.resize(kSide, kSide);

  auto *basemap = new TileLayer(QStringLiteral("basemap"), std::move(source));
  stack.addLayer(basemap);
  canvas.setVisibleExtent(TileGrid::worldExtent());

  QImage image(kSide, kSide, QImage::Format_ARGB32);
  image.fill(Qt::white);
  canvas.render(&image);

  // At least the four of zoom 1. The framing margin pushes the view a little
  // past the date line, so wrapped copies are drawn on each side too — which
  // is the scheme working, not a fault.
  ASSERT_GE(basemap->lastDrawnTileCount(), 4);

  const QPointF middle = canvas.transform().toScreen(QPointF(0.0, 0.0));

  // Across both shared boundaries: a pixel that is not fully covered shows
  // the white beneath, and those seams tile the whole map with a grid. The
  // scan stops short of the edges so the margin — and the attribution in the
  // bottom corner — are not mistaken for seams.
  for (int i = 40; i < 400; ++i)
  {
    for (int offset = -2; offset <= 2; ++offset)
    {
      const int atX = qRound(middle.x()) + offset;
      const int atY = qRound(middle.y()) + offset;

      ASSERT_EQ(image.pixelColor(atX, i), QColor(Qt::black))
        << "vertical seam at " << atX << "," << i;
      ASSERT_EQ(image.pixelColor(i, atY), QColor(Qt::black))
        << "horizontal seam at " << i << "," << atY;
    }
  }
}

TEST_F(TileLayerTest, RespectsTheProvidersDeepestZoom)
{
  auto source = std::make_unique<FakeTileSource>();
  FakeTileSource *raw = source.get();
  raw->maxZoom = 3;

  TileLayer layer(QStringLiteral("basemap"), std::move(source));

  MapTransform transform;
  transform.fit(QRectF(0.0, 0.0, 200.0, 200.0), QSizeF(512.0, 512.0), 0.0);

  QImage canvas(512, 512, QImage::Format_ARGB32);
  QPainter painter(&canvas);
  layer.render(painter, transform);
  painter.end();

  // Asking beyond what a provider has returns nothing at all, so the request
  // has to be clamped rather than sent.
  EXPECT_EQ(layer.lastZoom(), 3);

  for (const TileId &id : raw->requested)
  {
    EXPECT_LE(id.zoom, 3);
  }
}

TEST_F(TileLayerTest, AbandonsPendingTilesWhenTheZoomChanges)
{
  auto source = std::make_unique<FakeTileSource>();
  FakeTileSource *raw = source.get();

  TileLayer layer(QStringLiteral("basemap"), std::move(source));

  QImage canvas(512, 512, QImage::Format_ARGB32);
  QPainter painter(&canvas);

  layer.render(painter, worldView());
  const int afterFirst = raw->cancelCount;

  MapTransform closer;
  closer.fit(QRectF(0.0, 0.0, 50000.0, 50000.0), QSizeF(512.0, 512.0), 0.0);
  layer.render(painter, closer);

  painter.end();

  EXPECT_GT(raw->cancelCount, afterFirst)
    << "tiles for the zoom just left were still being waited for";
}

TEST_F(TileLayerTest, IsABackdropAndCoversTheWorld)
{
  TileLayer layer(QStringLiteral("basemap"),
                  std::make_unique<FakeTileSource>());

  EXPECT_TRUE(layer.isBasemap());
  EXPECT_EQ(layer.extent(), TileGrid::worldExtent());
}

// ── Provider definitions ────────────────────────────────────────────────────

TEST_F(TileLayerTest, FillsInTheProvidersUrlTemplate)
{
  BasemapProvider provider;
  provider.urlTemplate = QStringLiteral("https://x/{z}/{x}/{y}.png");

  EXPECT_EQ(NetworkTileSource::urlFor(provider, {5, 9, 12}),
            QStringLiteral("https://x/5/9/12.png"));
}

TEST_F(TileLayerTest, EveryBuiltInProviderCarriesItsAttribution)
{
  const QVector<BasemapProvider> providers =
    NetworkTileSource::builtinProviders();

  ASSERT_FALSE(providers.isEmpty());

  for (const BasemapProvider &provider : providers)
  {
    // Attribution is a condition of use for every free provider here, so a
    // provider without it is one this application must not ship.
    EXPECT_FALSE(provider.attribution.isEmpty())
      << provider.name.toStdString() << " has no attribution";
    EXPECT_TRUE(provider.urlTemplate.contains(QStringLiteral("{z}")));
    EXPECT_TRUE(provider.urlTemplate.contains(QStringLiteral("{x}")));
    EXPECT_TRUE(provider.urlTemplate.contains(QStringLiteral("{y}")));
    EXPECT_GT(provider.maximumZoom, 0);
  }

  // An unknown name falls back to the first provider rather than to an
  // unusable one with an empty template.
  EXPECT_EQ(NetworkTileSource::builtinProvider(QStringLiteral("nope")).name,
            providers.first().name);
  EXPECT_EQ(NetworkTileSource::builtinProvider(providers.at(1).name).name,
            providers.at(1).name);
}

// ── Attribution on the map (C1d) ────────────────────────────────────────────

TEST_F(TileLayerTest, TheMapDrawsTheProvidersRequiredCredit)
{
  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  canvas.resize(400, 200);
  canvas.setBackgroundColor(Qt::white);

  auto *basemap = new TileLayer(QStringLiteral("basemap"),
                                std::make_unique<FakeTileSource>());
  stack.addLayer(basemap);

  const auto darkPixelsInCorner = [&canvas]
  {
    QImage image(canvas.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    canvas.render(&image);

    int dark = 0;

    for (int y = 160; y < 200; ++y)
    {
      for (int x = 200; x < 400; ++x)
      {
        if (image.pixelColor(x, y).lightness() < 120)
        {
          ++dark;
        }
      }
    }

    return dark;
  };

  EXPECT_GT(darkPixelsInCorner(), 10)
    << "the provider's credit is a condition of use and was not drawn";

  // A hidden layer is not on the map, so its provider is owed nothing.
  basemap->setVisible(false);
  EXPECT_EQ(darkPixelsInCorner(), 0);
}
