#include "stdafx.h"
#include "gmodelcomponent.h"
#include "gdefaultselectiongraphic.h"

using namespace HydroCouple;


const QString GModelComponent::sc_descriptionHtml =
      "<h2>[Caption]</h2>"
      "<h4>[Id]</h4>"
      "<h5><i>Status : [Status]</i></h5>"
      "<hr>"
      "<div>"
      "<img alt=\"icon\" src='[IconPath]' width=\"40\" align=\"left\" />"
      "<p>  [Description]</p>"
      "</div>";


QBrush GModelComponent::s_triggerBrush(QColor(255, 255, 255), Qt::BrushStyle::SolidPattern);
QPen GModelComponent::s_triggerPen(QBrush(QColor(255, 0, 0), Qt::BrushStyle::SolidPattern), 5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin);


GModelComponent::GModelComponent(IModelComponent* model, HydroCoupleProject *parent)
   : GNode(model->id(),model->caption(),GNode::Component), m_isTrigger(false),
     m_moveExchangeItemsWhenMoved(true),
     m_readFromFile(false),
     m_ignoreSignalsFromComponent(false)
{

   setFlag(GraphicsItemFlag::ItemSendsScenePositionChanges, true);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsMovable, false);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsSelectable, false);
   m_textItem->setFlag(GraphicsItemFlag::ItemIsFocusable, false);

   m_parent = parent;
   m_modelComponent = model;

   connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)),
           this, SLOT(onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)));
   connect(dynamic_cast<QObject*>(m_modelComponent), SIGNAL(propertyChanged(const QString &)), this, SLOT(onPropertyChanged(const QString&)));


   connect(this, &GNode::propertyChanged,this, &GModelComponent::onPropertyChanged);
   connect(this, &GNode::doubleClicked,this, &GModelComponent::onDoubleClicked);

   onCreateTextItem();
}

GModelComponent::~GModelComponent()
{
   deleteExchangeItems();
   delete m_modelComponent;
}

HydroCoupleProject* GModelComponent::project() const
{
   return m_parent;
}

void GModelComponent::setProject(HydroCoupleProject *project)
{
   m_parent = project;
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

      emit propertyChanged("Trigger");
      emit postMessage(m_modelComponent->id() + " has been set as the trigger" );
      emit hasChanges();
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
   emit propertyChanged("TriggerPen");
   emit hasChanges();
   update();
}

QBrush GModelComponent::triggerBrush() const
{
   return s_triggerBrush;
}

void GModelComponent::setTriggerBrush(const QBrush& triggerBrush)
{
   s_triggerBrush = triggerBrush;
   emit propertyChanged("TriggerBrush");
   emit hasChanges();
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

QHash<QString,IInput*> GModelComponent::inputs() const
{
   return m_inputs;
}

QHash<QString, IOutput*> GModelComponent::outputs() const
{
   return m_outputs;
}

QHash<QString,GInput*> GModelComponent::inputGraphicObjects() const
{
   return m_inputGraphicObjects;
}

QHash<QString, GOutput*> GModelComponent::outputGraphicObjects() const
{
   return m_outputGraphicObjects;
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

GModelComponent* GModelComponent::readComponentFile(const QFileInfo & fileInfo, HydroCoupleProject* project, QList<QString>& errorMessages)
{
   QFile file(fileInfo.absoluteFilePath());

   if(file.open(QIODevice::ReadOnly))
   {
      QXmlStreamReader xmlReader(&file);

      while(!xmlReader.isEndDocument() && !xmlReader.hasError())
      {
         if(!xmlReader.name().compare("ModelComponent", Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement)
         {
            while (!((xmlReader.isEndElement() || xmlReader.isEndDocument())
                     && !xmlReader.name().compare("ModelComponent", Qt::CaseInsensitive))
                   && !xmlReader.hasError())
            {
               GModelComponent* modelComponent = readComponent(xmlReader, project , errorMessages);

               if(modelComponent)
               {
                  modelComponent->m_readFromFile = true;
                  modelComponent->m_exportFile = fileInfo;
                  return modelComponent;
               }
            }
         }
         xmlReader.readNext();
      }
   }

   return nullptr;
}

GModelComponent* GModelComponent::readComponent(QXmlStreamReader & xmlReader, HydroCoupleProject* project, QList<QString>& errorMessages)
{
   if(!xmlReader.name().compare("ModelComponent", Qt::CaseInsensitive) && !xmlReader.hasError()
         && xmlReader.tokenType() == QXmlStreamReader::StartElement )
   {
      QXmlStreamAttributes attributes = xmlReader.attributes();

      if(attributes.hasAttribute("ModelComponentFile"))
      {
         QStringRef value = attributes.value("ModelComponentFile");
         QFileInfo fileInfo(value.toString());

         if(fileInfo.isRelative() && project)
         {
            fileInfo = project->projectFile().dir().absoluteFilePath(value.toString());
         }

         if(fileInfo.exists())
         {
            GModelComponent* modelComponent = readComponentFile(fileInfo, project, errorMessages);
            return modelComponent;
         }
         else
         {
            errorMessages.append("ModelComponentFile specified was not found : "+ fileInfo.absoluteFilePath() +" : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
         }
      }
      else
      {

         if(attributes.hasAttribute("ModelComponentLibrary"))
         {
            QStringRef value = attributes.value("ModelComponentLibrary");
            QFileInfo fileInfo(value.toString());

            if(fileInfo.isRelative() && project)
            {
               fileInfo = project->projectFile().dir().absoluteFilePath(value.toString());
            }

            if(fileInfo.exists())
            {
               IModelComponentInfo* modelComponentInfo = dynamic_cast<IModelComponentInfo*>(project->m_componentManager->loadComponent(fileInfo));

               if(modelComponentInfo)
               {
                  IModelComponent* component = modelComponentInfo->createComponentInstance();
                  GModelComponent* gcomponent = new GModelComponent(component, project);

                  if(attributes.hasAttribute("IsTrigger"))
                  {
                     QString trigVal = attributes.value("IsTrigger").toString();

                     if(!trigVal.compare("True" , Qt::CaseInsensitive))
                     {
                        gcomponent->setTrigger(true);
                     }
                  }

                  if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
                  {
                     QString xposS = attributes.value("XPos").toString();
                     QString yposS = attributes.value("YPos").toString();

                     bool ok;

                     double xloc = xposS.toDouble(&ok);
                     double yloc = yposS.toDouble(&ok);

                     if(ok)
                     {
                        gcomponent->setPos(xloc,yloc);
                     }
                  }

                  while (!(xmlReader.isEndElement() && !xmlReader.name().compare("ModelComponent", Qt::CaseInsensitive)) && !xmlReader.hasError())
                  {
                     if(!xmlReader.name().compare("Arguments", Qt::CaseInsensitive) && !xmlReader.hasError()
                           && xmlReader.tokenType() == QXmlStreamReader::StartElement)
                     {
                        while (!(xmlReader.isEndElement() && !xmlReader.name().compare("Arguments", Qt::CaseInsensitive)) && !xmlReader.hasError())
                        {
                           if(!xmlReader.name().compare("Argument", Qt::CaseInsensitive) && !xmlReader.hasError()  && xmlReader.tokenType() == QXmlStreamReader::StartElement)
                           {
                              readArgument(xmlReader, component);

                              while (!(xmlReader.isEndElement() && !xmlReader.name().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
                              {
                                 xmlReader.readNext();
                              }
                           }
                           xmlReader.readNext();
                        }

                        component->initialize();

                     }
                     xmlReader.readNext();
                  }

                  return gcomponent;
               }
               else
               {
                  errorMessages.append("Unable to load component specified at: "+ fileInfo.absoluteFilePath() +" : Column " + QString::number(xmlReader.columnNumber()) + " Row " + QString::number(xmlReader.lineNumber()));
               }
            }
            else
            {
               errorMessages.append("ModelComponentFile specified was not found : "+ fileInfo.absoluteFilePath() +" : Column " + QString::number(xmlReader.columnNumber()) +  " Row " + QString::number(xmlReader.lineNumber()));
            }
         }
      }

      while (!(xmlReader.isEndElement()
               && !xmlReader.name().compare("ModelComponent", Qt::CaseInsensitive))
             && !xmlReader.hasError())
      {
         xmlReader.readNext();
      }
   }

   return nullptr;
}

void GModelComponent::readComponentConnections(QXmlStreamReader & xmlReader, QList<QString>& errorMessages)
{
   if(!xmlReader.name().compare("ModelComponentConnection",Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
   {
      while(!(xmlReader.isEndElement() && !xmlReader.name().compare("ModelComponentConnection", Qt::CaseInsensitive)) && !xmlReader.hasError())
      {
         if(!xmlReader.name().compare("OutputExchangeItem",Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement)
         {

            QXmlStreamAttributes attributes = xmlReader.attributes();

            if(attributes.hasAttribute("OutputExchangeItemId"))
            {
               QStringRef id = attributes.value("OutputExchangeItemId");

               GOutput* outputExchangeItem = nullptr;

               for(GOutput* output : m_outputGraphicObjects.values())
               {
                  if(!output->output()->id().compare(id.toString()))
                  {

                     if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
                     {
                        QString xposS = attributes.value("XPos").toString();
                        QString yposS = attributes.value("YPos").toString();

                        bool ok;

                        double xloc = xposS.toDouble(&ok);
                        double yloc = yposS.toDouble(&ok);

                        if(ok)
                        {
                           output->setPos(xloc,yloc);
                        }
                     }

                     outputExchangeItem = output;
                     break;
                  }
               }

               if(outputExchangeItem)
               {
                  outputExchangeItem->readOutputExchangeItemConnections(xmlReader, errorMessages);
               }
            }
         }
         xmlReader.readNext();
      }
   }
}

void GModelComponent::writeComponent(const QFileInfo &fileInfo)
{
   QFile file(fileInfo.absoluteFilePath());

   if(file.open(QIODevice::WriteOnly | QIODevice::Truncate))
   {
      QXmlStreamWriter xmlWriter(&file);
      xmlWriter.setAutoFormatting(true);

      xmlWriter.writeStartElement("ModelComponent");
      {
         QString relPath = fileInfo.dir().relativeFilePath(m_modelComponent->componentInfo()->libraryFilePath());
         xmlWriter.writeAttribute("Name",m_modelComponent->componentInfo()->name());

         if(m_isTrigger)
         {
            xmlWriter.writeAttribute("IsTrigger" , "True");
         }

         xmlWriter.writeAttribute("ModelComponentLibrary",relPath);

         xmlWriter.writeAttribute("XPos" , QString::number(pos().x()));
         xmlWriter.writeAttribute("YPos" , QString::number(pos().y()));

         xmlWriter.writeStartElement("Arguments");
         {
            for(IArgument* argument : m_modelComponent->arguments())
            {
               xmlWriter.writeStartElement("Argument");
               {
                  xmlWriter.writeAttribute("ArgumentId",argument->id());

                  switch (argument->currentArgumentIOType())
                  {
                     case HydroCouple::File:
                        {
                           xmlWriter.writeAttribute("ArgumentIOType","File");
                           xmlWriter.writeCharacters(fileInfo.dir().relativeFilePath(argument->toString()));
                        }
                        break;
                     case HydroCouple::String:
                        {
                           xmlWriter.writeAttribute("ArgumentIOType","String");

                           QString input = argument->toString();

                           if(!input.contains("</"))
                           {
                              xmlWriter.writeCharacters(argument->toString());
                           }
                           else
                           {
                              QXmlStreamReader xmlReader(input);

                              while (xmlReader.name().isEmpty() || xmlReader.name().isNull())
                              {
                                 xmlReader.readNext();
                              }

                              while (!xmlReader.atEnd())
                              {
                                 xmlWriter.writeCurrentToken(xmlReader);
                                 xmlReader.readNext();
                              }
                           }
                        }
                        break;
                  }
               }
               xmlWriter.writeEndElement();
            }
         }
         xmlWriter.writeEndElement();
      }
      xmlWriter.writeEndElement();
   }

   m_readFromFile = true;
   m_exportFile = fileInfo;
}

void GModelComponent::writeComponent(QXmlStreamWriter &xmlWriter)
{
   QDir projectDir = m_parent->projectFile().dir();
   xmlWriter.setAutoFormatting(true);

   xmlWriter.writeStartElement("ModelComponent");
   {
      if(m_readFromFile)
      {
         writeComponent(m_exportFile);
         xmlWriter.writeAttribute("ModelComponentFile",projectDir.relativeFilePath(m_exportFile.absoluteFilePath()));
      }
      else
      {
         QString relPath = projectDir.relativeFilePath(m_modelComponent->componentInfo()->libraryFilePath());
         xmlWriter.writeAttribute("Name",m_modelComponent->componentInfo()->name());

         if(m_isTrigger)
         {
            xmlWriter.writeAttribute("IsTrigger" , "True");
         }

         xmlWriter.writeAttribute("ModelComponentLibrary",relPath);

         xmlWriter.writeAttribute("XPos" , QString::number(pos().x()));
         xmlWriter.writeAttribute("YPos" , QString::number(pos().y()));

         xmlWriter.writeStartElement("Arguments");
         {
            for(IArgument* argument : m_modelComponent->arguments())
            {
               xmlWriter.writeStartElement("Argument");
               {
                  xmlWriter.writeAttribute("Id",argument->id());

                  switch (argument->currentArgumentIOType())
                  {
                     case HydroCouple::File:
                        {
                           xmlWriter.writeAttribute("ArgumentIOType","File");
                           xmlWriter.writeTextElement("ArgumentText" , projectDir.relativeFilePath(argument->toString()));
                        }
                        break;
                     case HydroCouple::String:
                        {
                           xmlWriter.writeAttribute("ArgumentIOType","String");
                           QString input = argument->toString();

                           if(!input.contains("</"))
                           {
                              xmlWriter.writeCharacters(argument->toString());
                           }
                           else
                           {
                              QXmlStreamReader xmlReader(input);

                              while (xmlReader.name().isEmpty() || xmlReader.name().isNull())
                              {
                                 xmlReader.readNext();
                              }

                              while (!xmlReader.atEnd())
                              {
                                 xmlWriter.writeCurrentToken(xmlReader);
                                 xmlReader.readNext();
                              }
                           }
                        }
                        break;
                  }
               }
               xmlWriter.writeEndElement();
            }
         }
         xmlWriter.writeEndElement();
      }
   }
   xmlWriter.writeEndElement();
}

void GModelComponent::writeComponentConnections(QXmlStreamWriter &xmlWriter)
{
   bool found  = false;

   for(GOutput* output :  m_outputGraphicObjects.values())
   {
      if(output->connections().length())
         found = true;
   }

   if(!found)
      return ;

   int index = m_parent->modelComponents().indexOf(this);

   xmlWriter.writeStartElement("ModelComponentConnection");
   {
      xmlWriter.writeAttribute("SourceModelComponentIndex" , QString::number(index));

      for(GOutput* output :  m_outputGraphicObjects.values())
      {
         if(output->connections().length())
         {
            output->writeExchangeItemConnections(xmlWriter);
         }
      }
   }
   xmlWriter.writeEndElement();
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

bool GModelComponent::createConnection(GNode *consumer)
{
   if(consumer->nodeType() == GNode::Output)
   {
      for(GConnection *connection : m_connections)
      {
         if(connection->consumer() == consumer)
            return false;
      }

      GConnection* connection = new GConnection(this,consumer);
      m_connections.append(connection);

      emit connectionAdded(connection);
      emit propertyChanged("Connections");
      emit hasChanges();

      if(scene())
      {
         scene()->addItem(connection);
      }

      return true;
   }


   return false;
}

bool GModelComponent::deleteConnection(GConnection *connection)
{
   if(m_connections.removeAll(connection))
   {
      if(scene())
      {
         scene()->removeItem(connection);
         scene()->removeItem(connection->consumer());
      }

      m_outputGraphicObjects.remove(((GOutput*)connection->consumer())->id());

      delete connection->consumer();
      delete connection;
      emit propertyChanged("Connections");
      emit hasChanges();
      return true;
   }
   else
   {
      return false;
   }
}

bool GModelComponent::deleteConnection(GNode *consumer)
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

void GModelComponent::deleteConnections()
{
   while (m_connections.length())
   {
      deleteConnection(m_connections[0]);
   }
}

QVariant GModelComponent::itemChange(GraphicsItemChange change, const QVariant &value)
{
   if(change == QGraphicsItem::ItemPositionChange && m_moveExchangeItemsWhenMoved)
   {
      QPointF move = value.toPointF() - pos();

      for(GInput* input : m_inputGraphicObjects.values())
      {
         input->moveBy(move.x() , move.y());
      }

      for(GOutput* output : m_outputGraphicObjects.values())
      {
         output->moveBy(move.x() , move.y());
      }
   }

   return GNode::itemChange(change, value);
}

void GModelComponent::deleteExchangeItems()
{
   m_inputs.clear();
   m_outputs.clear();

   qDeleteAll(m_inputGraphicObjects.values());
   m_inputGraphicObjects.clear();

   deleteConnections();
}

void GModelComponent::createExchangeItems()
{

   m_inputs.clear();
   m_outputs.clear();


   QPointF p = pos();
   QRectF bound = boundingRect();

   qreal xl = p.x() - 1.5*m_size;
   qreal xr = p.x() + m_size + 2* m_margin + 1.5*m_size;

   qreal lc = p.y() + bound.height()/2.0 - 40.0 ;
   bool sign = true;
   int count = 0;


   for(IOutput *output : m_modelComponent->outputs())
   {
      m_outputs[output->id()] = output;

      GOutput* goutput = nullptr;

      if(m_outputGraphicObjects.contains(output->id()))
      {
         goutput = m_outputGraphicObjects[output->id()];
      }
      else
      {
         GOutput* goutput = new GOutput(output->id(),this);
         m_outputGraphicObjects[output->id()] = goutput;

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

         connect(goutput,SIGNAL(hasChanges()),this,SLOT(onChildHasChanges()));
         createConnection(goutput);
      }
   }

   QHash<QString,GOutput*>::iterator it = m_outputGraphicObjects.begin();

   while(it != m_outputGraphicObjects.end())
   {
      if(!m_outputs.contains(it.value()->id()))
      {
         emit postMessage("Output exchangeitem with id " +  it.value()->id() + " could not be found therefore it will be removed");
         m_outputGraphicObjects.erase(it);
         deleteConnection(it.value());
      }
      else
      {
         it++;
      }
   }

   sign = true;
   count  = 0;


   for(IInput *input : m_modelComponent->inputs())
   {
      m_inputs[input->id()] = input;

      GInput* ginput = nullptr;

      if(m_inputGraphicObjects.contains(input->id()))
      {
         ginput = m_inputGraphicObjects[input->id()];
      }
      else
      {
         IMultiInput* minput = dynamic_cast<IMultiInput*>(input);

         if(minput)
         {
            ginput = new GMultiInput(minput->id(), this);
         }
         else
         {
            ginput = new GInput(input->id(),this);
         }

         m_inputGraphicObjects[ginput->id()] = ginput;

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

         connect(ginput,SIGNAL(hasChanges()),this,SLOT(onChildHasChanges()));
         ginput->createConnection(this);
      }
   }

   QHash<QString,GInput*>::iterator itinp = m_inputGraphicObjects.begin();

   while(itinp != m_inputGraphicObjects.end())
   {
      if(!m_inputs.contains(itinp.value()->id()))
      {
         emit postMessage("Input exchangeitem with id " +  itinp.value()->id() + " could not be found therefore it will be removed");
         m_inputGraphicObjects.erase(itinp);
         delete itinp.value();
      }
      else
      {
         itinp++;
      }
   }

   emit hasChanges();
}

void GModelComponent::readArgument(QXmlStreamReader &xmlReader , IModelComponent* component)
{
   if(!xmlReader.name().compare("Argument", Qt::CaseInsensitive) && !xmlReader.hasError() && xmlReader.tokenType() == QXmlStreamReader::StartElement )
   {
      QXmlStreamAttributes attributes = xmlReader.attributes();

      if(attributes.hasAttribute("Id") && attributes.hasAttribute("ArgumentIOType"))
      {
         QStringRef argumentId = attributes.value("Id");
         QStringRef argIOType = attributes.value("ArgumentIOType");
         IArgument* targument = nullptr;

         for(IArgument* argument : component->arguments())
         {
            qDebug() << argument->id();

            if(!argumentId.toString().compare(argument->id() , Qt::CaseInsensitive))
            {
               targument = argument;

               QString value;
               QXmlStreamWriter writer(&value);

               while(!(xmlReader.isEndElement() && !xmlReader.name().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
               {
                  xmlReader.readNext();
                  writer.writeCurrentToken(xmlReader);
                  qDebug() << xmlReader.text();
               }

               if(!argIOType.toString().compare("File", Qt::CaseInsensitive))
               {
                  targument->readValues(value , true);
               }
               else
               {
                  qDebug() << value;
                  targument->readValues(value);
               }

               break;
            }
         }
      }
   }
}

void GModelComponent::onComponentStatusChanged(const QSharedPointer<IComponentStatusChangeEventArgs> & statusChangedEvent)
{
   //#ifndef QT_DEBUG

   if(statusChangedEvent->status() ==  ComponentStatus::Initialized)
   {
      createExchangeItems();
   }
   else if(statusChangedEvent->status() == ComponentStatus::Created)
   {
      m_inputs.clear();
      m_outputs.clear();
   }

   if(!signalsBlocked())
   onCreateTextItem();

   emit componentStatusChanged(statusChangedEvent);
   emit postMessage(statusChangedEvent->message());

   //#endif
}

void GModelComponent::onPropertyChanged(const QString& propertyName)
{

   if (!propertyName.compare("Status", Qt::CaseInsensitive) ||
       !propertyName.compare("Id", Qt::CaseInsensitive) ||
       !propertyName.compare("Caption", Qt::CaseInsensitive) ||
       !propertyName.compare("Description", Qt::CaseInsensitive)
       )
   {
      onCreateTextItem();
   }

   emit propertyChanged(propertyName);

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
