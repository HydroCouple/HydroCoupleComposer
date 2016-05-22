#include "stdafx.h"
#include "gconnection.h"
#include "gnode.h"
#include "gexchangeitems.h"

#pragma region static variables

float GConnection::m_arrowLength(40.0f);
float GConnection::m_arrowWidth(30.0f);
QPen GConnection::m_selectedPen = QPen(QBrush(QColor(255, 99, 71), Qt::BrushStyle::SolidPattern), 6, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::MiterJoin);
QFont GConnection::m_font ;
int GConnection::s_zindex = -1000;

#pragma endregion

using namespace std;

GConnection::GConnection(GNode *producer, GNode *consumer , QGraphicsObject* parent)
   :QGraphicsObject(parent),
     m_pen(QBrush(QColor(0.0, 150.0, 255.0), Qt::BrushStyle::SolidPattern),4, Qt::PenStyle::SolidLine,
           Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::MiterJoin),
     m_brush (QBrush(Qt::GlobalColor::white))

{
   m_font = QFont();
   m_producer = producer;
   m_consumer = consumer;
   m_numConnectionsText = new QGraphicsTextItem(this);

   m_font.setBold(true);
   m_font.setPointSizeF(25);

   connect(m_producer, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_producer, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_producer, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_producer, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));

   connect(m_consumer, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_consumer, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_consumer, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_consumer, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));

   setFlag(GraphicsItemFlag::ItemIsSelectable, true);
   setFlag(GraphicsItemFlag::ItemIsFocusable, true);
   setFlag(GraphicsItemFlag::ItemStacksBehindParent, true);

   QString htmltext ="<h3 align=\"center\">Connection</h3>"
                     "<h4 align=\"left\">Producer</h4>"
                     "<p align=\"center\">" + producer->caption() +"</p>"
                                                                   "<p align=\"center\">" + producer->id() +"</p>"
                                                                                                            "<h4 align=\"left\">Consumer</h4>"
                                                                                                            "<p align=\"center\">" + consumer->caption() +"</p>"
                                                                                                                                                          "<p align=\"center\">" + consumer->id() +"</p>";

   setToolTip(htmltext);

   setZValue(s_zindex);
   s_zindex--;

   parentLocationOrSizeChanged();
   retrieveValidAdaptedOutputArguments();
}

GConnection::~GConnection()
{

   qDeleteAll(m_validAdaptedOutputs);
   m_validAdaptedOutputs.clear();

   if(scene())
   {
      scene()->removeItem(this);
   }
}

GNode* GConnection::producer() const
{
   return m_producer;
}

GNode* GConnection::consumer() const
{
   return m_consumer;
}

float GConnection::arrowLength() const
{
   return m_arrowLength;
}

void GConnection::setArrowLength(float value)
{
   m_arrowLength = value;
   parentLocationOrSizeChanged();
}

float GConnection::arrowWidth() const
{
   return m_arrowWidth;
}

void GConnection::setArrowWidth(float value)
{
   m_arrowWidth = value;
   parentLocationOrSizeChanged();
}

QPen GConnection::pen() const
{
   return m_pen;
}

void GConnection::setPen(const QPen& pen)
{
   m_pen = pen;
}

QBrush GConnection::brush() const
{
   return m_brush;
}

void GConnection::setBrush(const QBrush& brush)
{
   m_brush = brush;
}

QPen GConnection::selectedPen() const
{
   return m_selectedPen;
}

void GConnection::setSelectedPen(const QPen& pen)
{
   m_selectedPen = pen;
}

QFont GConnection::font() const
{
   return m_font  ;
}

void GConnection::setFont(const QFont& font)
{
   m_font = font;
}

QRectF GConnection::boundingRect() const
{
   return m_boundary;
}

QPainterPath GConnection::shape() const
{
   return m_path;
}

void GConnection::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
   m_path = QPainterPath(m_start);
   m_path.quadTo(m_c1, m_mid);
   m_path.quadTo(m_c2, m_end);

   QPen pen ;

   if (isSelected())
   {
      pen = m_selectedPen;
   }
   else
   {
      pen = m_pen;
   }

   painter->setPen(pen);
   painter->drawPath(m_path);
   painter->setBrush(pen.brush());
   painter->drawPolygon(m_arrowPoint, 3, Qt::FillRule::OddEvenFill);

   QPolygonF g = QPolygonF();
   g.append(m_arrowPoint[0]);
   g.append(m_arrowPoint[1]);
   g.append(m_arrowPoint[2]);
   m_path.addPolygon(g);

}

QVariant GConnection::itemChange(GraphicsItemChange change, const QVariant &value)
{
   if(change == ItemSelectedHasChanged)
   {
      if(isSelected())
      {
         qreal z =0;

         for(QGraphicsItem* it : scene()->items())
         {
            if(dynamic_cast<GConnection*>(it) &&  it->zValue() > z)
               z = it->zValue();
         }

         setZValue(z + 0.000000000001);
      }
   }

   return QGraphicsObject::itemChange(change, value);
}

void GConnection::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event)
{
   emit doubleClicked(this);
   QGraphicsObject::mouseDoubleClickEvent(event);
}

void GConnection::parentLocationOrSizeChanged()
{

   QRectF b1 = m_producer->sceneBoundingRect();
   QRectF b2 = m_consumer->sceneBoundingRect();

   //beginning and end of line
   m_start = b1.center();
   m_end = b2.center();
   m_mid = (m_start + m_end) / 2.0;
   QPointF diff = m_end - m_start;
   //line
   QLineF line(m_start, m_end);

   //unit line
   QVector3D uline(diff);
   uline.normalize();

   //normal vector to line
   QLineF nline = line.normalVector();

   //unit normal
   QVector3D nuvector(nline.p2() - nline.p1());
   nuvector.normalize();

   double lineLength = line.length();

   //control point 1
   QVector3D s1 = uline * lineLength * 0.25;
   QPointF p1 = s1.toPointF() + m_start;
   diff = diff * -1;
   double curveHeight = abs(diff.x()) < abs(diff.y()) ? abs(diff.x()) : abs(diff.y());

   if ((diff.x() >= 0 && diff.y() >= 0) || (diff.x() < 0 && diff.y() < 0))
      curveHeight = -curveHeight;


   m_c1 = p1 + nuvector.toPointF() * - 0.25 * curveHeight;

   m_labelLocation = p1 + nuvector.toPointF() * -0.125 * curveHeight;

   //control point 2
   QVector3D s2 = uline * lineLength * 0.75;
   QPointF p2 = s2.toPointF() + m_start;

   m_c2 = p2 + nuvector.toPointF() * 0.25 * curveHeight;


   //arrow points;
   QPointF ap1, ap2, ap3;

   ap1 = m_mid;

   QLineF aline(m_c1, m_mid);
   QLineF nualine = aline.normalVector().unitVector();

   QPointF arrowEnd = ap1 - QPointF(aline.unitVector().dx(), aline.unitVector().dy()) * (m_arrowLength);

   ap2 = arrowEnd +  QPointF(nualine.dx(), nualine.dy() ) * 0.5* m_arrowWidth;
   ap3 = arrowEnd -  QPointF(nualine.dx(), nualine.dy()) * 0.5* m_arrowWidth;

   QPointF labelLoc = (m_mid - ((0.5*m_arrowWidth+5+m_numConnectionsText->boundingRect().height()) * nuvector.toPoint()));

   QList<QPointF> allPoints;
   allPoints << m_start << m_mid << m_end << m_c1 << m_c2 << ap1 << ap2 << ap3 ;

   QPointF zeroPoint = minPosition(allPoints);
   QPointF maxPoint = maxPosition(allPoints);

   m_labelLocation = m_labelLocation - zeroPoint;
   m_start = m_start - zeroPoint;
   m_end = m_end - zeroPoint;
   m_mid = m_mid - zeroPoint;
   m_c1 = m_c1 - zeroPoint;
   m_c2 = m_c2 - zeroPoint;
   labelLoc = labelLoc - zeroPoint;

   m_numConnectionsText->setPos(labelLoc);

   m_arrowPoint[0] = ap1 - zeroPoint;
   m_arrowPoint[1] = ap2 - zeroPoint;
   m_arrowPoint[2] = ap3 - zeroPoint;

   m_boundary.setCoords(0, 0, maxPoint.x() - zeroPoint.x(), maxPoint.y() - zeroPoint.y());

   setPos(zeroPoint);

}

void GConnection::retrieveValidAdaptedOutputArguments()
{

}

QPointF GConnection::minPosition(const QList<QPointF> & points)
{
   double x = std::numeric_limits<double>::max();
   double y = x;

   for (int i = 0; i < points.count(); i++)
   {
      QPointF p = points[i];

      if (p.x() < x)
         x = p.x();

      if (p.y() < y)
         y = p.y();
   }

   return QPointF(x, y);
}

QPointF GConnection::maxPosition(const QList<QPointF> & points)
{
   double x = std::numeric_limits<double>::min();
   double y = x;

   for (int i = 0; i < points.count(); i++)
   {
      QPointF p = points[i];

      if (p.x() > x)
         x = p.x();

      if (p.y() > y)
         y = p.y();
   }
   return QPointF(x, y);
}
