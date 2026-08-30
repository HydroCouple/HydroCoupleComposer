#include "ui/dialogs/ogcservicedialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
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
      tr("https://example.org/geoserver/wms — WMS or WMTS"));

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
    connect(m_buttons, &QDialogButtonBox::accepted, this,
            &OgcServiceDialog::accept);
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
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    // WMS first, because it is the older and far commoner of the two, and
    // an address that is neither costs one request to find out.
    ask(ServiceKind::Wms);
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

    // Neither, so far. A server that only speaks the other one answers a
    // WMS request with a refusal, which is not an answer about what it
    // has — so it is asked again in the other dialect before giving up.
    if (asked == ServiceKind::Wms)
    {
      ask(ServiceKind::Wmts);

      return;
    }

    // Whatever the second attempt said is the better message: it is the
    // one that failed last, and both attempts failed the same way.
    const HydroCouple::Ogc::WmsCapabilities refusal =
      HydroCouple::Ogc::parseWmsCapabilities(body);

    m_statusText = !refusal.message.isEmpty()
                     ? refusal.message
                     : (error.isEmpty()
                          ? tr("That address is not a WMS or a WMTS.")
                          : error);

    m_status->setText(m_statusText);
  }

  void OgcServiceDialog::showWms(const QByteArray &body)
  {
    m_wms = HydroCouple::Ogc::parseWmsCapabilities(body);

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

    return m_url->text();
  }

  QString OgcServiceDialog::status() const
  {
    return m_statusText;
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
    std::unique_ptr<OgcTileSource> source;

    if (choice.kind == ServiceKind::Wms)
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
    source->setAttribution(serviceTitle());

    return source;
  }

} // namespace HydroCouple::Composer
