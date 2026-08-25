/*!
 * \file   tilegrid.h
 * \author Caleb Buahin
 * \brief  TileGrid — the slippy-map tile scheme, as arithmetic.
 *
 * Every tiled basemap in use — OpenStreetMap, the commercial providers, and
 * WMTS's GoogleMapsCompatible matrix set — is the same grid: the Web Mercator
 * world square, halved in each direction at every zoom level. Keeping that
 * arithmetic in one place, with no network and no painting, means the part
 * most likely to be subtly wrong is the part that is easiest to test.
 *
 * Tile Y counts from the **north**, which is the one convention that catches
 * everybody: a grid that counts from the south renders a world that is
 * upside down, tile by tile, and each individual tile still looks correct.
 */

#ifndef HYDROCOUPLECOMPOSER_MAP_TILEGRID_H
#define HYDROCOUPLECOMPOSER_MAP_TILEGRID_H

#include <QMetaType>
#include <QRectF>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief One tile's address.
   */
  struct TileId
  {
      int zoom = 0;
      int x = 0;
      int y = 0;

      bool operator==(const TileId &other) const
      {
        return zoom == other.zoom && x == other.x && y == other.y;
      }
  };

  //! Hash so tiles can key a cache.
  inline size_t qHash(const TileId &id, size_t seed = 0)
  {
    return ::qHash(static_cast<quint64>(id.zoom) << 58
                     ^ static_cast<quint64>(id.x) << 29
                     ^ static_cast<quint64>(id.y),
                   seed);
  }

  /*!
   * \brief One tile and where it goes.
   *
   * The id is wrapped into the grid, because that is what the provider is
   * asked for; the extent is not, because a view panned past the date line
   * draws the same tile again further along rather than on top of itself.
   */
  struct TilePlacement
  {
      TileId id;
      QRectF extent;  //!< Where to draw it, in EPSG:3857 metres.
  };

  /*!
   * \brief The Web Mercator tile scheme.
   */
  class TileGrid
  {
    public:
      //! Half the width of the Web Mercator world square, in metres.
      static constexpr double kWorldHalfSpan = 20037508.342789244;

      //! Side of one tile image, in pixels. Universal across providers.
      static constexpr int kTileSize = 256;

      //! Deepest zoom worth requesting; beyond this providers have no data.
      static constexpr int kMaxZoom = 22;

      /*!
       * \brief The whole world, in EPSG:3857 metres.
       */
      [[nodiscard]] static QRectF worldExtent();

      /*!
       * \brief How many tiles span the world at \a zoom.
       * \param zoom Zoom level.
       */
      [[nodiscard]] static int tilesAcross(int zoom);

      /*!
       * \brief The extent one tile covers, in EPSG:3857 metres.
       * \param tile The tile to measure.
       */
      [[nodiscard]] static QRectF tileExtent(const TileId &tile);

      /*!
       * \brief The zoom whose tiles are closest to \a scale.
       *
       * Chosen so one tile pixel is about one screen pixel: coarser wastes
       * the display, finer wastes bandwidth drawing detail nobody can see.
       *
       * \param scale Pixels per metre, from the map transform.
       */
      [[nodiscard]] static int zoomForScale(double scale);

      /*!
       * \brief Every tile at \a zoom that \a extent touches.
       *
       * Tiles outside the world in X wrap around, as the scheme intends —
       * panning past the date line shows the map continuing, not a void.
       * Tiles outside it in Y are dropped: there is no world above the pole.
       *
       * \param extent World rectangle in EPSG:3857 metres.
       * \param zoom Zoom level.
       */
      [[nodiscard]] static QVector<TilePlacement> tilesForExtent(
        const QRectF &extent, int zoom);

      /*!
       * \brief Converts longitude and latitude to EPSG:3857 metres.
       *
       * Provided so callers need no CRS object for the one conversion the
       * tile scheme is defined by.
       *
       * \param longitude Degrees east.
       * \param latitude Degrees north; clamped to the scheme's limit.
       */
      [[nodiscard]] static QPointF fromLonLat(double longitude,
                                              double latitude);
  };

} // namespace HydroCouple::Composer

Q_DECLARE_METATYPE(HydroCouple::Composer::TileId)

#endif // HYDROCOUPLECOMPOSER_MAP_TILEGRID_H
