#include "map/tilegrid.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Latitude where the Mercator projection reaches the world square's edge.
    constexpr double kMaxLatitude = 85.05112877980659;
  }

  QRectF TileGrid::worldExtent()
  {
    return QRectF(-kWorldHalfSpan, -kWorldHalfSpan, kWorldHalfSpan * 2.0,
                  kWorldHalfSpan * 2.0);
  }

  int TileGrid::tilesAcross(int zoom)
  {
    return 1 << std::clamp(zoom, 0, kMaxZoom);
  }

  QRectF TileGrid::tileExtent(const TileId &tile)
  {
    const int across = tilesAcross(tile.zoom);
    const double span = kWorldHalfSpan * 2.0 / across;

    // Tile Y counts from the north, so the top edge is the world's top minus
    // whole tiles, and the rectangle grows downward from there.
    const double left = -kWorldHalfSpan + tile.x * span;
    const double top = kWorldHalfSpan - tile.y * span;

    return QRectF(QPointF(left, top - span), QPointF(left + span, top))
      .normalized();
  }

  int TileGrid::zoomForScale(double scale)
  {
    if (!(scale > 0.0))
    {
      return 0;
    }

    // At zoom z the world is 256·2^z pixels wide and 2·kWorldHalfSpan metres
    // wide, so the scale that renders tiles at their native resolution is
    // 256·2^z / (2·kWorldHalfSpan). Solve for z.
    const double worldPixels = scale * kWorldHalfSpan * 2.0;
    const double zoom = std::log2(worldPixels / kTileSize);

    return std::clamp(static_cast<int>(std::round(zoom)), 0, kMaxZoom);
  }

  QVector<TilePlacement> TileGrid::tilesForExtent(const QRectF &extent,
                                                  int zoom)
  {
    QVector<TilePlacement> tiles;

    if (extent.isNull() || !extent.isValid())
    {
      return tiles;
    }

    const int level = std::clamp(zoom, 0, kMaxZoom);
    const int across = tilesAcross(level);
    const double span = kWorldHalfSpan * 2.0 / across;

    const auto column = [&](double x) { return (x + kWorldHalfSpan) / span; };
    const auto row = [&](double y) { return (kWorldHalfSpan - y) / span; };

    // The trailing edge is ceil-minus-one, not floor: an extent whose right
    // edge lands exactly on a tile boundary — the whole world, most obviously
    // — otherwise picks up an extra column of tiles that it only touches.
    const int firstColumn = static_cast<int>(std::floor(column(extent.left())));
    const int lastColumn =
      static_cast<int>(std::ceil(column(extent.right()))) - 1;
    const int firstRow = static_cast<int>(std::floor(row(extent.bottom())));
    const int lastRow = static_cast<int>(std::ceil(row(extent.top()))) - 1;

    // A very wide view can ask for more of the world than exists; asking the
    // provider for millions of tiles is never the right answer.
    if (static_cast<qint64>(lastColumn - firstColumn + 1)
          * (lastRow - firstRow + 1)
        > 4096)
    {
      return tiles;
    }

    for (int row = firstRow; row <= lastRow; ++row)
    {
      // No world above the pole: rows outside the grid are dropped rather
      // than wrapped, unlike columns.
      if (row < 0 || row >= across)
      {
        continue;
      }

      for (int column = firstColumn; column <= lastColumn; ++column)
      {
        // Columns wrap, so panning past the date line continues the map.
        int wrapped = column % across;

        if (wrapped < 0)
        {
          wrapped += across;
        }

        // The extent uses the unwrapped column, so a repeated tile lands
        // where the view expects it rather than back at the first copy.
        const double left = -kWorldHalfSpan + column * span;
        const double top = kWorldHalfSpan - row * span;

        tiles.append({{level, wrapped, row},
                      QRectF(QPointF(left, top - span),
                             QPointF(left + span, top))
                        .normalized()});
      }
    }

    return tiles;
  }

  QPointF TileGrid::fromLonLat(double longitude, double latitude)
  {
    const double clamped =
      std::clamp(latitude, -kMaxLatitude, kMaxLatitude);

    const double x = longitude * kWorldHalfSpan / 180.0;
    const double y =
      std::log(std::tan((90.0 + clamped) * M_PI / 360.0)) / (M_PI / 180.0);

    return QPointF(x, y * kWorldHalfSpan / 180.0);
  }

} // namespace HydroCouple::Composer
