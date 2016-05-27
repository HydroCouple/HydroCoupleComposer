#include "stdafx.h"
#include "gnode.h"
#include "gdefaultselectiongraphic.h"
#include "assert.h"
#include "gexchangeitems.h"

const QString GNode::sc_captionHtml =
      "<h1 align=\"center\">[Id]</h1>"
      "<h2 align=\"center\"><i>[Caption]</i></h2>";


int GNode::s_zindex(1000);
QBrush GNode::s_selectedBrush(QColor(255, 230, 220), Qt::BrushStyle::SolidPattern);
QPen GNode::s_selectedPen(QBrush(QColor(255, 0, 0), Qt::BrushStyle::SolidPattern), 4.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

GNode::GNode(const QString& id, const QString& caption, NodeType type)
   :QGraphicsObject(nullptr),
      m_margin(15), m_id(id), m_caption(caption), m_nodeType(type),
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
   emit propertyChanged("Id");
   emit hasChanges();
}

QString GNode::caption() const
{
   return m_caption;
}

void GNode::setCaption(const QString &caption)
{
   m_caption = caption;
   onCreateTextItem();
   emit propertyChanged("Caption");
   emit hasChanges();
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
   emit propertyChanged("Size");
   emit hasChanges();
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
      emit propertyChanged("Margin");
      emit hasChanges();
   }
}

int GNode::cornerRadius() const
{
   return m_cornerRadius;
}

void GNode::setCornerRadius(int cornerRadius)
{
   m_cornerRadius = cornerRadius;
   emit propertyChanged("CornerRadius");
   emit hasChanges();
   onCreateTextItem();
}

QPen GNode::pen() const
{
   return m_pen;
}

void GNode::setPen(const QPen& pen)
{
   m_pen = pen;
   emit propertyChanged("Pen");
   emit hasChanges();
   update();
}

QBrush GNode::brush() const
{
   return m_brush;
}

void GNode::setBrush(const QBrush& brush)
{
   m_brush = brush;
   emit propertyChanged("Brush");
   emit hasChanges();
   update();
}

QPen GNode::selectedPen() const
{
   return s_selectedPen;
}

void GNode::setSelectedPen(const QPen& selectedPen)
{
   s_selectedPen = selectedPen;
   emit propertyChanged("SelectedPen");
   emit hasChanges();
   update();
}

QBrush GNode::selectedBrush() const
{
   return s_selectedBrush;
}

void GNode::setSelectedBrush(const QBrush& selectedBrush)
{
   s_selectedBrush = selectedBrush;
   emit propertyChanged("SelectedBrush");
   emit hasChanges();
   update();
}

QFont GNode::font() const
{
   return m_font;
}

void GNode::setFont(const QFont &font)
{
   m_font = font;
   emit propertyChanged("Font");
   emit hasChanges();
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

void GNode::onChildHasChanges()
{
  emit hasChanges();
}
