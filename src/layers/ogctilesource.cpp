#include "layers/ogctilesource.h"

#include "map/tilegrid.h"

#include <hydrocoupleogc/crsurn.h>
#include <hydrocoupleogc/wmsrequest.h>

namespace HydroCouple::Composer
{
  using HydroCouple::Ogc::HttpClient;
  using HydroCouple::Ogc::HttpResponse;

  namespace
  {
    //! How many decoded tiles to keep. A screenful is about thirty.
    constexpr int kCacheSize = 400;
  }

  OgcTileSource::OgcTileSource(QObject *parent)
    : QObject(parent), m_client(new HttpClient(this)), m_cache(kCacheSize)
  {
  }

  void OgcTileSource::setTileReadyCallback(std::function<void()> callback)
  {
    m_tileReady = std::move(callback);
  }

  void OgcTileSource::setCredentials(
    const HydroCouple::Ogc::ServiceCredentials &credentials)
  {
    m_credentials = credentials;
  }

  void OgcTileSource::setCacheDirectory(const QString &path)
  {
    m_client->setCacheDirectory(path);
  }

  void OgcTileSource::setAttribution(const QString &attribution)
  {
    m_attribution = attribution;
  }

  QString OgcTileSource::reason() const
  {
    return m_reason;
  }

  int OgcTileSource::pendingCount() const
  {
    return int(m_pending.size());
  }

  QImage OgcTileSource::tile(const TileId &tile)
  {
    const QString url = urlFor(tile);

    if (url.isEmpty())
    {
      return {};
    }

    if (const QImage *cached = m_cache.object(url))
    {
      return *cached;
    }

    return {};
  }

  void OgcTileSource::request(const TileId &tile)
  {
    const QString url = urlFor(tile);

    if (url.isEmpty() || m_cache.contains(url) || m_pending.contains(url)
        || m_refused.contains(url))
    {
      return;
    }

    const HttpClient::RequestId id =
      m_client->get(QUrl(url), m_credentials,
                    [this, url](const HttpResponse &response) {
                      m_pending.remove(url);

                      QImage image;

                      if (response.ok)
                      {
                        image.loadFromData(response.body);
                      }

                      if (image.isNull())
                      {
                        // A service refusing a tile answers with an XML
                        // exception, often under a 200 and an image content
                        // type. Asking again would only be refused again,
                        // once per repaint.
                        m_refused.insert(url);

                        return;
                      }

                      m_cache.insert(url, new QImage(image));

                      if (m_tileReady)
                      {
                        m_tileReady();
                      }
                    });

    if (id != 0)
    {
      m_pending.insert(url, id);
    }
  }

  void OgcTileSource::cancelPending()
  {
    m_pending.clear();
    m_client->cancelAll();
  }

  QString OgcTileSource::attribution() const
  {
    return m_attribution;
  }

  // ── WMS ─────────────────────────────────────────────────────────────────

  WmsTileSource::WmsTileSource(
    const HydroCouple::Ogc::WmsCapabilities &capabilities,
    const QStringList &layers, const QString &format, QObject *parent)
    : OgcTileSource(parent),
      m_capabilities(capabilities),
      m_layers(layers),
      m_format(format)
  {
    if (!capabilities.ok)
    {
      m_reason = capabilities.message.isEmpty()
                   ? QStringLiteral("The service did not describe itself.")
                   : capabilities.message;

      return;
    }

    if (layers.isEmpty())
    {
      m_reason = QStringLiteral("No layer was chosen.");

      return;
    }

    // Every chosen layer has to be drawable in the same system, because one
    // GetMap draws them all.
    for (const QString &name : layers)
    {
      bool found = false;

      for (const HydroCouple::Ogc::WmsLayerInfo &layer : capabilities.layers)
      {
        if (layer.name != name)
        {
          continue;
        }

        found = true;
        const QString spelling = webMercatorSpelling(layer);

        if (spelling.isEmpty())
        {
          m_reason = QStringLiteral(
                       "The layer \"%1\" is not published in Web Mercator.")
                       .arg(name);

          return;
        }

        if (m_crs.isEmpty())
        {
          m_crs = spelling;
        }
      }

      if (!found)
      {
        m_reason =
          QStringLiteral("The service does not publish a layer \"%1\".")
            .arg(name);

        return;
      }
    }
  }

  bool WmsTileSource::isUsable() const
  {
    return m_reason.isEmpty() && !m_crs.isEmpty();
  }

  QString WmsTileSource::webMercatorSpelling(
    const HydroCouple::Ogc::WmsLayerInfo &layer)
  {
    // The same projection under the names servers have given it over the
    // years: the EPSG code, Google's original private code, and ESRI's.
    // The request must echo a spelling the server itself listed.
    static const QStringList codes = {QStringLiteral("3857"),
                                      QStringLiteral("900913"),
                                      QStringLiteral("102100")};

    for (const QString &code : codes)
    {
      for (const QString &advertised : layer.crsIdentifiers)
      {
        const HydroCouple::Ogc::CrsIdentifier identifier =
          HydroCouple::Ogc::parseCrsIdentifier(advertised);

        if (identifier.code == code)
        {
          return advertised;
        }
      }
    }

    return {};
  }

  QString WmsTileSource::urlFor(const TileId &tile) const
  {
    if (!isUsable() || tile.zoom > m_maximumZoom)
    {
      return {};
    }

    HydroCouple::Ogc::WmsGetMapRequest request;
    request.layers = m_layers;
    request.crs = m_crs;
    request.extent = TileGrid::tileExtent(tile);
    request.widthPixels = TileGrid::kTileSize;
    request.heightPixels = TileGrid::kTileSize;
    request.format = m_format;

    // A basemap is what everything else is drawn over, so it is the one
    // layer that does not want a transparent background.
    request.transparent = false;

    return buildGetMapUrl(m_capabilities, request);
  }

  int WmsTileSource::maximumZoom() const
  {
    return m_maximumZoom;
  }

  void WmsTileSource::setMaximumZoom(int zoom)
  {
    m_maximumZoom = zoom;
  }

  // ── WMTS ────────────────────────────────────────────────────────────────

  WmtsTileSource::WmtsTileSource(
    const HydroCouple::Ogc::WmtsCapabilities &capabilities,
    const QString &layerId, const QString &matrixSetId, const QString &style,
    const QString &format, QObject *parent)
    : OgcTileSource(parent),
      m_capabilities(capabilities),
      m_matrixSetId(matrixSetId),
      m_style(style),
      m_format(format)
  {
    if (!capabilities.ok)
    {
      m_reason = capabilities.message.isEmpty()
                   ? QStringLiteral("The service did not describe itself.")
                   : capabilities.message;

      return;
    }

    for (const HydroCouple::Ogc::WmtsLayerInfo &layer : capabilities.layers)
    {
      if (layer.identifier == layerId)
      {
        m_layer = layer;

        break;
      }
    }

    if (m_layer.identifier.isEmpty())
    {
      m_reason = QStringLiteral("The service does not publish a layer \"%1\".")
                   .arg(layerId);

      return;
    }

    if (!m_layer.tileMatrixSetIds.contains(matrixSetId))
    {
      m_reason =
        QStringLiteral("The layer \"%1\" is not published on \"%2\".")
          .arg(layerId, matrixSetId);

      return;
    }

    const HydroCouple::Ogc::WmtsTileMatrixSet *set =
      capabilities.matrixSet(matrixSetId);

    if (!set)
    {
      m_reason = QStringLiteral("The service describes no pyramid \"%1\".")
                   .arg(matrixSetId);

      return;
    }

    if (!set->isWebMercatorQuad())
    {
      m_reason = QStringLiteral(
                   "\"%1\" is not the standard web pyramid, which is the "
                   "only grid this map can draw.")
                   .arg(matrixSetId);
    }
  }

  bool WmtsTileSource::isUsable() const
  {
    return m_reason.isEmpty();
  }

  const HydroCouple::Ogc::WmtsTileMatrix *WmtsTileSource::matrixForZoom(
    int zoom) const
  {
    const HydroCouple::Ogc::WmtsTileMatrixSet *set =
      m_capabilities.matrixSet(m_matrixSetId);

    if (!set)
    {
      return nullptr;
    }

    const int across = TileGrid::tilesAcross(zoom);

    for (const HydroCouple::Ogc::WmtsTileMatrix &matrix : set->matrices)
    {
      if (matrix.matrixWidth == across && matrix.matrixHeight == across)
      {
        return &matrix;
      }
    }

    return nullptr;
  }

  QString WmtsTileSource::urlFor(const TileId &tile) const
  {
    if (!isUsable())
    {
      return {};
    }

    const HydroCouple::Ogc::WmtsTileMatrix *matrix = matrixForZoom(tile.zoom);

    if (!matrix)
    {
      return {};
    }

    // Tile row counts from the north in both schemes, so y is the row as it
    // stands.
    return buildWmtsTileUrl(m_capabilities, m_layer, m_matrixSetId,
                            matrix->identifier, tile.y, tile.x, m_style,
                            m_format);
  }

  int WmtsTileSource::maximumZoom() const
  {
    int deepest = 0;

    for (int zoom = 0; zoom <= TileGrid::kMaxZoom; ++zoom)
    {
      if (matrixForZoom(zoom))
      {
        deepest = zoom;
      }
    }

    return deepest;
  }

} // namespace HydroCouple::Composer
