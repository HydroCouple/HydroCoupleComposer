#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GInput::GInput(const QString &inputId, GModelComponent *parent)
  : GExchangeItem(inputId, NodeType::Input,  parent),
    m_inputId(inputId)
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
  deleteConnections();

  while(m_providers.size())
  {
    GOutput* provider = m_providers[0];
    provider->deleteConnection(this);
  }
}

QString GInput::id() const
{
  if(input())
  {
    return input()->id();
  }
  else
  {
    return GNode::id();
  }
}

QString GInput::caption() const
{
  if(input())
  {
    return input()->caption();
  }
  else
  {
    return GNode::caption();
  }
}

bool GInput::isValid() const
{
  return input() != nullptr;
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

IMultiInput* GInput::multiInput() const
{
  if(input())
  {
    return dynamic_cast<IMultiInput*>(input());
  }

  return nullptr;
}

GOutput* GInput::provider() const
{
  if(m_providers.size() == 1)
  {
    return m_providers[0];
  }

  return nullptr;
}

QList<GOutput*> GInput::providers() const
{
  return m_providers;
}

GInput::InputType GInput::getInputType()
{
  if(multiInput())
    return GInput::Multi;
  else if(input())
    return GInput::Single;
  else
    return GInput::Unknown;
}

bool GInput::addProvider(GOutput *provider, QString &message)
{
  //validate if initialized otherwise just add.
  if(!m_providers.contains(provider))
  {
    InputType inputType = getInputType();

    switch (inputType)
    {
      case GInput::Single:
        {
          if(provider->output())
          {
            if(input()->canConsume(provider->output(), message) && m_providers.size() <= 1)
            {
              if(m_providers.size() == 1)
              {
                m_providers[0]->deleteConnection(this);
              }

              m_providers.append(provider);
              return true;
            }
            else
            {
              printf("%s \n", message.toStdString().c_str());
              return false;
            }
          }
          else
          {
            m_providers.append(provider);
            return true;
          }
        }
        break;
      case GInput::Multi:
        {
          if(provider->output())
          {
            if(input()->canConsume(provider->output(), message))
            {
              m_providers.append(provider);
              return true;
            }
            else
            {
              return false;
            }
          }
          else
          {
            m_providers.append(provider);
            return true;
          }
        }
        break;
      default:
        {
          m_providers.append(provider);
          return true;
        }
        break;
    }
  }
  else
  {
    message = "Provider already exists";

    return false;
  }
}

bool GInput::removeProvider(GOutput *provider)
{
  int removed = m_providers.removeAll(provider);

  if(removed)
  {
    IOutput *output = provider->output();

    if(output)
    {
      if(multiInput())
      {
        output->removeConsumer(multiInput());
      }
      else if(input() && input()->provider() == output)
      {
        input()->setProvider(nullptr);
      }
    }
  }

  return removed;
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
  if(multiInput())
  {
    while (multiInput()->providers().length())
    {
      multiInput()->removeProvider(multiInput()->providers()[0]);
    }
  }
  else if(input())
  {
    input()->setProvider(nullptr);
  }
}

bool GInput::reEstablishConnections(QString &errorMessage)
{
  errorMessage = "";

  if(m_providers.size())
  {
    if(multiInput())
    {
      for(GOutput *provider : m_providers)
      {
        if(provider->output())
        {
          if(input()->canConsume(provider->output(), errorMessage))
          {
            multiInput()->addProvider(provider->output());
          }
          else
          {
            errorMessage = "Error! Input " + id() + " cannot consume provider " + provider->id() + "!";
            return false;
          }
        }
        else
        {
          errorMessage = "Error! Provider " + provider->id() + " does not exist for consumer " + id() + "!";
          return false;
        }
      }
    }
    else if(input())
    {
      if(m_providers.size() == 1)
      {
        GOutput *provider = m_providers[0];

        if(provider && provider->output())
        {
          if(input()->canConsume(provider->output(),errorMessage))
          {
            input()->setProvider(provider->output());
          }
          else
          {
            errorMessage = "Error! Input " + id() + " cannot consume provider " + provider->id() + "!";
            return false;
          }
        }
        else
        {
          errorMessage = "Error! Provider " + provider->id() + " does not exist for consumer " + id() + "!";
          return false;
        }
      }
      else
      {
        errorMessage = "Error! More than one provider declared for an " + id() + " that requires only one provider!";
        return false;
      }
    }
    else
    {
      errorMessage = "Error! Consumer " + id() + " is not a multi input but has multiple providers!";
      return false;
    }

    return true;
  }

  //  if(m_provider)
  //  {
  //    if(input())
  //    {
  //      if(m_provider->output())
  //      {
  //        if(input()->canConsume(m_provider->output(), errorMessage))
  //        {
  //          input()->setProvider(m_provider->output());
  //          return true;
  //        }
  //      }
  //      else
  //      {
  //        errorMessage = "Error! Provider " + m_provider->id() + " does not exist for consumer " + m_inputId + "!";
  //      }
  //    }
  //    else
  //    {
  //      errorMessage = "Error! Consumer " + m_inputId + " does not exist!";
  //    }

  //    return false;
  //  }

  return true;
}

void GInput::reEstablishSignalSlotConnections()
{
  if(input())
  {
    QObject* object = dynamic_cast<QObject*>(input());

    disconnect(object, SIGNAL(propertyChanged(const QString &)),
               this, SLOT(onPropertyChanged(const QString &)));

    connect(object, SIGNAL(propertyChanged(const QString &)),
            this, SLOT(onPropertyChanged(const QString &)));

  }
}

bool GInput::createConnection(GNode *node, QString &message)
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

  message = "Connection is invalid!";

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
