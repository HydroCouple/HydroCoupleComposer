#include "stdafx.h"
#include "modelstatusitem.h"

using namespace HydroCouple;

ModelStatusItem::ModelStatusItem(HydroCouple::IModelComponent *component, ModelStatusItem *parent)
   :QObject(parent)
{
   m_component = component;
   m_parent = parent;

   m_status = new ModelStatusItemStatusChangeEventArgsWrapper(this);
   m_status->setStatus(m_component->status());

   connect(dynamic_cast<QObject*>(m_component) , SIGNAL(componentStatusChanged(const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> &))
           ,this , SLOT(onComponentStatusChanged(const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> &)));

   connect(dynamic_cast<QObject*>(m_component) , SIGNAL(propertyChanged(const QString &))
           ,this , SLOT(onPropertyChanged(const QString &)));

   resetChildren();

}

ModelStatusItem::~ModelStatusItem()
{

}

HydroCouple::IModelComponent* ModelStatusItem::component() const
{
   return m_component;
}

ModelStatusItemStatusChangeEventArgsWrapper* ModelStatusItem::status() const
{
   return m_status;
}

QModelIndex ModelStatusItem::index(int column) const
{
   return m_indexes[column];
}

QList<ModelStatusItem*> ModelStatusItem::childModelStatusItems() const
{
   return m_children;
}

ModelStatusItem* ModelStatusItem::parentModelStatusItem() const
{
   return m_parent;
}

void ModelStatusItem::resetChildren()
{
   emit childrenChanging();

   qDeleteAll(m_children);
   m_children.clear();

   QList<IModelComponent*> clones = m_component->clones();

   for(IModelComponent* model : clones)
   {
      ModelStatusItem* item = new ModelStatusItem(model, this);
      m_children.append(item);
   }

   emit childrenChanged();
}

void ModelStatusItem::onComponentStatusChanged(const std::shared_ptr<IComponentStatusChangeEventArgs> &statusChangedEvent)
{
   m_status->setStatus(statusChangedEvent);
   emit componentStatusChanged(statusChangedEvent);
}

void ModelStatusItem::onPropertyChanged(const QString & propertyName)
{
   if(!propertyName.compare("Clones"))
   {
      resetChildren();
   }
   else
   {
      emit propertyChanged();
   }
}
