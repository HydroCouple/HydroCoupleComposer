#ifndef GNODE_H
#define GNODE_H

#include <QGraphicsObject>
#include <QPen>
#include <QBrush>
#include <QFont>
#include "gconnection.h"

class GNode : public QGraphicsObject
{

      Q_OBJECT

   public:
      enum NodeType
      {
         Input,
         MultiInput,
         Output,
         AdaptedOutput,
         Component
      };

      Q_ENUMS(NodeType)

      Q_PROPERTY(GNode::NodeType NodeType READ nodeType)
      Q_PROPERTY(int Size READ size WRITE setSize)
      Q_PROPERTY(int Margin READ margin WRITE setMargin)
      Q_PROPERTY(int CornerRadius READ cornerRadius WRITE setCornerRadius)
      Q_PROPERTY(QPen Pen READ pen WRITE setPen NOTIFY propertyChanged)
      Q_PROPERTY(QBrush Brush READ brush WRITE setBrush NOTIFY propertyChanged)
      Q_PROPERTY(QPen SelectedPen READ selectedPen WRITE setSelectedPen NOTIFY propertyChanged)
      Q_PROPERTY(QBrush SelectedBrush READ selectedBrush WRITE setSelectedBrush NOTIFY propertyChanged)
      Q_PROPERTY(QFont Font READ font WRITE setFont NOTIFY propertyChanged)
      Q_PROPERTY(QList<GConnection*> Connections READ connections NOTIFY propertyChanged)

   public:
      GNode(const QString& id, const QString& caption, NodeType nodeType, QGraphicsObject* parent = nullptr);

      virtual ~GNode();

      QString id() const;

      void setId(const QString & id);

      QString caption() const;

      void setCaption(const QString& caption);

      NodeType nodeType() const;

      int size() const;

      virtual void setSize(int size);

      int margin() const;

      virtual void setMargin(int margin);

      int cornerRadius() const;

      virtual void setCornerRadius(int cornerRadius);

      QPen pen() const;

      virtual void setPen(const QPen& pen);

      QBrush brush() const;

      virtual void setBrush(const QBrush& brush);

      QPen selectedPen() const;

      virtual void setSelectedPen(const QPen& selectedPen);

      QBrush selectedBrush() const;

      virtual void setSelectedBrush(const QBrush& selectedBrush);

      QFont font() const;

      virtual void setFont(const QFont& font);

      virtual QRectF boundingRect() const override;

      QList<GConnection*> connections() const;

      virtual bool createConnection(GNode* consumer);

      virtual bool deleteConnection(GConnection* connection);

      virtual bool deleteConnection(GNode* consumer);

      void deleteConnections();
      
      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

   signals:

      void doubleClicked(GNode * component);

      virtual void propertyChanged(const QString& propertyName, const QVariant& value);

      void connectionAdded(GConnection *connection);

   protected:

      virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

      void mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event) override;

   private slots:

      virtual void onCreateTextItem();

      virtual void onChildPropertyChanged();

   protected:
      int m_margin , m_size, m_cornerRadius , m_xmargin, m_ymargin , m_height , m_width;
      QString m_id, m_caption;
      NodeType m_nodeType;
      QGraphicsTextItem* m_textItem ;
      QRectF m_boundingRect;
      QPen m_pen ;
      static QPen s_selectedPen;
      QBrush m_brush;
      static QBrush s_selectedBrush;
      static const QString sc_captionHtml;
      QFont m_font;
      QList<GConnection*> m_connections;
      static int s_zindex;
};

Q_DECLARE_METATYPE(GNode*)

#endif // GNODE_H

