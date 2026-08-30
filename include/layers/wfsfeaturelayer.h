/*!
 * \file   wfsfeaturelayer.h
 * \author Caleb Buahin
 * \brief  WfsFeatureLayer — features fetched from a web feature service.
 *
 * The first layer here whose data is the point rather than the backdrop. A
 * WMS answers with a picture of a catchment; a WFS answers with the
 * catchment, so what arrives can be selected, imported as a mesh domain,
 * and used to parameterise a model.
 *
 * Built from bytes already in hand rather than fetching for itself: the
 * dialog that asked the service what it holds is the thing that knows
 * which collection was chosen and over what ground, and a layer that
 * fetched on construction could not report why the answer was unusable.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_WFSFEATURELAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_WFSFEATURELAYER_H

#include "layers/featurelayer.h"

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief Features from a WFS, drawn and selectable like any others.
   */
  class WfsFeatureLayer : public FeatureLayer
  {
    public:
      /*!
       * \brief Reads a GetFeature response.
       *
       * GDAL decodes the bytes. It could not have fetched them — this
       * build has no curl, so its WFS driver is absent entirely — but
       * decoding GeoJSON or GML it has been handed is exactly what it is
       * for, and going through OGR means every geometry type the standard
       * admits arrives through one tested path.
       *
       * \param body The response body.
       * \param name What to call the layer.
       * \param[out] message Why not, on failure — the service's own words
       *             when it refused, since a refusal arrives as a document
       *             rather than as an HTTP error.
       * \returns The layer, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<WfsFeatureLayer> fromResponse(
        const QByteArray &body, const QString &name, QString &message);

      ~WfsFeatureLayer() override;

      /*!
       * \brief The collection this came from, as the service names it.
       */
      [[nodiscard]] QString typeName() const;

      void setTypeName(const QString &typeName);

    private:
      explicit WfsFeatureLayer(const QString &name);

      QString m_typeName;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_WFSFEATURELAYER_H
