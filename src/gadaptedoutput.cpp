#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GAdaptedOutput::GAdaptedOutput(IAdaptedOutput *adaptedOutput,
                               IAdaptedOutputFactory *factory,
                               GOutput *adaptee, GInput *input)

   :GOutput(adaptedOutput->id() , adaptee->modelComponent())
{
   setCaption(adaptedOutput->caption());

   QObject* object = dynamic_cast<QObject*>(adaptedOutput);

   connect(object, SIGNAL(propertyChanged(const QString &)),
           this, SLOT(onPropertyChanged(const QString &)));

   m_adaptedOutput = adaptedOutput;
   m_nodeType = NodeType::AdaptedOutput;
   m_adaptee = adaptee;
   m_adaptedOutputFactory = factory;
   m_input = input;
}

GAdaptedOutput::~GAdaptedOutput()
{
  delete m_adaptedOutput;
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


