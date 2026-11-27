/*!
 * \file   test_httpuriresolver.cpp
 * \brief  Phase 6 verification — a model argument reading an http(s) URI.
 *
 * The SDK defines what resolving a URI means and refuses to do it; Composer
 * is the host that can. These gates cover the wiring (the application really
 * does install one), the boundary (only http and https are claimed, and a
 * file: URI never reaches it), and the two failure modes that a blocking
 * resolver adds: a refusal that loses the server's reason, and a request
 * that never starts leaving a nested event loop with nothing to quit it.
 */

#include "core/composerapplication.h"
#include "core/httpuriresolver.h"

#include "hydrocouplesdk/core/dimension.h"
#include "hydrocouplesdk/core/valuedefinition.h"
#include "hydrocouplesdk/data/argument1d.h"
#include "hydrocouplesdk/io/uriresolver.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include <filesystem>
#include <fstream>
#include <memory>

using namespace HydroCouple::Composer;
using AT = HydroCouple::IArgument::ArgumentInputType;

namespace
{
  /*!
   * \brief An HTTP server in the test process, on loopback.
   */
  class BodyServer : public QTcpServer
  {
    public:
      BodyServer() { listen(QHostAddress::LocalHost, 0); }

      void answerWith(int status, const QByteArray &body,
                      const QByteArray &contentType = "application/json",
                      const QByteArray &reason = "OK")
      {
        m_status = status;
        m_body = body;
        m_contentType = contentType;
        m_reason = reason;
      }

      [[nodiscard]] QString url(const QString &path) const
      {
        return QStringLiteral("http://127.0.0.1:%1%2")
          .arg(serverPort())
          .arg(path);
      }

      [[nodiscard]] int requestCount() const { return m_requests; }

    protected:
      void incomingConnection(qintptr handle) override
      {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        QObject::connect(socket, &QTcpSocket::readyRead, socket,
                         [this, socket] { onReadyRead(socket); });
        QObject::connect(socket, &QTcpSocket::disconnected, socket,
                         [socket] { socket->deleteLater(); });
      }

    private:
      void onReadyRead(QTcpSocket *socket)
      {
        m_buffers[socket].append(socket->readAll());

        if (!m_buffers[socket].contains("\r\n\r\n"))
        {
          return;
        }

        m_buffers.remove(socket);
        ++m_requests;

        QByteArray head =
          "HTTP/1.1 " + QByteArray::number(m_status) + " " + m_reason + "\r\n";
        head += "Content-Type: " + m_contentType + "\r\n";
        head += "Content-Length: " + QByteArray::number(m_body.size()) +
                "\r\nConnection: close\r\n\r\n";

        socket->write(head);
        socket->write(m_body);
        socket->flush();
        socket->disconnectFromHost();
      }

      QHash<QTcpSocket *, QByteArray> m_buffers;
      QByteArray m_body = "[]";
      QByteArray m_contentType = "application/json";
      QByteArray m_reason = "OK";
      int m_status = 200;
      int m_requests = 0;
  };

  class HttpUriResolverTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_httpuriresolver";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      HydroCouple::SDK::Dimension m_dim{"cell", "Cell index"};

      static ComposerApplication *s_app;
  };

  ComposerApplication *HttpUriResolverTest::s_app = nullptr;
}

// ── The wiring ────────────────────────────────────────────────────────────

// Without this the SDK refuses every https argument, and nothing else in
// this file could pass.
TEST_F(HttpUriResolverTest, TheApplicationInstallsAResolverForTheSdk)
{
  EXPECT_NE(HydroCouple::SDK::IO::uriResolver(), nullptr)
    << "an argument given an https address has no way to read it";
}

TEST_F(HttpUriResolverTest, ItClaimsTheWebAndNothingElse)
{
  HttpUriResolver resolver;

  EXPECT_TRUE(resolver.handles("http"));
  EXPECT_TRUE(resolver.handles("https"));

  // file: is the SDK's own business, and nothing here speaks the rest.
  EXPECT_FALSE(resolver.handles("file"));
  EXPECT_FALSE(resolver.handles("ftp"));
  EXPECT_FALSE(resolver.handles("s3"));
}

// ── Reading ───────────────────────────────────────────────────────────────

TEST_F(HttpUriResolverTest, AnArgumentReadsItsValuesFromAServer)
{
  BodyServer server;
  server.answerWith(200, "[7, 8, 9]");

  auto quantity = std::unique_ptr<HydroCouple::SDK::Quantity>(
    HydroCouple::SDK::Quantity::unitLess("Flow"));
  HydroCouple::SDK::Argument1DInt argument("flow", &m_dim, 0, quantity.get(),
                                           nullptr);

  std::string message;
  ASSERT_TRUE(argument.initialize(
    server.url(QStringLiteral("/flow.json")).toStdString(), AT::URL, message))
    << message;

  EXPECT_EQ(argument.length(), 3);
  EXPECT_EQ(argument[2], 9);
  EXPECT_EQ(server.requestCount(), 1);
}

// A coverage is bytes, not text, and a body that stops at the first NUL is
// a truncated raster.
TEST_F(HttpUriResolverTest, ABodyComesBackWholeEvenWithNulsInIt)
{
  const QByteArray binary = QByteArray("II\x2A\0", 4) + QByteArray("\0\0tail", 6);

  BodyServer server;
  server.answerWith(200, binary, "image/tiff");

  HttpUriResolver resolver;
  std::string contents;
  std::string message;

  ASSERT_TRUE(resolver.resolve(
    server.url(QStringLiteral("/coverage.tif")).toStdString(), contents,
    message))
    << message;

  EXPECT_EQ(contents.size(), static_cast<std::size_t>(binary.size()));
  EXPECT_EQ(contents, std::string(binary.constData(), binary.size()));
}

// ── Refusing ──────────────────────────────────────────────────────────────

// The body is valid JSON on purpose. Services answer errors with documents,
// and a resolver that returns whatever came back would hand "[1, 2]" to the
// argument as data and report a successful read of a page that says no.
TEST_F(HttpUriResolverTest, ARefusalIsARefusalEvenWhenItsBodyParses)
{
  BodyServer server;
  server.answerWith(404, "[1, 2]", "application/json", "Not Found");

  auto quantity = std::unique_ptr<HydroCouple::SDK::Quantity>(
    HydroCouple::SDK::Quantity::unitLess("Flow"));
  HydroCouple::SDK::Argument1DInt argument("flow", &m_dim, 0, quantity.get(),
                                           nullptr);

  std::string message;
  EXPECT_FALSE(argument.initialize(
    server.url(QStringLiteral("/missing.json")).toStdString(), AT::URL,
    message))
    << "a 404 was read as two values";
  EXPECT_EQ(argument.length(), 0);
}

// What the server said is the useful half of a failure; "could not read it"
// sends the reader to check their network for a typo in a layer name.
TEST_F(HttpUriResolverTest, TheServersOwnReasonIsWhatTheCallerIsTold)
{
  BodyServer server;
  server.answerWith(404, "no such series", "text/plain", "Not Found");

  HttpUriResolver resolver;
  std::string contents;
  std::string message;

  EXPECT_FALSE(resolver.resolve(
    server.url(QStringLiteral("/missing.json")).toStdString(), contents,
    message));
  EXPECT_NE(message.find("Not Found"), std::string::npos) << message;
}

// resolve() is public and virtual, so it is reachable with an address the
// client will not ask for. Entering the event loop anyway would wait for a
// callback that is never coming -- a hang, which is worse than a wrong
// answer because nothing reports it.
TEST_F(HttpUriResolverTest, AnAddressTheClientWillNotAskForIsRefusedNotAwaited)
{
  HttpUriResolver resolver;
  std::string contents;
  std::string message;

  EXPECT_FALSE(resolver.resolve("//example.org/flow.json", contents, message));
  EXPECT_FALSE(message.empty());
}

// ── The boundary with the SDK ─────────────────────────────────────────────

// The SDK reads local things itself. With a resolver installed it must still
// do so, or a host would take over reading files by being present.
TEST_F(HttpUriResolverTest, AFileUriIsStillTheSdksOwnBusiness)
{
  auto *resolver =
    static_cast<HttpUriResolver *>(HydroCouple::SDK::IO::uriResolver());
  ASSERT_NE(resolver, nullptr);

  // The installed resolver is shared with every other test in this file, so
  // what proves it was left alone is that it did not move -- not that it was
  // never used.
  const QString before = resolver->lastUri();

  const std::filesystem::path path =
    std::filesystem::path(COMPOSER_CONFIGURATOR_FIXTURE_DIR) / "rating.json";

  std::string contents;
  std::string message;
  ASSERT_TRUE(HydroCouple::SDK::IO::readReference(path.string(), contents,
                                                  message))
    << message;
  EXPECT_NE(contents.find("2.5"), std::string::npos);

  EXPECT_EQ(resolver->lastUri(), before)
    << "reading a local file went out to the network";

  // And the file: spelling of the same path is the SDK's own business too.
  contents.clear();
  ASSERT_TRUE(HydroCouple::SDK::IO::readReference(
    "file://" + path.generic_string(), contents, message))
    << message;
  EXPECT_NE(contents.find("2.5"), std::string::npos);
  EXPECT_EQ(resolver->lastUri(), before);
}
