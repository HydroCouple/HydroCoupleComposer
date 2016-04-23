#ifndef GCONNECTION_H
#define GCONNECTION_H

#include <QGraphicsObject>
#include <QPen>
#include <QBrush>
#include <QFont>

class GNode;
class GOutput;
class GInput;
class GAdaptedOutput;
class GExchangeItem;
class GModelComponent;

class GConnection : public QGraphicsObject
{
      friend class GNode;
      friend class GComponent;
      friend class GInput;
      friend class GOutput;
      friend class GAdaptedOutput;


      Q_OBJECT

      Q_PROPERTY(GNode* Producer READ producer)
      Q_PROPERTY(GNode* Consumer READ consumer)
      Q_PROPERTY(QPen Pen READ pen WRITE setPen)
      Q_PROPERTY(QBrush Brush READ brush WRITE setBrush)
      Q_PROPERTY(QPen SelectedPen READ selectedPen WRITE setSelectedPen)
      Q_PROPERTY(QFont Font READ font WRITE setFont)
      Q_PROPERTY(float ArrowLength READ arrowLength WRITE setArrowLength)
      Q_PROPERTY(float ArrowWidth READ arrowWidth WRITE setArrowWidth)

   private:

      GConnection();

      GConnection(GNode* producer, GNode* consumer , QGraphicsObject* parent = nullptr);

   public:
      virtual ~GConnection();

      GNode* producer() const;

      GNode* consumer() const;

      float arrowLength() const;

      void setArrowLength(float value);

      float arrowWidth() const;

      void setArrowWidth(float value);

      QPen pen() const;

      void setPen(const QPen& pen);

      QBrush brush() const;

      void setBrush(const QBrush& brush);

      QPen selectedPen() const;

      void setSelectedPen(const QPen& pen);

      QFont font() const;

      void setFont(const QFont& font);

      virtual QRectF boundingRect() const override;

      QPainterPath shape() const override;

      virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

      bool insertNode(GAdaptedOutput* adaptedOutput);

   signals:

      void doubleClicked(GConnection* connection);

   protected:

      QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

      void mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event) override;

   private slots:

      void parentLocationOrSizeChanged();

   private:

      QPointF minPosition(const QList<QPointF> & points);

      QPointF maxPosition(const QList<QPointF> & points);


   protected:
      GNode* m_producer, *m_consumer;
      QPointF m_start, m_end , m_mid , m_c1, m_c2;
      QPointF m_arrowPoint[3];
      QRectF m_boundary;
      QPainterPath m_path;
      QPen m_pen;
      QGraphicsTextItem* m_numConnectionsText;
      static QPen m_selectedPen;
      QBrush m_brush;
      QPointF m_labelLocation;
      static QFont m_font;
      static float m_arrowLength , m_arrowWidth;
      static int s_zindex;

};


Q_DECLARE_METATYPE(GConnection*)

#endif // GCONNECTION_H

