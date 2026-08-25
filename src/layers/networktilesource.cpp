#include "layers/networktilesource.h"

#include "core/composerapplication.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Decoded tiles held in memory. 512 tiles is a few screens' worth.
    constexpr int kMemoryCacheTiles = 512;

    //! Disk cache ceiling. Tiles are small; a generous cache spares the
    //! provider's servers, which free basemaps ask of every client.
    constexpr qint64 kDiskCacheBytes = 256LL * 1024 * 1024;

    QString cacheKey(const TileId &tile)
    {
      return QStringLiteral("%1/%2/%3").arg(tile.zoom).arg(tile.x).arg(tile.y);
    }
  }

  NetworkTileSource::NetworkTileSource(const BasemapProvider &provider,
                                       QObject *parent)
    : QObject(parent), m_provider(provider), m_cache(kMemoryCacheTiles)
  {
    m_network = new QNetworkAccessManager(this);

    auto *disk = new QNetworkDiskCache(this);
    disk->setCacheDirectory(
      QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("basemap-tiles")));
    disk->setMaximumCacheSize(kDiskCacheBytes);

    m_network->setCache(disk);
  }

  NetworkTileSource::~NetworkTileSource()
  {
    cancelPending();
  }

  void NetworkTileSource::setTileReadyCallback(std::function<void()> callback)
  {
    m_tileReady = std::move(callback);
  }

  QImage NetworkTileSource::tile(const TileId &tile)
  {
    const QString key = cacheKey(tile);

    if (const QImage *cached = m_cache.object(key))
    {
      return *cached;
    }

    return {};
  }

  void NetworkTileSource::request(const TileId &tile)
  {
    const QString key = cacheKey(tile);

    // Already asked for. Without this the same tile is fetched once per
    // repaint for as long as it takes to arrive.
    if (m_inFlight.contains(key))
    {
      return;
    }

    QNetworkRequest request{QUrl(urlFor(m_provider, tile))};

    // Every free tile provider's usage policy requires an identifying agent,
    // and OpenStreetMap's serves an error page without one.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("HydroCoupleComposer/%1")
                        .arg(ComposerApplication::versionString()));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);

    QNetworkReply *reply = m_network->get(request);

    m_inFlight.insert(key);
    m_replies.insert(reply, tile);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, tile] { onReplyFinished(reply, tile); });
  }

  void NetworkTileSource::cancelPending()
  {
    const QHash<QNetworkReply *, TileId> pending = m_replies;

    m_replies.clear();
    m_inFlight.clear();

    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it)
    {
      // Disconnected first: abort() emits finished(), and the handler would
      // otherwise run against the bookkeeping just cleared.
      it.key()->disconnect(this);
      it.key()->abort();
      it.key()->deleteLater();
    }
  }

  void NetworkTileSource::onReplyFinished(QNetworkReply *reply,
                                          const TileId &tile)
  {
    const QString key = cacheKey(tile);

    m_inFlight.remove(key);
    m_replies.remove(reply);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
      return;
    }

    QImage image;

    // A provider that is rate-limiting or out of tiles answers with an HTML
    // error page, which is a successful reply carrying no image.
    if (!image.loadFromData(reply->readAll()))
    {
      return;
    }

    m_cache.insert(key, new QImage(image));

    if (m_tileReady)
    {
      m_tileReady();
    }
  }

  QString NetworkTileSource::attribution() const
  {
    return m_provider.attribution;
  }

  int NetworkTileSource::maximumZoom() const
  {
    return m_provider.maximumZoom;
  }

  const BasemapProvider &NetworkTileSource::provider() const
  {
    return m_provider;
  }

  QString NetworkTileSource::urlFor(const BasemapProvider &provider,
                                    const TileId &tile)
  {
    QString url = provider.urlTemplate;

    url.replace(QLatin1String("{z}"), QString::number(tile.zoom));
    url.replace(QLatin1String("{x}"), QString::number(tile.x));
    url.replace(QLatin1String("{y}"), QString::number(tile.y));

    return url;
  }

  QVector<BasemapProvider> NetworkTileSource::builtinProviders()
  {
    // Only providers whose terms allow this use, each with the attribution
    // those terms require. The attribution is drawn on the map, not buried
    // in an about box.
    return {
      {QStringLiteral("OpenStreetMap"),
       QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png"),
       QStringLiteral("© OpenStreetMap contributors"), 19},
      {QStringLiteral("Carto Light"),
       QStringLiteral(
         "https://basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png"),
       QStringLiteral("© OpenStreetMap contributors © CARTO"), 20},
      {QStringLiteral("Carto Dark"),
       QStringLiteral("https://basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png"),
       QStringLiteral("© OpenStreetMap contributors © CARTO"), 20},
      {QStringLiteral("OpenTopoMap"),
       QStringLiteral("https://tile.opentopomap.org/{z}/{x}/{y}.png"),
       QStringLiteral("© OpenStreetMap contributors, SRTM · © OpenTopoMap"),
       17}};
  }

  BasemapProvider NetworkTileSource::builtinProvider(const QString &name)
  {
    const QVector<BasemapProvider> providers = builtinProviders();

    for (const BasemapProvider &provider : providers)
    {
      if (provider.name == name)
      {
        return provider;
      }
    }

    return providers.first();
  }

} // namespace HydroCouple::Composer
