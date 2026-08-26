/*!
 * \file   spatialreference.h
 * \author Caleb Buahin
 * \brief  GDAL-backed coordinate reference systems and reprojection.
 *
 * Composer implements HydroCouple's own `ISpatialReferenceSystem` rather than
 * introducing a parallel CRS type: spatial data items already carry that
 * interface, so a layer, a component and the map all describe their CRS the
 * same way and no conversion layer is needed between them.
 *
 * GDAL/PROJ does the actual work. Reprojection is deliberately a separate
 * object with an explicit lifetime — building a coordinate transformation is
 * expensive enough that doing it per point, as a convenience function would,
 * is the difference between a map that pans and one that does not.
 */

#ifndef HYDROCOUPLECOMPOSER_GIS_SPATIALREFERENCE_H
#define HYDROCOUPLECOMPOSER_GIS_SPATIALREFERENCE_H

#include "hydrocouplespatial.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <memory>
#include <string>

class OGRSpatialReference;
class OGRCoordinateTransformation;

namespace HydroCouple::Composer
{

  /*!
   * \brief A coordinate reference system, backed by GDAL.
   */
  class SpatialReference final
    : public HydroCouple::Spatial::ISpatialReferenceSystem
  {
    public:
      ~SpatialReference() override;

      SpatialReference(const SpatialReference &) = delete;
      SpatialReference &operator=(const SpatialReference &) = delete;

      /*!
       * \brief Builds a CRS from an authority code, e.g. EPSG:4326.
       * \param authName Authority name, typically "EPSG".
       * \param code Code within that authority.
       * \param[out] message Diagnostic on failure.
       * \returns The CRS, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<SpatialReference> fromAuthority(
        const QString &authName, int code, QString &message);

      /*!
       * \brief Builds a CRS from WKT, a PROJ string, or "EPSG:xxxx".
       * \param definition The CRS definition text.
       * \param[out] message Diagnostic on failure.
       */
      [[nodiscard]] static std::unique_ptr<SpatialReference> fromDefinition(
        const QString &definition, QString &message);

      //! WGS 84 (EPSG:4326), the lingua franca for geographic coordinates.
      [[nodiscard]] static std::unique_ptr<SpatialReference> wgs84();

      //! WGS 84 / Pseudo-Mercator (EPSG:3857), used by tiled basemaps.
      [[nodiscard]] static std::unique_ptr<SpatialReference> webMercator();

      // ── ISpatialReferenceSystem ──────────────────────────────────────────

      [[nodiscard]] int authSRID() const override;

      [[nodiscard]] const std::string &authName() const override;

      [[nodiscard]] const std::string &srText() const override;

      [[nodiscard]] HydroCouple::IUnit::DistanceUnits distanceUnits()
        const override;

      // ── Extras ───────────────────────────────────────────────────────────

      /*!
       * \brief Human-readable name, e.g. "WGS 84 / Pseudo-Mercator".
       */
      [[nodiscard]] QString description() const;

      /*!
       * \brief Whether coordinates are angular (longitude/latitude).
       */
      [[nodiscard]] bool isGeographic() const;

      /*!
       * \brief Whether coordinates are linear — metres, feet, and the like.
       */
      [[nodiscard]] bool isProjected() const;

      /*!
       * \brief How many metres one of this system's linear units is.
       *
       * One for a metre-based system, 0.3048 for a foot-based one. Needed
       * wherever a length on the ground has to become a length in the world:
       * a scale computed without it reads the same 1:N over a survey-foot
       * state plane as over a metric one, and is wrong by 3.28 in the first.
       *
       * \returns The conversion factor, or 1 for a system with no linear
       *          units — a geographic one, whose units are degrees.
       */
      [[nodiscard]] double linearUnitsToMetres() const;

      /*!
       * \brief Whether this and \a other describe the same system.
       * \param other CRS to compare with.
       */
      [[nodiscard]] bool isSameAs(const SpatialReference &other) const;

      /*!
       * \brief The underlying GDAL object.
       */
      [[nodiscard]] OGRSpatialReference *handle() const;

    private:
      explicit SpatialReference(OGRSpatialReference *reference);

      void cacheIdentity();

      OGRSpatialReference *m_reference = nullptr;
      std::string m_authName;
      std::string m_srText;
      int m_authSRID = 0;
  };

  /*!
   * \brief Reprojects points between two coordinate reference systems.
   *
   * Construct once and reuse: PROJ resolves an operation pipeline on
   * construction, which is far too costly to repeat per point.
   */
  class CoordinateTransform
  {
    public:
      ~CoordinateTransform();

      CoordinateTransform(const CoordinateTransform &) = delete;
      CoordinateTransform &operator=(const CoordinateTransform &) = delete;

      /*!
       * \brief Builds a transform from \a source to \a target.
       * \param source Source CRS.
       * \param target Target CRS.
       * \param[out] message Diagnostic on failure.
       * \returns The transform, or nullptr when no operation exists.
       */
      [[nodiscard]] static std::unique_ptr<CoordinateTransform> between(
        const SpatialReference &source, const SpatialReference &target,
        QString &message);

      /*!
       * \brief Transforms one point.
       * \param point Point in the source CRS.
       * \param[out] ok Set to false when the point cannot be transformed.
       * \returns The point in the target CRS; unspecified when \a ok is false.
       */
      [[nodiscard]] QPointF transform(const QPointF &point,
                                      bool *ok = nullptr) const;

      /*!
       * \brief Transforms many points in one call.
       *
       * Points that fail are left unchanged and reported, because dropping
       * them silently would quietly deform a geometry instead of failing.
       *
       * \param points Points to transform, modified in place.
       * \returns The number of points that could not be transformed.
       */
      int transformInPlace(QVector<QPointF> &points) const;

      /*!
       * \brief Whether source and target are the same, making this a no-op.
       */
      [[nodiscard]] bool isIdentity() const;

    private:
      CoordinateTransform(OGRCoordinateTransformation *transform,
                          bool identity);

      OGRCoordinateTransformation *m_transform = nullptr;
      bool m_identity = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_GIS_SPATIALREFERENCE_H
