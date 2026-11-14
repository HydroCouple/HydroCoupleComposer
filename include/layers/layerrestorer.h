/*!
 * \file   layerrestorer.h
 * \author Caleb Buahin
 * \brief  LayerRestorer — rebuilding the layers a composition was saved with.
 *
 * The counterpart to MapLayer::persistentState(). A saved layer is a recipe
 * rather than a copy: a file path, or a service address and the name of one
 * thing on it. Rebuilding a file layer is a function call; rebuilding a
 * service layer is a conversation, because an OGC tile source is constructed
 * from a capabilities document and the recipe holds only the address it came
 * from. So layers arrive one at a time as they become available, in no
 * guaranteed order, and some may never arrive at all — a service can be down,
 * or moved, or now want a password.
 *
 * A layer that fails to come back is reported and skipped. Reopening a
 * composition must not fail because a basemap did.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_LAYERRESTORER_H
#define HYDROCOUPLECOMPOSER_LAYERS_LAYERRESTORER_H

#include <hydrocoupleogc/httpclient.h>
#include <hydrocoupleogc/wcscapabilities.h>
#include <hydrocoupleogc/wfscapabilities.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

namespace HydroCouple::Composer
{
  class MapLayer;

  /*!
   * \brief Rebuilds saved layers, fetching what has to be fetched.
   */
  class LayerRestorer : public QObject
  {
      Q_OBJECT

    public:
      explicit LayerRestorer(QObject *parent = nullptr);

      ~LayerRestorer() override;

      //! Called once per layer that came back. The caller takes ownership.
      using LayerReady = std::function<void(MapLayer *)>;

      //! Called once per layer that did not, with what to tell the user.
      using LayerFailed = std::function<void(const QString &)>;

      /*!
       * \brief Rebuilds every layer in \a layers.
       *
       * Returns as soon as the file-backed ones are built and the requests
       * for the rest are made. Order is not preserved: a file layer is ready
       * before a service has answered, and two services answer in whatever
       * order they answer.
       *
       * \param layers What was saved.
       * \param onLayer  Called for each layer rebuilt.
       * \param onFailed Called for each that could not be.
       */
      void restore(const QJsonArray &layers, const LayerReady &onLayer,
                   const LayerFailed &onFailed);

      /*!
       * \brief Whether anything is still being waited for.
       *
       * For tests, and for a caller that wants to know when the map is
       * finally as it was saved.
       */
      [[nodiscard]] int pendingCount() const;

    private:
      void restoreOne(const QJsonObject &entry, const LayerReady &onLayer,
                      const LayerFailed &onFailed);

      [[nodiscard]] MapLayer *restoreFile(const QJsonObject &entry,
                                          QString &message) const;

      void restoreService(const QJsonObject &entry, const LayerReady &onLayer,
                          const LayerFailed &onFailed);

      void fetchFeatures(const HydroCouple::Ogc::WfsCapabilities &capabilities,
                         const QJsonObject &entry, const LayerReady &onLayer,
                         const LayerFailed &onFailed);

      void describeThenFetchCoverage(
        const HydroCouple::Ogc::WcsCapabilities &capabilities,
        const QJsonObject &entry, const LayerReady &onLayer,
        const LayerFailed &onFailed);

      HydroCouple::Ogc::HttpClient *m_client = nullptr;
      int m_pending = 0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_LAYERRESTORER_H
