#include "map/maptransform.h"

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{

  MapTransform::MapTransform(const QRectF &extent, const QSizeF &viewport)
  {
    fit(extent, viewport, 0.0);
  }

  void MapTransform::fit(const QRectF &extent, const QSizeF &viewport,
                         double marginFraction)
  {
    m_viewport = viewport;

    if (viewport.width() <= 0.0 || viewport.height() <= 0.0)
    {
      return;
    }

    m_center = extent.center();

    // A degenerate extent — a single point, or a perfectly straight line of
    // features — still has to produce a usable view rather than an infinite
    // or zero scale.
    const double width = extent.width() > 0.0 ? extent.width() : 1.0;
    const double height = extent.height() > 0.0 ? extent.height() : 1.0;

    const double margin = 1.0 + std::max(0.0, marginFraction) * 2.0;

    const double scaleX = viewport.width() / (width * margin);
    const double scaleY = viewport.height() / (height * margin);

    // The smaller scale is the one that fits both axes.
    m_scale = std::min(scaleX, scaleY);
  }

  QPointF MapTransform::toScreen(const QPointF &world) const
  {
    // World Y grows northward, widget Y grows downward, hence the negation.
    return QPointF(
      m_viewport.width() * 0.5 + (world.x() - m_center.x()) * m_scale,
      m_viewport.height() * 0.5 - (world.y() - m_center.y()) * m_scale);
  }

  QPointF MapTransform::toWorld(const QPointF &screen) const
  {
    if (m_scale <= 0.0)
    {
      return m_center;
    }

    return QPointF(
      m_center.x() + (screen.x() - m_viewport.width() * 0.5) / m_scale,
      m_center.y() - (screen.y() - m_viewport.height() * 0.5) / m_scale);
  }

  QRectF MapTransform::visibleExtent() const
  {
    if (!isValid())
    {
      return {};
    }

    const QPointF topLeft = toWorld(QPointF(0.0, 0.0));
    const QPointF bottomRight =
      toWorld(QPointF(m_viewport.width(), m_viewport.height()));

    return QRectF(QPointF(topLeft.x(), bottomRight.y()),
                  QPointF(bottomRight.x(), topLeft.y()))
      .normalized();
  }

  double MapTransform::scale() const
  {
    return m_scale;
  }

  QSizeF MapTransform::viewport() const
  {
    return m_viewport;
  }

  void MapTransform::setViewport(const QSizeF &viewport)
  {
    // Centre and scale are preserved, so a resize reveals more of the world
    // rather than rescaling what is already drawn.
    m_viewport = viewport;
  }

  void MapTransform::panByPixels(const QPointF &pixels)
  {
    if (m_scale <= 0.0)
    {
      return;
    }

    m_center -= QPointF(pixels.x() / m_scale, -pixels.y() / m_scale);
  }

  void MapTransform::zoomAt(double factor, const QPointF &anchor)
  {
    if (factor <= 0.0 || m_scale <= 0.0)
    {
      return;
    }

    // Hold the world point under the anchor fixed: read it before the scale
    // changes, then shift the centre so it lands back on the same pixel.
    const QPointF worldUnderAnchor = toWorld(anchor);

    m_scale *= factor;

    const QPointF worldAfter = toWorld(anchor);
    m_center += worldUnderAnchor - worldAfter;
  }

  bool MapTransform::isValid() const
  {
    return m_scale > 0.0 && m_viewport.width() > 0.0 &&
           m_viewport.height() > 0.0;
  }

} // namespace HydroCouple::Composer
