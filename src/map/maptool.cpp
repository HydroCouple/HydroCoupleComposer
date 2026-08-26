#include "map/maptool.h"

#include "map/mapcanvas.h"

#include <QMouseEvent>
#include <QRubberBand>

#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief How far the mouse may travel and still count as a click.
     *
     * The same number the canvas used when pan and pick shared a button, and
     * for the same reason: one gesture has to be told from the other by
     * whether the view moved.
     */
    constexpr int kClickSlopPixels = 3;

    //! What a click zooms by, when a zoom drag turns out to be a click.
    constexpr double kClickZoomFactor = 2.0;
  }

  MapTool::MapTool(MapCanvas *canvas) : m_canvas(canvas) {}

  MapTool::~MapTool() = default;

  QCursor MapTool::idleCursor() const
  {
    return Qt::ArrowCursor;
  }

  // ── PanTool ───────────────────────────────────────────────────────────────

  bool PanTool::press(QMouseEvent *event)
  {
    if (event->button() != Qt::LeftButton || !m_canvas->transform().isValid())
    {
      return false;
    }

    m_panning = true;
    m_pressPosition = event->pos();
    m_lastPosition = event->pos();

    m_canvas->setCursor(Qt::ClosedHandCursor);

    return true;
  }

  bool PanTool::move(QMouseEvent *event)
  {
    if (!m_panning)
    {
      return false;
    }

    m_canvas->panByPixels(QPointF(event->pos() - m_lastPosition));
    m_lastPosition = event->pos();

    return true;
  }

  bool PanTool::release(QMouseEvent *event)
  {
    if (!m_panning || event->button() != Qt::LeftButton)
    {
      return false;
    }

    m_panning = false;
    m_canvas->setCursor(idleCursor());

    // A press that did not move the view was a click, not a pan. Picking on
    // release rather than on press is also what keeps a drag that happens to
    // start on a feature from selecting it.
    const QPoint travelled = event->pos() - m_pressPosition;

    if (std::abs(travelled.x()) <= kClickSlopPixels &&
        std::abs(travelled.y()) <= kClickSlopPixels)
    {
      m_canvas->pickAndSelectAt(event->pos());
    }

    return true;
  }

  QCursor PanTool::idleCursor() const
  {
    return Qt::OpenHandCursor;
  }

  // ── ZoomTool ──────────────────────────────────────────────────────────────

  ZoomTool::ZoomTool(MapCanvas *canvas) : MapTool(canvas) {}

  ZoomTool::~ZoomTool() = default;

  bool ZoomTool::press(QMouseEvent *event)
  {
    if (event->button() != Qt::LeftButton || !m_canvas->transform().isValid())
    {
      return false;
    }

    m_dragging = true;
    m_origin = event->pos();

    if (!m_band)
    {
      m_band = std::make_unique<QRubberBand>(QRubberBand::Rectangle,
                                             m_canvas);
      m_band->setObjectName(QStringLiteral("zoomRubberBand"));
    }

    m_band->setGeometry(QRect(m_origin, QSize()));
    m_band->show();

    return true;
  }

  bool ZoomTool::move(QMouseEvent *event)
  {
    if (!m_dragging)
    {
      return false;
    }

    // normalized(), so dragging up and to the left draws a rectangle rather
    // than a negative one that shows nothing.
    m_band->setGeometry(QRect(m_origin, event->pos()).normalized());

    return true;
  }

  bool ZoomTool::release(QMouseEvent *event)
  {
    if (!m_dragging || event->button() != Qt::LeftButton)
    {
      return false;
    }

    const QRect rectangle = QRect(m_origin, event->pos()).normalized();

    endGesture();

    // Width and height explicitly, not isNull() or isEmpty(): a zero-area
    // QRect reports itself null, so the obvious guard would also throw away
    // a legitimate one-pixel drag — and C4a already paid for that lesson
    // with QRectF and a point layer's bounds.
    if (rectangle.width() > kClickSlopPixels &&
        rectangle.height() > kClickSlopPixels)
    {
      m_canvas->zoomToScreenRect(rectangle);
    }
    else
    {
      // Too small to be a rectangle, so it was a click. Zooming about the
      // point is what every GIS does with one, and framing a degenerate
      // rectangle is what the alternative would do.
      m_canvas->zoomAtPixel(kClickZoomFactor, event->pos());
    }

    return true;
  }

  void ZoomTool::endGesture()
  {
    m_dragging = false;

    if (m_band)
    {
      m_band->hide();
    }
  }

  QCursor ZoomTool::idleCursor() const
  {
    return Qt::CrossCursor;
  }

} // namespace HydroCouple::Composer
