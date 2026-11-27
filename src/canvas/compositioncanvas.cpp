#include "canvas/compositioncanvas.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QWheelEvent>

namespace HydroCouple::Composer
{

  CompositionCanvas::CompositionCanvas(CompositionScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent),
      m_scene(scene)
  {
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(true);
    setObjectName(QStringLiteral("compositionCanvas"));
  }

  CompositionScene *CompositionCanvas::compositionScene() const
  {
    return m_scene;
  }

  void CompositionCanvas::frameComposition()
  {
    if (!scene())
    {
      return;
    }

    const QRectF bounds = scene()->itemsBoundingRect();

    if (bounds.isEmpty())
    {
      return;
    }

    // A margin, so the outermost ports are not flush against the frame and
    // still have room for the connection stubs that leave them.
    fitInView(bounds.adjusted(-40.0, -40.0, 40.0, 40.0),
              Qt::KeepAspectRatio);
  }

  void CompositionCanvas::keyPressEvent(QKeyEvent *event)
  {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
      if (m_scene && m_scene->removeSelection() > 0)
      {
        event->accept();
        return;
      }
    }

    QGraphicsView::keyPressEvent(event);
  }

  void CompositionCanvas::wheelEvent(QWheelEvent *event)
  {
    if (event->modifiers() & Qt::ControlModifier)
    {
      const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
      scale(factor, factor);
      event->accept();
      return;
    }

    QGraphicsView::wheelEvent(event);
  }

  void CompositionCanvas::dragEnterEvent(QDragEnterEvent *event)
  {
    if (event->mimeData()->hasFormat(QLatin1String(kComponentMimeType)))
    {
      event->acceptProposedAction();
      return;
    }

    QGraphicsView::dragEnterEvent(event);
  }

  void CompositionCanvas::dragMoveEvent(QDragMoveEvent *event)
  {
    if (event->mimeData()->hasFormat(QLatin1String(kComponentMimeType)))
    {
      event->acceptProposedAction();
      return;
    }

    QGraphicsView::dragMoveEvent(event);
  }

  void CompositionCanvas::dropEvent(QDropEvent *event)
  {
    if (!m_scene ||
        !event->mimeData()->hasFormat(QLatin1String(kComponentMimeType)))
    {
      QGraphicsView::dropEvent(event);
      return;
    }

    const QString componentInfoId = QString::fromUtf8(
      event->mimeData()->data(QLatin1String(kComponentMimeType)));

    if (componentInfoId.isEmpty())
    {
      event->ignore();
      return;
    }

    m_scene->addComponentAt(componentInfoId, componentInfoId,
                            mapToScene(event->position().toPoint()));

    event->acceptProposedAction();
  }

} // namespace HydroCouple::Composer
