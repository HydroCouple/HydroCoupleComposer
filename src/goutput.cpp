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
  if(!xmlReader.name().toString().compare("OutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
  {
    while(!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("OutputExchangeItem", Qt::CaseInsensitive)) && !xmlReader.hasError())
    {
      if(!xmlReader.name().toString().compare("Connections",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
      {
        while(!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("Connections", Qt::CaseInsensitive)) && !xmlReader.hasError())
        {
          if(!xmlReader.name().toString().compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
          {
            readAdaptedOutputExchangeItem(xmlReader,errorMessages);
          }
          else if(!xmlReader.name().toString().compare("InputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
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
                  createConnection(input);
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
  if(!xmlReader.name().toString().compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) &&
     xmlReader.tokenType() == QXmlStreamReader::StartElement)
  {

    QXmlStreamAttributes attributes = xmlReader.attributes();

    if(attributes.hasAttribute("AdaptedOutputExchangeItemId") &&
       attributes.hasAttribute("AdaptedOutputFactoryId") &&
       attributes.hasAttribute("InputExchangeItemModelComponentIndex") &&
       attributes.hasAttribute("InputExchangeItemId"))
    {

      bool fromComponentLibrary = false;

      QString adaptedOutputFactoryId = attributes.value("AdaptedOutputFactoryId").toString();

      if(attributes.hasAttribute("AdaptedOutputFactoryComponentLibrary"))
      {
        QString componentFilePath = attributes.value("AdaptedOutputFactoryComponentLibrary").toString();
        m_project->componentManager()->loadComponent(QFileInfo(componentFilePath));
        fromComponentLibrary = true;
      }

      int inputComponentIndex = attributes.value("InputExchangeItemModelComponentIndex").toInt();

      if(inputComponentIndex < modelComponent()->project()->modelComponents().length())
      {
        GModelComponent* inputModelComponent = modelComponent()->project()->modelComponents()[inputComponentIndex];

        QString inputExchangeItemId = attributes.value("InputExchangeItemId").toString();

        if(inputModelComponent->inputs().contains(inputExchangeItemId))
        {

          GInput* input = inputModelComponent->inputGraphicObjects()[inputExchangeItemId];
          QString adaptedOutputId = attributes.value("AdaptedOutputExchangeItemId").toString();

          GAdaptedOutput* gAdaptedOutput = new GAdaptedOutput(adaptedOutputId, this, input, adaptedOutputFactoryId, fromComponentLibrary);


          if(attributes.hasAttribute("XPos") && attributes.hasAttribute("YPos"))
          {
            QString xposS = attributes.value("XPos").toString();
            QString yposS = attributes.value("YPos").toString();

            bool ok;

            double xloc = xposS.toDouble(&ok);
            double yloc = yposS.toDouble(&ok);

            if(ok)
            {
              gAdaptedOutput->setPos(xloc,yloc);
            }
          }

          createConnection(gAdaptedOutput);

          //Read arguments and connections
          while (!(xmlReader.isEndElement() &&
                   !xmlReader.name().toString().compare("AdaptedOutputExchangeItem", Qt::CaseInsensitive)) &&
                 !xmlReader.hasError())
          {
            if(!xmlReader.name().toString().compare("Arguments", Qt::CaseInsensitive) &&
               !xmlReader.hasError()
               && xmlReader.tokenType() == QXmlStreamReader::StartElement)
            {
              while (!(xmlReader.isEndElement() &&
                       !xmlReader.name().toString().compare("Arguments", Qt::CaseInsensitive)) &&
                     !xmlReader.hasError())
              {
                if(!xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive) &&
                   !xmlReader.hasError()  &&
                   xmlReader.tokenType() == QXmlStreamReader::StartElement)
                {
                  readArgument(xmlReader,gAdaptedOutput->adaptedOutput());

                  while (!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
                  {
                    xmlReader.readNext();
                  }
                }
                xmlReader.readNext();
              }
            }
            else if(!xmlReader.name().toString().compare("Connections", Qt::CaseInsensitive) &&
                    !xmlReader.hasError()
                    && xmlReader.tokenType() == QXmlStreamReader::StartElement)
            {

              gAdaptedOutput->readAdaptedOutputExchangeItemConnections(xmlReader,errorMessages);
            }

            xmlReader.readNext();
          }
        }
        else
        {
          throw "Input exchangeitem " + inputExchangeItemId + " could not be found in specified component";
        }
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
  if(!xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive) &&
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

          while(!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
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

    while (!(xmlReader.isEndElement() && !xmlReader.name().toString().compare("Argument", Qt::CaseInsensitive)) && !xmlReader.hasError())
    {
      xmlReader.readNext();
    }
  }
}

void GOutput::deEstablishConnections()
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

void GOutput::reEstablishConnections()
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

        adaptedOutput->reEstablishConnections();

        if(adaptedOutput->adaptedOutput())
        {
          output()->addAdaptedOutput(adaptedOutput->adaptedOutput());
        }
      }
    }
  }
}

void GOutput::reEstablishSignalSlotConnections()
{
  if(output())
  {
    QObject* object = dynamic_cast<QObject*>(output());
    connect(object, SIGNAL(propertyChanged(const QString &)),
            this, SLOT(onPropertyChanged(const QString &)));
  }
}

bool GOutput::createConnection(GNode *consumer)
{
  if(!m_connections.contains(consumer))
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
            QString message;

            if(m_component == input->modelComponent() || !input->input()->canConsume(output(), message))
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

      GConnection* connection = new GConnection(this,consumer);
      m_connections[consumer] = connection;

      switch (consumer->nodeType())
      {

        case GNode::Input:
          {
            GInput *input = (GInput*)consumer;

            if(input->provider())
            {
              for(GConnection *connection : m_connections)
              {
                if(connection->consumer() == input)
                {
                  input->provider()->deleteConnection(connection);
                }
              }
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

      emit connectionAdded(connection);
      emit propertyChanged("Connections");
      emit hasChanges();

      return true;
    }
  }

  return false;
}

void GOutput::deleteConnection(GConnection *connection)
{
  GNode *consumer = connection->consumer();

  if(m_connections.contains(consumer))
  {
    delete connection;
    GInput *input = nullptr;

    //consumer <---- adapter <---- adapteee
    if(consumer->nodeType() == GNode::Input)
    {
      input = dynamic_cast<GInput*>(consumer);
      input->setProvider(nullptr);

      if(output() && input->input())
      {
        output()->removeConsumer(input->input());
        input->input()->setProvider(nullptr);
      }
    }
    else if(consumer->nodeType() == GNode::MultiInput)
    {
      GMultiInput *minput = dynamic_cast<GMultiInput*>(consumer);
      minput->removeProvider(this);

      if(output() && minput->multiInput())
      {
        output()->removeConsumer(minput->multiInput());
        minput->multiInput()->removeProvider(output());
      }

      input = minput;
    }
    else if(consumer->nodeType() == GNode::AdaptedOutput)
    {
      GAdaptedOutput *adaptedOutput = dynamic_cast<GAdaptedOutput*>(consumer);

      if(adaptedOutput->adaptedOutput())
      {
        output()->removeAdaptedOutput(adaptedOutput->adaptedOutput());
      }

      delete adaptedOutput;
    }

    emit propertyChanged("Connections");
    emit hasChanges();
  }
}

void GOutput::deleteConnection(GNode *node)
{
  if(m_connections.contains(node))
  {
    GConnection *connection = m_connections[node];
    GNode *consumer = connection->consumer();

    if(m_connections.contains(consumer))
    {
      delete connection;
      GInput *input = nullptr;

      //consumer <---- adapter <---- adapteee
      if(consumer->nodeType() == GNode::Input)
      {
        input = dynamic_cast<GInput*>(consumer);
        input->setProvider(nullptr);

        if(output() && input->input())
        {
          output()->removeConsumer(input->input());
          input->input()->setProvider(nullptr);
        }
      }
      else if(consumer->nodeType() == GNode::MultiInput)
      {
        GMultiInput *minput = dynamic_cast<GMultiInput*>(consumer);
        minput->removeProvider(this);

        if(output() && minput->multiInput())
        {
          output()->removeConsumer(minput->multiInput());
          minput->multiInput()->removeProvider(output());
        }

        input = minput;
      }
      else if(consumer->nodeType() == GNode::AdaptedOutput)
      {
        GAdaptedOutput *adaptedOutput = dynamic_cast<GAdaptedOutput*>(consumer);

        if(adaptedOutput->adaptedOutput())
        {
          output()->removeAdaptedOutput(adaptedOutput->adaptedOutput());
        }

        delete adaptedOutput;
      }

      emit propertyChanged("Connections");
      emit hasChanges();
    }
  }
}

void GOutput::deleteConnections()
{
  while (m_connections.size())
  {
    deleteConnection(m_connections.begin().value());
  }
}
