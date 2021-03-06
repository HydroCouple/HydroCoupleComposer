#include "stdafx.h"
#include "gconnection.h"
#include "gnode.h"
#include "gexchangeitems.h"
#include "hydrocouple.h"
#include "gmodelcomponent.h"

#include <QtWidgets>

float GConnection::m_arrowLength(40.0f);
float GConnection::m_arrowWidth(30.0f);
QPen GConnection::m_selectedPen = QPen(QBrush(QColor(255, 99, 71), Qt::BrushStyle::SolidPattern), 6, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::MiterJoin);
QFont GConnection::m_font ;
int GConnection::s_zindex = -1000;


using namespace std;
using namespace HydroCouple;

GConnection::GConnection(GNode *producer, GNode *consumer , QGraphicsObject* parent)
  : QGraphicsObject(parent),
    m_pen(QBrush(QColor(0.0, 150.0, 255.0), Qt::BrushStyle::SolidPattern),4, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::MiterJoin),
    m_brush (QBrush(Qt::GlobalColor::white))
{
  m_font = QFont();
  m_producer = producer;
  m_consumer = consumer;
  m_numConnectionsText = new QGraphicsTextItem(this);

  m_font.setBold(true);
  m_font.setPointSizeF(30);

  connect(m_producer, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_producer, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_producer, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_producer, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_producer, SIGNAL(propertyChanged(const QString &)), this, SLOT(onNodePropertyChanged(const QString &)));

  connect(m_consumer, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_consumer, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_consumer, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_consumer, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));
  connect(m_consumer, SIGNAL(propertyChanged(const QString &)), this, SLOT(onNodePropertyChanged(const QString &)));

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

  setAcceptDrops(true);
  setToolTip(htmltext);

  setZValue(s_zindex);
  s_zindex--;

  parentLocationOrSizeChanged();

  if(producer->scene())
  {
    producer->scene()->addItem(this);
  }
}

GConnection::~GConnection()
{
  m_producer->m_connections.remove(m_consumer);

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

bool GConnection::isValid() const
{
  switch (m_producer->nodeType())
  {
    case GNode::Output:
    case GNode::AdaptedOutput:
      {
        GOutput *output = (GOutput*)m_producer;

        switch (m_consumer->nodeType())
        {
          case GNode::Input:
            {
              GInput* input = (GInput*) m_consumer;
              QString message = "";

              if(output->output() == nullptr || input->input() == nullptr ||
                 !(input->input()->canConsume(output->output(), message)))
              {
                if(!message.isEmpty())
                  printf("%s\n", message.toStdString().c_str());

                return false;
              }

              if(input->getInputType() == GInput::InputType::Single
                 && input->providers().size() > 1)
              {
                  return false;
              }
            }
            break;
          case GNode::AdaptedOutput:
            {
              GAdaptedOutput *adaptedOutput = (GAdaptedOutput*) m_consumer;

              if(output->output() == nullptr ||
                 adaptedOutput->output() == nullptr ||
                 (adaptedOutput->output() && output->output() &&
                  adaptedOutput->adaptedOutput()->adaptee() != output->output()))
              {
                return false;
              }
            }
            break;
        }
      }
      break;
  }

  return true;
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

  if(!isValid())
  {
    double size = 50.0;
    double xstart = m_errorLocation.x() - size / 2.0;
    double ystart = m_errorLocation.y() - size / 2.0;

    painter->drawImage(QRectF(xstart,ystart,size,size), GNode::s_errorImage);
  }
}

bool GConnection::insertAdaptedOutput(const QString& adaptedOutputId, const QString& adaptedOutputFactoryId, bool fromComponentLibrary)
{
  if((m_producer->nodeType() ==  GNode::NodeType::Output || m_producer->nodeType() == GNode::NodeType::AdaptedOutput) &&
     (m_consumer->nodeType() == GNode::NodeType::Input))
  {

    GOutput* prod = dynamic_cast<GOutput*>(m_producer);
    GInput* input =  dynamic_cast<GInput*>(m_consumer);

    //    if(m_consumer->nodeType() == GNode::Input)
    //    {
    //      input =
    //      input->setProvider(nullptr);
    //    }
    //    else if(m_consumer->nodeType() == GNode::MultiInput)
    //    {
    //      GMultiInput* minput = dynamic_cast<GMultiInput*>(m_consumer);
    input->removeProvider(prod);
    //      input = minput;
    //    }

    prod->m_connections.remove(m_consumer);

    GAdaptedOutput* adaptedOutput = new GAdaptedOutput(adaptedOutputId, prod, input, adaptedOutputFactoryId, fromComponentLibrary);
    QPointF loc = (m_producer->pos() + m_consumer->pos())/2.0;
    adaptedOutput->setPos(loc);
    adaptedOutput->modelComponent()->project()->onSetHasChanges();
    connect(adaptedOutput , SIGNAL(doubleClicked(GNode*)) , adaptedOutput->modelComponent() , SLOT(onDoubleClicked(GNode*)));

    disconnect(m_consumer, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
    disconnect(m_consumer, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
    disconnect(m_consumer, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
    disconnect(m_consumer, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));

    QString err;
    adaptedOutput->createConnection(m_consumer, err);
    m_consumer = adaptedOutput;
    prod->m_connections[m_consumer] = this;

    connect(m_consumer, SIGNAL(xChanged()), this, SLOT(parentLocationOrSizeChanged()));
    connect(m_consumer, SIGNAL(yChanged()), this, SLOT(parentLocationOrSizeChanged()));
    connect(m_consumer, SIGNAL(zChanged()), this, SLOT(parentLocationOrSizeChanged()));
    connect(m_consumer, SIGNAL(scaleChanged()), this, SLOT(parentLocationOrSizeChanged()));

    parentLocationOrSizeChanged();
    update();

    return true;

  }

  return false;
}

QVariant GConnection::itemChange(GraphicsItemChange change, const QVariant &value)
{
  if(change == ItemSelectedHasChanged)
  {
    if(isSelected())
    {
      qreal z = 0;

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

void GConnection::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
  if(event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") && canAcceptDrop(event->mimeData()))
  {
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void GConnection::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
  if(event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
  {
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void GConnection::dropEvent(QGraphicsSceneDragDropEvent *event)
{
  if((m_producer->nodeType() == GNode::NodeType::Output || m_producer->nodeType() == GNode::NodeType::AdaptedOutput) &&
     (m_consumer->nodeType() == GNode::NodeType::Input))
  {
    if(event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
    {
      QByteArray encoded = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
      QDataStream stream(&encoded, QIODevice::ReadOnly);

      while (!stream.atEnd())
      {
        int row, col;
        QMap<int, QVariant> roleMapData;
        stream >> row >> col >> roleMapData;

        /* do something with the data */
        QVariant value = roleMapData[Qt::UserRole + 1];

        if (value.type() == QVariant::Map)
        {
          QMap<QString,QVariant> mapData = value.toMap();

          if(mapData.contains("AdaptedOutput"))
          {

            QString adaptedOutputId = mapData["AdaptedOutput"].toString();

            if(mapData.contains("AdaptedOutputFactory"))
            {
              this->insertAdaptedOutput(adaptedOutputId, mapData["AdaptedOutputFactory"].toString());
              event->accept();
            }
            else if(mapData.contains("AdaptedOutputFactoryComponentInfo"))
            {
              this->insertAdaptedOutput(adaptedOutputId, mapData["AdaptedOutputFactoryComponentInfo"].toString(), true);
            }

            event->accept();
          }

          event->ignore();
        }
      }
    }
  }

  event->ignore();
}

void GConnection::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event)
{
  emit doubleClicked(this);
  QGraphicsObject::mouseDoubleClickEvent(event);
}

bool GConnection::canAcceptDrop(const QMimeData *data)
{
  if (data->hasFormat("application/x-qabstractitemmodeldatalist"))
  {
    QByteArray encoded = data->data("application/x-qabstractitemmodeldatalist");
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    while (!stream.atEnd())
    {
      int row, col;
      QMap<int, QVariant> roleDataMap;
      //      stream >> row >> col >> roleDataMap;

      /* do something with the data */
      QVariant value = roleDataMap[Qt::UserRole];

      if (value.type() == QVariant::Map)
      {
        QMap<QString,QVariant> map = value.toMap();

        if(map.contains("AdaptedOutput") &&
           (map.contains("AdaptedOutputFactory") || map.contains("AdaptedOutputFactoryComponent")))
        {
          return true;
        }

        return false;
      }
    }
  }

  return false;
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
  m_errorLocation = ap1 - QPointF(aline.unitVector().dx(), aline.unitVector().dy()) * (m_arrowLength + 25.0);

  ap2 = arrowEnd +  QPointF(nualine.dx(), nualine.dy() ) * 0.5 * m_arrowWidth;
  ap3 = arrowEnd -  QPointF(nualine.dx(), nualine.dy()) * 0.5 * m_arrowWidth;

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
  m_errorLocation = m_errorLocation - zeroPoint;

  labelLoc = labelLoc - zeroPoint;

  m_numConnectionsText->setPos(labelLoc);

  m_arrowPoint[0] = ap1 - zeroPoint;
  m_arrowPoint[1] = ap2 - zeroPoint;
  m_arrowPoint[2] = ap3 - zeroPoint;

  m_boundary.setCoords(0, 0, maxPoint.x() - zeroPoint.x(), maxPoint.y() - zeroPoint.y());

  setPos(zeroPoint);

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

void GConnection::onNodePropertyChanged(const QString &propertyName)
{
  if(scene())
  {
    QString htmltext ="<h3 align=\"center\">Connection</h3>"
                      "<h4 align=\"left\">Producer</h4>"
                      "<p align=\"center\">" + m_producer->caption() +"</p>"
                                                                      "<p align=\"center\">" + m_producer->id() +"</p>"
                                                                                                                 "<h4 align=\"left\">Consumer</h4>"
                                                                                                                 "<p align=\"center\">" + m_consumer->caption() +"</p>"
                                                                                                                                                                 "<p align=\"center\">" + m_consumer->id() +"</p>";

    setToolTip(htmltext);
    update(QRectF());
  }
}
