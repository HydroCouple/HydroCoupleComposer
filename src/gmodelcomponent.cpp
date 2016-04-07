#include "stdafx.h"
#include "gmodelcomponent.h"
#include "gdefaultselectiongraphic.h"

using namespace HydroCouple;

#pragma region variables

const QString GModelComponent::sc_descriptionHtml =
      "<h2>[Caption]</h2>"
      "<h4>[Id]</h4>"
      "<h5><i>Status : [Status]</i></h5>"
      "<hr>"
      "<div>"
      "<img alt=\"icon\" src='[IconPath]' width=\"40\" align=\"left\" />"
      "<p>[Description]</p>"
      "</div>";

QBrush GModelComponent::s_brush(QColor(255, 255, 255), Qt::BrushStyle::SolidPattern);
QPen GModelComponent::s_pen(QBrush(QColor(0,150,255), Qt::BrushStyle::SolidPattern), 5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

QBrush GModelComponent::s_triggerBrush(QColor(255, 255, 255), Qt::BrushStyle::SolidPattern);
QPen GModelComponent::s_triggerPen(QBrush(QColor(255, 0, 0), Qt::BrushStyle::SolidPattern), 5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

QBrush GModelComponent::s_selectedBrush(QColor(255, 230, 220), Qt::BrushStyle::SolidPattern);
QPen GModelComponent::s_selectedPen(QBrush(QColor(255, 0, 0), Qt::BrushStyle::SolidPattern), 5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

QFont GModelComponent::m_font;

#pragma endregion

GModelComponent::GModelComponent(IModelComponent* model, HydroCoupleProject *parent)
   : QGraphicsObject(), m_margin(15), m_cornerRadius(15), m_width(250), m_isTrigger(false)
{
   m_font = QFont();
   m_parent = parent;
   m_modelComponent = model;
   m_textItem = new QGraphicsTextItem(this);

   connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs &)),
           this, SLOT(onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs&)));

   connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(propertyChanged(const QString&, const QVariant&)), this, SLOT(onPropertyChanged(const QString&, const QVariant&)));

   onCreateTextItem();

   setFlag(GraphicsItemFlag::ItemIsMovable, true);
   setFlag(GraphicsItemFlag::ItemIsSelectable, true);
   setFlag(GraphicsItemFlag::ItemIsFocusable, true);


}

GModelComponent::~GModelComponent()
{
   for(int i = 0 ; i < m_modelComponentConnections.length() ; i++)
   {
      emit componentConnectionDeleting(m_modelComponentConnections[i]);
   }

   qDeleteAll(m_modelComponentConnections);
   m_modelComponentConnections.clear();
   delete m_modelComponent;

}

IModelComponent* GModelComponent::modelComponent() const
{
   return m_modelComponent;
}

ComponentStatus GModelComponent::status() const
{
   return m_modelComponent->status();
}

QList<GModelComponentConnection*> GModelComponent::modelComponentConnections() const
{
   return m_modelComponentConnections;
}

void GModelComponent::deleteAllConnections()
{
   for(GModelComponentConnection* connection : m_modelComponentConnections)
   {
      emit deleteComponentConnection(connection);
   }

   qDeleteAll(m_modelComponentConnections);
   m_modelComponentConnections.clear();
}

bool GModelComponent::trigger() const
{
   return m_isTrigger;
}

void GModelComponent::setTrigger(bool trigger)
{
   if (m_isTrigger != trigger)
   {
      m_isTrigger = trigger;

      if (m_isTrigger)
      {
         emit setAsTrigger(this);
      }

      emit propertyChanged("Trigger", m_isTrigger);
      emit postMessage(m_modelComponent->id() + " has been set as the trigger" );
      update();
   }
}

int GModelComponent::width() const
{
   return m_width;
}

void GModelComponent::setWidth(int width)
{
   if (width < 50 + m_margin * 2)
   {
      m_width = 50 + 2 * m_margin;
   }
   else
   {
      m_width = width;
   }

   //m_textItem->setPos(0, 0);
   m_textItem->setTextWidth(m_width - 2 * m_margin);
   m_textItem->setPos(m_margin, m_margin);
   update();

   emit propertyChanged("Width", m_width);
}

int GModelComponent::margin() const
{
   return m_margin;
}

void GModelComponent::setMargin(int margin)
{
   //not elegant fix later
   if (2 * margin < m_width - 2 * m_margin)
   {
      m_margin = margin;
      //m_textItem->setPos(0, 0);
      m_textItem->setTextWidth(m_width - 2 * m_margin);
      m_textItem->setPos(m_margin, m_margin);

      update();
   }

   emit propertyChanged("Margin", m_margin);
}

int GModelComponent::cornerRadius() const
{
   return m_cornerRadius;
}

void GModelComponent::setCornerRadius(int cornerRadius)
{
   m_cornerRadius = cornerRadius;
   emit propertyChanged("CornerRadius", m_cornerRadius);
   onCreateTextItem();
}

QGraphicsTextItem* GModelComponent::graphicsTextItem() const
{
  return m_textItem;
}

QPen GModelComponent::pen() const
{
   return s_pen;
}

void GModelComponent::setPen(const QPen& pen)
{
   s_pen = pen;
   emit propertyChanged("Pen", s_pen);
   update();
}

QBrush GModelComponent::brush() const
{
   return s_brush;
}

void GModelComponent::setBrush(const QBrush& brush)
{
   s_brush = brush;
   emit propertyChanged("Brush", s_brush);
   update();
}

QPen GModelComponent::selectedPen() const
{
   return s_selectedPen;
}

void GModelComponent::setSelectedPen(const QPen& selectedPen)
{
   s_selectedPen = selectedPen;
   emit propertyChanged("SelectedPen", s_selectedPen);
   update();
}

QBrush GModelComponent::selectedBrush() const
{
   return s_selectedBrush;
}

void GModelComponent::setSelectedBrush(const QBrush& selectedBrush)
{
   s_selectedBrush = selectedBrush;
   emit propertyChanged("SelectedBrush", s_selectedBrush);
   update();
}

QPen GModelComponent::triggerPen() const
{
   return s_triggerPen;
}

void GModelComponent::setTriggerPen(const QPen& triggerPen)
{
   s_triggerPen = triggerPen;
   emit propertyChanged("TriggerPen", s_triggerPen);
   update();
}

QBrush GModelComponent::triggerBrush() const
{
   return s_triggerBrush;
}

void GModelComponent::setTriggerBrush(const QBrush& triggerBrush)
{
   s_triggerBrush = triggerBrush;
   emit propertyChanged("TriggerBrush", s_triggerBrush);
   update();
}

QFont GModelComponent::font() const
{
   return m_font;
}

void GModelComponent::setFont(const QFont &font)
{
   m_font = font;
   emit propertyChanged("Font", m_font);

   for(GModelComponent* component : m_parent->modelComponents())
   {
      component->onCreateTextItem();
   }
}

QRectF GModelComponent::boundingRect() const
{
   QRectF bRect = m_textItem->boundingRect();
   bRect.setCoords(0, 0, bRect.width() + 2 * m_margin, bRect.height() + 2 * m_margin);
   return bRect;
}

void GModelComponent::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
   QPen pen = s_pen;
   QBrush brush = s_brush;
   QPainterPath m_path = QPainterPath();


   QRectF bRect = m_textItem->boundingRect();
   bRect.setCoords(0, 0, bRect.width() + 2 * m_margin, bRect.height() + 2 * m_margin);
   m_path.addRoundedRect(bRect, m_cornerRadius, m_cornerRadius, Qt::SizeMode::AbsoluteSize);


   if (option->state & QStyle::State_Selected)
   {
      pen = s_selectedPen;
      brush = s_selectedBrush;
   }
   else if (m_isTrigger)
   {
      pen = s_triggerPen;
      brush = s_triggerBrush;
   }

   painter->setPen(pen);
   painter->setBrush(brush);
   painter->drawPath(m_path);

   if (option->state & QStyle::State_Selected)
   {
      GDefaultSelectionGraphic::paint(bRect, painter);
   }
}

QVariant GModelComponent::itemChange(GraphicsItemChange change, const QVariant &value)
{
   if(change == ItemSelectedHasChanged)
   {
      if(isSelected())
      {
         qreal z = 0;

         for(QGraphicsItem* it : scene()->items())
         {
            if(dynamic_cast<GModelComponent*>(it) &&  it->zValue() > z)
               z = it->zValue();
         }

         setZValue(z + 0.000000000001);
      }
   }

   return QGraphicsObject::itemChange(change, value);
}


bool GModelComponent::createComponentModelConnection(GModelComponent* component)
{
    for(GModelComponentConnection* connection :  m_modelComponentConnections)

    {
       if(connection->consumerComponent() == component)
       {
          return false;
       }
    }

    GModelComponentConnection* connection = new GModelComponentConnection(this,component);
    m_modelComponentConnections.append(connection);
    emit componentConnectionAdded(connection);
    m_parent->setHasChanges(true);
    return true;
}

bool GModelComponent::deleteComponentConnection(GModelComponentConnection *connection)
{
   if(m_modelComponentConnections.removeAll(connection))
   {
      emit componentConnectionDeleting(connection);
      delete connection;
      m_parent->setHasChanges(true);
      return true;
   }
   else
   {
      return false;
   }
}

QString GModelComponent::modelComponentStatusAsString(ComponentStatus status)
{
   switch (status)
   {
      case ComponentStatus::Created:
         return "Created";
         break;
      case ComponentStatus::Initializing:
         return "Initializing";
         break;
      case ComponentStatus::Initialized:
         return "Initialized";
         break;
      case ComponentStatus::Validating:
         return "Validating";
         break;
      case ComponentStatus::Valid:
         return "Valid";
         break;
      case ComponentStatus::WaitingForData:
         return "WaitingForData";
         break;
      case ComponentStatus::Invalid:
         return "Invalid";
         break;
      case ComponentStatus::Preparing:
         return "Preparing";
         break;
      case ComponentStatus::Updating:
         return "Updating";
         break;
      case ComponentStatus::Updated:
         return "Updated";
         break;
      case ComponentStatus::Done:
         return "Done";
         break;
      case ComponentStatus::Finishing:
         return "Finishing";
         break;
      case ComponentStatus::Finished:
         return "Finished";
         break;
      case ComponentStatus::Failed:
         return "Failed";
         break;
      default:
         return	QString();
         break;
   }
}

GModelComponent* GModelComponent::readComponentFile(const QFileInfo& file)
{
   return nullptr;
}

GModelComponent* GModelComponent::readComponentSection(const QXmlStreamReader &xmlReader)
{
   return nullptr;
}

void GModelComponent::onComponentStatusChanged(const IComponentStatusChangeEventArgs& statusChangedEvent)
{
   emit componentStatusChanged(statusChangedEvent);
}

void GModelComponent::onPropertyChanged(const QString& propertyName, const QVariant& value)
{
   if (!propertyName.compare("Status", Qt::CaseInsensitive) ||
       !propertyName.compare("Id", Qt::CaseInsensitive) ||
       !propertyName.compare("Caption", Qt::CaseInsensitive) ||
       !propertyName.compare("Description", Qt::CaseInsensitive)
       )
   {
      onCreateTextItem();
   }

   emit propertyChanged(propertyName, value);
}

void GModelComponent::onCreateTextItem()
{
   m_textItem->setFont(m_font);
   QString desc(sc_descriptionHtml);
   QFileInfo iconFile(QFileInfo(m_modelComponent->componentInfo()->libraryFilePath()).dir(), m_modelComponent->componentInfo()->iconFilePath());

   desc.replace("[Id]", m_modelComponent->id())
         .replace("[Caption]", m_modelComponent->caption())
         .replace("[Description]", m_modelComponent->description())
         .replace("[Status]", modelComponentStatusAsString(m_modelComponent->status()))
         .replace("[IconPath]", iconFile.absoluteFilePath());

   setToolTip(desc);

   m_textItem->setHtml(desc);
   m_textItem->setTextWidth(m_width - 2 * m_margin);
   m_textItem->setPos(m_margin, m_margin);
}
