/*!
 * \file   scenesource.h
 * \author Caleb Buahin
 * \brief  ISceneSource — a layer's contribution to the 3D scene.
 *
 * Asked of every layer in the stack, exactly as LayerStyle is: the scene puts
 * one question to the whole stack and builds nothing for the layers that
 * answer nullptr. That is what lets a basemap, a tile layer and a labelled
 * point layer sit in the same stack as a terrain mesh without any of them
 * knowing a 3D view exists.
 *
 * It is a separate interface rather than a method on MapLayer because most
 * layers have no 3D form and would carry an empty override forever — the same
 * reasoning that kept labelling and masks off MapLayer's base.
 *
 * Geometry is built against a SceneContext rather than from the layer alone,
 * because draping is not a layer-local property: where a network line sits in
 * space is decided by the terrain it is drawn over, which belongs to a
 * different layer entirely. Handing that context in keeps the layer ignorant
 * of the stack — it is told what it is being composed with, it does not go
 * looking.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H
#define HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H

#include "scene/scenegeometry.h"

#include <QPointF>
#include <QRectF>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief Answers how high the ground is, for the layers drawn over it.
   *
   * The 3D counterpart of a basemap: one layer in the stack defines the
   * surface, and everything else in the stack is composed against it. Kept
   * apart from ISceneSource because supplying geometry and being a surface
   * others rest on are different capabilities — a network line has the first
   * and not the second.
   *
   * Coordinates are the map's CRS, not the layer's own. A terrain answering
   * in its own projection would be sampled with points from another one, and
   * the drape would be wrong by however far the two disagree — which on a
   * small extent is small enough to look like a modelling artefact.
   */
  class ITerrainSource
  {
    public:
      virtual ~ITerrainSource() = default;

      /*!
       * \brief The ground elevation at \a point.
       *
       * \param point Map-CRS position to sample.
       * \param[out] elevation The ground elevation there.
       * \returns True when \a point lies on the terrain; false leaves
       *          \a elevation untouched, so a caller can decide between
       *          skipping the point and holding the last good value.
       */
      [[nodiscard]] virtual bool elevationAt(const QPointF &point,
                                             double &elevation) const = 0;

      /*!
       * \brief The map-CRS footprint the terrain answers over.
       */
      [[nodiscard]] virtual QRectF terrainExtent() const = 0;

      /*!
       * \brief A characteristic spacing between samples, in map units.
       *
       * What a drape densifies to. Asked of the terrain rather than guessed
       * at by the layer being draped, because the only length that matters
       * here is the one over which the surface can change — a step finer than
       * the terrain's own cells buys nothing but vertices, and one coarser
       * cuts corners off the relief it was supposed to follow.
       */
      [[nodiscard]] virtual double terrainResolution() const = 0;
  };

  /*!
   * \brief What a layer is being composed with, when it builds its geometry.
   */
  struct SceneContext
  {
      //! The surface to drape on, or nullptr when the stack has none.
      const ITerrainSource *terrain = nullptr;

      /*!
       * \brief The world rectangle the scene is about, or an empty one.
       *
       * What a layer that covers everywhere should show. A tiled basemap's
       * extent is the planet, so texturing its own extent would spend the
       * whole image on an ocean and leave the model a pixel wide. Layers with
       * an extent of their own ignore this and show all of themselves.
       *
       * Empty means the scene holds no data — only backdrops — and there is
       * then nothing for a backdrop to be behind.
       */
      QRectF focus;
  };

  /*!
   * \brief How a layer places itself against the terrain.
   *
   * On the scene interface rather than on any one layer type, because every
   * source has to answer it: a vector layer has no third coordinate of its
   * own, and a basemap has no third coordinate either. It lived on
   * FeatureLayer until C5d, which is why a draped basemap could not be turned
   * off — the two surface layers had no say in it at all.
   */
  enum class SceneDrape
  {
    //! At z = 0. What a layer over a stack with no terrain in it gets.
    Flat,

    //! Laid on the terrain, densified finely enough to follow it.
    Terrain,

    /*!
     * \brief A vertical curtain from the terrain up to a set height.
     *
     * How a buried or a low-relief network stays legible: a line lying on a
     * hillside is hidden by the first fold of ground in front of it, and a
     * pipe network that disappears behind terrain is not a view of a network.
     *
     * Meaningless for something that is already a surface — see
     * supportsExtrusion() — where it reads as Terrain.
     */
    Extruded
  };

  /*!
   * \brief Supplies geometry for the 3D scene.
   */
  class ISceneSource
  {
    public:
      virtual ~ISceneSource() = default;

      /*!
       * \brief Whether this source may be elected as the scene's terrain.
       *
       * Capability and consent are different questions: terrain() says a
       * source COULD serve heights, this says it is offered. A mesh with
       * elevations consents by default -- it always has been the terrain.
       * A raster does not: a single band is not necessarily heights (a
       * rainfall grid is one band too), and a data raster added above a
       * mesh must not silently steal the ground out from under the scene.
       */
      [[nodiscard]] bool terrainEnabled() const { return m_terrainEnabled; }

      /*!
       * \brief Offers or withdraws this source as terrain.
       *
       * Virtual for the same reason setDrape() is: only the layer knows
       * what to invalidate and how to ask to be redrawn.
       *
       * \param enabled True to let the election consider this source.
       */
      virtual void setTerrainEnabled(bool enabled)
      {
        m_terrainEnabled = enabled;
      }

      /*!
       * \brief Where this source sits relative to the terrain.
       */
      [[nodiscard]] SceneDrape drape() const { return m_drape; }

      /*!
       * \brief Sets where this source sits relative to the terrain.
       *
       * Virtual because storing the choice is only half of it: a layer that
       * caches built geometry has to throw that cache away and ask to be
       * redrawn, and only the layer knows what it cached.
       *
       * \param drape The placement wanted.
       */
      virtual void setDrape(SceneDrape drape) { m_drape = drape; }

      /*!
       * \brief How far an extruded source stands up, in map units.
       */
      [[nodiscard]] double extrusionHeight() const
      {
        return m_extrusionHeight;
      }

      /*!
       * \brief Sets the extrusion height.
       * \param height Height in map units; the unit is whatever the map's CRS
       *        measures in, so there is no sensible default but zero.
       */
      virtual void setExtrusionHeight(double height)
      {
        m_extrusionHeight = height;
      }

      /*!
       * \brief Whether Extruded means anything for this source.
       *
       * False for anything that is already a surface. An editor uses this to
       * offer only the placements that will do something, rather than listing
       * one that silently behaves as another.
       */
      [[nodiscard]] virtual bool supportsExtrusion() const { return false; }

      /*!
       * \brief The geometry batches to draw, in draw order.
       *
       * A list rather than one batch because a single layer routinely needs
       * more than one primitive type — a filled surface and the edges that
       * make its structure legible cannot share an index buffer.
       *
       * Building is on demand and may be expensive; callers cache.
       *
       * \param context What the layer is being composed with.
       */
      [[nodiscard]] virtual QVector<SceneGeometry> sceneGeometry(
        const SceneContext &context) const = 0;

      /*!
       * \brief The box the geometry occupies, in world coordinates.
       *
       * Answerable without building the geometry, because framing the view
       * must not cost a full tessellation of every layer in the stack.
       */
      [[nodiscard]] virtual Bounds3D sceneBounds() const = 0;

      /*!
       * \brief This source as a surface others may rest on, or nullptr.
       *
       * Hung off the scene source rather than off MapLayer because only
       * something already contributing to the scene can be terrain for it:
       * a surface nothing draws is a surface nothing can be checked against.
       */
      [[nodiscard]] virtual const ITerrainSource *terrain() const
      {
        return nullptr;
      }

    protected:
      /*!
       * \brief The terrain to compose against, honouring the drape choice.
       *
       * \param context What the layer is being composed with.
       * \returns The context's terrain, or nullptr when this source is to
       *          stay flat.
       */
      [[nodiscard]] const ITerrainSource *drapeTarget(
        const SceneContext &context) const
      {
        return m_drape == SceneDrape::Flat ? nullptr : context.terrain;
      }

      SceneDrape m_drape = SceneDrape::Terrain;
      bool m_terrainEnabled = true;
      double m_extrusionHeight = 0.0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H
