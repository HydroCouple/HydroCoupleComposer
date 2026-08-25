/*!
 * \file   gdalrasterlayer.h
 * \author Caleb Buahin
 * \brief  GdalRasterLayer — a raster dataset drawn under the vectors.
 *
 * Reads only what the view needs, at the resolution the view can show:
 * a terrain model is routinely far larger than any screen, and decoding all
 * of it to draw a few hundred pixels is the difference between a map that
 * pans and one that does not.
 *
 * Reprojection uses GDAL's warped VRT rather than transforming pixels by
 * hand — resampling a grid correctly is a solved problem and not one worth
 * solving twice.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_GDALRASTERLAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_GDALRASTERLAYER_H

#include "map/maplayer.h"
#include "render/colorramp.h"

#include <QImage>
#include <QString>

#include <memory>

class GDALDataset;

namespace HydroCouple::Composer
{

  /*!
   * \brief A raster dataset drawn on the map.
   */
  class GdalRasterLayer : public MapLayer
  {
    public:
      /*!
       * \brief Opens \a filePath through GDAL.
       * \param filePath Dataset to read.
       * \param[out] message Diagnostic on failure.
       * \returns The layer, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<GdalRasterLayer> open(
        const QString &filePath, QString &message);

      ~GdalRasterLayer() override;

      /*!
       * \brief The file this layer was read from.
       */
      [[nodiscard]] QString filePath() const;

      /*!
       * \brief The dataset's size in pixels.
       */
      [[nodiscard]] QSize rasterSize() const;

      /*!
       * \brief How many bands the dataset holds.
       */
      [[nodiscard]] int bandCount() const;

      /*!
       * \brief Whether the dataset is drawn as colour rather than shaded.
       */
      [[nodiscard]] bool isColorImage() const;

      /*!
       * \brief The ramp a single-band raster is shaded with.
       */
      [[nodiscard]] const ColorRamp &ramp() const;

      /*!
       * \brief Sets the ramp a single-band raster is shaded with.
       * \param ramp The ramp to use.
       */
      void setRamp(const ColorRamp &ramp);

      /*!
       * \brief The value range a single-band raster is shaded across.
       * \param[out] minimum Lowest value.
       * \param[out] maximum Highest value.
       */
      void valueRange(double &minimum, double &maximum) const;

      /*!
       * \brief The image produced by the most recent draw.
       *
       * Kept so a test can look at what was actually rendered rather than
       * only at whether rendering was attempted.
       */
      [[nodiscard]] const QImage &lastImage() const;

      // ── MapLayer ─────────────────────────────────────────────────────────

      [[nodiscard]] QRectF extent() const override;

      void render(QPainter &painter, const MapTransform &transform) override;

    protected:
      void onMapCrsChanged() override;

    private:
      GdalRasterLayer(const QString &name, const QString &filePath);

      //! The dataset to read from — warped into the map's CRS when needed.
      [[nodiscard]] GDALDataset *readable();

      void computeRange();

      QString m_filePath;

      GDALDataset *m_dataset = nullptr;
      GDALDataset *m_warped = nullptr;

      QRectF m_extent;
      QSize m_size;
      int m_bandCount = 0;
      bool m_colorImage = false;

      double m_minimum = 0.0;
      double m_maximum = 1.0;

      ColorRamp m_ramp;
      QImage m_lastImage;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_GDALRASTERLAYER_H
