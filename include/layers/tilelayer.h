/*!
 * \file   tilelayer.h
 * \author Caleb Buahin
 * \brief  TileLayer — a slippy-map basemap, and the source it reads from.
 *
 * The layer draws whatever tiles it has and asks for the ones it does not.
 * It never blocks: a basemap that waited for the network would freeze the map
 * on every pan, which is worse than a map that fills in a moment later.
 *
 * The source is an interface rather than a hardwired QNetworkAccessManager so
 * the layer can be tested without a network — a basemap test that needs the
 * internet is a test that fails for reasons having nothing to do with the
 * code.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_TILELAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_TILELAYER_H

#include "map/maplayer.h"
#include "scene/groundplane.h"
#include "scene/scenesource.h"
#include "map/tilegrid.h"

#include <QHash>
#include <QImage>
#include <QString>

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief Supplies tile images to a TileLayer.
   */
  class ITileSource
  {
    public:
      virtual ~ITileSource() = default;

      /*!
       * \brief The tile's image if it is to hand, or a null image.
       *
       * Must not block. A null image means "not yet", not "never".
       *
       * \param tile The tile wanted.
       */
      [[nodiscard]] virtual QImage tile(const TileId &tile) = 0;

      /*!
       * \brief Asks for a tile that was not to hand.
       * \param tile The tile to fetch.
       */
      virtual void request(const TileId &tile) = 0;

      /*!
       * \brief Called when the layer no longer wants pending tiles.
       *
       * A pan that outruns the network otherwise leaves a queue of requests
       * for tiles nobody will look at.
       */
      virtual void cancelPending() = 0;

      /*!
       * \brief Human-readable attribution the map must display.
       *
       * Every free tile provider requires it, so the layer treats it as part
       * of the data rather than as an optional nicety.
       */
      [[nodiscard]] virtual QString attribution() const = 0;

      /*!
       * \brief The deepest zoom this source has tiles for.
       */
      [[nodiscard]] virtual int maximumZoom() const = 0;
  };

  /*!
   * \brief A tiled basemap layer.
   */
  class TileLayer : public MapLayer, public ISceneSource
  {
    public:
      /*!
       * \brief Constructs the layer.
       * \param name User-visible layer name.
       * \param source Where tiles come from; the layer takes ownership.
       * \param parent Owning object.
       */
      TileLayer(const QString &name, std::unique_ptr<ITileSource> source,
                QObject *parent = nullptr);

      ~TileLayer() override;

      /*!
       * \brief The whole Web Mercator world.
       *
       * A basemap covers everywhere, so framing it frames the planet. The
       * canvas therefore uses it only when nothing else has an extent —
       * see isBasemap().
       */
      [[nodiscard]] QRectF extent() const override;

      void render(QPainter &painter, const MapTransform &transform) override;

      /*!
       * \brief The tile source.
       */
      [[nodiscard]] ITileSource *source() const;

      /*!
       * \brief The zoom used by the most recent draw, or -1.
       */
      [[nodiscard]] int lastZoom() const;

      /*!
       * \brief How many tiles the most recent draw actually painted.
       */
      [[nodiscard]] int lastDrawnTileCount() const;

      /*!
       * \brief Whether this layer is a basemap and belongs at the bottom.
       */
      [[nodiscard]] bool isBasemap() const override;

      // ── ISceneSource ─────────────────────────────────────────────────────

      /*!
       * \brief This layer, as the 3D scene's geometry supplier.
       */
      [[nodiscard]] const ISceneSource *sceneSource() const override;

      /*!
       * \brief The ground under the scene, wearing this basemap.
       *
       * Textured over the scene's focus rather than over this layer's own
       * extent, which is the planet: spending a megapixel on the whole world
       * would leave the modelled catchment about a pixel across.
       *
       * A scene with no data in it gets nothing. There is no rectangle a
       * backdrop should cover when there is nothing for it to be behind, and
       * the alternative — falling back to the globe — is a globe viewer,
       * which this is not.
       *
       * \param context The scene's focus, and the terrain to lay it on.
       */
      [[nodiscard]] QVector<SceneGeometry> sceneGeometry(
        const SceneContext &context) const override;

      /*!
       * \brief Nothing.
       *
       * A basemap covers everywhere, so framing it frames the planet — the
       * same reasoning as isBasemap(), and the reason the scene's focus can
       * be read straight off SceneRenderer::sceneBounds() without a rule of
       * its own for backdrops.
       */
      [[nodiscard]] Bounds3D sceneBounds() const override;

      /*!
       * \brief The provider's required credit.
       */
      [[nodiscard]] QString attribution() const override;

      /*!
       * \brief Call when a requested tile has arrived.
       */
      void onTileReady();

    private:
      std::unique_ptr<ITileSource> m_source;
      int m_lastZoom = -1;
      int m_lastDrawnTileCount = 0;

      //! The scene's texture, and the focus it was built for. Mutable because
      //! it is a cache; keyed on the focus because that is what decides the
      //! picture, and tiles arriving later change it again.
      mutable GroundImage m_ground;
      mutable QRectF m_groundFocus;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_TILELAYER_H
