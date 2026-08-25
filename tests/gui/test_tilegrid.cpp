/*!
 * \file   test_tilegrid.cpp
 * \brief  Phase C1d verification — the slippy-map tile scheme.
 *
 * Checked against the scheme's published values rather than against itself:
 * a grid that counted tile rows from the south instead of the north would be
 * perfectly self-consistent, would round-trip, and would render the world
 * upside down one tile at a time.
 */

#include "map/tilegrid.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace HydroCouple::Composer;

TEST(TileGridTest, ZoomZeroIsOneTileCoveringTheWorld)
{
  EXPECT_EQ(TileGrid::tilesAcross(0), 1);

  const QRectF tile = TileGrid::tileExtent({0, 0, 0});
  const QRectF world = TileGrid::worldExtent();

  EXPECT_NEAR(tile.left(), world.left(), 1e-6);
  EXPECT_NEAR(tile.right(), world.right(), 1e-6);
  EXPECT_NEAR(tile.width(), TileGrid::kWorldHalfSpan * 2.0, 1e-6);
}

TEST(TileGridTest, EachZoomHalvesTheTiles)
{
  EXPECT_EQ(TileGrid::tilesAcross(1), 2);
  EXPECT_EQ(TileGrid::tilesAcross(10), 1024);

  EXPECT_NEAR(TileGrid::tileExtent({1, 0, 0}).width(),
              TileGrid::tileExtent({0, 0, 0}).width() / 2.0, 1e-6);
}

TEST(TileGridTest, TileYCountsFromTheNorth)
{
  // The single most common way to get this scheme wrong. At zoom 1, tile
  // (0,0) is the north-west quadrant and (0,1) the south-west one.
  const QRectF northWest = TileGrid::tileExtent({1, 0, 0});
  const QRectF southWest = TileGrid::tileExtent({1, 0, 1});

  EXPECT_GT(northWest.center().y(), southWest.center().y())
    << "tile row 0 is not the northern one — the world renders upside down";

  EXPECT_NEAR(northWest.top(), 0.0, 1e-6);
  EXPECT_NEAR(northWest.bottom(), TileGrid::kWorldHalfSpan, 1e-6);
  EXPECT_NEAR(southWest.bottom(), 0.0, 1e-6);
}

TEST(TileGridTest, TilesTileTheWorldWithoutGapsOrOverlap)
{
  const int across = TileGrid::tilesAcross(2);
  double area = 0.0;

  for (int x = 0; x < across; ++x)
  {
    for (int y = 0; y < across; ++y)
    {
      const QRectF tile = TileGrid::tileExtent({2, x, y});
      area += tile.width() * tile.height();

      // Every tile must sit inside the world square.
      EXPECT_TRUE(TileGrid::worldExtent()
                    .adjusted(-1.0, -1.0, 1.0, 1.0)
                    .contains(tile));
    }
  }

  const QRectF world = TileGrid::worldExtent();
  EXPECT_NEAR(area, world.width() * world.height(), 1.0);
}

TEST(TileGridTest, ConvertsLongitudeAndLatitudeToTheWorldSquare)
{
  // Known answers for Web Mercator: the origin is (0, 0); 180° east is the
  // right edge; and 45° north is a published 5621521.486 m.
  const QPointF origin = TileGrid::fromLonLat(0.0, 0.0);
  EXPECT_NEAR(origin.x(), 0.0, 1e-6);
  EXPECT_NEAR(origin.y(), 0.0, 1e-6);

  EXPECT_NEAR(TileGrid::fromLonLat(180.0, 0.0).x(), TileGrid::kWorldHalfSpan,
              1e-6);
  EXPECT_NEAR(TileGrid::fromLonLat(0.0, 45.0).y(), 5621521.486, 1.0);

  // Beyond the scheme's latitude limit the projection runs to infinity, so
  // it is clamped rather than allowed to produce one.
  const QPointF pole = TileGrid::fromLonLat(0.0, 90.0);
  EXPECT_TRUE(std::isfinite(pole.y()));
  EXPECT_NEAR(pole.y(), TileGrid::kWorldHalfSpan, 1.0);
}

TEST(TileGridTest, ChoosesTheZoomThatMatchesTheScreen)
{
  // At zoom z the world is 256·2^z pixels across. Ask for exactly that and
  // the same z must come back.
  for (const int zoom : {0, 4, 12, 18})
  {
    const double worldPixels = TileGrid::kTileSize * std::pow(2.0, zoom);
    const double scale = worldPixels / (TileGrid::kWorldHalfSpan * 2.0);

    EXPECT_EQ(TileGrid::zoomForScale(scale), zoom);
  }

  EXPECT_EQ(TileGrid::zoomForScale(0.0), 0);
  EXPECT_LE(TileGrid::zoomForScale(1.0e12), TileGrid::kMaxZoom);
}

TEST(TileGridTest, ListsExactlyTheTilesAViewTouches)
{
  // A view over the north-west quadrant at zoom 1 needs one tile.
  const QVector<TilePlacement> one = TileGrid::tilesForExtent(
    QRectF(-1.0e6, 1.0e6, 1.0e5, 1.0e5), 1);

  ASSERT_EQ(one.size(), 1);
  EXPECT_EQ(one.first().id.x, 0);
  EXPECT_EQ(one.first().id.y, 0);

  // The whole world at zoom 1 needs all four.
  const QVector<TilePlacement> all =
    TileGrid::tilesForExtent(TileGrid::worldExtent(), 1);

  EXPECT_EQ(all.size(), 4);
}

TEST(TileGridTest, WrapsColumnsAcrossTheDateLineButNotRowsOverThePole)
{
  const double span = TileGrid::kWorldHalfSpan;

  // A view straddling the date line: the tiles on the far side come back
  // wrapped into the grid, but placed where the view expects them.
  const QVector<TilePlacement> wrapped = TileGrid::tilesForExtent(
    QRectF(QPointF(span * 0.5, -span * 0.1), QPointF(span * 1.5, span * 0.1)),
    1);

  ASSERT_FALSE(wrapped.isEmpty());

  bool sawWrappedPlacement = false;

  for (const TilePlacement &placement : wrapped)
  {
    EXPECT_GE(placement.id.x, 0);
    EXPECT_LT(placement.id.x, TileGrid::tilesAcross(1));

    if (placement.extent.left() > span - 1.0)
    {
      sawWrappedPlacement = true;
    }
  }

  EXPECT_TRUE(sawWrappedPlacement)
    << "a tile past the date line was drawn back at the first copy";

  // Above the pole there is no world, so those rows are dropped rather than
  // wrapped round to the bottom of the map.
  const QVector<TilePlacement> polar = TileGrid::tilesForExtent(
    QRectF(QPointF(-span * 0.1, span * 0.9), QPointF(span * 0.1, span * 3.0)),
    1);

  for (const TilePlacement &placement : polar)
  {
    EXPECT_GE(placement.id.y, 0);
    EXPECT_LT(placement.id.y, TileGrid::tilesAcross(1));
  }
}

TEST(TileGridTest, RefusesToAskForAnAbsurdNumberOfTiles)
{
  // A deep zoom over a wide extent would name millions of tiles; asking a
  // provider for them is never the right answer.
  const QVector<TilePlacement> tiles =
    TileGrid::tilesForExtent(TileGrid::worldExtent(), 18);

  EXPECT_TRUE(tiles.isEmpty());
}

TEST(TileGridTest, PlacementExtentsMatchTheirTiles)
{
  const QVector<TilePlacement> tiles =
    TileGrid::tilesForExtent(TileGrid::worldExtent(), 2);

  ASSERT_FALSE(tiles.isEmpty());

  for (const TilePlacement &placement : tiles)
  {
    const QRectF canonical = TileGrid::tileExtent(placement.id);

    EXPECT_NEAR(placement.extent.width(), canonical.width(), 1e-6);
    EXPECT_NEAR(placement.extent.center().y(), canonical.center().y(), 1e-6);
  }
}
