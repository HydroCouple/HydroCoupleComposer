/*!
 * \file   httpuriresolver.h
 * \author Caleb Buahin
 * \brief  HttpUriResolver — how a model argument reads an http(s) URI.
 *
 * The SDK defines what resolving a URI means and refuses to do it: it has no
 * HTTP client and deliberately no Qt, so an argument given an https address
 * is told that no resolver is installed. Composer is a host that can install
 * one, because HydroCoupleOgc's fetch tier is already in the process for the
 * basemaps.
 *
 * \par Why this blocks
 * `IArgument::initialize()` is synchronous and returns a bool, so there is no
 * seam through which a fetch could be awaited. The resolver therefore spins a
 * nested event loop, with user input excluded — a component is part-way
 * through initializing on the stack below, and letting the user close its
 * configurator or delete the component while that is true would destroy the
 * object the loop returns into.
 */

#ifndef HYDROCOUPLECOMPOSER_CORE_HTTPURIRESOLVER_H
#define HYDROCOUPLECOMPOSER_CORE_HTTPURIRESOLVER_H

#include "hydrocouplesdk/io/uriresolver.h"

#include <QString>

#include <memory>

namespace HydroCouple::Ogc
{
  class HttpClient;
}

namespace HydroCouple::Composer
{

  /*!
   * \brief Reads http and https URIs for model arguments.
   */
  class HttpUriResolver : public HydroCouple::SDK::IO::UriResolver
  {
    public:
      HttpUriResolver();
      ~HttpUriResolver() override;

      /*!
       * \brief Whether \a scheme is one this resolver fetches.
       *
       * http and https only. A `file:` URI never reaches a resolver — the
       * SDK reads those itself — and nothing here speaks ftp.
       */
      [[nodiscard]] bool handles(const std::string &scheme) const override;

      /*!
       * \brief Fetches \a uri, blocking the caller until it is known.
       */
      [[nodiscard]] bool resolve(const std::string &uri, std::string &contents,
                                 std::string &message) override;

      /*!
       * \brief How long a stalled transfer waits before it is given up on.
       * \param milliseconds Zero leaves the client's default in place.
       */
      void setTransferTimeout(int milliseconds);

      /*!
       * \brief The endpoint of the last resolve(), for diagnostics.
       */
      [[nodiscard]] QString lastUri() const;

    private:
      std::unique_ptr<HydroCouple::Ogc::HttpClient> m_client;
      QString m_lastUri;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CORE_HTTPURIRESOLVER_H
