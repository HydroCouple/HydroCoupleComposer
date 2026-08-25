/*!
 * \file   gdalvectorlayer.h
 * \author Caleb Buahin
 * \brief  GdalVectorLayer — a vector dataset read through OGR.
 *
 * A loader, not a renderer: it fills a FeatureLayer from a file and leaves
 * drawing, reprojection, styling and labelling to the base, which a
 * component's spatial data items fill the same way.
 *
 * Reads once into memory rather than querying per frame. The datasets a
 * composition references are network geometry and catchment boundaries —
 * small enough to hold, and far too slow to re-read on every pan.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_GDALVECTORLAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_GDALVECTORLAYER_H

#include "layers/featurelayer.h"

#include <QStringList>

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief A vector dataset drawn on the map.
   */
  class GdalVectorLayer : public FeatureLayer
  {
    public:
      /*!
       * \brief Opens \a filePath through OGR.
       * \param filePath Dataset to read.
       * \param[out] message Diagnostic on failure.
       * \param layerName Sublayer to read; empty takes the first.
       * \returns The layer, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<GdalVectorLayer> open(
        const QString &filePath, QString &message,
        const QString &layerName = QString());

      /*!
       * \brief The sublayer names a dataset holds.
       *
       * A GeoPackage or a shapefile directory routinely holds several, and
       * opening only the first would silently ignore the rest.
       *
       * \param filePath Dataset to inspect.
       */
      [[nodiscard]] static QStringList sublayerNames(const QString &filePath);

      ~GdalVectorLayer() override;

      /*!
       * \brief The file this layer was read from.
       */
      [[nodiscard]] QString filePath() const;

    private:
      GdalVectorLayer(const QString &name, const QString &filePath);

      QString m_filePath;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_GDALVECTORLAYER_H
