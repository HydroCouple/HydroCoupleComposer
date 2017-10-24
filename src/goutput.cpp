#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"
//#include <exception>

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

                if(component->inputGraphicObjects().contains(id))
                {
                  GInput* input = component->inputGraphicObjects()[id];

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
                  QString message;
                  createConnection(input, message);

                  if(!message.isEmpty())
                    errorMessages.append(message);
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
  if(!xmlReader.name().compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) &&
     xmlReader.tokenType() == QXmlStreamReader::StartElement)
  {

    QXmlStreamAttributes attributes = xmlReader.attributes();

    if(attributes.hasAttribute("AdaptedOutputExchangeItemId")
       && attributes.hasAttribute("AdaptedOutputFactoryId")
       && attributes.hasAttribute("InputExchangeItemModelComponentIndex")
       && attributes.hasAttribute("InputExchangeItemId")
       )
    {

      QString adaptedOutputFactoryId = attributes.value("AdaptedOutputFactoryId").toString();
      IAdaptedOutputFactory* factory = nullptr;

      if(attributes.hasAttribute("AdaptedOutputFactoryComponentLibrary"))
      {
        QString componentFilePath = attributes.value("AdaptedOutputFactoryComponentLibrary").toString();

        if(!m_component->project()->componentManager()->adaptedOutputFactoryComponentById().contains(adaptedOutputFactoryId))
        {
          if(!QFileInfo::exists(componentFilePath))
          {
            QFileInfo componentFile = m_component->project()->projectFile().dir().absoluteFilePath(componentFilePath);

            if(!componentFile.exists())
            {
              throw "Invalid component file exception";
            }
            componentFilePath = componentFile.absoluteFilePath();
          }
          m_component->project()->componentManager()->loadComponent(QFileInfo(componentFilePath));
        }

        factory = m_component->project()->componentManager()->adaptedOutputFactoryComponentById()[adaptedOutputFactoryId];
      }
      else
      {
        for(IAdaptedOutputFactory* afactory : m_component->modelComponent()->adaptedOutputFactories())
        {
          if(!afactory->id().compare(adaptedOutputFactoryId))
          {
            factory = afactory;
            break;
          }
        }
      }

      if(factory)
      {
        int inputComponentIndex = attributes.value("InputExchangeItemModelComponentIndex").toInt();

        if(inputComponentIndex < modelComponent()->project()->modelComponents().length())
        {
          GModelComponent* inputModelComponent = modelComponent()->project()->modelComponents()[inputComponentIndex];
          QString inputExchangeItemId = attributes.value("InputExchangeItemId").toString();

          if(inputModelComponent->inputs().contains(inputExchangeItemId))
          {
            QString adaptedOutputId = attributes.value("AdaptedOutputExchangeItemId").toString();
            IIdentity* adaptedOutputIdentity = nullptr;

            GInput* input = inputModelComponent->inputGraphicObjects()[inputExchangeItemId];
            QList<IIdentity*> identities = factory->getAvailableAdaptedOutputIds(output() , input->input());

            for(IIdentity* identity : identities)
            {
              if(!identity->id().compare(adaptedOutputId))
              {
                adaptedOutputIdentity  = identity;
                break;
              }
            }

            if(adaptedOutputIdentity)
            {
              GAdaptedOutput* adaptedOutput = new GAdaptedOutput(adaptedOutputIdentity , factory , this , input);

              if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
              {
                QString xposS = attributes.value("XPos").toString();
                QString yposS = attributes.value("YPos").toString();

                bool ok;

                double xloc = xposS.toDouble(&ok);
                double yloc = yposS.toDouble(&ok);

                if(ok)
                {
                  adaptedOutput->setPos(xloc,yloc);
                }
              }

              QString message;
              createConnection(adaptedOutput, message);

              if(!message.isEmpty())
                errorMessages.append(message);

              //Read arguments and connections
              while (!(xmlReader.isEndElement() &&
                       !xmlReader.name().compare("AdaptedOutputExchangeItem", Qt::CaseInsensitive)) &&
                     !xmlReader.hasError())
              {
                if(!xmlReader.name().compare("Arguments", Qt::CaseInsensitive) &&
                   !xmlReader.hasError()
                   && xmlReader.tokenType() == QXmlStreamReader::StartElement)
                {
                  while (!(xmlReader.isEndElement() &&
                           !xmlReader.name().compare("Arguments", Qt::CaseInsensitive)) &&
                         !xmlReader.hasError())
                  {
                    if(!xmlReader.name().compare("Argument", Qt::CaseInsensitive) &&
                       !xmlReader.hasError()  &&
                       xmlReader.tokenType() == QXmlStreamReader::StartElement)
                    {
                      readArgument(xmlReader,adaptedOutput->adaptedOutput());

                      while (!(xmlReader.isEndElement() && !xmlReader.name().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
                      {
                        xmlReader.readNext();
                      }
                    }
                    xmlReader.readNext();
                  }
                }
                else if(!xmlReader.name().compare("Connections", Qt::CaseInsensitive) &&
                        !xmlReader.hasError()
                        && xmlReader.tokenType() == QXmlStreamReader::StartElement)
                {

                  adaptedOutput->readAdaptedOutputExchangeItemConnections(xmlReader,errorMessages);
                }

                xmlReader.readNext();
              }


            }
          }
          else
          {
            throw "Input exchangeitem " + inputExchangeItemId + " could not be found in specified component";
          }
        }
        else
        {
          throw "Input exchange item ";
        }
      }
      else
      {
        throw "AdaptedOutput Factory was not found";
      }
    }
    else
    {
      errorMessages.append("Invaid adapted exchange");
      return;
    }
  }
}

void GOutput::readArgument(QXmlStreamReader &xmlReader, IAdaptedOutput *adaptedOutput)
{
  if(!xmlReader.name().compare("Argument", Qt::CaseInsensitive) &&
     !xmlReader.hasError() &&
     xmlReader.tokenType() == QXmlStreamReader::StartElement )
  {
    QXmlStreamAttributes attributes = xmlReader.attributes();
    QString message;

    if(attributes.hasAttribute("Id") && attributes.hasAttribute("ArgumentIOType"))
    {
      QStringRef Id = attributes.value("Id");
      QStringRef argIOType = attributes.value("ArgumentIOType");
      IArgument* targument = nullptr;

      for(IArgument* argument : adaptedOutput->arguments())
      {
        if(!Id.toString().compare(argument->id() , Qt::CaseInsensitive))
        {
          targument = argument;

          QString value;
          QXmlStreamWriter writer(&value);

          while(!(xmlReader.isEndElement() &&
                !xmlReader.name().compare("Argument", Qt::CaseInsensitive)) &&
                !xmlReader.hasError())
          {
            xmlReader.readNext();
            writer.writeCurrentToken(xmlReader);
          }

          if(!argIOType.toString().compare("File", Qt::CaseInsensitive))
          {
            targument->readValues(value, message, true);
          }
          else
          {
            targument->readValues(value, message);
          }

          break;
        }
      }
    }

    while (!(xmlReader.isEndElement() && !xmlReader.name().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
    {
      xmlReader.readNext();
    }
  }
}

bool GOutput::createConnection(GNode *consumer, QString &message)
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

          if(m_component == input->modelComponent() ||
             !input->input()->canConsume(output(), message))
          {
            return false;
          }
        }
        break;
      case GNode::AdaptedOutput:
        {
          //add connection
          connect(consumer , SIGNAL(doubleClicked(GNode*)) , modelComponent() , SLOT(onDoubleClicked(GNode*)));
        }
        break;
      default:
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
        }
        break;
      case GNode::MultiInput:
        {
          GMultiInput *input = (GMultiInput*)consumer;
          input->addProvider(this);
        }
        break;
      default:
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

void GOutput::reestablishSignalSlotConnections()
{
  if(output())
  {
    QObject* object = dynamic_cast<QObject*>(output());
    connect(object, SIGNAL(propertyChanged(const QString &)),
            this, SLOT(onPropertyChanged(const QString &)));
  }
}
