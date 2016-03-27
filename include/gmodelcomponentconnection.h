#ifndef GMODELCOMPONENTCONNECTION
#define GMODELCOMPONENTCONNECTION

#include "gmodelcomponent.h"

class GModelComponentConnection : public QGraphicsObject
{

      Q_OBJECT
      Q_PROPERTY(GModelComponent* ProducerComponent READ producerComponent)
      Q_PROPERTY(GModelComponent* ConsumerComponent READ consumerComponent)
      //Q_PROPERTY(QList<GComponentItemConnection*> ItemConnections READ itemConnections)
      Q_PROPERTY(QPen Pen READ pen WRITE setPen)
      Q_PROPERTY(QPen SelectedPen READ selectedPen WRITE setSelectedPen)
      Q_PROPERTY(float ArrowLength READ arrowLength WRITE setArrowLength)
      Q_PROPERTY(float ArrowWidth READ arrowWidth WRITE setArrowWidth)
      Q_PROPERTY(QGraphicsTextItem* NumberOfConnectionsGraphicsTextItem READ numberOfConnectionsGraphicsTextItem)

   public:
      GModelComponentConnection(GModelComponent* const producer, GModelComponent* const consumer);

      virtual ~GModelComponentConnection();

      GModelComponent* producerComponent() const;

      GModelComponent* consumerComponent() const;

      bool equals(const GModelComponentConnection* connection) const ;

      void addComponentItemConnection(GModelComponentConnection* connection);

      bool removeComponentItemConnection(GModelComponentConnection* connection);

      //QList<GComponentItemConnection*> itemConnections() const;

      float arrowLength() const;

      void setArrowLength(float value);

      float arrowWidth() const;

      void setArrowWidth(float value);

      QPen pen() const;

      void setPen(const QPen& pen);

      QPen selectedPen() const;

      void setSelectedPen(const QPen& pen);

      //bounding rect for this component
      QRectF	boundingRect() const;

      QGraphicsTextItem* numberOfConnectionsGraphicsTextItem() const;

      //paint component
      void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0) override;

      /*!
       * \brief QGraphicsItem::itemChange
       * \param change
       * \param value
       * \return
       */
      QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;


      QPainterPath shape() const override;

      void mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event) override;

   signals:
      void doubleClicked(QGraphicsSceneMouseEvent * event);

   private:
      QPointF minPosition(const QList<QPointF> & points);

      QPointF maxPosition(const QList<QPointF> & points);

      //bool containsConnection(GComponentItemConnection* const connection);

   public slots:
      void parentLocationOrSizeChanged();

   private:
      GModelComponent* m_producerModel;
      GModelComponent* m_consumerModel;
      QGraphicsTextItem* m_numConnectionsText;
      //QList<GComponentItemConnection*> m_itemConnections;
      QRectF m_boundary;
      QPointF m_start, m_end , m_mid , m_c1, m_c2;
      QPointF m_arrowPoint[3];
      QPainterPath m_path;
      static QPen m_pen, m_selectedPen;
      static float m_arrowLength , m_arrowWidth;
      static int zLocation;
};

#endif // GMODELCOMPONENTCONNECTION

