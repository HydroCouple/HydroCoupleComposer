#include "stdafx.h"
#include "gexchangeitems.h"

using namespace HydroCouple;

GAdaptedOutput::GAdaptedOutput(IAdaptedOutput *adaptedOutput,
                               IAdaptedOutputFactory *factory,
                               GOutput *adaptee, GInput *input,
                               QGraphicsObject *parent)
   :GOutput(adaptedOutput , parent)
{
   m_nodeType = NodeType::AdaptedOutput;
   m_adaptee = adaptee;
   m_adaptedOutputFactory = factory;
   m_input = input;
}

GAdaptedOutput::~GAdaptedOutput()
{
  delete m_adaptedOutput;
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

IAdaptedOutputFactory* GAdaptedOutput::factory() const
{
   return m_adaptedOutputFactory;
}
