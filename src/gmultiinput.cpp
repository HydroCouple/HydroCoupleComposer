#include "stdafx.h"
#include "gexchangeitems.h"

using namespace HydroCouple;

GMultiInput::GMultiInput(IMultiInput * input, QGraphicsObject * parent)
   :GInput(input, parent)
{
   m_multiInput = input;
   m_nodeType = NodeType::MultiInput;
}

GMultiInput::~GMultiInput()
{
   for(GOutput* provider : m_providers)
   {
      provider->deleteConnection(this);
   }
}

IMultiInput* GMultiInput::multiInput() const
{
   return m_multiInput;
}

QList<GOutput*> GMultiInput::providers() const
{
   return m_providers;
}

void GMultiInput::addProvider(GOutput *provider)
{
   if(!m_providers.contains(provider))
      m_providers.append(provider);
}

void GMultiInput::removeProvider(GOutput *provider)
{
   m_providers.removeAll(provider);
}
