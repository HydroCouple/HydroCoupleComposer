#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

#include <QGraphicsScene>

using namespace HydroCouple;

GAdaptedOutput::GAdaptedOutput(const QString &adaptedOutputId,
                               GOutput *adaptee,
                               GInput *input,
                               const QString &adaptedOutputFactoryId,
                               bool fromComponentLibrary)

  : GOutput(adaptedOutputId, adaptee->modelComponent()),
    m_adaptedOutput(nullptr),
    m_fromComponentLibrary(fromComponentLibrary)
{

  m_adaptedOutputId = adaptedOutputId;
  m_nodeType = NodeType::AdaptedOutput;
  m_adaptee = adaptee;
  m_adaptedOutputFactoryId = adaptedOutputFactoryId;
  m_input = input;

  if(m_adaptee->output())
  {
    if(m_fromComponentLibrary)
    {
      m_adaptedOutput = modelComponent()->project()->componentManager()->createAdaptedOutputInstance(adaptedOutputFactoryId,adaptedOutputId, m_adaptee->output(), input->input());
    }
    else
    {
      m_adaptedOutput = modelComponent()->createAdaptedOutputInstance(adaptedOutputFactoryId,adaptedOutputId, m_adaptee->output(), input->input());
    }

    if(m_adaptedOutput)
    {
      setCaption(m_adaptedOutput->caption());

      QObject* object = dynamic_cast<QObject*>(m_adaptedOutput);

      connect(object, SIGNAL(propertyChanged(const QString &)),
              this, SLOT(onPropertyChanged(const QString &)));
    }
  }

  setCornerRadius(80);
  setSize(30);
  setBrush(QBrush(QColor(255, 255, 255), Qt::SolidPattern));
  setPen(QPen(QBrush(QColor(0,150,255), Qt::BrushStyle::SolidPattern),
              5.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin));

  connect(m_adaptee, SIGNAL(xChanged()), this, SLOT(adapteeOrInputLocationChanged()));
  connect(m_adaptee, SIGNAL(yChanged()), this, SLOT(adapteeOrInputLocationChanged()));
  connect(m_adaptee, SIGNAL(zChanged()), this, SLOT(adapteeOrInputLocationChanged()));
  connect(m_adaptee, SIGNAL(scaleChanged()), this, SLOT(adapteeOrInputLocationChanged()));

  connect(m_input, SIGNAL(xChanged()), this, SLOT(adapteeOrInputLocationChanged()));
  connect(m_input, SIGNAL(yChanged()), this, SLOT(adapteeOrInputLocationChanged()));
  connect(m_input, SIGNAL(zChanged()), this, SLOT(adapteeOrInputLocationChanged()));
  connect(m_input, SIGNAL(scaleChanged()), this, SLOT(adapteeOrInputLocationChanged()));


  if(adaptee->scene())
  {
    adaptee->scene()->addItem(this);
  }
}

GAdaptedOutput::~GAdaptedOutput()
{

  if(m_fromComponentLibrary)
  {
    if(m_adaptedOutput)
    {
      modelComponent()->project()->componentManager()->removeAdaptedOutputInstance(m_adaptedOutputFactoryId, m_adaptedOutput);
    }
  }

  //remove from componenent manager
  deleteConnections();

  //  if(m_adaptee && m_adaptee->nodeType() == GNode::AdaptedOutput)
  //  {
  //    GAdaptedOutput *adaptee = dynamic_cast<GAdaptedOutput*>(m_adaptee);

  //    if(adaptee->m_input == m_input)
  //    {
  //      m_adaptee = nullptr;
  //      delete adaptee;
  //    }
  //  }

  if(m_adaptedOutput)
  {
    delete m_adaptedOutput;
  }
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

void GAdaptedOutput::writeExchangeItemConnections(QXmlStreamWriter &xmlWriter)
{
  xmlWriter.writeStartElement("AdaptedOutputExchangeItem");
  {
    HydroCoupleProject* project = m_component->project();

    int sindex = project->modelComponents().indexOf(m_input->modelComponent());

    xmlWriter.writeAttribute("AdaptedOutputExchangeItemId" , m_adaptedOutputId);

    if(m_adaptedOutput)
    {
      xmlWriter.writeAttribute("AdaptedOutputExchangeCaption" , m_adaptedOutput->caption());
    }

    xmlWriter.writeAttribute("AdaptedOutputFactoryId" , m_adaptedOutputFactoryId);


    if(m_fromComponentLibrary)
    {
      xmlWriter.writeAttribute("FromComponentLibrary", "True");

      IAdaptedOutputFactoryComponentInfo * adaptedOutputFactoryComponentInfo = modelComponent()->project()->componentManager()->findAdaptedOutputFactoryComponentInfo(m_adaptedOutputFactoryId);

      if(adaptedOutputFactoryComponentInfo)
      {
        QFileInfo fileInfo = m_component->project()->projectFile().dir().relativeFilePath(adaptedOutputFactoryComponentInfo->libraryFilePath());
        xmlWriter.writeAttribute("AdaptedOutputFactoryComponentLibrary" , fileInfo.filePath());
      }
    }
    else
    {

      IAdaptedOutputFactory *adaptedOutputFactory =  nullptr;

      for(IAdaptedOutputFactory *factory : modelComponent()->modelComponent()->componentInfo()->adaptedOutputFactories())
      {
        if(!QString::compare(factory->id(), m_adaptedOutputFactoryId))
        {
          adaptedOutputFactory = factory;
          break;
        }
      }

      if(adaptedOutputFactory)
        xmlWriter.writeAttribute("AdaptedOutputFactoryCaption" , adaptedOutputFactory->caption());

      xmlWriter.writeAttribute("FromComponentLibrary", "False");
    }

    xmlWriter.writeAttribute("InputExchangeItemModelComponentIndex" , QString::number(sindex));
    xmlWriter.writeAttribute("InputExchangeItemId" , m_input->id());

    xmlWriter.writeAttribute("XPos" , QString::number(pos().x()));
    xmlWriter.writeAttribute("YPos" , QString::number(pos().y()));

    if(m_adaptedOutput)
    {
      xmlWriter.writeStartElement("Arguments");
      {
        for(IArgument* argument : adaptedOutput()->arguments())
        {
          xmlWriter.writeStartElement("Argument");
          {
            xmlWriter.writeAttribute("Id",argument->id());

            switch (argument->currentArgumentIOType())
            {
              case IArgument::File:
                {
                  xmlWriter.writeAttribute("ArgumentIOType","File");
                  xmlWriter.writeCharacters(m_component->project()->projectFile().dir().relativeFilePath(argument->toString()));
                }
                break;
              case IArgument::String:
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
  if(!xmlReader.name().string()->compare("Connections",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
  {
    while(!(xmlReader.isEndElement() && !xmlReader.name().string()->compare("Connections", Qt::CaseInsensitive)) && !xmlReader.hasError())
    {
      if(!xmlReader.name().string()->compare("AdaptedOutputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
      {
        readAdaptedOutputExchangeItem(xmlReader,errorMessages);
      }
      else if(!xmlReader.name().string()->compare("InputExchangeItem",Qt::CaseInsensitive) && xmlReader.tokenType() == QXmlStreamReader::StartElement)
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
}

void GAdaptedOutput::deEstablishConnections()
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

void GAdaptedOutput::reEstablishConnections()
{
  IAdaptedOutput* adaptedOutput = nullptr;

  if(m_adaptee->output())
  {

    if(m_fromComponentLibrary)
    {
      adaptedOutput = modelComponent()->project()->componentManager()->createAdaptedOutputInstance(m_adaptedOutputFactoryId, m_adaptedOutputId, m_adaptee->output(), m_input->input());
    }
    else
    {
      adaptedOutput = modelComponent()->createAdaptedOutputInstance(m_adaptedOutputFactoryId, m_adaptedOutputId, m_adaptee->output(), m_input->input());
    }

    if(adaptedOutput)
    {
      setCaption(m_adaptedOutput->caption());

      QObject* object = dynamic_cast<QObject*>(adaptedOutput);

      connect(object, SIGNAL(propertyChanged(const QString &)),
              this, SLOT(onPropertyChanged(const QString &)));
    }

    QString message;

    for(IArgument *nargument : adaptedOutput->arguments())
    {
      for(IArgument*argument : m_adaptedOutput->arguments())
      {
        if(!nargument->id().compare(argument->id()))
        {
          nargument->readValues(argument, message);
          break;
        }
      }
    }

    delete m_adaptedOutput;
    m_adaptedOutput = nullptr;
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
        adaptedOutput->reEstablishConnections();

        if(adaptedOutput->adaptedOutput())
        {
          output()->addAdaptedOutput(adaptedOutput->adaptedOutput());
        }
      }
    }
  }
}

void GAdaptedOutput::reEstablishSignalSlotConnections()
{
  if(m_adaptedOutput)
  {
    setCaption(m_adaptedOutput->caption());

    QObject* object = dynamic_cast<QObject*>(m_adaptedOutput);

    connect(object, SIGNAL(propertyChanged(const QString &)),
            this, SLOT(onPropertyChanged(const QString &)));
  }
}

void GAdaptedOutput::deleteConnection(GConnection *connection)
{
  GNode *consumer = connection->consumer();

  if(m_connections.contains(consumer))
  {
    delete connection;

    emit propertyChanged("Connections");
    emit hasChanges();

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

    if(m_adaptee && ((input && input == m_input) || m_connections.size() == 0))
    {
      m_adaptee->deleteConnection(this);
    }
  }
}

void GAdaptedOutput::deleteConnection(GNode *node)
{
  if(m_connections.contains(node))
  {
    GConnection *connection = m_connections[node];
    GNode *consumer = connection->consumer();

    if(m_connections.contains(consumer))
    {
      delete connection;

      emit propertyChanged("Connections");
      emit hasChanges();

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

      if(m_adaptee && ((input && input == m_input) || m_connections.size() == 0))
      {
        m_adaptee->deleteConnection(this);
      }
    }
  }
}

void GAdaptedOutput::adapteeOrInputLocationChanged()
{
  QPointF position = (m_adaptee->pos() + m_input->pos()) / 2.0 ;
  setPos(position);
}
