#include "stdafx.h"
#include "hydrocoupleproject.h"
#include "gmodelcomponent.h"

HydroCoupleProject::HydroCoupleProject(QObject *parent)
   : QObject(parent) , m_hasChanges(true)
{

}

HydroCoupleProject::~HydroCoupleProject()
{
   qDeleteAll(m_modelComponents);
   m_modelComponents.clear();
}

QFileInfo HydroCoupleProject::projectFile() const
{
   return m_projectFile;
}

QList<GModelComponent*> HydroCoupleProject::modelComponents() const
{
   return m_modelComponents;
}

void HydroCoupleProject::addComponent(GModelComponent *component)
{
   if (!contains(component))
   {
      m_modelComponents.append(component);
      connect(component, SIGNAL(setAsTrigger(GModelComponent*)), this, SLOT(onTriggerChanged(GModelComponent*)));
      emit componentAdded(component);
      m_hasChanges = true;
      emit stateModified(m_hasChanges);
   }
}

bool HydroCoupleProject::deleteComponent(GModelComponent *component)
{
   if (m_modelComponents.removeAll(component))
   {
      emit componentDeleting(component);

      for(GModelComponent* model : m_modelComponents)
      {
         for(GModelComponentConnection* connection : model->modelComponentConnections())
         {
            if(connection->consumerComponent() == component)
            {
               model->deleteComponentConnection(connection);
            }
         }
      }

      delete component;      m_hasChanges = true;
      emit stateModified(m_hasChanges);
      return true;
   }
   else
   {
      return false;
   }
}

bool HydroCoupleProject::hasChanges() const
{
   return m_hasChanges;
}

HydroCoupleProject* HydroCoupleProject::readProjectFile(const QFileInfo &file)
{
   //implemented later
   return nullptr;
}

void HydroCoupleProject::onTriggerChanged(GModelComponent* triggerModelComponent)
{
   for (QList<GModelComponent*>::Iterator it = m_modelComponents.begin();
        it != m_modelComponents.end(); it++)
   {
      if (*it != triggerModelComponent)
      {
         (*it)->setTrigger(false);
      }
   }
}

void HydroCoupleProject::onSaveProject()
{
   m_hasChanges = false;
   emit stateModified(m_hasChanges);
}

void HydroCoupleProject::onSaveProjectAs(const QFileInfo &file)
{
   m_projectFile = file;

   m_hasChanges = false;
   emit stateModified(m_hasChanges);
}

void HydroCoupleProject::setHasChanges(bool hasChanges)
{
   m_hasChanges = hasChanges;
   emit stateModified(m_hasChanges);
}

bool HydroCoupleProject::contains(GModelComponent* component) const
{
   for (int i = 0; i < m_modelComponents.count(); i++)
   {
      GModelComponent* mcomponent = m_modelComponents[i];

      if (mcomponent == component)
      {
         return true;
         break;
      }
   }

   return false;
}
