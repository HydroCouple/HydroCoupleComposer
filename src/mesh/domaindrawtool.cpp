#include "mesh/domaindrawtool.h"

#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "mesh/meshdomainmodel.h"

#include <QMouseEvent>

namespace HydroCouple::Composer
{
  DomainDrawTool::DomainDrawTool(MapCanvas *canvas, MeshDomainModel *model,
                                 DomainPart part)
    : MapTool(canvas), m_model(model), m_part(part)
  {
  }

  DomainPart DomainDrawTool::part() const
  {
    return m_part;
  }

  const QPolygonF &DomainDrawTool::pending() const
  {
    return m_pending;
  }

  int DomainDrawTool::minimumVertices(DomainPart part)
  {
    switch (part)
    {
      case DomainPart::Boundary:
      case DomainPart::Holes:
        return 3;

      case DomainPart::Breaklines:
        return 2;

      case DomainPart::ForcedPoints:
        return 1;
    }

    return 3;
  }

  bool DomainDrawTool::press(QMouseEvent *event)
  {
    if (!m_canvas->transform().isValid())
    {
      return false;
    }

    // Right-click finishes what has been drawn. Taken even when nothing has
    // been drawn yet, so a stray right-click does not fall through to
    // whatever the canvas would otherwise do with it.
    if (event->button() == Qt::RightButton)
    {
      finish();
      return true;
    }

    if (event->button() != Qt::LeftButton)
    {
      return false;
    }

    m_pending.append(m_canvas->transform().toWorld(event->pos()));

    // A point has nothing to accumulate, so the first click is the whole
    // gesture.
    if (m_part == DomainPart::ForcedPoints)
    {
      finish();
      return true;
    }

    refreshSketch({}, false);

    return true;
  }

  bool DomainDrawTool::move(QMouseEvent *event)
  {
    if (m_pending.isEmpty())
    {
      return false;
    }

    // The segment to the cursor is drawn but never stored: it shows where
    // the next vertex would go, and a version that appended it would grow
    // the shape by one vertex per mouse move.
    refreshSketch(m_canvas->transform().toWorld(event->pos()), true);

    return true;
  }

  bool DomainDrawTool::release(QMouseEvent *event)
  {
    Q_UNUSED(event)

    // Vertices are placed on the press, so there is nothing to do here. The
    // release is still taken, so it does not reach the canvas as a click on
    // empty map and clear the selection under the shape being drawn.
    return !m_pending.isEmpty();
  }

  void DomainDrawTool::refreshSketch(const QPointF &cursor, bool hasCursor)
  {
    QPolygonF shown = m_pending;

    if (hasCursor)
    {
      shown.append(cursor);
    }

    const bool closed = m_part == DomainPart::Boundary
                        || m_part == DomainPart::Holes;

    m_canvas->setSketch(shown, closed);
  }

  void DomainDrawTool::finish()
  {
    const QPolygonF drawn = m_pending;

    // Cleared first, so a model listener that repaints during the commit
    // cannot see both the sketch and the committed shape at once.
    m_pending.clear();
    m_canvas->setSketch({}, false);

    if (!m_model || drawn.size() < minimumVertices(m_part))
    {
      return;
    }

    switch (m_part)
    {
      case DomainPart::Boundary:
        m_model->setBoundary(drawn);
        break;

      case DomainPart::Holes:
        m_model->addHole(drawn);
        break;

      case DomainPart::Breaklines:
        m_model->addConstraintLine(drawn);
        break;

      case DomainPart::ForcedPoints:
        m_model->addPoint(drawn.first());
        break;
    }
  }

  void DomainDrawTool::cancel()
  {
    if (m_pending.isEmpty())
    {
      return;
    }

    // Discarded, not committed: a boundary abandoned after two clicks is not
    // a boundary, and putting it in the domain would leave the user to find
    // a ring they never finished.
    m_pending.clear();
    m_canvas->setSketch({}, false);
  }

  QCursor DomainDrawTool::idleCursor() const
  {
    return Qt::CrossCursor;
  }

} // namespace HydroCouple::Composer
