/*!
 * \file   ogctilesource.h
 * \author Caleb Buahin
 * \brief  Tiles from an OGC web service, for the existing TileLayer.
 *
 * A WMS and a WMTS reach the map the same way an XYZ provider does: as an
 * ITileSource behind TileLayer, which already knows how to draw a slippy
 * map, keep a coarser tile until a finer one arrives, and not block while
 * it waits. What differs is only how a tile's URL is arrived at, so that is
 * the one thing the two subclasses here supply.
 *
 * WMS has no tiles of its own — it takes a bounding box — so WmsTileSource
 * asks for one GetMap per tile of the Web Mercator grid. That is a
 * deliberate divergence from openswmm.gui, which issues one GetMap per
 * viewport: cutting the request up costs more round trips but makes every
 * answer cacheable and reusable at the next pan, and lets the layer show
 * something before the whole view has arrived.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_OGCTILESOURCE_H
#define HYDROCOUPLECOMPOSER_LAYERS_OGCTILESOURCE_H

#include "layers/tilelayer.h"

#include <hydrocoupleogc/httpclient.h>
#include <hydrocoupleogc/servicecredentials.h>
#include <hydrocoupleogc/wmscapabilities.h>
#include <hydrocoupleogc/wmtscapabilities.h>

#include <QCache>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

namespace HydroCouple::Composer
{

  /*!
   * \brief What every OGC tile source does the same way.
   *
   * Holds the decoded-image cache, the outstanding requests and the client
   * they go through. A subclass supplies only the URL for a tile.
   */
  class OgcTileSource : public QObject, public ITileSource
  {
      Q_OBJECT

    public:
      explicit OgcTileSource(QObject *parent = nullptr);

      /*!
       * \brief Sets what to call when a requested tile arrives.
       * \param callback Invoked on the GUI thread, once per tile.
       */
      void setTileReadyCallback(std::function<void()> callback);

      //! What the service needs before it will answer.
      void setCredentials(const HydroCouple::Ogc::ServiceCredentials
                            &credentials);

      //! Where fetched tiles are kept between sessions; off until set.
      void setCacheDirectory(const QString &path);

      //! What the map must display for this service.
      void setAttribution(const QString &attribution);

      /*!
       * \brief Whether this source can produce tiles at all.
       *
       * False when the service does not offer what a Web Mercator basemap
       * needs — see reason().
       */
      [[nodiscard]] virtual bool isUsable() const = 0;

      //! Why not, when isUsable() is false.
      [[nodiscard]] QString reason() const;

      /*!
       * \brief How many tiles have been asked for and not yet answered.
       *
       * The layer has no use for it, but it is the only way to tell "not
       * arrived yet" from "arrived and was nothing", which are the same
       * blank square on screen.
       */
      [[nodiscard]] int pendingCount() const;

      // ── ITileSource ──────────────────────────────────────────────────────

      [[nodiscard]] QImage tile(const TileId &tile) override;

      void request(const TileId &tile) override;

      void cancelPending() override;

      [[nodiscard]] QString attribution() const override;

      /*!
       * \brief The URL one tile would be fetched from.
       *
       * Public because it is the thing worth testing about a service: given
       * what the server published, does the request name the right ground.
       *
       * \param tile The tile wanted.
       * \returns The URL, or empty when this source cannot serve that tile.
       */
      [[nodiscard]] virtual QString urlFor(const TileId &tile) const = 0;

    protected:
      QString m_reason;

    private:
      HydroCouple::Ogc::HttpClient *m_client = nullptr;
      HydroCouple::Ogc::ServiceCredentials m_credentials;
      QString m_attribution;

      //! Decoded tiles, so a pan back over covered ground costs nothing.
      QCache<QString, QImage> m_cache;

      QHash<QString, HydroCouple::Ogc::HttpClient::RequestId> m_pending;

      /*!
       * \brief Tiles the service answered with something that is not an
       *        image.
       *
       * Remembered because the layer asks again on every repaint, and a
       * service that refuses one tile refuses it every time — without this
       * a single bad tile becomes a request per frame.
       */
      QSet<QString> m_refused;

      std::function<void()> m_tileReady;
  };

  /*!
   * \brief One GetMap per tile of the Web Mercator grid.
   */
  class WmsTileSource : public OgcTileSource
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs a source over \a capabilities.
       * \param capabilities What the server published.
       * \param layers Layer names, drawn bottom to top as listed.
       * \param format An advertised image format, or empty for the first.
       * \param parent Owning object.
       */
      WmsTileSource(const HydroCouple::Ogc::WmsCapabilities &capabilities,
                    const QStringList &layers,
                    const QString &format = QString(),
                    QObject *parent = nullptr);

      [[nodiscard]] bool isUsable() const override;

      [[nodiscard]] QString urlFor(const TileId &tile) const override;

      [[nodiscard]] int maximumZoom() const override;

      //! How deep the map may zoom on this service; 19 unless set.
      void setMaximumZoom(int zoom);

      /*!
       * \brief How the server spells Web Mercator, of the spellings it
       *        offers.
       *
       * Servers advertise the same system as EPSG:3857, as a URN, and — on
       * older software — as EPSG:900913 or ESRI's 102100. The request has
       * to echo one the server actually listed, so this reports which.
       *
       * \param layer The layer to be drawn.
       * \returns The server's own spelling, or empty when it offers none.
       */
      [[nodiscard]] static QString webMercatorSpelling(
        const HydroCouple::Ogc::WmsLayerInfo &layer);

    private:
      HydroCouple::Ogc::WmsCapabilities m_capabilities;
      QStringList m_layers;
      QString m_format;
      QString m_crs;
      int m_maximumZoom = 19;
  };

  /*!
   * \brief Tiles from a WMTS published on the familiar web pyramid.
   *
   * Only matrix sets that are the slippy-map grid are accepted. A server
   * free to lay its pyramid out on any grid it likes can only be drawn by a
   * matrix-aware client, and TileGrid — the whole map's tiling — is Web
   * Mercator by construction.
   */
  class WmtsTileSource : public OgcTileSource
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs a source over \a capabilities.
       * \param capabilities What the server published.
       * \param layerId The layer's identifier.
       * \param matrixSetId Which pyramid to read.
       * \param style The style, or empty for the layer's default.
       * \param format An advertised format, or empty for the first.
       * \param parent Owning object.
       */
      WmtsTileSource(const HydroCouple::Ogc::WmtsCapabilities &capabilities,
                     const QString &layerId, const QString &matrixSetId,
                     const QString &style = QString(),
                     const QString &format = QString(),
                     QObject *parent = nullptr);

      [[nodiscard]] bool isUsable() const override;

      [[nodiscard]] QString urlFor(const TileId &tile) const override;

      [[nodiscard]] int maximumZoom() const override;

    private:
      /*!
       * \brief The pyramid level that is \a zoom.
       *
       * Found by how many tiles the level spans rather than by its
       * identifier or its position: identifiers are not always the zoom
       * number, and matching on tile count is exact where matching on
       * scale would be a floating-point comparison.
       *
       * \param zoom Slippy-map zoom level.
       * \returns The level, or nullptr when the pyramid has no such level.
       */
      [[nodiscard]] const HydroCouple::Ogc::WmtsTileMatrix *matrixForZoom(
        int zoom) const;

      HydroCouple::Ogc::WmtsCapabilities m_capabilities;
      HydroCouple::Ogc::WmtsLayerInfo m_layer;
      QString m_matrixSetId;
      QString m_style;
      QString m_format;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_OGCTILESOURCE_H
