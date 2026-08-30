/*!
 * \file   test_ogctilesource.cpp
 * \brief  A WMS and a WMTS, drawn as a slippy map.
 *
 * The capabilities documents are real ones, saved. The one gate that
 * actually fetches talks to an HTTP server running inside this process on
 * the loopback interface — a basemap test that needed a public server would
 * be a test that fails for reasons having nothing to do with this code, as
 * tilelayer.h says of its own fake source.
 */

#include "core/composerapplication.h"
#include "layers/ogctilesource.h"
#include "map/tilegrid.h"

#include <hydrocoupleogc/wmscapabilities.h>
#include <hydrocoupleogc/wmtscapabilities.h>

#include <gtest/gtest.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

using namespace HydroCouple::Composer;
using namespace HydroCouple::Ogc;

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

  WmsCapabilities wms()
  {
    return parseWmsCapabilities(
      fixture(QStringLiteral("wms-1.3.0-nested.xml")));
  }

  WmtsCapabilities wmts()
  {
    return parseWmtsCapabilities(
      fixture(QStringLiteral("wmts-1.0.0-resourceurl.xml")));
  }

  QString parameter(const QString &url, const QString &name)
  {
    return QUrlQuery(QUrl(url).query()).queryItemValue(name);
  }

  //! A small PNG, as a server would send one.
  QByteArray pngBytes()
  {
    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(Qt::green);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    return bytes;
  }

  /*!
   * \brief An HTTP server in this process, answering every request the same.
   */
  class LocalServer : public QTcpServer
  {
    public:
      LocalServer() { listen(QHostAddress::LocalHost, 0); }

      void reply(const QByteArray &contentType, const QByteArray &body)
      {
        m_contentType = contentType;
        m_body = body;
      }

      [[nodiscard]] QString endpoint() const
      {
        return QStringLiteral("http://127.0.0.1:%1/wms").arg(serverPort());
      }

      [[nodiscard]] int requestCount() const { return m_requests; }

    protected:
      void incomingConnection(qintptr handle) override
      {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        QObject::connect(socket, &QTcpSocket::readyRead, socket,
                         [this, socket]() {
                           m_buffers[socket].append(socket->readAll());

                           if (!m_buffers[socket].contains("\r\n\r\n"))
                           {
                             return;
                           }

                           m_buffers.remove(socket);
                           ++m_requests;

                           QByteArray response = "HTTP/1.1 200 OK\r\n";
                           response += "Content-Type: " + m_contentType
                                       + "\r\n";
                           response += "Content-Length: "
                                       + QByteArray::number(m_body.size())
                                       + "\r\n";
                           response += "Connection: close\r\n\r\n";
                           response += m_body;

                           socket->write(response);
                           socket->flush();
                           socket->disconnectFromHost();
                         });

        QObject::connect(socket, &QTcpSocket::disconnected, socket,
                         [socket]() { socket->deleteLater(); });
      }

    private:
      QByteArray m_contentType = "image/png";
      QByteArray m_body = "not an image";
      int m_requests = 0;
      QMap<QTcpSocket *, QByteArray> m_buffers;
  };

  bool waitFor(const std::function<bool()> &done, int milliseconds = 4000)
  {
    QElapsedTimer timer;
    timer.start();

    while (!done() && timer.elapsed() < milliseconds)
    {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }

    return done();
  }
}

// ── WMS ─────────────────────────────────────────────────────────────────────

TEST(WmsTileSourceTest, EachTileOfTheGridBecomesOneRequestForThatGround)
{
  const WmsCapabilities capabilities = wms();
  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();

  WmsTileSource source(capabilities, QStringList{QStringLiteral("OSM-WMS")});

  ASSERT_TRUE(source.isUsable()) << source.reason().toStdString();

  // The north-west tile of zoom 1: the top-left quarter of the world.
  const QString url = source.urlFor(TileId{1, 0, 0});

  ASSERT_FALSE(url.isEmpty());
  EXPECT_EQ(parameter(url, QStringLiteral("LAYERS")),
            QStringLiteral("OSM-WMS"));
  EXPECT_EQ(parameter(url, QStringLiteral("WIDTH")),
            QString::number(TileGrid::kTileSize));

  const QStringList box =
    parameter(url, QStringLiteral("BBOX")).split(QLatin1Char(','));

  ASSERT_EQ(box.size(), 4);
  EXPECT_NEAR(box.at(0).toDouble(), -TileGrid::kWorldHalfSpan, 1.0);
  EXPECT_NEAR(box.at(1).toDouble(), 0.0, 1.0);
  EXPECT_NEAR(box.at(2).toDouble(), 0.0, 1.0);
  EXPECT_NEAR(box.at(3).toDouble(), TileGrid::kWorldHalfSpan, 1.0)
    << "the tile's ground is not the north-west quarter of the world";

  // A basemap is what everything else is drawn over, so it is the one layer
  // that wants an opaque background.
  EXPECT_EQ(parameter(url, QStringLiteral("TRANSPARENT")),
            QStringLiteral("FALSE"));
}

TEST(WmsTileSourceTest, TheServersOwnSpellingOfWebMercatorIsWhatIsAskedFor)
{
  WmsCapabilities capabilities = wms();
  ASSERT_TRUE(capabilities.ok);

  // A server that lists the projection only as a URN. The request has to
  // echo a spelling the server itself published, not a spelling this
  // program prefers.
  for (WmsLayerInfo &layer : capabilities.layers)
  {
    if (layer.name == QLatin1String("OSM-WMS"))
    {
      layer.crsIdentifiers =
        QStringList{QStringLiteral("urn:ogc:def:crs:EPSG::3857"),
                    QStringLiteral("EPSG:4326")};
    }
  }

  WmsTileSource source(capabilities, QStringList{QStringLiteral("OSM-WMS")});

  ASSERT_TRUE(source.isUsable()) << source.reason().toStdString();
  EXPECT_EQ(parameter(source.urlFor(TileId{1, 0, 0}), QStringLiteral("CRS")),
            QStringLiteral("urn:ogc:def:crs:EPSG::3857"));

  // The names the same projection has gone by are all recognised.
  WmsLayerInfo google;
  google.crsIdentifiers = QStringList{QStringLiteral("EPSG:900913")};
  EXPECT_EQ(WmsTileSource::webMercatorSpelling(google),
            QStringLiteral("EPSG:900913"));
}

TEST(WmsTileSourceTest, AServiceThatCannotDrawWebMercatorSaysSoRatherThanDraws)
{
  WmsCapabilities capabilities = wms();
  ASSERT_TRUE(capabilities.ok);

  for (WmsLayerInfo &layer : capabilities.layers)
  {
    if (layer.name == QLatin1String("OSM-WMS"))
    {
      layer.crsIdentifiers = QStringList{QStringLiteral("EPSG:4326")};
    }
  }

  const WmsTileSource geographic(capabilities,
                                 QStringList{QStringLiteral("OSM-WMS")});

  // Asking for a Web Mercator box from a server that does not offer it
  // gets an exception report per tile, or worse, a map of somewhere else.
  EXPECT_FALSE(geographic.isUsable());
  EXPECT_TRUE(geographic.reason().contains(QStringLiteral("OSM-WMS")))
    << geographic.reason().toStdString();
  EXPECT_TRUE(geographic.urlFor(TileId{1, 0, 0}).isEmpty());

  const WmsTileSource unknown(wms(),
                              QStringList{QStringLiteral("no-such-layer")});

  EXPECT_FALSE(unknown.isUsable());
  EXPECT_TRUE(unknown.reason().contains(QStringLiteral("no-such-layer")));
}

// ── WMTS ────────────────────────────────────────────────────────────────────

TEST(WmtsTileSourceTest, ATileIsAskedForByItsLevelRowAndColumn)
{
  const WmtsCapabilities capabilities = wmts();
  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();

  const WmtsTileSource source(capabilities,
                              QStringLiteral("geolandbasemap"),
                              QStringLiteral("google3857"));

  ASSERT_TRUE(source.isUsable()) << source.reason().toStdString();

  // Row counts from the north in both schemes, so the slippy y is the row
  // as it stands and no flip belongs anywhere.
  const QString url = source.urlFor(TileId{10, 558, 357});

  ASSERT_FALSE(url.isEmpty());
  EXPECT_TRUE(url.endsWith(QStringLiteral("/10/357/558.png")))
    << url.toStdString();

  // And through the published template rather than a query, which is what
  // reaches the server's pool of hosts.
  EXPECT_FALSE(url.contains(QStringLiteral("REQUEST=GetTile")))
    << url.toStdString();
}

TEST(WmtsTileSourceTest, AZoomThePyramidDoesNotHaveIsNotAskedFor)
{
  const WmtsCapabilities capabilities = wmts();
  ASSERT_TRUE(capabilities.ok);

  const WmtsTileSource source(capabilities,
                              QStringLiteral("geolandbasemap"),
                              QStringLiteral("google3857"));

  ASSERT_TRUE(source.isUsable());

  // The pyramid has twenty-one levels, so the deepest is twenty.
  EXPECT_EQ(source.maximumZoom(), 20);
  EXPECT_TRUE(source.urlFor(TileId{21, 0, 0}).isEmpty())
    << "a level the server does not publish was asked for anyway";
}

TEST(WmtsTileSourceTest, APyramidOnAForeignGridIsRefusedRatherThanDrawnWrong)
{
  WmtsCapabilities capabilities = wmts();
  ASSERT_TRUE(capabilities.ok);

  // The same service, in the same projection, with 512-pixel tiles. Every
  // tile of it exists and every one of them is somewhere else than a
  // slippy-map client would put it.
  for (WmtsTileMatrixSet &set : capabilities.matrixSets)
  {
    for (WmtsTileMatrix &matrix : set.matrices)
    {
      matrix.tileWidth = 512;
      matrix.tileHeight = 512;
    }
  }

  const WmtsTileSource source(capabilities,
                              QStringLiteral("geolandbasemap"),
                              QStringLiteral("google3857"));

  EXPECT_FALSE(source.isUsable());
  EXPECT_FALSE(source.reason().isEmpty());
  EXPECT_TRUE(source.urlFor(TileId{10, 558, 357}).isEmpty());

  const WmtsTileSource wrongSet(wmts(), QStringLiteral("geolandbasemap"),
                                QStringLiteral("no-such-pyramid"));

  EXPECT_FALSE(wrongSet.isUsable());
}

// ── fetching ────────────────────────────────────────────────────────────────

namespace
{
  /*!
   * \brief The fetching gates, which need an event loop to fetch on.
   */
  class OgcTileSourceTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_ogctilesource";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static inline ComposerApplication *s_app = nullptr;
  };
}

TEST_F(OgcTileSourceTest, ATileArrivesOnceAndIsThenToHand)
{
  LocalServer server;
  server.reply("image/png", pngBytes());

  WmsCapabilities capabilities = wms();
  ASSERT_TRUE(capabilities.ok);
  capabilities.getMapUrl = server.endpoint();

  WmsTileSource source(capabilities, QStringList{QStringLiteral("OSM-WMS")});
  ASSERT_TRUE(source.isUsable()) << source.reason().toStdString();

  source.setAttribution(QStringLiteral("© Example"));

  int ready = 0;
  source.setTileReadyCallback([&] { ++ready; });

  const TileId tile{2, 1, 1};

  // Nothing to hand yet, and the layer must not have been made to wait.
  EXPECT_TRUE(source.tile(tile).isNull());

  source.request(tile);
  source.request(tile);

  ASSERT_TRUE(waitFor([&] { return ready > 0; }));

  EXPECT_FALSE(source.tile(tile).isNull())
    << "the tile arrived and was not kept";
  EXPECT_EQ(server.requestCount(), 1)
    << "the same tile was asked for more than once";
  EXPECT_EQ(source.attribution(), QStringLiteral("© Example"));

  // Asking again for something already held costs nothing.
  source.request(tile);
  waitFor([&] { return server.requestCount() > 1; }, 200);

  EXPECT_EQ(server.requestCount(), 1);
}

TEST_F(OgcTileSourceTest, ATileTheServiceRefusesIsNotAskedForEveryFrame)
{
  LocalServer server;

  // What a WMS sends when it will not draw something: an exception report,
  // under a 200 and often with an image content type.
  server.reply("image/png",
               "<ServiceExceptionReport><ServiceException>Layer not "
               "defined</ServiceException></ServiceExceptionReport>");

  WmsCapabilities capabilities = wms();
  ASSERT_TRUE(capabilities.ok);
  capabilities.getMapUrl = server.endpoint();

  WmsTileSource source(capabilities, QStringList{QStringLiteral("OSM-WMS")});
  ASSERT_TRUE(source.isUsable());

  int ready = 0;
  source.setTileReadyCallback([&] { ++ready; });

  const TileId tile{2, 1, 1};

  source.request(tile);

  // Waited out rather than raced: until the refusal has been read, asking
  // again is skipped as already in flight, and the gate would be about
  // that instead.
  ASSERT_TRUE(waitFor([&] { return source.pendingCount() == 0; }));
  ASSERT_EQ(server.requestCount(), 1);

  // The layer asks again on every repaint. A refusal that is not remembered
  // is a request per frame, forever.
  for (int attempt = 0; attempt < 5; ++attempt)
  {
    source.request(tile);
  }

  waitFor([&] { return server.requestCount() > 1; }, 300);

  EXPECT_EQ(server.requestCount(), 1)
    << "a tile the service refused was asked for again";
  EXPECT_TRUE(source.tile(tile).isNull());

  // And nothing was announced as arrived: a repaint for a tile that is not
  // there is a repaint for nothing.
  EXPECT_EQ(ready, 0)
    << "the layer was told a tile had arrived when none had";
}
