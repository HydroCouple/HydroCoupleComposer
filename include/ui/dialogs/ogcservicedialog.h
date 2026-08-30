/*!
 * \file   ogcservicedialog.h
 * \author Caleb Buahin
 * \brief  OgcServiceDialog — adding a web map service as a basemap.
 *
 * The user pastes an address; everything else is asked of the server. Which
 * service it is, which version it speaks, which layers it has and which of
 * them can actually be drawn on this map are all answers, not questions —
 * a dialog that made the user classify their own URL would be asking them
 * to read the capabilities document themselves.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_DIALOGS_OGCSERVICEDIALOG_H
#define HYDROCOUPLECOMPOSER_UI_DIALOGS_OGCSERVICEDIALOG_H

#include "layers/ogctilesource.h"
#include "layers/wfsfeaturelayer.h"

#include <hydrocoupleogc/httpclient.h>
#include <hydrocoupleogc/servicecredentials.h>
#include <hydrocoupleogc/servicediscovery.h>
#include <hydrocoupleogc/wfscapabilities.h>
#include <hydrocoupleogc/wmscapabilities.h>
#include <hydrocoupleogc/wmtscapabilities.h>

#include <QDialog>
#include <QRectF>
#include <QString>

#include <memory>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace HydroCouple::Composer
{

  /*!
   * \brief Connects to a WMS or WMTS and picks a layer off it.
   */
  class OgcServiceDialog : public QDialog
  {
      Q_OBJECT

    public:
      explicit OgcServiceDialog(QWidget *parent = nullptr);

      ~OgcServiceDialog() override;

      /*!
       * \brief Asks the service at the address now entered what it offers.
       *
       * Returns as soon as the request is made; the layer list fills in
       * when the answer arrives. Public because it is what the Connect
       * button does, and a test should press the button rather than
       * reach past it.
       */
      void connectToService();

      /*!
       * \brief Which kind of service was found at that address.
       */
      [[nodiscard]] HydroCouple::Ogc::ServiceKind serviceKind() const;

      /*!
       * \brief The ground a feature request should be limited to.
       *
       * Longitude and latitude. A feature service holds a country and a
       * model needs one catchment, so without this the request is limited
       * only by a feature count and returns an arbitrary few thousand from
       * wherever the service starts counting.
       *
       * \param bounds The map's current extent, or a null rectangle.
       */
      void setPreferredExtent(const QRectF &bounds);

      /*!
       * \brief A source for the map layer now chosen.
       *
       * \returns The source, or nullptr when nothing usable is chosen or
       *          the choice is a feature collection. The caller takes
       *          ownership.
       */
      [[nodiscard]] std::unique_ptr<OgcTileSource> createSource() const;

      /*!
       * \brief The features fetched for the chosen collection.
       *
       * Filled before the dialog accepts, because a feature request is
       * the one thing here that can fail after the user has chosen: the
       * collection exists and the request is well formed and the service
       * may still answer that it holds nothing over that ground. Failing
       * before the dialog closes lets it say so where the user is looking.
       *
       * \returns The layer, or nullptr. The caller takes ownership.
       */
      [[nodiscard]] std::unique_ptr<WfsFeatureLayer> takeFeatureLayer();

      //! What to call the layer this dialog would add.
      [[nodiscard]] QString layerName() const;

      //! What the service says it is, once connected.
      [[nodiscard]] QString serviceTitle() const;

      //! What went wrong, or how many layers were found.
      [[nodiscard]] QString status() const;

    private:
      //! One row of the list: everything needed to build its source.
      struct Choice
      {
          HydroCouple::Ogc::ServiceKind kind =
            HydroCouple::Ogc::ServiceKind::Unknown;

          QString layerId;
          QString title;

          //! WMTS only: which pyramid this row is for.
          QString matrixSetId;

          //! Empty when the layer can be drawn.
          QString unusableReason;
      };

      void ask(HydroCouple::Ogc::ServiceKind kind);

      //! Fetches the chosen collection, then accepts.
      void fetchFeaturesThenAccept();

      void showWfs(const QByteArray &body);

      void onCapabilities(HydroCouple::Ogc::ServiceKind asked,
                          const QByteArray &body, const QString &error);

      void showWms(const QByteArray &body);

      void showWmts(const QByteArray &body);

      void fill();

      [[nodiscard]] HydroCouple::Ogc::ServiceCredentials credentials() const;

      QLineEdit *m_url = nullptr;
      QLineEdit *m_username = nullptr;
      QLineEdit *m_password = nullptr;
      QPushButton *m_connect = nullptr;
      QListWidget *m_layers = nullptr;
      QLabel *m_status = nullptr;
      QDialogButtonBox *m_buttons = nullptr;

      HydroCouple::Ogc::HttpClient *m_client = nullptr;
      HydroCouple::Ogc::WmsCapabilities m_wms;
      HydroCouple::Ogc::WmtsCapabilities m_wmts;
      HydroCouple::Ogc::WfsCapabilities m_wfs;
      HydroCouple::Ogc::ServiceKind m_kind =
        HydroCouple::Ogc::ServiceKind::Unknown;
      QRectF m_preferredExtent;
      std::unique_ptr<WfsFeatureLayer> m_featureLayer;
      QList<Choice> m_choices;
      QString m_statusText;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_DIALOGS_OGCSERVICEDIALOG_H
