/*!
 * \file   networktilesource.h
 * \author Caleb Buahin
 * \brief  NetworkTileSource — tiles over HTTP, and the built-in basemaps.
 *
 * Holds a memory cache in front of Qt's disk cache: panning back over ground
 * already covered must not re-decode a PNG, let alone re-fetch it. Requests
 * in flight are tracked so the same tile is not asked for twice, and are
 * abandoned wholesale when the view moves on.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_NETWORKTILESOURCE_H
#define HYDROCOUPLECOMPOSER_LAYERS_NETWORKTILESOURCE_H

#include "layers/tilelayer.h"

#include <QCache>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace HydroCouple::Composer
{

  /*!
   * \brief One named tile provider.
   */
  struct BasemapProvider
  {
      QString name;
      QString urlTemplate;  //!< With {z}, {x} and {y} placeholders.
      QString attribution;
      int maximumZoom = 19;
  };

  /*!
   * \brief Tiles fetched over HTTP.
   */
  class NetworkTileSource : public QObject, public ITileSource
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs a source for \a provider.
       * \param provider The tile provider to read from.
       * \param parent Owning object.
       */
      explicit NetworkTileSource(const BasemapProvider &provider,
                                 QObject *parent = nullptr);

      ~NetworkTileSource() override;

      /*!
       * \brief Sets what to call when a requested tile arrives.
       * \param callback Invoked on the GUI thread, once per tile.
       */
      void setTileReadyCallback(std::function<void()> callback);

      // ── ITileSource ──────────────────────────────────────────────────────

      [[nodiscard]] QImage tile(const TileId &tile) override;

      void request(const TileId &tile) override;

      void cancelPending() override;

      [[nodiscard]] QString attribution() const override;

      [[nodiscard]] int maximumZoom() const override;

      /*!
       * \brief The provider this source reads from.
       */
      [[nodiscard]] const BasemapProvider &provider() const;

      /*!
       * \brief The URL for one tile.
       * \param provider Provider whose template to fill in.
       * \param tile The tile wanted.
       */
      [[nodiscard]] static QString urlFor(const BasemapProvider &provider,
                                          const TileId &tile);

      /*!
       * \brief The built-in providers, in menu order.
       */
      [[nodiscard]] static QVector<BasemapProvider> builtinProviders();

      /*!
       * \brief A built-in provider by name; the first one when unknown.
       * \param name Provider name.
       */
      [[nodiscard]] static BasemapProvider builtinProvider(const QString &name);

    private:
      void onReplyFinished(QNetworkReply *reply, const TileId &tile);

      BasemapProvider m_provider;
      QNetworkAccessManager *m_network = nullptr;

      //! Decoded tiles, so a pan back over covered ground costs nothing.
      QCache<QString, QImage> m_cache;

      QSet<QString> m_inFlight;
      QHash<QNetworkReply *, TileId> m_replies;
      std::function<void()> m_tileReady;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_NETWORKTILESOURCE_H
