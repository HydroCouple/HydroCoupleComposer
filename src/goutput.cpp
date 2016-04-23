#include "stdafx.h"
#include "gexchangeitems.h"

using namespace HydroCouple;

GOutput::GOutput(HydroCouple::IOutput* output, QGraphicsObject* parent)
   : GExchangeItem(output, NodeType::Output , parent)
{
   m_output = output;
}

GOutput::~GOutput()
{

}

IOutput* GOutput::output() const
{
   return m_output;
}

