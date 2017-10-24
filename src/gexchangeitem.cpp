#include "stdafx.h"
#include "gexchangeitems.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;

GExchangeItem::GExchangeItem(const QString &exchangeItemId, GNode::NodeType type, GModelComponent *parent)
   : GNode(exchangeItemId, "" , type , parent->project()),
     m_exchangeItemId(exchangeItemId)
{
   m_component = parent;
}

GExchangeItem::~GExchangeItem()
{

}

GModelComponent* GExchangeItem::modelComponent() const
{
   return m_component;
}

void GExchangeItem::onPropertyChanged(const QString &propertyName)
{
   if(!propertyName.compare("id" ,Qt::CaseInsensitive) ||
      !propertyName.compare("caption", Qt::CaseInsensitive))
   {
      setId(exchangeItem()->id());
      setCaption(exchangeItem()->caption());
   }
}
