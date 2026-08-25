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

} // namespace HydroCouple::Composer
