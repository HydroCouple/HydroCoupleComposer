#include "custompropertyitems.h"


using namespace HydroCouple;

void CustomPropertyItems::registerCustomPropertyItems(QPropertyModel* propertyModel)
{
   //IModelComponent*
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IModelComponent*>(), &IModelComponentPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IComponentInfo*>(), &IComponentInfoPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IModelComponentInfo*>(), &IModelComponentInfoPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IModelComponent*>(), &IModelComponentPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IModelComponent*>>(), &IModelComponentListPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IAdaptedOutputFactoryComponentInfo*>(), &IAdaptedOutputFactoryComponentInfoPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IAdaptedOutputFactory*>(), &IAdaptedOutputFactoryPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IAdaptedOutputFactory*>>(), &IAdaptedOutputFactoryListPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IArgument*>(), &IArgumentPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IArgument*>>(), &IArgumentListPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IInput*>(), &IInputPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IInput*>>(), &IInputListPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<IOutput*>(), &IOutputPropertyItem::staticMetaObject);
    propertyModel->registerCustomPropertyItemType((QMetaType::Type) qMetaTypeId<QList<IOutput*>>(), &IOutputListPropertyItem::staticMetaObject);
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
      QObject* tmv = (QObject*) values[i];
      retValues.append(tmv);
   }

   return retValues;
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


IAdaptedOutputFactoryComponentInfoPropertyItem::IAdaptedOutputFactoryComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IAdaptedOutputFactoryComponentInfo*>(value)) , prop , parent)
{

}

IAdaptedOutputFactoryPropertyItem::IAdaptedOutputFactoryPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IAdaptedOutputFactory*>(value)) , prop , parent)
{

}

IAdaptedOutputFactoryListPropertyItem::IAdaptedOutputFactoryListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue(convertToQObjectList<IAdaptedOutputFactory*>(value)), prop , parent)
{
     setQVariantToQObjectListConverter( & convertToQObjectList<IAdaptedOutputFactory*>);
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



IInputPropertyItem::IInputPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent)
   : QObjectPropertyItem( dynamic_cast<QObject*>(qvariant_cast<IInput*>(value)) , prop , parent)
{

}

IInputListPropertyItem::IInputListPropertyItem(const QVariant &value, const QMetaProperty &prop, QObjectClassPropertyItem *parent)
   : QObjectListPropertyItem(QVariant::fromValue( convertToQObjectList<IInput*>(value)), prop , parent)
{
     setQVariantToQObjectListConverter( & convertToQObjectList<IInput*>);
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
