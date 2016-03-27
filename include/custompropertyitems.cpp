#include "custompropertyitems.h"


using namespace HydroCouple;

void CustomPropertyItems::registerCustomPropertyItems(QPropertyModel* propertyModel)
{

   //IModelComponent*

   //int type = qMetaTypeId<HydroCouple::IModelComponentInfo*>();

}

void CustomPropertyItems::registerCustomEditor()
{

}

IModelComponentPropertyItem::IModelComponentPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IModelComponent*>(value)) , prop , parent)
{
}

IModelComponentPropertyItem::~IModelComponentPropertyItem()
{
}
