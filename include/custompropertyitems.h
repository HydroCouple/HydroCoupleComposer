#ifndef CUSTOMPROPERTYITEMS_H
#define CUSTOMPROPERTYITEMS_H

#include "qobjectpropertyitem.h"
#include "qpropertymodel.h"
#include "hydrocouple.h"
#include "qobjectlistpropertyitem.h"

class CustomPropertyItems
{
   public:
      static void registerCustomPropertyItems(QPropertyModel* propertyModel);

      static void registerCustomEditor();
};


template<typename T>
QList<QObject*> convertToQObjectList(const QList<T>& values);

class IComponentInfoPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};

class IModelComponentInfoPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IModelComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};

class IModelComponentPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IModelComponentPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};

class IModelComponentListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IModelComponentListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);
};

class IAdaptedOutputFactoryComponentInfoPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IAdaptedOutputFactoryComponentInfoPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};

class IAdaptedOutputFactoryPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IAdaptedOutputFactoryPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};


class IAdaptedOutputFactoryListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IAdaptedOutputFactoryListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);
};


class IArgumentPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IArgumentPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};

class IArgumentListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT

   public:
      Q_INVOKABLE IArgumentListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);
};


class IInputPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IInputPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};


class IInputListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IInputListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);
};

class IOutputPropertyItem : public QObjectPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IOutputPropertyItem(const QVariant& value , const QMetaProperty& prop, QPropertyItem * parent);
};

class IOutputListPropertyItem : public QObjectListPropertyItem
{
      Q_OBJECT
   public:
      Q_INVOKABLE IOutputListPropertyItem(const QVariant& value , const QMetaProperty& prop, QObjectClassPropertyItem * parent);
};


Q_DECLARE_METATYPE(HydroCouple::IComponentInfo*)
Q_DECLARE_METATYPE(HydroCouple::IModelComponentInfo*)
Q_DECLARE_METATYPE(HydroCouple::IModelComponent*)
Q_DECLARE_METATYPE(HydroCouple::IAdaptedOutputFactoryComponentInfo*)
Q_DECLARE_METATYPE(HydroCouple::IAdaptedOutputFactory*)
Q_DECLARE_METATYPE(QList<HydroCouple::IAdaptedOutputFactory*>)
Q_DECLARE_METATYPE(HydroCouple::IArgument*)
Q_DECLARE_METATYPE(QList<HydroCouple::IArgument*>)
Q_DECLARE_METATYPE(HydroCouple::IInput*)
Q_DECLARE_METATYPE(QList<HydroCouple::IInput*>)
Q_DECLARE_METATYPE(HydroCouple::IOutput*)
Q_DECLARE_METATYPE(QList<HydroCouple::IOutput*>)



#endif // CUSTOMPROPERTYITEMS_H
