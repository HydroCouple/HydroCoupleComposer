#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GInput::GInput(const QString &inputId, GModelComponent *parent)
   : GExchangeItem(inputId, NodeType::Input,  parent),
     m_input(inputId),
     m_provider(nullptr)
{

   if(m_component->inputs().contains(m_input))
   {
      IInput* input = m_component->inputs()[m_input];
      setCaption(input->caption());

      QObject* object = dynamic_cast<QObject*>(input);

      connect(object, SIGNAL(propertyChanged(const QString &)),
              this, SLOT(onPropertyChanged(const QString &)));
   }
}

GInput::~GInput()
{
   deleteConnections();

   if(m_provider)
   {
      m_provider->deleteConnection(this);
   }
}

HydroCouple::IExchangeItem* GInput::exchangeItem() const
{
   if(m_component->inputs().contains(m_input))
   {
      IInput* input = m_component->inputs()[m_input];
      return input;
   }

   return nullptr;
}

IInput* GInput::input() const
{
   if(m_component->inputs().contains(m_input))
   {
      IInput* input = m_component->inputs()[m_input];
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

bool GInput::createConnection(GNode *consumer)
{
   if(consumer->nodeType() == GNode::Component)
   {
      GModelComponent *component = (GModelComponent*) consumer;

      if(component && this->modelComponent() == component)
      {
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

         emit connectionAdded(connection);
         emit propertyChanged("Connections");
         emit hasChanges();

         if(scene())
         {
            scene()->addItem(connection);
         }

         return true;
      }
   }

   return false;
}

bool GInput::deleteConnection(GConnection *connection)
{
   if(m_connections.removeAll(connection))
   {
      if(scene())
      {
         scene()->removeItem(connection);
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

bool GInput::deleteConnection(GNode *consumer)
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

void GInput::deleteConnections()
{
   while (m_connections.length())
   {
      deleteConnection(m_connections[0]);
   }
}

void GInput::disestablishConnections()
{
   if(input())
   {
      input()->setProvider(nullptr);
   }
}

void GInput::reestablishConnections()
{
   if(input())
   {
      if(m_provider && m_provider->output())
      {
         input()->setProvider(m_provider->output());
      }
   }
}

void GInput::reestablishSignalSlotConnections()
{
  if(input())
  {
     setCaption(input()->caption());

     QObject* object = dynamic_cast<QObject*>(input());

     connect(object, SIGNAL(propertyChanged(const QString &)),
             this, SLOT(onPropertyChanged(const QString &)));

  }
}
