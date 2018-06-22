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


  m_componentId = m_component->id();
  m_componentCaption = m_component->caption();

  resetChildren();



}

ModelStatusItem::~ModelStatusItem()
{ 
  delete m_status;
}

QString ModelStatusItem::componenentId() const
{
  return m_componentId;
}

QString ModelStatusItem::componentCaption() const
{
  return m_componentCaption;
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
  else if(!propertyName.compare("Id", Qt::CaseInsensitive) &&
          !propertyName.compare("Caption", Qt::CaseSensitive))
  {

    m_componentId = m_component->id();
    m_componentCaption = m_component->caption();
    emit propertyChanged();
  }
}
