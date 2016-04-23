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


QBrush GModelComponent::s_triggerBrush(QColor(255, 255, 255), Qt::BrushStyle::SolidPattern);
QPen GModelComponent::s_triggerPen(QBrush(QColor(255, 0, 0), Qt::BrushStyle::SolidPattern), 5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);

#pragma endregion

GModelComponent::GModelComponent(IModelComponent* model, HydroCoupleProject *parent)
   : GNode(model->id(),model->caption(),GNode::Component), m_isTrigger(false), m_moveExchangeItemsWhenMoved(true)
{

   setFlag(GraphicsItemFlag::ItemSendsScenePositionChanges, true);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsMovable, false);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsSelectable, false);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsFocusable, false);

   m_parent = parent;
   m_modelComponent = model;

   connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(componentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs &)),
           this, SLOT(onComponentStatusChanged(const HydroCouple::IComponentStatusChangeEventArgs&)));
   connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(propertyChanged(const QString&, const QVariant&)), this, SLOT(onPropertyChanged(const QString&, const QVariant&)));


   connect(this, &GNode::propertyChanged,this, &GModelComponent::onPropertyChanged);
   connect(this, &GNode::doubleClicked,this, &GModelComponent::onDoubleClicked);

   onCreateTextItem();
}

GModelComponent::~GModelComponent()
{
   qDeleteAll(m_inputExchangeItems);
   m_inputExchangeItems.clear();

   delete m_modelComponent;
}

IModelComponent* GModelComponent::modelComponent() const
{
   return m_modelComponent;
}

QString GModelComponent::status() const
{
   return modelComponentStatusAsString( m_modelComponent->status());
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

bool GModelComponent::moveExchangeItemsWhenMoved() const
{
   return m_moveExchangeItemsWhenMoved;
}

void GModelComponent::setMoveExchangeItemsWhenMoved(bool move)
{
   m_moveExchangeItemsWhenMoved = move;
}

QList<GInput*> GModelComponent::inputExchangeItems() const
{
   return m_inputExchangeItems;
}

QList<GOutput*> GModelComponent::outputExchangeItems() const
{
   return m_outputExchangeItems;
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

QRectF GModelComponent::boundingRect() const
{
   QRectF m_boundingRect(0, 0, m_textItem->boundingRect().width() + 2 * m_xmargin, m_textItem->boundingRect().height() + 2 * m_ymargin);
   return m_boundingRect;
}

void GModelComponent::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
   QPen pen = m_pen;
   QBrush brush = m_brush;
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
   if(change == QGraphicsItem::ItemPositionChange && m_moveExchangeItemsWhenMoved)
   {
      QPointF move = value.toPointF() - pos();

      for(GInput* input : m_inputExchangeItems)
      {
         input->moveBy(move.x() , move.y());
      }

      for(GOutput* output : m_outputExchangeItems)
      {
         output->moveBy(move.x() , move.y());
      }
   }

   return GNode::itemChange(change, value);
}

void GModelComponent::createExchangeItems()
{

   for(GConnection* connection : m_connections)
   {
      m_connections.removeAll(connection);
      delete connection;
   }

   for(GOutput* output : m_outputExchangeItems)
   {
      m_outputExchangeItems.removeAll(output);
      delete output;
   }

   for(GInput* input : m_inputExchangeItems)
   {
      m_inputExchangeItems.removeAll(input);
      delete input;
   }

   QPointF p = pos();
   QRectF bound = boundingRect();

   qreal xl = p.x() - 1.5*m_size;
   qreal xr = p.x() + m_size + 2* m_margin + 1.5*m_size;

   qreal lc = p.y() + bound.height()/2.0 - 40.0 ;
   bool sign = true;
   int count = 0;

   for(IOutput* output : m_modelComponent->outputs())
   {
      GOutput* goutput = new GOutput(output);
      m_outputExchangeItems.append(goutput);

      if(sign)
      {
         goutput->setPos(xl, count * 180 + lc);
         sign = false;
         count++;
      }
      else
      {
         goutput->setPos(xl, -count * 180 + lc);
         sign = true;
      }

      if(scene())
      {
         scene()->addItem(goutput);
      }

      createConnection(goutput);
   }

   sign = true;
   count  = 0;

   for(IInput* input : m_modelComponent->inputs())
   {
      GInput* ginput = new GInput(input, nullptr);
      m_inputExchangeItems.append(ginput);

      if(sign)
      {
         ginput->setPos(xr, count * 180 + lc);
         sign = false;
         count++;
      }
      else
      {
         ginput->setPos(xr, -count * 180 + lc);
         sign = true;
      }

      if(scene())
      {
         scene()->addItem(ginput);
      }

      ginput->createConnection(this);
   }
}

void GModelComponent::onComponentStatusChanged(const IComponentStatusChangeEventArgs& statusChangedEvent)
{
   onCreateTextItem();

   if(statusChangedEvent.status() ==  ComponentStatus::Initialized)
   {
      createExchangeItems();
   }

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

void GModelComponent::onDoubleClicked(GNode *node)
{
   GModelComponent* component = dynamic_cast<GModelComponent*>(node);
   emit doubleClicked(component);
}

void GModelComponent::onCreateTextItem()
{
   m_textItem->setFont(m_font);
   m_textItem->setScale(1.0);
   QString desc(sc_descriptionHtml);
   QFileInfo iconFile(QFileInfo(m_modelComponent->componentInfo()->libraryFilePath()).dir(), m_modelComponent->componentInfo()->iconFilePath());

   desc.replace("[Id]", m_modelComponent->id())
         .replace("[Caption]", m_modelComponent->caption())
         .replace("[Description]", m_modelComponent->description())
         .replace("[Status]", modelComponentStatusAsString(m_modelComponent->status()))
         .replace("[IconPath]", iconFile.absoluteFilePath());

   setToolTip(desc);

   m_textItem->setHtml(desc);
   m_textItem->setTextWidth(m_size - 2 * m_margin);
   m_textItem->setPos(m_margin, m_margin);

   m_xmargin = m_ymargin = m_margin;
}
