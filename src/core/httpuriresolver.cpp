#include "core/httpuriresolver.h"

#include "hydrocoupleogc/httpclient.h"

#include <QEventLoop>
#include <QUrl>

namespace HydroCouple::Composer
{

  HttpUriResolver::HttpUriResolver()
    : m_client(std::make_unique<HydroCouple::Ogc::HttpClient>())
  {
    m_client->setUserAgent(QStringLiteral("HydroCoupleComposer"));
  }

  HttpUriResolver::~HttpUriResolver() = default;

  bool HttpUriResolver::handles(const std::string &scheme) const
  {
    return scheme == "http" || scheme == "https";
  }

  bool HttpUriResolver::resolve(const std::string &uri, std::string &contents,
                                std::string &message)
  {
    const QString address = QString::fromStdString(uri);
    m_lastUri = address;

    const QUrl url(address);

    if (!url.isValid())
    {
      message = "Not a valid address: " + uri;
      return false;
    }

    QEventLoop loop;
    HydroCouple::Ogc::HttpResponse answer;

    const HydroCouple::Ogc::HttpClient::RequestId id = m_client->get(
      url,
      [&answer, &loop](const HydroCouple::Ogc::HttpResponse &response)
      {
        answer = response;
        loop.quit();
      });

    if (id == 0)
    {
      message = "Could not ask for " + uri;
      return false;
    }

    // A component is initializing on the stack below this call. Admitting
    // user input here would let the configurator that started it be closed,
    // and the loop would return into an object that no longer exists.
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    if (!answer.ok)
    {
      message = answer.error.isEmpty()
                  ? "Could not read " + uri
                  : answer.error.toStdString();
      return false;
    }

    contents = answer.body.toStdString();
    return true;
  }

  void HttpUriResolver::setTransferTimeout(int milliseconds)
  {
    m_client->setTransferTimeout(milliseconds);
  }

  QString HttpUriResolver::lastUri() const
  {
    return m_lastUri;
  }

} // namespace HydroCouple::Composer
