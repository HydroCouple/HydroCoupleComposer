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
#include "scene/groundplane.h"
#include "scene/scenesource.h"

#include <QImage>
#include <QString>
#include <QVector>

#include <memory>

class GDALDataset;

namespace HydroCouple::Composer
{

  /*!
   * \brief A raster dataset drawn on the map.
   */
  class GdalRasterLayer : public MapLayer,
                          public ISceneSource,
                          public ITerrainSource
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
       * \brief The builtin preset the ramp came from, e.g. "Viridis".
       *
       * Kept beside the ramp because a ColorRamp cannot name itself, and
       * both the dialog's combo and a saved composition need the name, not
       * the stops.
       */
      [[nodiscard]] QString rampName() const;

      //! Adopts a builtin preset by name; unknown names give Viridis.
      void setRampName(const QString &name);

      /*!
       * \brief Sets the value range the ramp is stretched over.
       *
       * The computed band range is only a default: a depth raster whose
       * outliers flatten everything else is re-stretched here.
       *
       * \param minimum Low end of the stretch.
       * \param maximum High end; ignored unless above the minimum.
       */
      void setValueRange(double minimum, double maximum);

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

      /*!
       * \brief Reads the raster's values at \a points.
       *
       * Bilinearly interpolated, because these are measurements of a
       * continuous surface rather than categories: an elevation model
       * sampled nearest-neighbour gives a mesh visible half-cell steps.
       *
       * The window covering the points is read once rather than a request
       * per point. A mesh has as many vertices as it has, and one RasterIO
       * call each turns a second of work into minutes.
       *
       * \param points  Positions in this raster's OWN system. Reprojecting
       *                is the caller's, because the caller knows what
       *                system its points are in and this does not.
       * \param values  Filled to match \a points. A point outside the
       *                raster, or on a no-data cell, is quiet NaN --
       *                distinguishable from a real reading, which zero is
       *                not.
       * \param message Set when nothing could be read at all.
       * \returns Whether the read happened; individual misses are NaN
       *          rather than failures.
       */
      [[nodiscard]] bool sample(const QVector<QPointF> &points,
                                QVector<double> &values,
                                QString &message) const;

      [[nodiscard]] QRectF extent() const override;

      void render(QPainter &painter, const MapTransform &transform) override;

      // ── ISceneSource ─────────────────────────────────────────────────────

      /*!
       * \brief This layer, as the 3D scene's geometry supplier.
       */
      [[nodiscard]] const ISceneSource *sceneSource() const override;

      /*!
       * \brief Sets where the raster sits, and asks for a redraw.
       *
       * See TileLayer::setDrape(): the scene caches what it was handed.
       *
       * \param drape The placement wanted.
       */
      void setDrape(SceneDrape drape) override;

      /*!
       * \brief The ground itself, wearing this raster.
       *
       * A raster has no geometry — it is a picture of a place — so its 3D
       * form is the surface it is a picture *of*, textured with what the map
       * would have drawn there and laid on whatever terrain the stack holds.
       *
       * \param context The terrain to lay it on, when the stack has one.
       */
      [[nodiscard]] QVector<SceneGeometry> sceneGeometry(
        const SceneContext &context) const override;

      /*!
       * \brief The raster's footprint, at ground level.
       */
      [[nodiscard]] Bounds3D sceneBounds() const override;

      /*!
       * \brief A single-band raster offers itself as the scene's terrain.
       *
       * A colour image is a picture of the ground, not the ground: three
       * bands of reflectance are not heights, and electing one would drape
       * everything onto its pixel values.
       */
      [[nodiscard]] const ITerrainSource *terrain() const override;

      //! Announces the change, so the scene re-drapes on the new ground.
      void setTerrainEnabled(bool enabled) override;

      /*!
       * \brief The ground the terrain cache covers, in the map's CRS.
       */
      [[nodiscard]] QRectF terrainExtent() const override;

      /*!
       * \brief The terrain cache's cell size, in map units.
       */
      [[nodiscard]] double terrainResolution() const override;

      /*!
       * \brief The ground height under \a point, in the map's CRS.
       *
       * Served from a downsampled in-memory copy of the first band -- never
       * per-point RasterIO, which turns a second of work into minutes -- so
       * terrain fidelity is capped at the cache's resolution while the 2D
       * picture keeps the full one. No-data declines rather than answering
       * zero: a hole in the survey is not sea level.
       */
      [[nodiscard]] bool elevationAt(const QPointF &point,
                                     double &elevation) const override;

    protected:
      void onProjectionChanged() override;

      GdalRasterLayer(const QString &name, const QString &filePath);

      /*!
       * \brief Takes ownership of an already-open dataset and reads what the
       *        layer needs from it.
       *
       * Everything open() does once GDAL has handed it a dataset: the band
       * count, the size, the extent, whether it is a picture or data, the
       * CRS it declares, and the value range a ramp is stretched over.
       *
       * Protected because a dataset need not come from a file. A coverage
       * fetched from a WCS arrives as bytes and is opened through /vsimem,
       * and everything after that point is identical.
       *
       * \param dataset An open dataset; taken over on success, untouched on
       *                failure so the caller can close it and say why.
       * \param message Set when the dataset cannot be used.
       * \returns Whether the dataset was adopted.
       */
      bool adoptDataset(GDALDataset *dataset, QString &message);

    private:

      //! The dataset to read from — warped into the map's CRS when needed.
      [[nodiscard]] GDALDataset *readable();

      void computeRange();

      QString m_filePath;

      GDALDataset *m_dataset = nullptr;
      GDALDataset *m_warped = nullptr;

      //! The downsampled first band the terrain answers from.
      struct TerrainCache
      {
          std::vector<float> heights;
          int width = 0;
          int height = 0;

          //! Upper-left corner and signed cell steps, in the map's CRS.
          double originX = 0.0;
          double originY = 0.0;
          double stepX = 1.0;
          double stepY = -1.0;

          QRectF extent;
          bool valid = false;
      };

      bool ensureTerrainCache() const;

      mutable TerrainCache m_terrain;

      QRectF m_extent;
      QSize m_size;
      int m_bandCount = 0;
      bool m_colorImage = false;

      double m_minimum = 0.0;
      double m_maximum = 1.0;

      ColorRamp m_ramp;
      QString m_rampName = QStringLiteral("Viridis");
      QImage m_lastImage;

      //! The scene's texture. Mutable because it is a cache: a const caller
      //! asking for geometry is not changing the layer, it is paying for work
      //! not yet done — the same reasoning as FeatureLayer's projected
      //! geometry. Dropped whenever the picture would change.
      mutable GroundImage m_ground;
      mutable bool m_groundValid = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_GDALRASTERLAYER_H
