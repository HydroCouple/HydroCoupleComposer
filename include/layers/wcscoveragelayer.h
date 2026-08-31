/*!
 * \file   wcscoveragelayer.h
 * \author Caleb Buahin
 * \brief  WcsCoverageLayer — a coverage fetched from a WCS, as a raster.
 *
 * The service that answers with values rather than a picture of them. That
 * is what makes it the one worth having for a model: a coverage of ground
 * elevations can put heights on mesh vertices, where a hillshade of the same
 * ground can only sit behind them.
 *
 * It derives from GdalRasterLayer rather than from MapLayer because, once
 * the bytes are decoded, a coverage IS a raster — and everything a raster
 * layer already has is exactly what a coverage needs: a colour ramp over its
 * value range, warping into the map's CRS, and a ground texture for the 3D
 * scene. The only difference is where the dataset came from, and GDAL cannot
 * tell: it is built without curl in this project, so it can decode bytes it
 * is handed but cannot fetch them. Qt fetches; GDAL decodes.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_WCSCOVERAGELAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_WCSCOVERAGELAYER_H

#include "layers/gdalrasterlayer.h"

#include <hydrocoupleogc/wcscapabilities.h>

#include <QByteArray>
#include <QString>

#include <memory>

namespace HydroCouple::Composer
{
  /*!
   * \brief A coverage fetched from a Web Coverage Service.
   */
  class WcsCoverageLayer : public GdalRasterLayer
  {
      Q_OBJECT

    public:
      ~WcsCoverageLayer() override;

      /*!
       * \brief Builds a layer from a GetCoverage response.
       *
       * \param body        The response bytes — a GeoTIFF, or anything else
       *                    GDAL can read.
       * \param name        What to call the layer.
       * \param message     Set when the response cannot be read, to what the
       *                    user should be told.
       * \returns The layer, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<WcsCoverageLayer> fromResponse(
        const QByteArray &body, const QString &name, QString &message);

      //! Where the coverage came from.
      [[nodiscard]] QString serviceUrl() const;
      void setServiceUrl(const QString &url);

      //! Which coverage, as the service spells it.
      [[nodiscard]] QString coverageId() const;
      void setCoverageId(const QString &identifier);

      /*!
       * \brief What the service said this coverage is.
       *
       * Kept because it holds what the picture cannot: the band names and
       * their units. A terrain sampler asking for heights in metres wants
       * the field called "hoogte", and only this says so.
       */
      [[nodiscard]] const HydroCouple::Ogc::WcsCoverageDescription &
      description() const;

      void setDescription(
        const HydroCouple::Ogc::WcsCoverageDescription &description);

      /*!
       * \brief The service and coverage, not the /vsimem path.
       *
       * The path GDAL was handed names nothing a user could open.
       */
      [[nodiscard]] QString sourceDescription() const override;

    private:
      WcsCoverageLayer(const QString &name, const QString &memoryPath);

      QString m_memoryPath;
      QString m_serviceUrl;
      QString m_coverageId;
      HydroCouple::Ogc::WcsCoverageDescription m_description;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_WCSCOVERAGELAYER_H
