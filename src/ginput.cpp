#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GInput::GInput(const QString &inputId, GModelComponent *parent)
  : GExchangeItem(inputId, NodeType::Input,  parent),
    m_inputId(inputId),
    m_provider(nullptr)
{

  if(m_component->inputs().contains(m_inputId))
  {
    IInput* input = m_component->inputs()[m_inputId];
    setCaption(input->caption());

    QObject* object = dynamic_cast<QObject*>(input);

    connect(object, SIGNAL(propertyChanged(const QString &)),
            this, SLOT(onPropertyChanged(const QString &)));
  }
}

GInput::~GInput()
{

  if(m_provider)
  {
    m_provider->deleteConnection(this);
  }

  deleteConnections();
}

HydroCouple::IExchangeItem* GInput::exchangeItem() const
{
  if(m_component->inputs().contains(m_inputId))
  {
    IInput* input = m_component->inputs()[m_inputId];
    return input;
  }

  return nullptr;
}

IInput* GInput::input() const
{
  if(m_component->inputs().contains(m_inputId))
  {
    IInput* input = m_component->inputs()[m_inputId];
    return input;
  }

  return nullptr;
}

GOutput* GInput::provider() const
{
  return m_provider;
}

void GInput::setProvider(GOutput *provider)
{
  m_provider = provider;
}

void GInput::writeExchangeItemConnections(QXmlStreamWriter &xmlWriter)
{
  xmlWriter.writeStartElement("InputExchangeItem");
  {
    int sindex =  m_component->project()->modelComponents().indexOf(m_component);
    xmlWriter.writeAttribute("ModelComponentIndex" , QString::number(sindex));
    xmlWriter.writeAttribute("InputExchangeItemId" , id());
    xmlWriter.writeAttribute("InputExchangeItemCaption" , caption());
    xmlWriter.writeAttribute("XPos" , QString::number(pos().x()));
    xmlWriter.writeAttribute("YPos" , QString::number(pos().y()));
  }
  xmlWriter.writeEndElement();
}

void GInput::deEstablishConnections()
{
  if(input())
  {
    input()->setProvider(nullptr);
  }
}

void GInput::reEstablishConnections()
{
  if(input())
  {
    if(m_provider && m_provider->output())
    {
      input()->setProvider(m_provider->output());
    }
  }
}

void GInput::reEstablishSignalSlotConnections()
{
  if(input())
  {
    setCaption(input()->caption());

    QObject* object = dynamic_cast<QObject*>(input());

    connect(object, SIGNAL(propertyChanged(const QString &)),
            this, SLOT(onPropertyChanged(const QString &)));

  }
}

bool GInput::createConnection(GNode *node)
{
  if(node->nodeType() == GNode::NodeType::Component)
  {
    GModelComponent *modelComponent = dynamic_cast<GModelComponent*>(node);

    if(modelComponent && this->modelComponent() == modelComponent)
    {
      for(GConnection *connection : m_connections)
      {
        if(connection->consumer() == modelComponent)
        {
          return false;
          break;
        }
      }

      GConnection* connection = new GConnection(this,modelComponent);
      m_connections[node] = connection;

      emit connectionAdded(connection);
      emit propertyChanged("Connections");
      emit hasChanges();

      return true;
    }
  }

  return false;
}

void GInput::deleteConnection(GConnection *connection)
{
  if(connection->producer() == this)
  {
    delete connection;
    emit propertyChanged("Connections");
    emit hasChanges();
  }
}

void GInput::deleteConnection(GNode *node)
{
  if(m_connections.contains(node))
  {
    GConnection *connection = m_connections[node];
    delete connection;
    emit propertyChanged("Connections");
    emit hasChanges();
  }
}

void GInput::deleteConnections()
{
  while (m_connections.size())
  {
    deleteConnection(m_connections.begin().value());
  }
}
