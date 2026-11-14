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
  /*!
   * \brief How a source that has no third coordinate of its own gets one.
   *
   * Placement, and only placement: extrusion is a separate, orthogonal
   * setting. The old SceneDrape conflated the two -- "Extruded" was a
   * placement that smuggled a height in -- so a layer could not be lifted
   * a metre above the terrain, or laid flat at a datum other than zero,
   * without the two meanings fighting.
   */
  enum class ZMode
  {
    //! Everything at one elevation. Constant 0 is the old "Flat".
    Constant,

    /*!
     * \brief Each feature at the elevation its own attribute names.
     *
     * The feature stays planar at that height -- an invert level, a gauge
     * datum -- plus the offset. Only meaningful for sources that carry
     * attributes; see ISceneSource::supportsAttributeZ().
     */
    FromAttribute,

    //! Laid on the elected terrain, densified finely enough to follow it.
    OnTerrain
  };

  /*!
   * \brief Where a source's base z comes from.
   */
  struct ZPolicy
  {
      ZMode mode = ZMode::OnTerrain;

      //! The elevation, when mode is Constant. Ignored otherwise.
      double constant = 0.0;

      //! The attribute read per feature, when mode is FromAttribute.
      QString field;

      //! Added to the base for FromAttribute and OnTerrain.
      double offset = 0.0;

      //! Setting a policy a source already has must not announce a change.
      friend bool operator==(const ZPolicy &a, const ZPolicy &b)
      {
        return a.mode == b.mode && a.constant == b.constant &&
               a.field == b.field && a.offset == b.offset;
      }

      friend bool operator!=(const ZPolicy &a, const ZPolicy &b)
      {
        return !(a == b);
      }
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
       * \brief Where this source's base z comes from.
       */
      [[nodiscard]] const ZPolicy &zPolicy() const { return m_zPolicy; }

      /*!
       * \brief Sets where this source's base z comes from.
       *
       * Virtual because storing the choice is only half of it: a layer that
       * caches built geometry has to throw that cache away and ask to be
       * redrawn, and only the layer knows what it cached.
       *
       * \param policy The placement wanted.
       */
      virtual void setZPolicy(const ZPolicy &policy) { m_zPolicy = policy; }

      /*!
       * \brief Whether FromAttribute means anything for this source.
       *
       * True only where features carry attributes a height could be read
       * from; a raster or a basemap has no per-feature anything.
       */
      [[nodiscard]] virtual bool supportsAttributeZ() const { return false; }

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
       * \brief How big a point marker is drawn, in map units; 0 is automatic.
       *
       * In map units rather than pixels, and a solid rather than a
       * camera-facing sprite. A screen-sized billboard is the nicer answer
       * and needs a third pipeline with its own shader; a solid needs none,
       * is correct from every angle, and is lit by the material already
       * there. Automatic sizes it from the layer's own extent, which is the
       * only scale a layer knows without asking the scene.
       */
      [[nodiscard]] double markerSize() const { return m_markerSize; }

      /*!
       * \brief Sets the marker size.
       * \param size Size in map units, or 0 to size it from the extent.
       */
      virtual void setMarkerSize(double size) { m_markerSize = size; }

      /*!
       * \brief Whether rings are filled in the scene.
       *
       * Off by default: filling an arbitrary ring against terrain is a
       * constrained triangulation, and an outline that follows the ground
       * already says what the map cannot. Fans work on a **convex** ring,
       * which is what a mesh face is — so this is what makes a component's
       * face-attached output read as a surface rather than as wireframe.
       * Rings that are not convex are left as outlines rather than
       * tessellated wrongly.
       */
      [[nodiscard]] bool fillsRings() const { return m_fillRings; }

      /*!
       * \brief Sets whether rings are filled.
       */
      virtual void setFillsRings(bool fills) { m_fillRings = fills; }

      /*!
       * \brief Whether filling rings would do anything for this source.
       */
      [[nodiscard]] virtual bool supportsRingFill() const { return false; }

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
      [[nodiscard]] const ITerrainSource *terrainFor(
        const SceneContext &context) const
      {
        return m_zPolicy.mode == ZMode::OnTerrain ? context.terrain : nullptr;
      }

      ZPolicy m_zPolicy;
      bool m_terrainEnabled = true;
      double m_extrusionHeight = 0.0;
      double m_markerSize = 0.0;
      bool m_fillRings = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENESOURCE_H
