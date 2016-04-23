#include "stdafx.h"
#include "gconnection.h"
#include "gexchangeitems.h"

GOutputInputConnection::GOutputInputConnection(GOutput *output, GInput *input , QGraphicsObject* parent)
   :GExchangeItemConnection(output,input, parent)
{
   m_output = output;
   m_input = input;
}

GOutputInputConnection::~GOutputInputConnection()
{

}

GOutput* GOutputInputConnection::producerItem() const
{
   return m_output;
}

GInput* GOutputInputConnection::consumerItem() const
{
   return m_input;
}
