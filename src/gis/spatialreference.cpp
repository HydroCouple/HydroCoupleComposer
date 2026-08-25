#include "gis/spatialreference.h"

#include <ogr_spatialref.h>

namespace HydroCouple::Composer
{

  namespace
  {
    //! Maps a CRS's linear unit onto the interface's distance enumeration.
    HydroCouple::IUnit::DistanceUnits distanceUnitsFor(
      const OGRSpatialReference &reference)
    {
      if (reference.IsGeographic())
      {
        // Angular coordinates are degrees — the interface has a value for
        // exactly this, so it is reported rather than flattened to Unknown.
        return HydroCouple::IUnit::DistanceUnits::Degrees;
      }

      const double metres = reference.GetLinearUnits(nullptr);

      // Compared with a tolerance: PROJ stores conversion factors, and exact
      // equality on doubles would classify a legitimate foot CRS as unknown.
      if (std::abs(metres - 1.0) < 1e-9)
      {
        return HydroCouple::IUnit::DistanceUnits::Meters;
      }

      if (std::abs(metres - 0.3048) < 1e-6)
      {
        return HydroCouple::IUnit::DistanceUnits::Feet;
      }

      if (std::abs(metres - 1000.0) < 1e-6)
      {
        return HydroCouple::IUnit::DistanceUnits::Kilometers;
      }

      return HydroCouple::IUnit::DistanceUnits::Unknown;
    }
  } // namespace

  // ── SpatialReference ──────────────────────────────────────────────────────

  SpatialReference::SpatialReference(OGRSpatialReference *reference)
    : m_reference(reference)
  {
    cacheIdentity();
  }

  SpatialReference::~SpatialReference()
  {
    if (m_reference)
    {
      m_reference->Release();
      m_reference = nullptr;
    }
  }

  void SpatialReference::cacheIdentity()
  {
    if (!m_reference)
    {
      return;
    }

    if (const char *authority = m_reference->GetAuthorityName(nullptr))
    {
      m_authName = authority;
    }

    if (const char *code = m_reference->GetAuthorityCode(nullptr))
    {
      m_authSRID = std::atoi(code);
    }

    char *wkt = nullptr;

    if (m_reference->exportToWkt(&wkt) == OGRERR_NONE && wkt)
    {
      m_srText = wkt;
    }

    CPLFree(wkt);
  }

  std::unique_ptr<SpatialReference> SpatialReference::fromAuthority(
    const QString &authName, int code, QString &message)
  {
    return fromDefinition(QStringLiteral("%1:%2").arg(authName).arg(code),
                          message);
  }

  std::unique_ptr<SpatialReference> SpatialReference::fromDefinition(
    const QString &definition, QString &message)
  {
    if (definition.trimmed().isEmpty())
    {
      message = QStringLiteral("empty CRS definition");
      return nullptr;
    }

    auto *reference = new OGRSpatialReference;

    // Traditional axis order keeps coordinates as (x, y) / (longitude,
    // latitude) throughout. Without it, PROJ honours each CRS's declared axis
    // order and EPSG:4326 arrives as (latitude, longitude) — which silently
    // transposes every map.
    reference->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    if (reference->SetFromUserInput(definition.toUtf8().constData()) !=
        OGRERR_NONE)
    {
      message = QStringLiteral("'%1' is not a recognised CRS").arg(definition);
      reference->Release();
      return nullptr;
    }

    return std::unique_ptr<SpatialReference>(new SpatialReference(reference));
  }

  std::unique_ptr<SpatialReference> SpatialReference::wgs84()
  {
    QString message;
    return fromDefinition(QStringLiteral("EPSG:4326"), message);
  }

  std::unique_ptr<SpatialReference> SpatialReference::webMercator()
  {
    QString message;
    return fromDefinition(QStringLiteral("EPSG:3857"), message);
  }

  int SpatialReference::authSRID() const
  {
    return m_authSRID;
  }

  const std::string &SpatialReference::authName() const
  {
    return m_authName;
  }

  const std::string &SpatialReference::srText() const
  {
    return m_srText;
  }

  HydroCouple::IUnit::DistanceUnits SpatialReference::distanceUnits() const
  {
    return m_reference ? distanceUnitsFor(*m_reference)
                       : HydroCouple::IUnit::DistanceUnits::Unknown;
  }

  QString SpatialReference::description() const
  {
    if (!m_reference)
    {
      return {};
    }

    const char *name = m_reference->GetName();

    return name ? QString::fromUtf8(name) : QString();
  }

  bool SpatialReference::isGeographic() const
  {
    return m_reference && m_reference->IsGeographic();
  }

  bool SpatialReference::isSameAs(const SpatialReference &other) const
  {
    if (!m_reference || !other.m_reference)
    {
      return false;
    }

    return m_reference->IsSame(other.m_reference) == TRUE;
  }

  OGRSpatialReference *SpatialReference::handle() const
  {
    return m_reference;
  }

  // ── CoordinateTransform ───────────────────────────────────────────────────

  CoordinateTransform::CoordinateTransform(
    OGRCoordinateTransformation *transform, bool identity)
    : m_transform(transform),
      m_identity(identity)
  {
  }

  CoordinateTransform::~CoordinateTransform()
  {
    if (m_transform)
    {
      OGRCoordinateTransformation::DestroyCT(m_transform);
      m_transform = nullptr;
    }
  }

  std::unique_ptr<CoordinateTransform> CoordinateTransform::between(
    const SpatialReference &source, const SpatialReference &target,
    QString &message)
  {
    if (!source.handle() || !target.handle())
    {
      message = QStringLiteral("a coordinate reference system is missing");
      return nullptr;
    }

    // Same CRS: skip PROJ entirely rather than build an identity pipeline.
    if (source.isSameAs(target))
    {
      return std::unique_ptr<CoordinateTransform>(
        new CoordinateTransform(nullptr, true));
    }

    OGRCoordinateTransformation *transform =
      OGRCreateCoordinateTransformation(source.handle(), target.handle());

    if (!transform)
    {
      message = QStringLiteral("no transformation from '%1' to '%2'")
                  .arg(source.description(), target.description());
      return nullptr;
    }

    return std::unique_ptr<CoordinateTransform>(
      new CoordinateTransform(transform, false));
  }

  bool CoordinateTransform::isIdentity() const
  {
    return m_identity;
  }

  QPointF CoordinateTransform::transform(const QPointF &point, bool *ok) const
  {
    if (m_identity)
    {
      if (ok)
      {
        *ok = true;
      }

      return point;
    }

    if (!m_transform)
    {
      if (ok)
      {
        *ok = false;
      }

      return point;
    }

    double x = point.x();
    double y = point.y();

    const int success = m_transform->Transform(1, &x, &y) ? 1 : 0;

    if (ok)
    {
      *ok = success == 1;
    }

    return success ? QPointF(x, y) : point;
  }

  int CoordinateTransform::transformInPlace(QVector<QPointF> &points) const
  {
    if (m_identity || points.isEmpty())
    {
      return 0;
    }

    if (!m_transform)
    {
      return points.size();
    }

    // One PROJ call for the whole run; per-point calls dominate pan and zoom.
    QVector<double> xs;
    QVector<double> ys;
    xs.reserve(points.size());
    ys.reserve(points.size());

    for (const QPointF &point : points)
    {
      xs.append(point.x());
      ys.append(point.y());
    }

    QVector<int> results(points.size(), 0);

    m_transform->Transform(points.size(), xs.data(), ys.data(), nullptr,
                           results.data());

    int failures = 0;

    for (int index = 0; index < points.size(); ++index)
    {
      if (results[index])
      {
        points[index] = QPointF(xs[index], ys[index]);
      }
      else
      {
        // Left as it was: silently dropping a vertex would deform the
        // geometry rather than report a problem.
        ++failures;
      }
    }

    return failures;
  }

} // namespace HydroCouple::Composer
