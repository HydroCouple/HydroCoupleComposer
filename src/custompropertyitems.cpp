#include "stdafx.h"
#include "custompropertyitems.h"
#include <assert.h>

using namespace HydroCouple;
using namespace HydroCouple::Temporal;

void CustomPropertyItems::registerCustomPropertyItems(QPropertyModel* propertyModel)
{
   //IModelComponent*
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IDescription*>(), &IDescriptionPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IIdentity*>(), &IIdentityPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IIdentity*>>(), &IIdentityListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IComponentInfo*>(), &IComponentInfoPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IModelComponentInfo*>(), &IModelComponentInfoPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IModelComponent*>(), &IModelComponentPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IModelComponent*>>(), &IModelComponentListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IDimension*>(), &IDimensionPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IDimension*>>(), &IDimensionListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IValueDefinition*>(), &IValueDefinitionPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IQuality*>(), &IQualityPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IUnitDimensions*>(), &IUnitDimensionsPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IUnit*>(), &IUnitPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IQuantity*>(), &IQuantityPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IComponentDataItem*>(), &IComponentDataItemPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IArgument*>(), &IArgumentPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IArgument*>>(), &IArgumentListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IAdaptedOutputFactoryComponentInfo*>(), &IAdaptedOutputFactoryComponentInfoPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IAdaptedOutputFactory*>(), &IAdaptedOutputFactoryPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IAdaptedOutputFactory*>>(), &IAdaptedOutputFactoryListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IArgument*>(), &IArgumentPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IInput*>(), &IInputPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IInput*>>(), &IInputListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IExchangeItem*>(), &IExchangeItemPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IOutput*>(), &IOutputPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IOutput*>>(), &IOutputListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<GNode*>(), &GNodePropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<ITime*>(), &ITimePropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<ITime*>>(), &ITimeListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<ITimeSpan*>(), &ITimeSpanPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<ITimeSpan*>(), &ITimeSpanListPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<ITimeComponentDataItem*>(), &ITimeComponentDataItemPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<ITimeExchangeItem*>(), &ITimeExchangeItemPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IIdBasedTimeComponentDataItem*>(), &IIdBasedTimeComponentDataItemPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IIdBasedTimeExchangeItem*>(), &IIdBasedTimeExchangeItemPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IUnit*>(), &IUnitPropertyItem::staticMetaObject);
   propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IUnitDimensions*>(), &IUnitDimensionsPropertyItem::staticMetaObject);

}

void CustomPropertyItems::registerCustomEditor()
{

}

template<typename T>
QList<QObject*> convertToQObjectList(const QVariant& value)
{
   QList<T> values = qvariant_cast< QList<T> >(value);
   QList<QObject*> retValues;

   for(int i = 0 ; i < values.length() ; i++)
   {
      QObject* tmv = dynamic_cast<QObject*>(values[i]);

      retValues.append(tmv);
   }

   return retValues;
}


IDescriptionPropertyItem::IDescriptionPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IDescription*>(value)) , prop , parent)
{

}

IIdentityPropertyItem::IIdentityPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IIdentity*>(value)) , prop , parent)
{

}

IIdentityListPropertyItem::IIdentityListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<IIdentity*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<IIdentity*>);
}

IComponentInfoPropertyItem::IComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IComponentInfo*>(value)) , prop , parent)
{

}

IModelComponentInfoPropertyItem::IModelComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IModelComponentInfo*>(value)) , prop , parent)
{

}

IModelComponentPropertyItem::IModelComponentPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IModelComponent*>(value)) , prop , parent)
{

}

IModelComponentListPropertyItem::IModelComponentListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<IModelComponent*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<IModelComponent*>);
}

IDimensionPropertyItem::IDimensionPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IDimension*>(value)) , prop , parent)
{

}

IDimensionListPropertyItem::IDimensionListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<IDimension*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<IDimension*>);
}

IValueDefinitionPropertyItem::IValueDefinitionPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IValueDefinition*>(value)) , prop , parent)
{

}

IQualityPropertyItem::IQualityPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IQuality*>(value)) , prop , parent)
{

}

IUnitDimensionsPropertyItem::IUnitDimensionsPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IUnitDimensions*>(value)) , prop , parent)
{

}

IUnitPropertyItem::IUnitPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IUnit*>(value)) , prop , parent)
{

}

IQuantityPropertyItem::IQuantityPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IQuantity*>(value)) , prop , parent)
{

}


IComponentDataItemPropertyItem::IComponentDataItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IComponentDataItem*>(value)) , prop , parent)
{

}

IArgumentPropertyItem::IArgumentPropertyItem(const QVariant &value, const QMetaProperty &prop, QPropertyItem *parent)
   :QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IArgument*>(value)) , prop , parent)
{

}

IArgumentListPropertyItem::IArgumentListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<IArgument*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<IArgument*>);
}

IAdaptedOutputFactoryComponentInfoPropertyItem::IAdaptedOutputFactoryComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IAdaptedOutputFactoryComponentInfo*>(value)) , prop , parent)
{

}

IAdaptedOutputFactoryPropertyItem::IAdaptedOutputFactoryPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IAdaptedOutputFactory*>(value)) , prop , parent)
{

}

IInputPropertyItem::IInputPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IInput*>(value)) , prop , parent)
{

}

IInputListPropertyItem::IInputListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue( convertToQObjectList<IInput*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<IInput*>);
}

IExchangeItemPropertyItem::IExchangeItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem(dynamic_cast<QObject*>(qvariant_cast<IExchangeItem*>(value)) , prop , parent)
{

}

IOutputPropertyItem::IOutputPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IOutput*>(value)) , prop , parent)
{

}

IOutputListPropertyItem::IOutputListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue( convertToQObjectList<IOutput*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<IOutput*>);
}

IAdaptedOutputFactoryListPropertyItem::IAdaptedOutputFactoryListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<IAdaptedOutputFactory*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<IAdaptedOutputFactory*>);
}

GNodePropertyItem::GNodePropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<GNode*>(value)) , prop , parent)
{

}

ITimePropertyItem::ITimePropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<ITime*>(value)) , prop , parent)
{

}

ITimeListPropertyItem::ITimeListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<ITime*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<ITime*>);
}

ITimeSpanPropertyItem::ITimeSpanPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<ITimeSpan*>(value)) , prop , parent)
{

}

ITimeSpanListPropertyItem::ITimeSpanListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<ITimeSpan*>(value)), prop , parent)
{
   setQVariantToQObjectListConverter( & convertToQObjectList<ITimeSpan*>);
}

ITimeComponentDataItemPropertyItem::ITimeComponentDataItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<ITimeComponentDataItem*>(value)) , prop , parent)
{

}

ITimeExchangeItemPropertyItem::ITimeExchangeItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<ITimeExchangeItem*>(value)) , prop , parent)
{

}


IIdBasedTimeComponentDataItemPropertyItem::IIdBasedTimeComponentDataItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IIdBasedTimeComponentDataItem*>(value)) , prop , parent)
{

}


IIdBasedTimeExchangeItemPropertyItem::IIdBasedTimeExchangeItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IIdBasedTimeExchangeItem*>(value)) , prop , parent)
{

}

