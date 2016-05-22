#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GMultiInput::GMultiInput(const QString &multiInputId,  GModelComponent* parent)
   :GInput(multiInputId, parent),
     m_multiInputId(multiInputId)
{
   m_nodeType = NodeType::MultiInput;

//   if(m_component->inputs().contains(m_multiInputId))
//   {
//      IInput* input = m_component->inputs()[m_multiInputId];
//      setCaption(input->caption());

//      QObject* object = dynamic_cast<QObject*>(input);

//      connect(object, SIGNAL(propertyChanged(const QString &)),
//              this, SLOT(onPropertyChanged(const QString &)));
//   }
}

GMultiInput::~GMultiInput()
{

    deleteConnections();

   for(GOutput* provider : m_providers)
   {
      provider->deleteConnection(this);
   }
}

IMultiInput* GMultiInput::multiInput() const
{
   if(m_component->inputs().contains(m_multiInputId))
   {
      return dynamic_cast<IMultiInput*>(m_component->inputs()[m_multiInputId]);
   }

   return nullptr;
}

QList<GOutput*> GMultiInput::providers() const
{
   return m_providers;
}

void GMultiInput::addProvider(GOutput *provider)
{
   if(!m_providers.contains(provider))
   {
      m_providers.append(provider);
      
      if(multiInput() && provider->output())
      {
         multiInput()->addProvider(provider->output());
      }
   }
}

void GMultiInput::removeProvider(GOutput *provider)
{
   if(m_providers.removeAll(provider))
   {
      if(provider->output() && multiInput())
      {
         multiInput()->removeProvider(provider->output());
      }

   }
}

