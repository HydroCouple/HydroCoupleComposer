#include "ui/dialogs/ogcservicedialog.h"

#include "gis/spatialreference.h"

#include <hydrocoupleogc/crsurn.h>
#include <hydrocoupleogc/servicediscovery.h>

#include <hydrocoupleogc/wcsrequest.h>
#include <hydrocoupleogc/wfsrequest.h>

#include <QDialogButtonBox>
#include <QJsonArray>
#include <QFormLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{
  using HydroCouple::Ogc::HttpResponse;
  using HydroCouple::Ogc::ServiceKind;

  OgcServiceDialog::OgcServiceDialog(QWidget *parent)
    : QDialog(parent), m_client(new HydroCouple::Ogc::HttpClient(this))
  {
    setWindowTitle(tr("Add Web Map Service"));
    setObjectName(QStringLiteral("ogcServiceDialog"));

    m_url = new QLineEdit(this);
    m_url->setObjectName(QStringLiteral("serviceUrlEdit"));
    m_url->setPlaceholderText(
      tr("https://example.org/geoserver/wms — a service address, or a tile "
         "template with {z}/{x}/{y} in it"));

    m_username = new QLineEdit(this);
    m_username->setObjectName(QStringLiteral("serviceUsernameEdit"));

    m_password = new QLineEdit(this);
    m_password->setObjectName(QStringLiteral("servicePasswordEdit"));
    m_password->setEchoMode(QLineEdit::Password);

    auto *form = new QFormLayout;
    form->addRow(tr("&Address"), m_url);
    form->addRow(tr("&User name"), m_username);
    form->addRow(tr("&Password"), m_password);

    m_connect = new QPushButton(tr("&Connect"), this);
    m_connect->setObjectName(QStringLiteral("serviceConnectButton"));

    m_layers = new QListWidget(this);
    m_layers->setObjectName(QStringLiteral("serviceLayerList"));

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("serviceStatusLabel"));
    m_status->setWordWrap(true);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                       | QDialogButtonBox::Cancel,
                                     this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Add Basemap"));
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_connect);
    layout->addWidget(m_layers, 1);
    layout->addWidget(m_status);
    layout->addWidget(m_buttons);

    connect(m_connect, &QPushButton::clicked, this,
            &OgcServiceDialog::connectToService);
    connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
      if (m_kind == ServiceKind::Wfs)
      {
        fetchFeaturesThenAccept();

        return;
      }

      if (m_kind == ServiceKind::Wcs)
      {
        describeThenFetchCoverage();

        return;
      }

      accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this,
            &OgcServiceDialog::reject);

    connect(m_layers, &QListWidget::currentRowChanged, this, [this](int row) {
      const bool usable = row >= 0 && row < m_choices.size()
                          && m_choices.at(row).unusableReason.isEmpty();

      m_buttons->button(QDialogButtonBox::Ok)->setEnabled(usable);

      if (row >= 0 && row < m_choices.size() && !usable)
      {
        m_status->setText(m_choices.at(row).unusableReason);
      }
    });

    resize(560, 480);
  }

  OgcServiceDialog::~OgcServiceDialog() = default;

  HydroCouple::Ogc::ServiceCredentials OgcServiceDialog::credentials() const
  {
    HydroCouple::Ogc::ServiceCredentials credentials;
    credentials.username = m_username->text();
    credentials.password = m_password->text();

    return credentials;
  }

  void OgcServiceDialog::connectToService()
  {
    m_layers->clear();
    m_choices.clear();
    m_wms = {};
    m_wmts = {};
    m_wfs = {};
    m_kind = ServiceKind::Unknown;
    m_featureLayer.reset();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    // A tile template is not a service and has no capabilities document to
    // ask for; the placeholders say so plainly, so the address is read
    // rather than the user asked which kind they pasted.
    if (XyzTileSource::isTemplate(m_url->text()))
    {
      probeTemplate();

      return;
    }

    // WMS first, because it is the older and far commoner of the two, and
    // an address that is neither costs one request to find out.
    ask(ServiceKind::Wms);
  }

  void OgcServiceDialog::probeTemplate()
  {
    const QString address = m_url->text();
    const XyzTileSource probe(address);
    const QString url = probe.urlFor(TileId{0, 0, 0});

    if (url.isEmpty())
    {
      m_statusText = probe.reason();
      m_status->setText(m_statusText);

      return;
    }

    m_statusText = tr("Asking %1 for a tile…").arg(address);
    m_status->setText(m_statusText);

    m_client->get(
      QUrl(url), credentials(),
      [this, address](const HttpResponse &response) {
        // What came back has to be an image. A server that answers an
        // error page under a 200 — which is how a wrong template usually
        // fails — passes every other check there is.
        QImage tile;

        if (!response.error.isEmpty())
        {
          m_statusText = tr("%1 did not answer: %2")
                           .arg(address, response.error);
          m_status->setText(m_statusText);

          return;
        }

        if (!tile.loadFromData(response.body))
        {
          m_statusText =
            tr("%1 answered, but not with a tile — check the address.")
              .arg(address);
          m_status->setText(m_statusText);

          return;
        }

        Choice choice;
        choice.xyz = true;
        choice.title = QUrl(address).host();

        if (choice.title.isEmpty())
        {
          choice.title = address;
        }

        m_choices.append(choice);
        fill();

        // fill() counts layers a service published; this one answered with
        // a tile, which is the whole of what there was to find out.
        m_statusText = tr("%1 serves tiles: %2 by %3.")
                         .arg(choice.title)
                         .arg(tile.width())
                         .arg(tile.height());
        m_status->setText(m_statusText);
      });
  }

  void OgcServiceDialog::ask(ServiceKind kind)
  {
    const QString url =
      HydroCouple::Ogc::buildCapabilitiesUrl(m_url->text(), kind);

    if (url.isEmpty())
    {
      m_statusText = tr("That is not a web address.");
      m_status->setText(m_statusText);

      return;
    }

    m_statusText = tr("Asking %1…").arg(m_url->text());
    m_status->setText(m_statusText);

    m_client->get(QUrl(url), credentials(),
                  [this, kind](const HttpResponse &response) {
                    onCapabilities(kind, response.body, response.error);
                  });
  }

  void OgcServiceDialog::onCapabilities(ServiceKind asked,
                                        const QByteArray &body,
                                        const QString &error)
  {
    const ServiceKind kind = HydroCouple::Ogc::detectServiceKind(body);

    if (kind == ServiceKind::Wms)
    {
      showWms(body);

      return;
    }

    if (kind == ServiceKind::Wmts)
    {
      showWmts(body);

      return;
    }

    if (kind == ServiceKind::Wfs)
    {
      showWfs(body);

      return;
    }

    if (kind == ServiceKind::Wcs)
    {
      showWcs(body);

      return;
    }

    // Not that one, so far. A server that speaks only another of them
    // answers with a refusal, which is not an answer about what it has —
    // so it is asked again in each remaining dialect before giving up.
    if (asked == ServiceKind::Wms)
    {
      ask(ServiceKind::Wmts);

      return;
    }

    if (asked == ServiceKind::Wmts)
    {
      ask(ServiceKind::Wfs);

      return;
    }

    if (asked == ServiceKind::Wfs)
    {
      ask(ServiceKind::Wcs);

      return;
    }

    // Whatever the second attempt said is the better message: it is the
    // one that failed last, and both attempts failed the same way.
    const HydroCouple::Ogc::WmsCapabilities refusal =
      HydroCouple::Ogc::parseWmsCapabilities(body);

    m_statusText = !refusal.message.isEmpty()
                     ? refusal.message
                     : (error.isEmpty()
                          ? tr("That address is not a WMS, WMTS, WFS or WCS.")
                          : error);

    m_status->setText(m_statusText);
  }

  void OgcServiceDialog::showWms(const QByteArray &body)
  {
    m_wms = HydroCouple::Ogc::parseWmsCapabilities(body);
    m_kind = m_wms.ok ? ServiceKind::Wms : ServiceKind::Unknown;

    if (!m_wms.ok)
    {
      m_statusText = m_wms.message;
      m_status->setText(m_statusText);

      return;
    }

    for (const HydroCouple::Ogc::WmsLayerInfo &layer : m_wms.layers)
    {
      // A layer with no name is a heading in the server's own tree; it is
      // not something a GetMap can ask for.
      if (!layer.isRequestable())
      {
        continue;
      }

      Choice choice;
      choice.kind = ServiceKind::Wms;
      choice.layerId = layer.name;
      choice.title = layer.title.isEmpty() ? layer.name : layer.title;

      if (WmsTileSource::webMercatorSpelling(layer).isEmpty())
      {
        choice.unusableReason =
          tr("\"%1\" is not published in Web Mercator, which is the "
             "projection this map's tiles are drawn in.")
            .arg(choice.title);
      }

      m_choices.append(choice);
    }

    fill();
  }

  void OgcServiceDialog::showWmts(const QByteArray &body)
  {
    m_wmts = HydroCouple::Ogc::parseWmtsCapabilities(body);
    m_kind = m_wmts.ok ? ServiceKind::Wmts : ServiceKind::Unknown;

    if (!m_wmts.ok)
    {
      m_statusText = m_wmts.message;
      m_status->setText(m_statusText);

      return;
    }

    for (const HydroCouple::Ogc::WmtsLayerInfo &layer : m_wmts.layers)
    {
      // A layer published on several pyramids is several choices: they are
      // different grids of the same ground and only some can be drawn.
      for (const QString &setId : layer.tileMatrixSetIds)
      {
        const HydroCouple::Ogc::WmtsTileMatrixSet *set =
          m_wmts.matrixSet(setId);

        Choice choice;
        choice.kind = ServiceKind::Wmts;
        choice.layerId = layer.identifier;
        choice.matrixSetId = setId;
        choice.title = QStringLiteral("%1 — %2")
                         .arg(layer.title.isEmpty() ? layer.identifier
                                                    : layer.title,
                              setId);

        if (!set || !set->isWebMercatorQuad())
        {
          choice.unusableReason =
            tr("\"%1\" is not the standard web tile grid, and only that "
               "grid can be drawn on this map.")
              .arg(setId);
        }

        m_choices.append(choice);
      }
    }

    fill();
  }

  void OgcServiceDialog::showWfs(const QByteArray &body)
  {
    m_wfs = HydroCouple::Ogc::parseWfsCapabilities(body);
    m_kind = m_wfs.ok ? ServiceKind::Wfs : ServiceKind::Unknown;

    if (!m_wfs.ok)
    {
      m_statusText = m_wfs.message;
      m_status->setText(m_statusText);

      return;
    }

    for (const HydroCouple::Ogc::WfsFeatureType &type : m_wfs.featureTypes)
    {
      Choice choice;
      choice.kind = ServiceKind::Wfs;
      choice.layerId = type.name;
      choice.title = type.title.isEmpty() ? type.name : type.title;

      if (HydroCouple::Ogc::preferredOutputFormat(type, m_wfs.outputFormats)
            .isEmpty())
      {
        choice.unusableReason =
          tr("\"%1\" is offered only in formats this program cannot read.")
            .arg(choice.title);
      }

      m_choices.append(choice);
    }

    fill();
  }

  void OgcServiceDialog::showWcs(const QByteArray &body)
  {
    m_wcs = HydroCouple::Ogc::parseWcsCapabilities(body);
    m_kind = m_wcs.ok ? ServiceKind::Wcs : ServiceKind::Unknown;

    if (!m_wcs.ok)
    {
      m_statusText = m_wcs.message;
      m_status->setText(m_statusText);

      return;
    }

    for (const HydroCouple::Ogc::WcsCoverageSummary &summary : m_wcs.coverages)
    {
      Choice choice;
      choice.kind = ServiceKind::Wcs;
      choice.layerId = summary.identifier;
      choice.title =
        summary.title.isEmpty() ? summary.identifier : summary.title;

      // No usability check here, unlike the other three. At 2.0 a summary
      // may carry nothing but an identifier -- no extent, no system, no
      // formats -- so there is nothing yet to judge it on. Whether a
      // coverage can be asked for is answered by DescribeCoverage, and
      // that is one request per coverage rather than one per service, so
      // it is left until one is chosen.
      m_choices.append(choice);
    }

    fill();
  }

  void OgcServiceDialog::describeThenFetchCoverage()
  {
    const int row = m_layers->currentRow();

    if (row < 0 || row >= m_choices.size())
    {
      return;
    }

    const QString identifier = m_choices.at(row).layerId;

    const QString url = HydroCouple::Ogc::buildDescribeCoverageUrl(
      m_url->text(), m_wcs.version, identifier);

    if (url.isEmpty())
    {
      m_statusText = tr("That coverage cannot be asked about.");
      m_status->setText(m_statusText);

      return;
    }

    m_statusText = tr("Asking about %1…").arg(m_choices.at(row).title);
    m_status->setText(m_statusText);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    m_client->get(QUrl(url), credentials(),
                  [this](const HydroCouple::Ogc::HttpResponse &response) {
                    const HydroCouple::Ogc::WcsCoverageDescription
                      description = HydroCouple::Ogc::
                        parseWcsCoverageDescription(response.body);

                    if (!description.ok)
                    {
                      m_statusText = description.message.isEmpty()
                                       ? response.error
                                       : description.message;
                      m_status->setText(m_statusText);
                      m_buttons->button(QDialogButtonBox::Ok)
                        ->setEnabled(true);

                      return;
                    }

                    fetchCoverage(description);
                  });
  }

  void OgcServiceDialog::fetchCoverage(
    const HydroCouple::Ogc::WcsCoverageDescription &description)
  {
    const int row = m_layers->currentRow();

    if (row < 0 || row >= m_choices.size())
    {
      return;
    }

    const QString title = m_choices.at(row).title;
    const QString identifier = m_choices.at(row).layerId;

    if (!description.isTwoDimensional())
    {
      // A coverage over time or depth needs those axes pinned too, and
      // which slice is wanted is the user's question rather than one to
      // guess at. Refused with the reason rather than fetched wrongly.
      m_statusText =
        tr("\"%1\" has axes this program cannot choose between: %2.")
          .arg(title, description.axisLabels.join(QStringLiteral(", ")));
      m_status->setText(m_statusText);
      m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);

      return;
    }

    QRectF wanted = description.boundsAsRect();

    if (wanted.isEmpty())
    {
      m_statusText = tr("\"%1\" does not say where it is.").arg(title);
      m_status->setText(m_statusText);
      m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);

      return;
    }

    // The map's view, when it can be expressed in the coverage's own
    // system. Without this a national elevation model is fetched whole:
    // correct, and a great many more cells than the ground anyone is
    // looking at.
    const HydroCouple::Ogc::CrsIdentifier crs =
      HydroCouple::Ogc::parseCrsIdentifier(description.envelopeCrs);

    if (crs.isValid() && !m_preferredExtent.isNull())
    {
      QString message;

      const std::unique_ptr<SpatialReference> coverageCrs =
        SpatialReference::fromAuthority(crs.authority, crs.code.toInt(),
                                        message);
      const std::unique_ptr<SpatialReference> geographic =
        SpatialReference::fromAuthority(QStringLiteral("EPSG"), 4326, message);

      if (coverageCrs && geographic)
      {
        const std::unique_ptr<CoordinateTransform> toCoverage =
          CoordinateTransform::between(*geographic, *coverageCrs, message);

        if (toCoverage)
        {
          bool ok = true;

          const QPointF lower =
            toCoverage->transform(m_preferredExtent.topLeft(), &ok);
          const QPointF upper =
            toCoverage->transform(m_preferredExtent.bottomRight(), &ok);

          const QRectF view = QRectF(lower, upper).normalized();

          // Only when it lands on the coverage. A view somewhere else
          // entirely intersects nothing, and clipping to an empty
          // rectangle would ask for a coverage of nowhere.
          if (ok && !view.isEmpty() && wanted.intersects(view))
          {
            wanted = wanted.intersected(view);
          }
        }
      }
    }

    HydroCouple::Ogc::WcsGetCoverageRequest request;
    request.coverageId = identifier;
    request.extent = wanted;

    // Kept so the same ground is asked for when the composition is reopened.
    m_coverageExtent = wanted;

    // Bounded, because a coverage is not a picture and its native
    // resolution is whatever the survey was: half a metre over a country,
    // for the model this was written against, which is millions of cells
    // for a request nobody meant to make.
    const double aspect = wanted.height() / wanted.width();
    const int width = 1024;

    request.size =
      QSize(width, qBound(1, static_cast<int>(width * aspect), 4096));

    const QString url = HydroCouple::Ogc::buildGetCoverageUrl(
      m_url->text(), m_wcs.version, request, description);

    if (url.isEmpty())
    {
      m_statusText = tr("\"%1\" cannot be asked for.").arg(title);
      m_status->setText(m_statusText);
      m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);

      return;
    }

    m_statusText = tr("Fetching %1…").arg(title);
    m_status->setText(m_statusText);

    const QString service = m_url->text();

    m_client->get(
      QUrl(url), credentials(),
      [this, title, identifier, service, url,
       description](const HydroCouple::Ogc::HttpResponse &response) {
        QString message;

        std::unique_ptr<WcsCoverageLayer> layer =
          WcsCoverageLayer::fromResponse(response.body, title, message);

        if (!layer)
        {
          m_statusText = message.isEmpty() ? response.error : message;
          m_status->setText(m_statusText);
          m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);

          return;
        }

        layer->setServiceUrl(service);
        layer->setCoverageId(identifier);
        layer->setDescription(description);
        layer->setPersistentState(persistentStateForChoice());

        // The request that produced this coverage, so a model argument can
        // read the same bytes without repeating the conversation that chose
        // it -- capabilities, then DescribeCoverage, then this.
        layer->setSourceUri(QUrl(url));

        m_coverageLayer = std::move(layer);

        accept();
      });
  }

  void OgcServiceDialog::fetchFeaturesThenAccept()
  {
    const int row = m_layers->currentRow();

    if (row < 0 || row >= m_choices.size())
    {
      return;
    }

    const HydroCouple::Ogc::WfsFeatureType *type =
      m_wfs.featureType(m_choices.at(row).layerId);

    if (!type)
    {
      return;
    }

    HydroCouple::Ogc::WfsGetFeatureRequest request;
    request.typeName = type->name;
    request.outputFormat =
      HydroCouple::Ogc::preferredOutputFormat(*type, m_wfs.outputFormats);

    // Asked for in longitude and latitude when the collection publishes
    // them, so the ground the map is looking at can be named in the same
    // terms. A collection published in a national grid alone is fetched
    // whole, up to the feature limit, because converting the box would
    // need the projection this parser deliberately does not carry.
    request.crs = type->spellingOf(QStringLiteral("EPSG:4326"));

    if (!request.crs.isEmpty() && !m_preferredExtent.isNull())
    {
      request.extent = m_preferredExtent;
    }

    const QString url = HydroCouple::Ogc::buildGetFeatureUrl(m_wfs, request);

    if (url.isEmpty())
    {
      m_statusText = tr("That collection cannot be asked for.");
      m_status->setText(m_statusText);

      return;
    }

    m_statusText = tr("Fetching %1…").arg(m_choices.at(row).title);
    m_status->setText(m_statusText);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    const QString name = m_choices.at(row).title;
    const QString typeName = type->name;

    m_client->get(QUrl(url), credentials(),
                  [this, name, typeName, url](const HttpResponse &response) {
                    QString message;

                    m_featureLayer =
                      WfsFeatureLayer::fromResponse(response.body, name,
                                                    message);

                    if (!m_featureLayer)
                    {
                      // Said here, where the user is looking, rather than
                      // after the dialog has closed on an empty layer.
                      m_statusText = response.ok || message.isEmpty()
                                       ? message
                                       : response.error;
                      m_status->setText(m_statusText);
                      m_buttons->button(QDialogButtonBox::Ok)
                        ->setEnabled(true);

                      return;
                    }

                    m_featureLayer->setTypeName(typeName);
                    m_featureLayer->setPersistentState(
                      persistentStateForChoice());
                    m_featureLayer->setSourceUri(QUrl(url));
                    accept();
                  });
  }

  ServiceKind OgcServiceDialog::serviceKind() const
  {
    return m_kind;
  }

  void OgcServiceDialog::setPreferredExtent(const QRectF &bounds)
  {
    m_preferredExtent = bounds;
  }

  std::unique_ptr<WfsFeatureLayer> OgcServiceDialog::takeFeatureLayer()
  {
    return std::move(m_featureLayer);
  }

  std::unique_ptr<WcsCoverageLayer> OgcServiceDialog::takeCoverageLayer()
  {
    return std::move(m_coverageLayer);
  }

  void OgcServiceDialog::fill()
  {
    int usable = 0;

    for (const Choice &choice : m_choices)
    {
      auto *item = new QListWidgetItem(choice.title, m_layers);

      if (!choice.unusableReason.isEmpty())
      {
        // Shown rather than hidden: a user looking for a layer that is
        // there needs to be told why it cannot be used, not left to
        // wonder where it went.
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        item->setToolTip(choice.unusableReason);
      }
      else
      {
        ++usable;
      }
    }

    m_statusText = usable > 0
                     ? tr("%1: %2 of %3 layers can be drawn here.")
                         .arg(serviceTitle())
                         .arg(usable)
                         .arg(m_choices.size())
                     : tr("%1 has nothing this map can draw.")
                         .arg(serviceTitle());

    m_status->setText(m_statusText);

    for (int row = 0; row < m_choices.size(); ++row)
    {
      if (m_choices.at(row).unusableReason.isEmpty())
      {
        m_layers->setCurrentRow(row);

        break;
      }
    }
  }

  QString OgcServiceDialog::serviceTitle() const
  {
    if (m_wms.ok && !m_wms.title.isEmpty())
    {
      return m_wms.title;
    }

    if (m_wmts.ok && !m_wmts.title.isEmpty())
    {
      return m_wmts.title;
    }

    if (m_wfs.ok && !m_wfs.title.isEmpty())
    {
      return m_wfs.title;
    }

    return m_url->text();
  }

  QString OgcServiceDialog::status() const
  {
    return m_statusText;
  }

  QJsonObject OgcServiceDialog::persistentStateForChoice() const
  {
    const int row = m_layers->currentRow();

    if (row < 0 || row >= m_choices.size())
    {
      return {};
    }

    const Choice &choice = m_choices.at(row);

    QJsonObject state;
    state.insert(QStringLiteral("url"), m_url->text());
    state.insert(QStringLiteral("name"), choice.title);

    if (choice.xyz)
    {
      // The template is the whole recipe, so this is the one saved layer
      // that needs no request to rebuild.
      state.insert(QStringLiteral("type"), QStringLiteral("xyz"));

      return state;
    }

    if (choice.kind == HydroCouple::Ogc::ServiceKind::Wms)
    {
      state.insert(QStringLiteral("type"), QStringLiteral("wms"));
      state.insert(QStringLiteral("layers"),
                   QJsonArray{choice.layerId});

      return state;
    }

    if (choice.kind == HydroCouple::Ogc::ServiceKind::Wmts)
    {
      state.insert(QStringLiteral("type"), QStringLiteral("wmts"));
      state.insert(QStringLiteral("layer"), choice.layerId);
      state.insert(QStringLiteral("matrixSet"), choice.matrixSetId);

      return state;
    }

    if (choice.kind == HydroCouple::Ogc::ServiceKind::Wfs)
    {
      state.insert(QStringLiteral("type"), QStringLiteral("wfs"));
      state.insert(QStringLiteral("typeName"), choice.layerId);

      // The ground it was asked for over. Without it a reopened
      // composition gets a different arbitrary few thousand features.
      if (!m_preferredExtent.isNull())
      {
        state.insert(QStringLiteral("extent"),
                     QJsonArray{m_preferredExtent.left(),
                                m_preferredExtent.top(),
                                m_preferredExtent.right(),
                                m_preferredExtent.bottom()});
      }

      return state;
    }

    if (choice.kind == HydroCouple::Ogc::ServiceKind::Wcs)
    {
      state.insert(QStringLiteral("type"), QStringLiteral("wcs"));
      state.insert(QStringLiteral("coverageId"), choice.layerId);

      if (m_coverageExtent.isValid())
      {
        state.insert(QStringLiteral("extent"),
                     QJsonArray{m_coverageExtent.left(), m_coverageExtent.top(),
                                m_coverageExtent.right(),
                                m_coverageExtent.bottom()});
      }

      return state;
    }

    return {};
  }

  QString OgcServiceDialog::layerName() const
  {
    const int row = m_layers->currentRow();

    if (row < 0 || row >= m_choices.size())
    {
      return {};
    }

    return m_choices.at(row).title;
  }

  std::unique_ptr<OgcTileSource> OgcServiceDialog::createSource() const
  {
    const int row = m_layers->currentRow();

    if (row < 0 || row >= m_choices.size())
    {
      return nullptr;
    }

    const Choice &choice = m_choices.at(row);

    if (choice.kind == ServiceKind::Wfs)
    {
      // A feature collection is not a backdrop; it arrives through
      // takeFeatureLayer().
      return nullptr;
    }

    std::unique_ptr<OgcTileSource> source;

    if (choice.xyz)
    {
      source = std::make_unique<XyzTileSource>(m_url->text());
    }
    else if (choice.kind == ServiceKind::Wms)
    {
      source = std::make_unique<WmsTileSource>(
        m_wms, QStringList{choice.layerId});
    }
    else
    {
      source = std::make_unique<WmtsTileSource>(m_wmts, choice.layerId,
                                                choice.matrixSetId);
    }

    // The source is the authority on whether it can be drawn, so a stale
    // or programmatic selection of a row the list greyed out is refused
    // here too rather than trusted to have been prevented.
    if (!source->isUsable())
    {
      return nullptr;
    }

    source->setCredentials(credentials());

    // A template has no title to publish, so the host is what the map can
    // honestly attribute the tiles to — never the template itself, which
    // would print the placeholders across the corner of the map.
    source->setAttribution(choice.xyz ? choice.title : serviceTitle());

    return source;
  }

} // namespace HydroCouple::Composer
