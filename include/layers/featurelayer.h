/*!
 * \file   featurelayer.h
 * \author Caleb Buahin
 * \brief  FeatureLayer — geometry with attributes, however it was obtained.
 *
 * A dataset read from a file and a component's spatial data item are the same
 * thing once loaded: features with geometry, attributes and a style. This
 * holds that in-memory form and everything that acts on it — reprojection,
 * drawing, labelling, classification — so the loaders above it differ only in
 * where the features come from.
 */

#ifndef HYDROCOUPLECOMPOSER_LAYERS_FEATURELAYER_H
#define HYDROCOUPLECOMPOSER_LAYERS_FEATURELAYER_H

#include "map/maplayer.h"
#include "render/attributeprovider.h"
#include "render/layerstyle.h"
#include "pick/centroidindex.h"
#include "scene/scenesource.h"

#include <QPolygonF>
#include <QSet>
#include <QString>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief What a feature's geometry is made of.
   */
  enum class GeometryKind
  {
    Point,
    Line,
    Polygon
  };

  /*!
   * \brief One feature: its geometry and its attributes.
   */
  struct VectorFeature
  {
      GeometryKind kind = GeometryKind::Point;

      //! Parts, in the layer's own CRS: rings of a polygon, or the pieces of
      //! a multi-part line. A point feature has one part of one vertex.
      QVector<QPolygonF> parts;

      QVector<QVariant> attributes;  //!< Parallel to the field list.
      QRectF bounds;
  };

  /*!
   * \brief A layer of features drawn on the map.
   */
  class FeatureLayer : public MapLayer,
                       public IAttributeProvider,
                       public ISceneSource
  {
    public:
      /*!
       * \brief Constructs an empty layer.
       * \param name User-visible layer name.
       */
      explicit FeatureLayer(const QString &name);

      ~FeatureLayer() override;

      /*!
       * \brief The features, in the layer's own CRS.
       */
      [[nodiscard]] const QVector<VectorFeature> &features() const;

      /*!
       * \brief What kind of geometry the layer holds.
       */
      [[nodiscard]] GeometryKind geometryKind() const;

      /*!
       * \brief Recomputes the style from the layer's own data.
       */
      bool restyle();

      /*!
       * \brief The feature at \a point, or -1.
       *
       * Nearest first: a click between two features takes the one it is
       * closer to, which is what the user meant by clicking there.
       *
       * \param point Map-CRS position, as a click converts to.
       * \param tolerance How far from a feature still counts as on it, in
       *        map units. A line has no area to click inside, and neither
       *        does a point.
       */
      [[nodiscard]] int pickAt(const QPointF &point, double tolerance) const;

      /*!
       * \brief Every feature that meets \a rectangle.
       *
       * What a rubber band asks. Touching counts, not only containment: a
       * conduit that runs across the box is inside what the user dragged
       * over even though neither of its ends is, and a band that took only
       * whole features would select nothing at all on a network of long
       * lines.
       *
       * Linear over the layer, deliberately. The centroid index answers
       * "nearest to a point", which is the wrong question here, and a band
       * is dragged once by hand rather than evaluated per frame.
       *
       * \param rectangle World-coordinate rectangle, in the map's CRS.
       * \returns The features it caught; empty when it caught none.
       */
      [[nodiscard]] QSet<int> pickIn(const QRectF &rectangle) const;

      /*!
       * \brief The features currently selected, by index.
       *
       * Held on the layer rather than on whichever view did the selecting,
       * so that the map, the 3D scene and the attribute table are three
       * views of one selection instead of three selections.
       */
      [[nodiscard]] const QSet<int> &selection() const;

      /*!
       * \brief Replaces the selection.
       * \param features Feature indices; out-of-range ones are dropped.
       */
      void setSelection(QSet<int> features);

      /*!
       * \brief Selects nothing.
       */
      void clearSelection();

      /*!
       * \brief How the layer places itself in the 3D scene.
       *
       * The scene interface owns the choice since C5d; these two names are
       * kept because they read better at a feature layer's call sites and
       * because renaming them would churn every one of them.
       */
      [[nodiscard]] SceneDrape sceneDrape() const;

      /*!
       * \brief Sets how the layer places itself in the 3D scene.
       *
       * Terrain is the default, and degrades to Flat by itself when the
       * stack holds no terrain — so a network opened beside a mesh drapes
       * without being told to, and one opened alone still appears.
       *
       * \param drape The placement to use.
       */
      void setSceneDrape(SceneDrape drape);

      void setDrape(SceneDrape drape) override;

      void setExtrusionHeight(double height) override;

      //! A line or a ring can stand up; that is what extrusion is for.
      [[nodiscard]] bool supportsExtrusion() const override { return true; }


      // ── MapLayer ─────────────────────────────────────────────────────────

      [[nodiscard]] QRectF extent() const override;

      void render(QPainter &painter, const MapTransform &transform) override;

      [[nodiscard]] const LayerStyle *style() const override;

      [[nodiscard]] LayerStyle *style() override;

      // ── IAttributeProvider ───────────────────────────────────────────────

      [[nodiscard]] QVector<AttributeField> attributeFields() const override;

      [[nodiscard]] int featureCount() const override;

      [[nodiscard]] QVariant attributeValue(int feature,
                                            const QString &field) const override;

      // ── ISceneSource ─────────────────────────────────────────────────────

      /*!
       * \brief This layer, as the 3D scene's geometry supplier.
       *
       * Every vector layer answers, rather than only the ones somebody
       * remembered to switch on: a network that is in the map and missing
       * from the scene reads as a rendering fault, and looking for the
       * setting that caused it is worse than the setting being there.
       * A point layer is the exception — see sceneGeometry().
       */
      [[nodiscard]] const ISceneSource *sceneSource() const override;

      /*!
       * \brief Lines for line and polygon layers, nothing for points.
       *
       * Polygons contribute their rings rather than filled surfaces: filling
       * one against terrain is a constrained triangulation, and a catchment
       * whose outline follows the ground already says what the map cannot.
       * Points contribute nothing, on MeshLayer's precedent — a point cloud
       * has no 3D form that is not invented.
       *
       * Colours come from the layer's own style, so a feature classified in
       * the map and the same feature in the scene cannot disagree.
       *
       * \param context The terrain to drape on, when the stack has one.
       */
      [[nodiscard]] QVector<SceneGeometry> sceneGeometry(
        const SceneContext &context) const override;

      /*!
       * \brief The layer's map-CRS footprint, at the heights it may occupy.
       *
       * The terrain is not consulted, because bounds exist to frame the view
       * before anything is built and sampling a surface for every vertex is
       * exactly the cost that is being avoided. The terrain layer's own
       * bounds carry the relief, and the scene is framed to both.
       */
      [[nodiscard]] Bounds3D sceneBounds() const override;

      /*!
       * \brief Every feature's geometry in the map's CRS.
       *
       * The projected form the 2D render walks, exposed because the 3D scene
       * needs precisely the same coordinates: a mesh reprojected one way for
       * the map and another for the scene would not line up with the layers
       * drawn beside it. Parallel to features(), and rebuilt on demand, so
       * the first call after a CRS change pays for the reprojection.
       */
      [[nodiscard]] const QVector<QVector<QPolygonF>> &projectedFeatures()
        const;

    protected:
      void onProjectionChanged() override;

      /*!
       * \brief Declares the attribute fields, in value order.
       * \param fields The fields each feature's attributes are parallel to.
       */
      void setFields(QVector<AttributeField> fields);

      /*!
       * \brief Adds a feature, growing the extent to include it.
       * \param feature The feature to add; its bounds are computed here.
       */
      void addFeature(VectorFeature feature);

      /*!
       * \brief Called once loading is done, to pick sensible defaults.
       *
       * Polygons open translucent so what lies beneath them stays visible;
       * that is not a sensible default for points.
       */
      void finishLoading();

      /*!
       * \brief Discards every feature and its extent.
       */
      void clearFeatures();

      /*!
       * \brief Replaces one feature's attribute values.
       *
       * Values, unlike geometry, are re-read as a model runs; this is how a
       * live data item writes the new ones without rebuilding its features.
       *
       * \param feature Feature index.
       * \param attributes The replacement values, parallel to the fields.
       */
      void setFeatureAttributes(int feature, QVector<QVariant> attributes);

    private:
      void rebuildProjected() const;

      //! Builds the pick index if it is not current; cheap when it is.
      void ensurePickIndex() const;

      //! Whether \a feature is within \a tolerance of \a point.
      [[nodiscard]] bool featureHit(int feature, const QPointF &point,
                                    double tolerance) const;

      QVector<VectorFeature> m_features;
      QVector<AttributeField> m_fields;
      GeometryKind m_kind = GeometryKind::Point;
      QRectF m_extent;
      bool m_extentValid = false;

      //! Geometry in the map's CRS, rebuilt only when a CRS changes. Mutable
      //! because it is a cache: a const caller asking for the projected form
      //! is not changing the layer, it is paying for work not yet done.
      mutable QVector<QVector<QPolygonF>> m_projected;
      mutable bool m_projectionValid = false;


      QSet<int> m_selection;

      //! Where each feature is, for picking. Mutable and rebuilt with the
      //! projection it indexes, for the same reason that one is: a const
      //! caller asking what is under a point is not changing the layer.
      mutable CentroidIndex m_pickIndex;
      mutable bool m_pickIndexValid = false;

      LayerStyle m_style;
  };

  /*!
   * \brief Whether \a left and \a right stand on the same ground.
   *
   * Compared vertex by vertex, within a tolerance taken from the extent,
   * because two runs of one model over one mesh write the same numbers and
   * two runs over different meshes do not — and a matching feature count
   * proves neither. What it buys is the right to treat feature N of one as
   * feature N of the other, which is what both differencing two runs and
   * overlaying their series depend on.
   *
   * \param left One layer's geometry.
   * \param right The other's.
   * \param[out] message Why they are not the same ground.
   */
  [[nodiscard]] bool sameGeometry(const FeatureLayer &left,
                                  const FeatureLayer &right,
                                  QString &message);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_FEATURELAYER_H
