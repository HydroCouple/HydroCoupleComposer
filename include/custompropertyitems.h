#ifndef CUSTOMPROPERTYITEMS_H
#define CUSTOMPROPERTYITEMS_H

#include "qobjectpropertyitem.h"
#include "qpropertymodel.h"
#include "hydrocoupletemporal.h"
#include "qobjectlistpropertyitem.h"
#include "gnode.h"

class CustomPropertyItems
{
   public:
      static void registerCustomPropertyItems(QPropertyModel* propertyModel);

      static void registerCustomEditor();
};


template<typename T>
QList<QObject*> convertToQObjectList(const QList<T>& values);

class IDescriptionPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IDescriptionPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IDescriptionPropertyItem(){}
};

class IIdentityPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IIdentityPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IIdentityPropertyItem(){}
};

class IIdentityListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IIdentityListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~IIdentityListPropertyItem(){}
};

class IComponentInfoPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IComponentInfoPropertyItem(){}
};

class IModelComponentInfoPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IModelComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IModelComponentInfoPropertyItem(){}

};

class IModelComponentPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IModelComponentPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IModelComponentPropertyItem(){}
};

class IModelComponentListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IModelComponentListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~IModelComponentListPropertyItem(){}
};

class IDimensionPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IDimensionPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IDimensionPropertyItem(){}
};

class IDimensionListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IDimensionListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~IDimensionListPropertyItem(){}
};

class IValueDefinitionPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IValueDefinitionPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IValueDefinitionPropertyItem(){}
};

class IQualityPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IQualityPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IQualityPropertyItem(){}
};

class IUnitDimensionsPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IUnitDimensionsPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IUnitDimensionsPropertyItem(){}
};

class IUnitPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IUnitPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IUnitPropertyItem(){}
};

class IQuantityPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IQuantityPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IQuantityPropertyItem(){}
};

class IComponentDataItemPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IComponentDataItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IComponentDataItemPropertyItem(){}
};

class IArgumentPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IArgumentPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IArgumentPropertyItem(){}
};

class IArgumentListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IArgumentListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~IArgumentListPropertyItem(){}
};

class IAdaptedOutputFactoryComponentInfoPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IAdaptedOutputFactoryComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IAdaptedOutputFactoryComponentInfoPropertyItem(){}
};

class IAdaptedOutputFactoryPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IAdaptedOutputFactoryPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IAdaptedOutputFactoryPropertyItem(){}
};

class IAdaptedOutputFactoryListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IAdaptedOutputFactoryListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~IAdaptedOutputFactoryListPropertyItem(){}
};

class IInputPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IInputPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IInputPropertyItem(){}
};

class IInputListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IInputListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~IInputListPropertyItem(){}
};

class IExchangeItemPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IExchangeItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IExchangeItemPropertyItem(){}
};

class IOutputPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IOutputPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~IOutputPropertyItem(){}
};

class IOutputListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IOutputListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~IOutputListPropertyItem(){}
};

class GNodePropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE GNodePropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~GNodePropertyItem(){}
};

class ITimePropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimePropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~ITimePropertyItem(){}
};

class ITimeListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimeListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~ITimeListPropertyItem(){}
};


class ITimeSpanPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimeSpanPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~ITimeSpanPropertyItem(){}
};

class ITimeSpanListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimeSpanListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);

      virtual ~ITimeSpanListPropertyItem(){}
};


class ITimeComponentDataItemPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimeComponentDataItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~ITimeComponentDataItemPropertyItem(){}
};


class ITimeExchangeItemPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimeExchangeItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~ITimeExchangeItemPropertyItem(){}
};


class ITimeSeriesIdBasedComponentDataItemPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimeSeriesIdBasedComponentDataItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~ITimeSeriesIdBasedComponentDataItemPropertyItem(){}
};



class ITimeSeriesIdBasedExchangeItemPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE ITimeSeriesIdBasedExchangeItemPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

      virtual ~ITimeSeriesIdBasedExchangeItemPropertyItem(){}
};

//class IUnitPropertyItem : public QObjectPropertyItem
//{
//      Q_OBJECT
//   public:
//      Q_INVOKABLE IUnitPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

//      virtual ~IUnitPropertyItem(){}
//};


//class IUnitDimensionsPropertyItem : public QObjectPropertyItem
//{
//      Q_OBJECT
//   public:
//      Q_INVOKABLE IUnitDimensionsPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);

//      virtual ~IUnitDimensionsPropertyItem(){}
//};

#endif // CUSTOMPROPERTYITEMS_H
