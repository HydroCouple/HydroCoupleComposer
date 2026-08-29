#include "mesh/domainedittool.h"

#include "map/mapcanvas.h"
#include "map/maptransform.h"
#include "mesh/domainsnap.h"
#include "mesh/meshdomainmodel.h"

#include <QMouseEvent>

namespace HydroCouple::Composer
{
  DomainEditTool::DomainEditTool(MapCanvas *canvas, MeshDomainModel *model)
    : QObject(nullptr), MapTool(canvas), m_model(model)
  {
    if (m_model)
    {
      connect(m_model, &MeshDomainModel::domainChanged, this,
              [this] { refreshHandles(); });
    }

    // Published straight away: picking up the editor is how the user asks to
    // see what there is to grab.
    refreshHandles();
  }

  DomainEditTool::~DomainEditTool() = default;

  const DomainVertex &DomainEditTool::grabbed() const
  {
    return m_grabbed;
  }

  double DomainEditTool::tolerance() const
  {
    return worldTolerance(kSnapPixels, m_canvas->transform().scale());
  }

  bool DomainEditTool::press(QMouseEvent *event)
  {
    if (!m_model || !m_canvas->transform().isValid())
    {
      return false;
    }

    const QPointF world = m_canvas->transform().toWorld(event->pos());

    if (event->button() == Qt::RightButton)
    {
      const DomainSnap hit =
        nearestVertex(m_model->domain(), world, tolerance());

      if (hit.hit)
      {
        m_hovered = DomainVertex{};
        m_model->removeVertex(hit.at);
      }

      // Taken either way, so a right-click that missed does not fall through
      // to whatever the canvas would otherwise make of it.
      return true;
    }

    if (event->button() != Qt::LeftButton)
    {
      return false;
    }

    DomainSnap hit = nearestVertex(m_model->domain(), world, tolerance());

    if (!hit.hit)
    {
      // No vertex, but perhaps an edge: adding a corner where there is none
      // is half of correcting a domain, and asking the user to redraw the
      // ring to do it is the other half of not having this tool.
      const DomainSnap edge =
        nearestEdge(m_model->domain(), world, tolerance());

      if (!edge.hit || !m_model->insertVertex(edge.at, edge.point))
      {
        return false;
      }

      hit = edge;
    }

    m_grabbed = hit.at;
    m_dragging = true;

    // Read back from the domain rather than taken from the snap, so an
    // insert that landed somewhere the model adjusted is still what cancel()
    // restores.
    vertexPosition(m_model->domain(), m_grabbed, m_original);

    refreshHandles();

    return true;
  }

  bool DomainEditTool::move(QMouseEvent *event)
  {
    if (!m_model || !m_canvas->transform().isValid())
    {
      return false;
    }

    const QPointF world = m_canvas->transform().toWorld(event->pos());

    if (!m_dragging)
    {
      const DomainSnap hover =
        nearestVertex(m_model->domain(), world, tolerance());
      const DomainVertex was = m_hovered;

      m_hovered = hover.hit ? hover.at : DomainVertex{};

      if (m_hovered != was)
      {
        refreshHandles();
      }

      // Not taken: hovering is not a gesture, and swallowing the move would
      // stop the canvas reporting the pointer's position.
      return false;
    }

    // Snapped to another vertex when one is within reach, so two shapes can
    // be made to meet exactly. Excluding the vertex being dragged, which is
    // always within reach of itself and would otherwise pin the drag to
    // where it started.
    const DomainSnap snap =
      nearestVertex(m_model->domain(), world, tolerance(), m_grabbed);

    m_model->moveVertex(m_grabbed, snap.hit ? snap.point : world);

    return true;
  }

  bool DomainEditTool::release(QMouseEvent *event)
  {
    Q_UNUSED(event)

    if (!m_dragging)
    {
      return false;
    }

    m_dragging = false;
    m_grabbed = DomainVertex{};

    refreshHandles();

    return true;
  }

  void DomainEditTool::cancel()
  {
    if (m_dragging && m_model)
    {
      m_model->moveVertex(m_grabbed, m_original);
    }

    m_dragging = false;
    m_grabbed = DomainVertex{};
    m_hovered = DomainVertex{};
    m_handles.clear();

    // The handles belong to the tool, not to the domain: leaving them behind
    // would offer the user something to grab that nothing is listening for.
    m_canvas->setVertexHandles({}, -1);
  }

  void DomainEditTool::refreshHandles()
  {
    if (!m_model)
    {
      m_handles.clear();
      m_canvas->setVertexHandles({}, -1);

      return;
    }

    const QVector<QPointF> vertices =
      domainVertices(m_model->domain(), m_handles);

    const DomainVertex active = m_dragging ? m_grabbed : m_hovered;

    m_canvas->setVertexHandles(
      vertices, active.isValid() ? int(m_handles.indexOf(active)) : -1);
  }

  QCursor DomainEditTool::idleCursor() const
  {
    return Qt::PointingHandCursor;
  }

} // namespace HydroCouple::Composer
