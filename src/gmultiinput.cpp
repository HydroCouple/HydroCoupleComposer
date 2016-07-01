#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GMultiInput::GMultiInput(const QString &multiInputId,  GModelComponent* parent)
  :GInput(multiInputId, parent),
    m_multiInputId(multiInputId)
{
  m_nodeType = NodeType::MultiInput;
}

GMultiInput::~GMultiInput()
{

  deleteConnections();

  if(!id().compare("6-N-Lat-Inf") && !modelComponent()->modelComponent()->id().compare("test1_split2"))
  {
    id();
  }

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
  }
}

void GMultiInput::removeProvider(GOutput *provider)
{
  if(m_providers.removeAll(provider))
  {
  }
}

void GMultiInput::disestablishConnections()
{
  if(multiInput())
  {
    while (multiInput()->providers().length())
    {
      multiInput()->removeProvider(multiInput()->providers()[0]);
    }
  }
}

void GMultiInput::reestablishConnections()
{
  if(multiInput())
  {
    for(GOutput * provider : m_providers)
    {
      if(provider->output())
      {
        multiInput()->addProvider(provider->output());
      }
    }
  }
}

void GMultiInput::reestablishSignalSlotConnections()
{
  if(multiInput())
  {
    QObject* object = dynamic_cast<QObject*>(input());

    connect(object, SIGNAL(propertyChanged(const QString &)),
            this, SLOT(onPropertyChanged(const QString &)));
  }
}
