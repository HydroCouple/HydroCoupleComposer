#include "stdafx.h"
#include "gexchangeitems.h"

using namespace HydroCouple;

GInput::GInput(IInput* input, QGraphicsObject* parent)
   : GExchangeItem(input, NodeType::Input,  parent), m_provider(nullptr)
{
   m_input = input;
}

GInput::~GInput()
{
   if(m_provider)
   {
      m_provider->deleteConnection(this);
   }
}

IInput* GInput::input() const
{
   return m_input;
}

GOutput* GInput::provider() const
{
   return m_provider;
}

void GInput::setProvider(GOutput *provider)
{
   m_provider = provider;
}


