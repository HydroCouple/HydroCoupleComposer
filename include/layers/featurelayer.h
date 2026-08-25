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

#include <QPolygonF>
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
  class FeatureLayer : public MapLayer, public IAttributeProvider
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

    protected:
      void onMapCrsChanged() override;

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
      void rebuildProjected();

      QVector<VectorFeature> m_features;
      QVector<AttributeField> m_fields;
      GeometryKind m_kind = GeometryKind::Point;
      QRectF m_extent;
      bool m_extentValid = false;

      //! Geometry in the map's CRS, rebuilt only when a CRS changes.
      QVector<QVector<QPolygonF>> m_projected;
      bool m_projectionValid = false;

      LayerStyle m_style;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_LAYERS_FEATURELAYER_H
