#include "map/maptool.h"

#include "core/preferencesmanager.h"
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
     * A preference, read on every gesture rather than once, so a change in
     * the dialog applies to the next press. One gesture has to be told from
     * the other by whether the view moved; zero would make every pick a
     * matter of holding perfectly still.
     */
    int clickSlopPixels()
    {
      return PreferencesManager::instance()->dragThresholdPixels();
    }

    //! What a click zooms by, when a zoom drag turns out to be a click.
    constexpr double kClickZoomFactor = 2.0;
  }

  MapTool::MapTool(MapCanvas *canvas) : m_canvas(canvas) {}

  MapTool::~MapTool() = default;

  QCursor MapTool::idleCursor() const
  {
    return Qt::ArrowCursor;
  }

  void MapTool::cancel() {}

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

    if (std::abs(travelled.x()) <= clickSlopPixels() &&
        std::abs(travelled.y()) <= clickSlopPixels())
    {
      // Shift adds and ⌘/Ctrl toggles, as every GIS does; the rule lives
      // with the stack so the map and the 3D view cannot disagree on it.
      m_canvas->pickAndSelectAt(event->pos(),
                                selectionModeFor(event->modifiers()));
    }

    return true;
  }

  QCursor PanTool::idleCursor() const
  {
    return Qt::OpenHandCursor;
  }

  // ── RubberBandTool ────────────────────────────────────────────────────────

  RubberBandTool::RubberBandTool(MapCanvas *canvas) : MapTool(canvas) {}

  RubberBandTool::~RubberBandTool() = default;

  bool RubberBandTool::press(QMouseEvent *event)
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
      m_band->setObjectName(QStringLiteral("mapRubberBand"));
    }

    m_band->setGeometry(QRect(m_origin, QSize()));
    m_band->show();

    return true;
  }

  bool RubberBandTool::move(QMouseEvent *event)
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

  bool RubberBandTool::release(QMouseEvent *event)
  {
    if (!m_dragging || event->button() != Qt::LeftButton)
    {
      return false;
    }

    const QRect rectangle = QRect(m_origin, event->pos()).normalized();

    // Asked once, here, so the click and the rectangle paths agree.
    m_selectionMode = selectionModeFor(event->modifiers());

    endGesture();

    // Width and height explicitly, not isNull() or isEmpty(): a zero-area
    // QRect reports itself null, so the obvious guard would also throw away
    // a legitimate one-pixel drag — and C4a already paid for that lesson
    // with QRectF and a point layer's bounds.
    if (rectangle.width() > clickSlopPixels() &&
        rectangle.height() > clickSlopPixels())
    {
      useRectangle(rectangle);
    }
    else
    {
      useClick(event->pos());
    }

    return true;
  }

  void RubberBandTool::endGesture()
  {
    m_dragging = false;

    if (m_band)
    {
      m_band->hide();
    }
  }

  // ── SelectTool ────────────────────────────────────────────────────────────

  void SelectTool::useRectangle(const QRect &rectangle)
  {
    m_canvas->selectIn(rectangle, m_selectionMode);
  }

  void SelectTool::useClick(const QPoint &pixel)
  {
    m_canvas->pickAndSelectAt(pixel, m_selectionMode);
  }

  QCursor SelectTool::idleCursor() const
  {
    return Qt::ArrowCursor;
  }

  // ── ZoomInTool ────────────────────────────────────────────────────────────

  void ZoomInTool::useRectangle(const QRect &rectangle)
  {
    m_canvas->zoomToScreenRect(rectangle);
  }

  void ZoomInTool::useClick(const QPoint &pixel)
  {
    // Too small to be a rectangle, so it was a click. Zooming about the
    // point is what every GIS does with one, and framing a degenerate
    // rectangle is what the alternative would do.
    m_canvas->zoomAtPixel(kClickZoomFactor, pixel);
  }

  QCursor ZoomInTool::idleCursor() const
  {
    return Qt::CrossCursor;
  }

  // ── ZoomOutTool ───────────────────────────────────────────────────────────

  void ZoomOutTool::useRectangle(const QRect &rectangle)
  {
    m_canvas->zoomOutToScreenRect(rectangle);
  }

  void ZoomOutTool::useClick(const QPoint &pixel)
  {
    m_canvas->zoomAtPixel(1.0 / kClickZoomFactor, pixel);
  }

  QCursor ZoomOutTool::idleCursor() const
  {
    return Qt::CrossCursor;
  }

  // ── TransectTool ──────────────────────────────────────────────────────────

  bool TransectTool::press(QMouseEvent *event)
  {
    if (event->button() != Qt::LeftButton || !m_canvas->transform().isValid())
    {
      return false;
    }

    m_drawing = true;
    m_origin = event->pos();

    return true;
  }

  bool TransectTool::move(QMouseEvent *event)
  {
    if (!m_drawing)
    {
      return false;
    }

    // Published as it is dragged, in world coordinates, so the preview stays
    // over the same ground if the view moves under it.
    m_canvas->setTransectLine(
      QPolygonF({m_canvas->transform().toWorld(m_origin),
                 m_canvas->transform().toWorld(event->pos())}));

    return true;
  }

  bool TransectTool::release(QMouseEvent *event)
  {
    if (!m_drawing || event->button() != Qt::LeftButton)
    {
      return false;
    }

    m_drawing = false;

    const QPoint travelled = event->pos() - m_origin;

    // Width and height separately, as the band tools check theirs: a section
    // drawn straight down a column is a vertical line, which has no width at
    // all and is exactly the drag someone cutting across a channel makes.
    if (std::abs(travelled.x()) <= clickSlopPixels()
        && std::abs(travelled.y()) <= clickSlopPixels())
    {
      // A click, not a line. The previous section is cleared rather than
      // left standing, the way clicking empty map clears a selection.
      m_canvas->setTransectLine({});
      return true;
    }

    m_canvas->setTransectLine(
      QPolygonF({m_canvas->transform().toWorld(m_origin),
                 m_canvas->transform().toWorld(event->pos())}));

    return true;
  }

  QCursor TransectTool::idleCursor() const
  {
    return Qt::CrossCursor;
  }

  void TransectTool::cancel()
  {
    if (m_drawing)
    {
      m_drawing = false;
      m_canvas->setTransectLine({});
    }
  }

} // namespace HydroCouple::Composer
