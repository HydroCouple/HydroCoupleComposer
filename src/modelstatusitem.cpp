#include "stdafx.h"
#include "modelstatusitem.h"

using namespace HydroCouple;

ModelStatusItem::ModelStatusItem(HydroCouple::IModelComponent *component, ModelStatusItem *parent)
  :QObject(parent)
{
  m_component = component;
  m_parent = parent;

  m_status = new ModelStatusChangeEventArg(this);
  m_status->setStatus(m_component->status());

  connect(dynamic_cast<QObject*>(m_component) , SIGNAL(componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &))
          ,this , SLOT(onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)));

  connect(dynamic_cast<QObject*>(m_component) , SIGNAL(propertyChanged(const QString &))
          ,this , SLOT(onPropertyChanged(const QString &)));

  resetChildren();

}

ModelStatusItem::~ModelStatusItem()
{
  delete m_status;
}

HydroCouple::IModelComponent* ModelStatusItem::component() const
{
  return m_component;
}

ModelStatusChangeEventArg* ModelStatusItem::status() const
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

  ICloneableModelComponent* cloneableModelComponent = dynamic_cast<ICloneableModelComponent*>(m_component);

  if(cloneableModelComponent)
  {
    QList<ICloneableModelComponent*> clones = cloneableModelComponent->clones();

    for(ICloneableModelComponent* model : clones)
    {
      ModelStatusItem* item = new ModelStatusItem(model, this);
      m_children.append(item);
    }

    emit childrenChanged();
  }
}

void ModelStatusItem::onComponentStatusChanged(const QSharedPointer<IComponentStatusChangeEventArgs> &statusChangedEvent)
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
