/*!
 * \file   rasterdataitemlayer.h
 * \author Caleb Buahin
 * \brief  RasterDataItemLayer — a component's raster data item, on the map.
 *
 * DataItemLayer draws the data items that are features once read — geometry,
 * networks, polyhedral surfaces — and a regular grid goes to MeshLayer,
 * because a grid is a mesh whose faces happen to be quads. A raster is
 * neither: millions of cells as polygons would be a mesh nobody can draw, so
 * it is shaded into an image the way GdalRasterLayer shades a GeoTIFF.
 *
 * Which is what lets a coverage be a layer and model data at once: the same
 * numbers a component publishes are the ones on the map.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_RASTERDATAITEMLAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_RASTERDATAITEMLAYER_H

#include "map/maplayer.h"
#include "render/colorramp.h"
#include "scene/groundplane.h"
#include "scene/scenesource.h"

#include <QImage>

#include <memory>

namespace HydroCouple
{
  class IComponentDataItem;

  namespace Spatial
  {
    class IRasterComponentDataItem;
  }
}

namespace HydroCouple::Composer
{

  /*!
   * \brief A component's raster data item drawn on the map.
   */
  class RasterDataItemLayer : public MapLayer, public ISceneSource
  {
    public:
      /*!
       * \brief Whether \a item is a raster this layer can draw.
       *
       * Answers for the published interface, not for any one implementation:
       * a component that declares IRasterComponentDataItem is drawable here
       * whoever wrote it.
       *
       * \param item The data item to test; null is not a raster.
       */
      [[nodiscard]] static bool isRaster(
        const HydroCouple::IComponentDataItem *item);

      /*!
       * \brief Builds a layer from \a item.
       *
       * The item must outlive the layer, which re-reads values from it on
       * every draw — the point of showing a live component rather than a
       * snapshot of one.
       *
       * \param item The raster data item to draw.
       * \param[out] message Diagnostic on failure.
       * \returns The layer, or nullptr when the item is not a drawable raster.
       */
      [[nodiscard]] static std::unique_ptr<RasterDataItemLayer> create(
        HydroCouple::IComponentDataItem *item, QString &message);

      ~RasterDataItemLayer() override;

      //! The item this layer draws.
      [[nodiscard]] HydroCouple::IComponentDataItem *dataItem() const;

      //! The band being shown, counting from zero.
      [[nodiscard]] int band() const;

      /*!
       * \brief Shows a different band.
       * \param band Band index, counting from zero; out of range is ignored.
       */
      void setBand(int band);

      //! The ramp the band is shaded with.
      [[nodiscard]] const ColorRamp &ramp() const;

      //! Sets the ramp the band is shaded with.
      void setRamp(const ColorRamp &ramp);

      //! The value range the ramp is stretched over.
      [[nodiscard]] QPair<double, double> valueRange() const;

      [[nodiscard]] QRectF extent() const override;

      void render(QPainter &painter, const MapTransform &transform) override;

      /*!
       * \brief The layer's 3D form: its shaded band, draped as a texture.
       *
       * The same recipe a GeoTIFF uses -- rendered to an image by its own 2D
       * path, laid over whatever terrain the scene elected -- which is what
       * lets a simulation's raster results appear in 3D at all. Until this
       * override, run results were the one whole class of layer silently
       * absent from the scene.
       */
      [[nodiscard]] const ISceneSource *sceneSource() const override;

      [[nodiscard]] QVector<SceneGeometry> sceneGeometry(
        const SceneContext &context) const override;

      [[nodiscard]] Bounds3D sceneBounds() const override;

      //! The last image drawn, for tests and for the 3D ground texture.
      [[nodiscard]] const QImage &lastImage() const;

    private:
      RasterDataItemLayer(const QString &name,
                          HydroCouple::Spatial::IRasterComponentDataItem *item);

      /*!
       * \brief Reads the whole band and works out what range to shade over.
       * \param[out] message Diagnostic when the item will not give values.
       */
      bool readBand(QString &message);

      HydroCouple::Spatial::IRasterComponentDataItem *m_item = nullptr;

      int m_band = 0;
      int m_width = 0;
      int m_height = 0;

      //! The band's values, row-major, no-data left as NaN.
      std::vector<double> m_values;

      double m_minimum = 0.0;
      double m_maximum = 0.0;

      //! The affine transform, in the interface's order.
      std::array<double, 6> m_geoTransform{0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

      ColorRamp m_ramp;
      QImage m_lastImage;

      //! The draped texture, rebuilt when the band, ramp or values change.
      mutable GroundImage m_ground;
      mutable bool m_groundValid = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_RASTERDATAITEMLAYER_H
