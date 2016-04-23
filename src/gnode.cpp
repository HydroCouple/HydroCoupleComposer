#include "stdafx.h"
#include "gnode.h"
#include "gdefaultselectiongraphic.h"
#include "assert.h"
#include "gexchangeitems.h"

const QString GNode::sc_captionHtml =
      "<h2 align=\"center\">[Caption]</h2>"
      "<h3 align=\"center\">[Id]</h3>";


int GNode::s_zindex(1000);
QBrush GNode::s_selectedBrush(QColor(255, 230, 220), Qt::BrushStyle::SolidPattern);
QPen GNode::s_selectedPen(QBrush(QColor(255, 0, 0), Qt::BrushStyle::SolidPattern), 4.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

GNode::GNode(const QString& id, const QString& caption, NodeType type, QGraphicsObject* parent)
   :QGraphicsObject(parent),
     m_id(id), m_caption(caption) , m_nodeType(type), m_margin(15),
     m_font(QFont())
{
   m_textItem = new QGraphicsTextItem(this);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsMovable, true);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsSelectable, true);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsFocusable, true);


   setFlag(GraphicsItemFlag::ItemIsMovable, true);
   setFlag(GraphicsItemFlag::ItemIsSelectable, true);
   setFlag(GraphicsItemFlag::ItemIsFocusable, true);

   setZValue(s_zindex);
   s_zindex ++;

   switch (type)
   {
      case NodeType::Component:
         {
            m_cornerRadius = 15 ;
            m_size = 250 ;
            m_brush = QBrush(QColor(255, 255, 255), Qt::BrushStyle::SolidPattern);
            m_pen = QPen(QBrush(QColor(0,150,255), Qt::BrushStyle::SolidPattern),
                         5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

         }
         break;
      case NodeType::Input:
         {
            m_cornerRadius = 80 ;
            m_size = 40 ;
            m_brush = QBrush(QColor(255, 255, 255,255), Qt::BrushStyle::SolidPattern);
            m_pen = QPen(QBrush(QColor(0,150,255), Qt::BrushStyle::SolidPattern),
                         5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);
         }
         break;
      case NodeType::Output:
         {
            m_cornerRadius = 80 ;
            m_size = 40 ;
            m_brush = QBrush(QColor(255, 0, 0), Qt::SolidPattern);
            m_pen = QPen(QBrush(QColor(255,0,0), Qt::BrushStyle::SolidPattern),
                         5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);
         }
         break;
      case NodeType::AdaptedOutput:
         {
            m_cornerRadius = 80 ;
            m_size = 40 ;
            m_brush = QBrush(QColor(255, 30, 0), Qt::BrushStyle::SolidPattern);
            m_pen = QPen(QBrush(QColor(0,150,255), Qt::BrushStyle::SolidPattern),
                         5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);
         }
         break;
      default:
         {
            m_cornerRadius = 15 ;
            m_size = 250 ;
            m_brush = QBrush(QColor(255, 255, 255), Qt::BrushStyle::SolidPattern);
            m_pen = QPen(QBrush(QColor(0,150,255), Qt::BrushStyle::SolidPattern),
                         5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);
         }
         break;
   }

   setPos(0,0);
   onCreateTextItem();
}

GNode::~GNode()
{
   qDeleteAll(m_connections);
   m_connections.clear();

   GAdaptedOutput* adaptedOutput = nullptr;
   if((adaptedOutput = dynamic_cast<GAdaptedOutput*>(this)))
   {
      adaptedOutput->adaptee()->deleteConnection(this);
   }

   if(scene())
   {
      scene()->removeItem(this);
   }
}

QString GNode::id() const
{
   return m_id;
}

void GNode::setId(const QString &id)
{
   m_id = id;
   onCreateTextItem();
   emit propertyChanged("Id" , m_id);
}

QString GNode::caption() const
{
   return m_caption;
}

void GNode::setCaption(const QString &caption)
{
   m_caption = caption;
   onCreateTextItem();
   emit propertyChanged("Caption" , m_caption);
}

GNode::NodeType GNode::nodeType() const
{
   return m_nodeType;
}

int GNode::size() const
{
   return m_size;
}

void GNode::setSize(int size)
{
   m_size = size;
   onCreateTextItem();
   emit propertyChanged("Size" , m_size);
}

int GNode::margin() const
{
   return m_margin;
}

void GNode::setMargin(int margin)
{
   if (2 * margin < m_size - 2 * m_margin)
   {
      m_margin = margin;
      onCreateTextItem();
      emit propertyChanged("Margin", m_margin);
   }
}

int GNode::cornerRadius() const
{
   return m_cornerRadius;
}

void GNode::setCornerRadius(int cornerRadius)
{
   m_cornerRadius = cornerRadius;
   emit propertyChanged("CornerRadius", m_cornerRadius);
   onCreateTextItem();
}

QPen GNode::pen() const
{
   return m_pen;
}

void GNode::setPen(const QPen& pen)
{
   m_pen = pen;
   emit propertyChanged("Pen", m_pen);
   update();
}

QBrush GNode::brush() const
{
   return m_brush;
}

void GNode::setBrush(const QBrush& brush)
{
   m_brush = brush;
   emit propertyChanged("Brush", m_brush);
   update();
}

QPen GNode::selectedPen() const
{
   return s_selectedPen;
}

void GNode::setSelectedPen(const QPen& selectedPen)
{
   s_selectedPen = selectedPen;
   emit propertyChanged("SelectedPen", s_selectedPen);
   update();
}

QBrush GNode::selectedBrush() const
{
   return s_selectedBrush;
}

void GNode::setSelectedBrush(const QBrush& selectedBrush)
{
   s_selectedBrush = selectedBrush;
   emit propertyChanged("SelectedBrush", s_selectedBrush);
   update();
}

QFont GNode::font() const
{
   return m_font;
}

void GNode::setFont(const QFont &font)
{
   m_font = font;
   emit propertyChanged("Font", m_font);
   onCreateTextItem();
}

QRectF GNode::boundingRect() const
{
   QRectF m_boundingRect(0,0, m_size + 2 * m_xmargin , m_size + 2 * m_ymargin);
   return m_boundingRect;
}

QList<GConnection*> GNode::connections() const
{
   return m_connections;
}

bool GNode::createConnection(GNode *consumer)
{
   if(consumer == this)
      return false;

   for(GConnection* connection :  m_connections)
   {
      if(connection->consumer() == consumer)
      {
         return false;
      }
   }
   
   GExchangeItem *producere , *consumere;

   if( (producere = dynamic_cast<GExchangeItem*>(this)) &&
       (consumere = dynamic_cast<GExchangeItem*>(consumer)) &&
       !producere->exchangeItem()->modelComponent()->id().compare(consumere->exchangeItem()->modelComponent()->id()))
   {
      return false;
   }



   if(dynamic_cast<GMultiInput*>(consumer))
   {
      GMultiInput* minput = dynamic_cast<GMultiInput*>(consumer);
      minput->addProvider(dynamic_cast<GOutput*>(this));
   }
   else if(dynamic_cast<GInput*>(consumer))
   {
      GInput* input = dynamic_cast<GInput*>(consumer);

      if(input->provider())
      {
         input->provider()->deleteConnection(input);
      }

      input->setProvider(dynamic_cast<GOutput*>(this));
   }

   GConnection* connection = new GConnection(this,consumer);
   m_connections.append(connection);

   emit connectionAdded(connection);
   emit propertyChanged("Connections" , QVariant::fromValue(connection));

   if(scene())
   {
      scene()->addItem(consumer);
      scene()->addItem(connection);
   }

   return true;
}

bool GNode::deleteConnection(GConnection *connection)
{
   if(m_connections.removeAll(connection))
   {
      emit propertyChanged("Connections" , QVariant::fromValue(connection));
      delete connection;

      GAdaptedOutput* adaptedOutput = nullptr;

      if((adaptedOutput = dynamic_cast<GAdaptedOutput*>(this)) && m_connections.length() == 0)
      {
         adaptedOutput->adaptee()->deleteConnection(this);
      }

      return true;
   }
   else
   {
      return false;
   }
}

bool GNode::deleteConnection(GNode *consumer)
{
   for(GConnection* connection : m_connections)
   {
      if(connection->consumer() == consumer)
      {
         return deleteConnection(connection);
      }
   }

   return false;
}

void GNode::deleteConnections()
{
   for(int i = 0 ; i < m_connections.length() ; i++)
   {
      delete m_connections[i];
   }

   m_connections.clear();
}

void GNode::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
   QPen pen = m_pen;
   QBrush brush = m_brush;
   QPainterPath m_path = QPainterPath();
   QRectF bRect = m_textItem->boundingRect();
   bRect.setCoords(0, 0,m_size + 2 * m_xmargin, m_size + 2 * m_ymargin);
   m_path.addRoundedRect(bRect, m_cornerRadius, m_cornerRadius, Qt::SizeMode::AbsoluteSize);

   if (option->state & QStyle::State_Selected)
   {
      pen = s_selectedPen;
      brush = s_selectedBrush;
   }

   painter->setPen(pen);
   painter->setBrush(brush);
   painter->drawPath(m_path);

   if (option->state & QStyle::State_Selected)
   {
      GDefaultSelectionGraphic::paint(bRect, painter);
   }
}

QVariant GNode::itemChange(GraphicsItemChange change, const QVariant &value)
{
   if(change == ItemSelectedHasChanged)
   {
      if(isSelected())
      {
         qreal z = 0;

         for(QGraphicsItem* it : scene()->items())
         {
            if(dynamic_cast<GNode*>(it) &&  it->zValue() > z)
               z = it->zValue();
         }
         setZValue(z + 0.000000000001);
      }
   }

   return QGraphicsObject::itemChange(change, value);
}

void GNode::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
   emit doubleClicked(this);
   QGraphicsObject::mouseDoubleClickEvent(event);
}

void GNode::onCreateTextItem()
{
   m_textItem->setFont(m_font);
   QString desc(sc_captionHtml);

   desc.replace("[Id]", m_id)
         .replace("[Caption]", m_caption);

   setToolTip(desc);
   m_textItem->setHtml(desc);
   m_textItem->setPos(-(m_textItem->boundingRect().width() - m_size-2*m_margin) /2.0 , - m_textItem->boundingRect().height() - 2);
   m_xmargin = m_ymargin = m_margin;
}
