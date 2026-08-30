/*!
 * \file   test_ogcservicedialog.cpp
 * \brief  Pasting an address and getting a basemap out of it.
 *
 * The dialog is driven through its own widgets — the address is typed into
 * the field and the Connect button is pressed — because what is worth
 * gating is what a user can reach, not what a private method returns. The
 * service it connects to is an HTTP server inside this process, answering
 * with real saved capabilities documents.
 */

#include "core/composerapplication.h"
#include "layers/ogctilesource.h"
#include "map/tilegrid.h"
#include "ui/dialogs/ogcservicedialog.h"

#include <gtest/gtest.h>

#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

using namespace HydroCouple::Composer;

namespace
{
  QByteArray fixture(const QString &name)
  {
    QFile file(QStringLiteral(COMPOSER_OGC_FIXTURE_DIR "/") + name);

    if (!file.open(QIODevice::ReadOnly))
    {
      return {};
    }

    return file.readAll();
  }

  /*!
   * \brief A server that answers according to what was asked of it.
   *
   * SERVICE=WMS gets the WMS document and SERVICE=WMTS the WMTS one, which
   * is what makes the dialog's two-step probe observable: a server that is
   * only one of the two refuses the other.
   */
  class ServiceServer : public QTcpServer
  {
    public:
      ServiceServer() { listen(QHostAddress::LocalHost, 0); }

      //! Which services this server pretends to offer.
      void offer(bool wms, bool wmts)
      {
        m_wms = wms;
        m_wmts = wmts;
      }

      /*!
       * \brief Publishes everything in geographic coordinates only.
       *
       * Which plenty of services do: Web Mercator is a web-map convention,
       * not something a data publisher owes anyone.
       */
      void withoutWebMercator() { m_stripWebMercator = true; }

      [[nodiscard]] QString endpoint() const
      {
        return QStringLiteral("http://127.0.0.1:%1/ows").arg(serverPort());
      }

      [[nodiscard]] QStringList asked() const { return m_asked; }

    protected:
      void incomingConnection(qintptr handle) override
      {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        QObject::connect(socket, &QTcpSocket::readyRead, socket,
                         [this, socket]() { onReadyRead(socket); });
        QObject::connect(socket, &QTcpSocket::disconnected, socket,
                         [socket]() { socket->deleteLater(); });
      }

    private:
      void onReadyRead(QTcpSocket *socket)
      {
        m_buffers[socket].append(socket->readAll());

        if (!m_buffers[socket].contains("\r\n\r\n"))
        {
          return;
        }

        const QByteArray head = m_buffers.take(socket);
        const QByteArray line = head.split('\n').value(0);
        const QString target = QString::fromUtf8(line.split(' ').value(1));
        const QString service =
          QUrlQuery(QUrl(target).query()).queryItemValue(
            QStringLiteral("SERVICE"));

        m_asked.append(service);

        QByteArray body;

        if (service == QLatin1String("WMS") && m_wms)
        {
          body = fixture(QStringLiteral("wms-1.3.0-nested.xml"));

          if (m_stripWebMercator)
          {
            body.replace("<CRS>EPSG:3857</CRS>", "");
            body.replace("<CRS>EPSG:900913</CRS>", "");
          }
        }
        else if (service == QLatin1String("WMTS") && m_wmts)
        {
          body = fixture(QStringLiteral("wmts-1.0.0-resourceurl.xml"));
        }
        else
        {
          // What a server that does not speak the dialect answers: an
          // exception report, under a 200.
          body =
            "<?xml version=\"1.0\"?><ows:ExceptionReport "
            "xmlns:ows=\"http://www.opengis.net/ows/1.1\"><ows:Exception>"
            "<ows:ExceptionText>Service not supported</ows:ExceptionText>"
            "</ows:Exception></ows:ExceptionReport>";
        }

        QByteArray response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/xml\r\n";
        response +=
          "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += body;

        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
      }

      bool m_wms = true;
      bool m_wmts = true;
      bool m_stripWebMercator = false;
      QStringList m_asked;
      QMap<QTcpSocket *, QByteArray> m_buffers;
  };

  class OgcServiceDialogTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_ogcservicedialog";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static bool waitFor(const std::function<bool()> &done,
                          int milliseconds = 4000)
      {
        QElapsedTimer timer;
        timer.start();

        while (!done() && timer.elapsed() < milliseconds)
        {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }

        return done();
      }

      //! Types \a url in and presses Connect.
      static void connectTo(OgcServiceDialog &dialog, const QString &url)
      {
        dialog.findChild<QLineEdit *>(QStringLiteral("serviceUrlEdit"))
          ->setText(url);
        dialog
          .findChild<QPushButton *>(QStringLiteral("serviceConnectButton"))
          ->click();
      }

      static QListWidget *layerList(OgcServiceDialog &dialog)
      {
        return dialog.findChild<QListWidget *>(
          QStringLiteral("serviceLayerList"));
      }

      static inline ComposerApplication *s_app = nullptr;
  };
}

TEST_F(OgcServiceDialogTest, AnAddressIsEnoughToListWhatAServiceOffers)
{
  ServiceServer server;
  server.offer(true, false);

  OgcServiceDialog dialog;
  connectTo(dialog, server.endpoint());

  ASSERT_TRUE(waitFor([&] { return layerList(dialog)->count() > 0; }))
    << dialog.status().toStdString();

  // The user said nothing about which service this is or which version it
  // speaks; both are answers, not questions.
  EXPECT_FALSE(dialog.serviceTitle().isEmpty());
  EXPECT_GT(layerList(dialog)->count(), 1);

  // Exactly the layers a GetMap can name. The headings of the server's own
  // tree inherit the root's coordinate systems, so offering them looks
  // perfectly drawable right up until the request goes out naming no
  // layer at all.
  const HydroCouple::Ogc::WmsCapabilities published =
    HydroCouple::Ogc::parseWmsCapabilities(
      fixture(QStringLiteral("wms-1.3.0-nested.xml")));

  ASSERT_TRUE(published.ok);
  EXPECT_EQ(layerList(dialog)->count(), published.requestableLayers().size())
    << "the list is not the set of layers that can be asked for";

  // And every one of them can be drawn on this map, so none is greyed out.
  for (int row = 0; row < layerList(dialog)->count(); ++row)
  {
    EXPECT_TRUE(
      layerList(dialog)->item(row)->flags().testFlag(Qt::ItemIsEnabled))
      << layerList(dialog)->item(row)->text().toStdString()
      << " was listed as undrawable";
  }

  // And something usable is already chosen, so Add is available without
  // hunting through the list first.
  EXPECT_TRUE(dialog.findChild<QDialogButtonBox *>()
                ->button(QDialogButtonBox::Ok)
                ->isEnabled());

  const std::unique_ptr<OgcTileSource> source = dialog.createSource();

  ASSERT_NE(source, nullptr) << dialog.status().toStdString();
  EXPECT_FALSE(source->urlFor(TileId{1, 0, 0}).isEmpty());
}

TEST_F(OgcServiceDialogTest, AServiceThatIsTheOtherOneIsTriedInTheOtherDialect)
{
  ServiceServer server;
  server.offer(false, true);

  OgcServiceDialog dialog;
  connectTo(dialog, server.endpoint());

  ASSERT_TRUE(waitFor([&] { return layerList(dialog)->count() > 0; }))
    << dialog.status().toStdString();

  // A WMTS refuses a WMS request rather than answering it, so an address
  // that is one and not the other has to be asked twice before it is
  // reported as neither.
  EXPECT_EQ(server.asked(),
            (QStringList{QStringLiteral("WMS"), QStringLiteral("WMTS")}));

  const std::unique_ptr<OgcTileSource> source = dialog.createSource();

  ASSERT_NE(source, nullptr) << dialog.status().toStdString();

  // The WMTS fixture publishes its layers through URL templates.
  EXPECT_TRUE(source->urlFor(TileId{10, 558, 357}).endsWith(
    QStringLiteral("/10/357/558.png")))
    << source->urlFor(TileId{10, 558, 357}).toStdString();
}

TEST_F(OgcServiceDialogTest, AnAddressThatIsNeitherSaysSoAndAddsNothing)
{
  ServiceServer server;
  server.offer(false, false);

  OgcServiceDialog dialog;
  connectTo(dialog, server.endpoint());

  ASSERT_TRUE(waitFor([&] { return server.asked().size() >= 2; }));
  ASSERT_TRUE(waitFor([&] { return !dialog.status().contains(
                                     QStringLiteral("Asking")); }));

  EXPECT_EQ(layerList(dialog)->count(), 0);
  EXPECT_EQ(dialog.createSource(), nullptr);

  // The server said why, and that is more use than this program's guess.
  EXPECT_TRUE(dialog.status().contains(QStringLiteral("not supported")))
    << dialog.status().toStdString();

  EXPECT_FALSE(dialog.findChild<QDialogButtonBox *>()
                 ->button(QDialogButtonBox::Ok)
                 ->isEnabled());
}

TEST_F(OgcServiceDialogTest, ALayerThisMapCannotDrawIsShownButNotOfferable)
{
  ServiceServer server;
  server.offer(true, false);

  // The same service published only in geographic coordinates, which is
  // how plenty of them are published: Web Mercator is a web-map
  // convention, not something a data publisher owes anyone.
  server.withoutWebMercator();

  OgcServiceDialog dialog;
  connectTo(dialog, server.endpoint());

  QListWidget *list = layerList(dialog);

  ASSERT_TRUE(waitFor([&] { return list->count() > 0; }))
    << dialog.status().toStdString();

  // Listed, every one of them: a user looking for a layer that is there
  // needs to be told why it cannot be used, not left to wonder where it
  // went.
  for (int row = 0; row < list->count(); ++row)
  {
    EXPECT_FALSE(list->item(row)->flags().testFlag(Qt::ItemIsEnabled))
      << "a layer with no Web Mercator was offered as though it could be "
         "drawn";
    EXPECT_FALSE(list->item(row)->toolTip().isEmpty())
      << "a layer was greyed out without saying why";
  }

  list->setCurrentRow(0);

  EXPECT_EQ(dialog.createSource(), nullptr);
  EXPECT_FALSE(dialog.findChild<QDialogButtonBox *>()
                 ->button(QDialogButtonBox::Ok)
                 ->isEnabled());
}

TEST_F(OgcServiceDialogTest, SomethingThatIsNotAnAddressIsNotFetched)
{
  ServiceServer server;

  OgcServiceDialog dialog;
  connectTo(dialog, QStringLiteral("this is not a url"));

  waitFor([&] { return !server.asked().isEmpty(); }, 300);

  EXPECT_TRUE(server.asked().isEmpty());
  EXPECT_EQ(layerList(dialog)->count(), 0);

  // And says so, rather than leaving "Asking…" up for a request that was
  // never made.
  EXPECT_TRUE(dialog.status().contains(QStringLiteral("not a web address")))
    << dialog.status().toStdString();
}
