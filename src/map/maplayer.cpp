#include "map/maplayer.h"

#include "gis/spatialreference.h"

#include <QUuid>

#include <algorithm>

namespace HydroCouple::Composer
{

  MapLayer::MapLayer(const QString &name, QObject *parent)
    : QObject(parent),
      m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_name(name)
  {
  }

  MapLayer::~MapLayer() = default;

  const LayerStyle *MapLayer::style() const
  {
    return nullptr;
  }

  LayerStyle *MapLayer::style()
  {
    return nullptr;
  }

  const ISceneSource *MapLayer::sceneSource() const
  {
    return nullptr;
  }

  bool MapLayer::isBasemap() const
  {
    return false;
  }

  QString MapLayer::attribution() const
  {
    return {};
  }

  QString MapLayer::sourceDescription() const
  {
    return m_sourceDescription;
  }

  void MapLayer::setSourceDescription(const QString &description)
  {
    m_sourceDescription = description;
  }

  const SpatialReference *MapLayer::mapCrs() const
  {
    return m_mapCrs.get();
  }

  void MapLayer::setMapCrs(std::shared_ptr<SpatialReference> crs)
  {
    m_mapCrs = std::move(crs);
    onMapCrsChanged();
  }

  void MapLayer::onMapCrsChanged()
  {
  }

  QString MapLayer::id() const
  {
    return m_id;
  }

  QString MapLayer::name() const
  {
    return m_name;
  }

  void MapLayer::setName(const QString &name)
  {
    if (name.isEmpty() || name == m_name)
    {
      return;
    }

    m_name = name;
    Q_EMIT nameChanged(m_name);
  }

  bool MapLayer::isVisible() const
  {
    return m_visible;
  }

  void MapLayer::setVisible(bool visible)
  {
    if (visible == m_visible)
    {
      return;
    }

    m_visible = visible;
    Q_EMIT appearanceChanged();
  }

  double MapLayer::opacity() const
  {
    return m_opacity;
  }

  void MapLayer::setOpacity(double opacity)
  {
    const double clamped = std::clamp(opacity, 0.0, 1.0);

    if (qFuzzyCompare(clamped + 1.0, m_opacity + 1.0))
    {
      return;
    }

    m_opacity = clamped;
    Q_EMIT appearanceChanged();
  }

  const SpatialReference *MapLayer::crs() const
  {
    return m_crs.get();
  }

  std::shared_ptr<SpatialReference> MapLayer::crsHandle() const
  {
    return m_crs;
  }

  void MapLayer::setCrs(std::shared_ptr<SpatialReference> crs)
  {
    m_crs = std::move(crs);

    // The extent is unchanged in the layer's own coordinates, but its
    // position on a map in any other CRS is not, so listeners that placed it
    // have to reconsider.
    Q_EMIT extentChanged();
    Q_EMIT appearanceChanged();
  }

  void MapLayer::notifyAppearanceChanged()
  {
    Q_EMIT appearanceChanged();
  }

  void MapLayer::notifyExtentChanged()
  {
    Q_EMIT extentChanged();
  }

} // namespace HydroCouple::Composer
