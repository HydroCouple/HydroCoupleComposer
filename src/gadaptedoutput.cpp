#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GAdaptedOutput::GAdaptedOutput(IIdentity *adaptedOutputId,
                               IAdaptedOutputFactory *factory,
                               GOutput *adaptee, GInput *input)

   :GOutput(adaptedOutputId->id() , adaptee->modelComponent()),
     m_adaptedOutput(nullptr)
{

   m_adaptedOutputId = adaptedOutputId;
   m_nodeType = NodeType::AdaptedOutput;
   m_adaptee = adaptee;
   m_adaptedOutputFactory = factory;
   m_input = input;

   if(m_adaptee->output() && input->input())
   {
      m_adaptedOutput = factory->createAdaptedOutput(m_adaptedOutputId , m_adaptee->output() , m_input->input());

      if(m_adaptedOutput)
      {
         setCaption(m_adaptedOutput->caption());

         QObject* object = dynamic_cast<QObject*>(m_adaptedOutput);

         connect(object, SIGNAL(propertyChanged(const QString &)),
                 this, SLOT(onPropertyChanged(const QString &)));
      }
   }
}

GAdaptedOutput::~GAdaptedOutput()
{
   delete m_adaptedOutput;
}

HydroCouple::IExchangeItem* GAdaptedOutput::exchangeItem() const
{
   return m_adaptedOutput;
}

HydroCouple::IOutput* GAdaptedOutput::output() const
{
   return m_adaptedOutput;
}

IAdaptedOutput* GAdaptedOutput::adaptedOutput() const
{
   return m_adaptedOutput;
}

GOutput* GAdaptedOutput::adaptee() const
{
   return m_adaptee;
}

GInput* GAdaptedOutput::input() const
{
   return m_input;
}

IAdaptedOutputFactory* GAdaptedOutput::factory() const
{
   return m_adaptedOutputFactory;
}

void GAdaptedOutput::writeExchangeItemConnections(QXmlStreamWriter &xmlWriter)
{
   xmlWriter.writeStartElement("AdaptedOutputExchangeItem");
   {
      HydroCoupleProject* project = m_component->project();

      int sindex = project->modelComponents().indexOf(m_input->modelComponent());

      xmlWriter.writeAttribute("AdaptedOutputExchangeItemId" , m_adaptedOutput->id());
      xmlWriter.writeAttribute("AdaptedOutputExchangeCaption" , m_adaptedOutput->caption());
      xmlWriter.writeAttribute("AdaptedOutputFactoryId" , m_adaptedOutputFactory->id());
      xmlWriter.writeAttribute("AdaptedOutputFactoryCaption" , m_adaptedOutputFactory->caption());

      IAdaptedOutputFactoryComponent* component = nullptr;

      if((component = dynamic_cast<IAdaptedOutputFactoryComponent*>(m_adaptedOutputFactory)))
      {
         QFileInfo fileInfo = m_component->project()->projectFile().dir().relativeFilePath(component->componentInfo()->libraryFilePath());

         xmlWriter.writeAttribute("AdaptedOutputFactoryComponentLibrary" , fileInfo.filePath());
      }

      xmlWriter.writeAttribute("InputExchangeItemModelComponentIndex" , QString::number(sindex));
      xmlWriter.writeAttribute("InputExchangeItemId" , m_input->id());

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

void GAdaptedOutput::readAdaptedOutputExchangeItemConnections(QXmlStreamReader &xmlReader, QList<QString> &errorMessages)
{
   if(!xmlReader.name().compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
   {
      while(!(xmlReader.isEndElement() && !xmlReader.name().compare("AdaptedOutputExchangeItem", Qt::CaseInsensitive)) && !xmlReader.hasError())
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

                        for(GInput* input : component->inputGraphicObjects())
                        {
                           if(!input->id().compare(id))
                           {
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

bool GAdaptedOutput::deleteConnection(GConnection *connection)
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
         delete connection;
         emit propertyChanged("Connections");
         emit hasChanges();
      }
      else if(connection->consumer()->nodeType() == GNode::Input)
      {
         GInput *input = (GInput*) connection->consumer();

         input->setProvider(nullptr);

         if(output() && input->input())
         {
            output()->removeConsumer(input->input());
         }

         delete connection;
         emit propertyChanged("Connections");
         emit hasChanges();

         if(m_input == input)
         {
            m_adaptee->deleteConnection(this);
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

         delete connection;
         emit propertyChanged("Connections");
         emit hasChanges();

         if(m_input == minput)
         {
            m_adaptee->deleteConnection(this);
         }

      }


      return true;
   }
   else
   {
      return false;
   }
}

void GAdaptedOutput::disestablishConnections()
{

}

void GAdaptedOutput::reestablishConnections()
{
   IAdaptedOutput* adaptedOutput;

   if(m_adaptee->output())
   {
      if(m_adaptee->nodeType() == GNode::Output && m_input->input())
      {
         adaptedOutput = m_adaptedOutputFactory->createAdaptedOutput(m_adaptedOutputId , m_adaptee->output() , m_input->input());
      }
      else if(m_adaptee->nodeType() == GNode::AdaptedOutput && m_input->input())
      {
         adaptedOutput = m_adaptedOutputFactory->createAdaptedOutput(m_adaptedOutputId , m_adaptee->output() , m_input->input());
      }

      for(IArgument *nargument : adaptedOutput->arguments())
      {
         for(IArgument*argument : m_adaptedOutput->arguments())
         {
            if(!nargument->id().compare(argument->id()))
            {
               nargument->readValues(argument);
               break;
            }
         }
      }

      delete m_adaptedOutput;
      m_adaptedOutput = adaptedOutput;

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
