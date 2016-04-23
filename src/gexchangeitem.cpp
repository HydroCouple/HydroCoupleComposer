#include "stdafx.h"
#include "gexchangeitems.h"

using namespace HydroCouple;

GExchangeItem::GExchangeItem(IExchangeItem* exchangeItem, GNode::NodeType type, QGraphicsObject *parent)
   : GNode(exchangeItem->id() , exchangeItem->caption(), type , parent)
{
   m_exchangeItem = exchangeItem;
}

GExchangeItem::~GExchangeItem()
{

}

IExchangeItem* GExchangeItem::exchangeItem() const
{
   return m_exchangeItem;
}
