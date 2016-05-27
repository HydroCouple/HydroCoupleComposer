#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GOutput::GOutput(const QString &outputId, GModelComponent* parent)
   : GExchangeItem(outputId, NodeType::Output , parent),
     m_outputId(outputId)
{
   if(m_component->outputs().contains(m_outputId))
   {
      IOutput* output = m_component->outputs()[m_outputId];
      setCaption(output->caption());

      QObject* object = dynamic_cast<QObject*>(output);

      connect(object, SIGNAL(propertyChanged(const QString &)),
              this, SLOT(onPropertyChanged(const QString &)));
   }
}

GOutput::~GOutput()
{
   deleteConnections();
}

IExchangeItem* GOutput::exchangeItem() const
{
   if(m_component->outputs().contains(m_outputId))
   {
      IOutput* output = m_component->outputs()[m_outputId];
      return output;
   }

   return nullptr;
}

IOutput* GOutput::output() const
{
   if(m_component->outputs().contains(m_outputId))
   {
      IOutput* output = m_component->outputs()[m_outputId];
      return output;
   }

   return nullptr;
}

void GOutput::writeExchangeItemConnections(QXmlStreamWriter &xmlWriter)
{
   xmlWriter.writeStartElement("OutputExchangeItem");
   {
      xmlWriter.writeAttribute("OutputExchangeItemId" ,id());
      xmlWriter.writeAttribute("OutputExchangeItemCaption" , caption());
      xmlWriter.writeAttribute("XPos" , QString::number(pos().x()));
      xmlWriter.writeAttribute("YPos" , QString::number(pos().y()));

      xmlWriter.writeStartElement("Connections");
      {
         for(GConnection* connection : m_connections)
         {
            GExchangeItem* exchangeItem = dynamic_cast<GExchangeItem*>(connection->consumer());
            exchangeItem->writeExchangeItemConnections(xmlWriter);
         }
      }
      xmlWriter.writeEndElement();
   }
   xmlWriter.writeEndElement();
}

void GOutput::readOutputExchangeItemConnections(QXmlStreamReader &xmlReader, QList<QString> & errorMessages)
{
   if(!xmlReader.name().compare("OutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
   {
      while(!(xmlReader.isEndElement() && !xmlReader.name().compare("OutputExchangeItem", Qt::CaseInsensitive)) && !xmlReader.hasError())
      {
         if(!xmlReader.name().compare("Connections",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
         {
            while(!(xmlReader.isEndElement() && !xmlReader.name().compare("Connections", Qt::CaseInsensitive)) && !xmlReader.hasError())
            {
               if(!xmlReader.name().compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
               {
                  readAdaptedOutputExchangeItem(xmlReader,errorMessages);
               }
               else if(!xmlReader.name().compare("InputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
               {

                  QXmlStreamAttributes attributes = xmlReader.attributes();

                  if(attributes.hasAttribute("InputExchangeItemId") && attributes.hasAttribute("ModelComponentIndex"))
                  {
                     int index = attributes.value("ModelComponentIndex").toInt();

                     if(index < m_component->project()->modelComponents().length())
                     {
                        GModelComponent* component = m_component->project()->modelComponents()[index];
                        QString id = attributes.value("InputExchangeItemId").toString();

                        for(GInput* input : component->inputGraphicObjects().values())
                        {
                           if(!input->id().compare(id))
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
                                    input->setPos(xloc,yloc);
                                 }
                              }

                              createConnection(input);
                              break;
                           }
                        }

                     }
                  }
               }
               xmlReader.readNext();
            }
         }
         xmlReader.readNext();
      }
   }
}

void GOutput::readAdaptedOutputExchangeItem(QXmlStreamReader &xmlReader, QList<QString> &errorMessages)
{
   if(!xmlReader.name().compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
   {
      QXmlStreamAttributes attributes = xmlReader.attributes();

      if(attributes.hasAttribute("AdaptedOutputExchangeItemId")
            && attributes.hasAttribute("AdaptedOutputFactoryId")
            && attributes.hasAttribute("InputExchangeItemId")
            )
      {
         QString adaptedOutputId = attributes.value("AdaptedOutputExchangeItemId").toString();
         //QString adaptedOutputId = attributes.value("AdaptedOutputExchangeItemId").toString();

         //         if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
         //         {
         //            QString xposS = attributes.value("XPos").toString();
         //            QString yposS = attributes.value("YPos").toString();

         //            bool ok;

         //            double xloc = xposS.toDouble(&ok);
         //            double yloc = yposS.toDouble(&ok);

         //            if(ok)
         //            {
         //               input->setPos(xloc,yloc);
         //            }
         //         }

      }
      else
      {
         errorMessages.append("Invaid adapted exchange");
         return;
      }

      //      while(!(xmlReader.isEndElement() && !xmlReader.name().compare("AdaptedOutputExchangeItem", Qt::CaseInsensitive)) && !xmlReader.hasError())
      //      {
      //         if(!xmlReader.name().compare("Connections",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
      //         {
      //            while(!(xmlReader.isEndElement() && !xmlReader.name().compare("Connections", Qt::CaseInsensitive)) && !xmlReader.hasError())
      //            {
      //               if(!xmlReader.name().compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
      //               {
      //                  readAdaptedOutputExchangeItem(xmlReader,errorMessages);
      //               }
      //               else if(!xmlReader.name().compare("InputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
      //               {

      //                  QXmlStreamAttributes attributes = xmlReader.attributes();

      //                  if(attributes.hasAttribute("InputExchangeItemId") && attributes.hasAttribute("ModelComponentIndex"))
      //                  {
      //                     int index = attributes.value("ModelComponentIndex").toInt();

      //                     if(index < m_parentModelComponent->project()->modelComponents().length())
      //                     {
      //                        GModelComponent* component = m_parentModelComponent->project()->modelComponents()[index];
      //                        QString id = attributes.value("InputExchangeItemId").toString();

      //                        for(GInput* input : component->inputExchangeItems())
      //                        {
      //                           if(!input->id().compare(id))
      //                           {
      //                              createConnection(input);
      //                              break;
      //                           }
      //                        }

      //                     }
      //                  }
      //               }
      //            }
      //         }
      //         xmlReader.readNext();
      //      }
   }
}

bool GOutput::createConnection(GNode *consumer)
{
   if(consumer->nodeType() == GNode::Input ||
         consumer->nodeType() == GNode::MultiInput ||
         consumer->nodeType() == GNode::AdaptedOutput )
   {

      switch (consumer->nodeType())
      {
         case GNode::Input:
         case GNode::MultiInput:
            {
               GInput* input = (GInput*) consumer;

               if(m_component == input->modelComponent())
               {
                  return false;
               }
            }
            break;
         case AdaptedOutput:
            {
               GAdaptedOutput* adaptedOutput = ( GAdaptedOutput*)consumer;

               if(adaptedOutput->modelComponent() == m_component)
               {
                  return false;
               }
            }
            break;
      }

      for(GConnection *connection : m_connections)
      {
         if(connection->consumer() == consumer)
         {
            return false;
            break;
         }
      }

      GConnection* connection = new GConnection(this,consumer);
      m_connections.append(connection);

      switch (consumer->nodeType())
      {

         case GNode::Input:
            {
               GInput *input = (GInput*)consumer;

               if(input->provider())
               {
                  input->provider()->deleteConnection(input);

               }

               input->setProvider(this);

               if(output() && input->input())
               {
                  output()->addConsumer(input->input());
               }
            }
            break;
         case GNode::MultiInput:
            {
               GMultiInput *input = (GMultiInput*)consumer;
               input->addProvider(this);

               if(output() && input->multiInput())
               {
                  output()->addConsumer(input->multiInput());
               }
            }
            break;
      }

      if(scene())
      {
         scene()->addItem(connection);
      }

      emit connectionAdded(connection);
      emit propertyChanged("Connections");
      emit hasChanges();


      return true;
   }

   return false;
}

bool GOutput::deleteConnection(GConnection *connection)
{
   if(m_connections.removeAll(connection))
   {
      if(scene())
      {
         scene()->removeItem(connection);
      }

      if(connection->consumer()->nodeType() == GNode::AdaptedOutput)
      {
         delete connection->consumer();
      }
      else if(connection->consumer()->nodeType() == GNode::Input)
      {
         GInput *input = (GInput*) connection->consumer();

         input->setProvider(nullptr);

         if(output() && input->input())
         {
            output()->removeConsumer(input->input());
         }
      }
      else if(connection->consumer()->nodeType() == GNode::MultiInput)
      {
         GMultiInput *minput = (GMultiInput*) connection->consumer();
         minput->removeProvider(this);

         if(output() && minput->multiInput())
         {
            output()->removeConsumer(minput->multiInput());
         }
      }

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

bool GOutput::deleteConnection(GNode *consumer)
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

void GOutput::deleteConnections()
{
   while (m_connections.length())
   {
      deleteConnection(m_connections[0]);
   }
}

void GOutput::disestablishConnections()
{
   if(output())
   {
      while (output()->consumers().length())
      {
         output()->removeConsumer(output()->consumers()[0]);
      }

      while (output()->adaptedOutputs().length())
      {
        output()->removeAdaptedOutput(output()->adaptedOutputs()[0]);
      }
   }
}

void GOutput::reestablishConnections()
{
   if(output())
   {
      QObject* object = dynamic_cast<QObject*>(output());
      connect(object, SIGNAL(propertyChanged(const QString &)),
              this, SLOT(onPropertyChanged(const QString &)));

      for(GConnection *connection: m_connections)
      {
         if(connection->consumer()->nodeType() ==  GNode::Input)
         {
            GInput* input = dynamic_cast<GInput*>(connection->consumer());

            if(input && input->input())
            {
               output()->addConsumer(input->input());
            }
         }
         else if(connection->consumer()->nodeType() ==  GNode::MultiInput)
         {
            GMultiInput* input = dynamic_cast<GMultiInput*>(connection->consumer());

            if(input && input->multiInput())
            {
               output()->addConsumer(input->multiInput());
            }
         }
         else if(connection->consumer()->nodeType() ==  GNode::AdaptedOutput)
         {
            GAdaptedOutput* adaptedOutput = dynamic_cast<GAdaptedOutput*>(connection->consumer());
            adaptedOutput->reestablishConnections();

            if(adaptedOutput->adaptedOutput())
            {
               output()->addAdaptedOutput(adaptedOutput->adaptedOutput());
            }
         }
      }
   }
}
