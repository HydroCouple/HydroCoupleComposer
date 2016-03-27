#include "stdafx.h"
#include "gmodelcomponentconnection.h"
#include "gdefaultselectiongraphic.h"

#pragma region static variables

float GModelComponentConnection::m_arrowLength(40.0f);
float GModelComponentConnection::m_arrowWidth(20.0f);
QPen GModelComponentConnection::m_pen = QPen(QBrush(QColor(0.0, 150.0, 255.0), Qt::BrushStyle::SolidPattern), 2, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::MiterJoin);
QPen GModelComponentConnection::m_selectedPen = QPen(QBrush(QColor(255, 99, 71), Qt::BrushStyle::SolidPattern), 4, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::MiterJoin);
int GModelComponentConnection::zLocation = 10000;

#pragma endregion

GModelComponentConnection::GModelComponentConnection(GModelComponent* const producer, GModelComponent* const consumer)
   : QGraphicsObject(nullptr)
{
   m_producerModel = producer;
   m_consumerModel = consumer;

   m_numConnectionsText = new QGraphicsTextItem(this);
   m_numConnectionsText->setToolTip("Number of Connections");
 //  m_numConnectionsText->setHtml(QString("<h1>%1</h1>").arg(m_itemConnections.count()));

   connect(m_producerModel, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_producerModel, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_producerModel, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_producerModel, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));

   connect(m_consumerModel, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_consumerModel, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_consumerModel, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
   connect(m_consumerModel, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));

   setFlag(GraphicsItemFlag::ItemIsSelectable, true);
   setFlag(GraphicsItemFlag::ItemIsFocusable, true);
   setFlag(GraphicsItemFlag::ItemStacksBehindParent, true);

   parentLocationOrSizeChanged();
}

GModelComponentConnection::~GModelComponentConnection()
{
   //qDeleteAll(m_itemConnections);
}

GModelComponent* GModelComponentConnection::producerComponent() const
{
   return m_producerModel;
}

GModelComponent* GModelComponentConnection::consumerComponent() const
{
   return m_consumerModel;
}

bool GModelComponentConnection::equals(const GModelComponentConnection* connection) const
{
   if (connection->producerComponent() == m_producerModel && connection->consumerComponent() == m_consumerModel)
      return true;
   else
      return false;
}

//void GModelComponentConnection::addComponentItemConnection(GComponentItemConnection* const connection)
//{
//   if (! containsConnection(connection) && !m_itemConnections.contains(connection))
//   {
//      m_itemConnections.append(connection);
//   }
//}

//bool GModelComponentConnection::removeComponentItemConnection(GComponentItemConnection* const connection)
//{
//   if (m_itemConnections.removeOne(connection))
//   {
//      delete connection;
//      return true;
//   }

//   return false;
//}

//QList<GComponentItemConnection*> GModelComponentConnection::itemConnections() const
//{
//   return m_itemConnections;
//}

QPen GModelComponentConnection::pen() const
{
   return m_pen;
}

void GModelComponentConnection::setPen(const QPen& pen)
{
   m_pen = pen;
}

QPen GModelComponentConnection::selectedPen() const
{
   return m_selectedPen;
}

void GModelComponentConnection::setSelectedPen(const QPen& pen)
{
   m_selectedPen = pen;
}

float GModelComponentConnection::arrowLength() const
{
   return m_arrowLength;
}

void GModelComponentConnection::setArrowLength(float value)
{
   m_arrowLength = value;
   parentLocationOrSizeChanged();
}

float GModelComponentConnection::arrowWidth() const
{
   return m_arrowWidth;
}

void GModelComponentConnection::setArrowWidth(float value)
{
   m_arrowWidth = value;
   parentLocationOrSizeChanged();
}

QRectF GModelComponentConnection::boundingRect() const
{
   return m_boundary;
}

QGraphicsTextItem* GModelComponentConnection::numberOfConnectionsGraphicsTextItem() const
{
   return m_numConnectionsText;
}

void GModelComponentConnection::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
   m_path = QPainterPath(m_start);
   m_path.quadTo(m_c1, m_mid);
   m_path.quadTo(m_c2, m_end);

   if (isSelected())
   {
      painter->setPen(m_selectedPen);
      painter->drawPath(m_path);
      painter->setBrush(m_selectedPen.brush());
      painter->drawPolygon(m_arrowPoint, 3, Qt::FillRule::OddEvenFill);
      QPolygonF g = QPolygonF();
      g.append(m_arrowPoint[0]);
      g.append(m_arrowPoint[1]);
      g.append(m_arrowPoint[2]);
      m_path.addPolygon(g);
      //SelectionGraphicItem::paint(m_boundary, painter, option, widget);
   }
   else
   {
      painter->setPen(m_pen);
      painter->drawPath(m_path);
      painter->setBrush(m_pen.brush());
      painter->drawPolygon(m_arrowPoint, 3, Qt::FillRule::OddEvenFill);
      QPolygonF g = QPolygonF();
      g.append(m_arrowPoint[0]);
      g.append(m_arrowPoint[1]);
      g.append(m_arrowPoint[2]);
      m_path.addPolygon(g);
   }
}


QVariant GModelComponentConnection::itemChange(GraphicsItemChange change, const QVariant &value)
{
   if(change == ItemSelectedHasChanged)
   {
      if(isSelected())
      {
         qreal z =0;

         for(QGraphicsItem* it : scene()->items())
         {
            if(dynamic_cast<GModelComponentConnection*>(it) &&  it->zValue() > z)
               z = it->zValue();
         }

         setZValue(z + 0.000000000001);
      }
   }

   return QGraphicsObject::itemChange(change, value);
}

QPainterPath GModelComponentConnection::shape() const
{
   return m_path;
}

void GModelComponentConnection::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event)
{
   emit doubleClicked(event);
}

QPointF GModelComponentConnection::minPosition(const QList<QPointF> & points)
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

QPointF GModelComponentConnection::maxPosition(const QList<QPointF> & points)
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

//bool GModelComponentConnection::containsConnection(GComponentItemConnection* const connection)
//{
//   for (int i = 0; i < m_itemConnections.count(); i++)
//   {
//      GComponentItemConnection* connectionC = m_itemConnections[i];

//      if (connectionC->producerItem() == connection->producerItem() &&
//          connectionC->consumerItem() == connection->consumerItem()
//          )
//      {
//         return true;
//      }
//   }

//   return false;
//}

void GModelComponentConnection::parentLocationOrSizeChanged()
{

   QRectF b1 = m_producerModel->sceneBoundingRect();
   QRectF b2 = m_consumerModel->sceneBoundingRect();

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


