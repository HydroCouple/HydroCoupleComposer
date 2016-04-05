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
      bool canAcceptDropAsModelComponentInfo(const QMimeData* data);
      bool canAcceptDropAsModelComponentInfo(const QMimeData* data, QString& id);
      QRectF getRectFrom(const QPointF& p1, const QPointF p2);

   signals:
      void modelComponentInfoDropped(const QPointF& scenePos, const QString& id);
      void statusChanged(const QString& status);

   private:
      QPointF m_previousMousePosition;
      QPointF m_mousePressPosition;
      bool m_mouseIsPressed;
      Tool m_currentTool;
      QRectF m_zoomRect;
      QGraphicsRectItem m_zoomItem;
      qreal totalScaleFactor;

};

#endif // GRAPHICSVIEW_H
