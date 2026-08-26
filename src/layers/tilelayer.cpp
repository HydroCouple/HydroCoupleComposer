#include "layers/tilelayer.h"

#include "map/maptransform.h"

#include <QPainter>

namespace HydroCouple::Composer
{

  TileLayer::TileLayer(const QString &name,
                       std::unique_ptr<ITileSource> source, QObject *parent)
    : MapLayer(name, parent), m_source(std::move(source))
  {
  }

  TileLayer::~TileLayer() = default;

  QRectF TileLayer::extent() const
  {
    return TileGrid::worldExtent();
  }

  bool TileLayer::isBasemap() const
  {
    return true;
  }

  QString TileLayer::attribution() const
  {
    return m_source ? m_source->attribution() : QString();
  }

  QString TileLayer::sourceDescription() const
  {
    // The attribution names the provider, which is what a basemap's source
    // is; asking the source interface for a second, near-identical string
    // would widen it for nothing.
    return attribution();
  }

  ITileSource *TileLayer::source() const
  {
    return m_source.get();
  }

  int TileLayer::lastZoom() const
  {
    return m_lastZoom;
  }

  int TileLayer::lastDrawnTileCount() const
  {
    return m_lastDrawnTileCount;
  }

  void TileLayer::onTileReady()
  {
    // The picture changed, so the scene's copy of it is stale. Dropped rather
    // than redrawn: a tile arriving for a map nobody is looking in 3D at
    // should not cost a megapixel of compositing.
    m_groundFocus = QRectF();

    notifyAppearanceChanged();
  }

  void TileLayer::render(QPainter &painter, const MapTransform &transform)
  {
    m_lastDrawnTileCount = 0;

    if (!m_source || !transform.isValid())
    {
      return;
    }

    const int zoom =
      qMin(TileGrid::zoomForScale(transform.scale()), m_source->maximumZoom());

    // A pan that outruns the network leaves requests for tiles nobody will
    // look at; the ones still wanted are asked for again below.
    if (zoom != m_lastZoom)
    {
      m_source->cancelPending();
    }

    m_lastZoom = zoom;

    const QVector<TilePlacement> placements =
      TileGrid::tilesForExtent(transform.visibleExtent(), zoom);

    // Off, so adjacent tiles do not blend along their shared edge and leave a
    // visible grid of seams across the map.
    const bool wasSmoothing =
      painter.testRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (const TilePlacement &placement : placements)
    {
      const QImage image = m_source->tile(placement.id);

      if (image.isNull())
      {
        m_source->request(placement.id);
        continue;
      }

      // Two opposite world corners; which of them lands where on screen is
      // settled by normalising, and the tile image is north-up either way.
      // Rounded to whole pixels: a tile boundary at a fractional position
      // leaves a partly-transparent seam column between neighbours.
      const QPointF cornerA =
        transform.toScreen(placement.extent.topLeft());
      const QPointF cornerB =
        transform.toScreen(placement.extent.bottomRight());

      const QRectF target(QPointF(qRound(cornerA.x()), qRound(cornerA.y())),
                          QPointF(qRound(cornerB.x()), qRound(cornerB.y())));

      painter.drawImage(target.normalized(), image);
      ++m_lastDrawnTileCount;
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, wasSmoothing);
  }

  const ISceneSource *TileLayer::sceneSource() const
  {
    return this;
  }

  Bounds3D TileLayer::sceneBounds() const
  {
    return {};
  }

  QVector<SceneGeometry> TileLayer::sceneGeometry(
    const SceneContext &context) const
  {
    QVector<SceneGeometry> batches;

    const QRectF focus = context.focus.normalized();

    if (focus.isEmpty())
    {
      return batches;
    }

    if (m_groundFocus != focus || !m_ground.isValid())
    {
      // The const_cast is the layer drawing itself into its own cache; see
      // GdalRasterLayer::sceneGeometry() for why render() being non-const
      // does not make this a mutation.
      m_ground = renderLayerToImage(const_cast<TileLayer &>(*this), focus,
                                    kGroundTexturePixels);
      m_groundFocus = focus;
    }

    SceneGeometry ground = buildGroundPlane(m_ground, context.terrain);

    if (!ground.isEmpty())
    {
      batches.append(std::move(ground));
    }

    return batches;
  }

} // namespace HydroCouple::Composer
