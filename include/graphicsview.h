#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QMimeData>


class GraphicsView : public QGraphicsView
{
      Q_OBJECT

   public:
      enum Tool
      {
         Default,
         ZoomIn,
         ZoomOut,
         Pan,
      };

      Q_ENUMS(Tool)

      GraphicsView(QWidget *parent = 0);

      virtual ~GraphicsView();

   public slots:

      void onCurrentToolChanged(int currentTool);

   protected:

      void dragEnterEvent(QDragEnterEvent * event) override;

      void dragMoveEvent(QDragMoveEvent * event) override;

      void dropEvent(QDropEvent * event) override;

      void wheelEvent(QWheelEvent * event) override;

      void mousePressEvent(QMouseEvent * event) override;

      void mouseReleaseEvent(QMouseEvent * event) override;

      void mouseMoveEvent(QMouseEvent * event) override;

      bool viewportEvent(QEvent *event) override;

   private:
      bool canAcceptDrop(const QMimeData* data);

      bool canAcceptDrop(const QMimeData* data , QMap<QString,QVariant>& dropData);

      QRectF getRectFrom(const QPointF &p1, const QPointF p2);

   signals:
      void itemDropped(const QPointF &scenePos, const QMap<QString,QVariant>& data);

      void statusChanged(const QString& status);

   private:
      QPointF m_scenePosition;
      QPointF m_viewportPosition;
      QPointF m_mousePressPosition;
      bool m_mouseIsPressed, m_isBusy;
      Tool m_currentTool;
      QRectF m_zoomRect;
      QGraphicsRectItem m_zoomItem;
      qreal totalScaleFactor;


};

#endif // GRAPHICSVIEW_H
