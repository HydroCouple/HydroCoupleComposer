#include "stdafx.h"
#include "componentmanager.h"

using namespace HydroCouple;

ComponentManager::ComponentManager(QObject *parent)
   : QObject(parent)
{
   m_componentFileExtensions.insert("*.dll");
   m_componentFileExtensions.insert("*.dylib");
   m_componentFileExtensions.insert("*.so");
}

ComponentManager::~ComponentManager()
{
   qDeleteAll(m_modelComponentInfoList);
   m_modelComponentInfoList.clear();

   qDeleteAll(m_adaptedOutputFactoryComponentInfoList);
   m_adaptedOutputFactoryComponentInfoList.clear();
}

QList<IModelComponentInfo*> ComponentManager::modelComponentInfoList() const
{
   return m_modelComponentInfoList;
}

IModelComponentInfo* ComponentManager::findModelComponentInfoById(const QString& id)
{
   for (QList<IModelComponentInfo*>::iterator it = m_modelComponentInfoList.begin();
        it != m_modelComponentInfoList.end(); it++)
   {
      IModelComponentInfo* modelComponentInfo = *it;

      if (!modelComponentInfo->id().compare(id))
      {
         return modelComponentInfo;
      }
   }

   return nullptr;
}

bool ComponentManager::removeModelComponentInfoById(const QString& id)
{
   IModelComponentInfo* componentInfo = findModelComponentInfoById(id);

   if(componentInfo && m_modelComponentInfoList.removeOne(componentInfo))
   {
      emit modelComponentInfoRemoved(componentInfo);
      delete componentInfo;
      return true;
   }
   else
   {
      return false;
   }
}

QList<IAdaptedOutputFactoryComponentInfo*> ComponentManager::adaptedOutputFactoryComponentInfoList() const
{
   return m_adaptedOutputFactoryComponentInfoList;
}

IAdaptedOutputFactoryComponentInfo* ComponentManager::findAdaptedOutputFactoryComponentInfoById(const QString& id)
{
   for (QList<IAdaptedOutputFactoryComponentInfo*>::iterator it = m_adaptedOutputFactoryComponentInfoList.begin();
        it != m_adaptedOutputFactoryComponentInfoList.end(); it++)
   {
      IAdaptedOutputFactoryComponentInfo*adaptedOutputFactoryComponentInfo = *it;

      if (!adaptedOutputFactoryComponentInfo->id().compare(id))
      {
         return adaptedOutputFactoryComponentInfo;
      }
   }

   return nullptr;
}

bool ComponentManager::removeAdaptedOutputFactoryComponentInfoById(const QString& id)
{
   IAdaptedOutputFactoryComponentInfo* componentInfo = findAdaptedOutputFactoryComponentInfoById(id);

   if(componentInfo && m_adaptedOutputFactoryComponentInfoList.removeOne(componentInfo))
   {
      emit adaptedOutputFactoryComponentRemoved(componentInfo);
      delete componentInfo;
      return true;
   }
   else
   {
      return false;
   }
}

bool ComponentManager::addComponent(const QFileInfo& file)
{
   QString message;

   if (file.exists())
   {
      qDebug() << file.filePath();

      if (hasValidExtension(file))
      {
         QPluginLoader pluginLoader(file.filePath());
         QObject* component = pluginLoader.instance();

         if (component)
         {
            IModelComponentInfo* mcomponentInfo = qobject_cast<IModelComponentInfo*>(component);

            if (mcomponentInfo)
            {

               qDebug() << mcomponentInfo->id();

               mcomponentInfo->setLibraryFilePath(file.filePath());

               for (IModelComponentInfo* model : m_modelComponentInfoList)
               {
                  qDebug() << model->id();

                  if (!mcomponentInfo->id().compare(model->id()))
                  {
                     return true;
                  }
               }

               m_modelComponentInfoList.append(mcomponentInfo);
               emit modelComponentInfoLoaded(mcomponentInfo);
               return true;
            }

            IAdaptedOutputFactoryComponentInfo* acomponentInfo = qobject_cast<IAdaptedOutputFactoryComponentInfo*>(component);

            if (acomponentInfo)
            {
               acomponentInfo->setLibraryFilePath(file.filePath());
               for (IAdaptedOutputFactoryComponentInfo* model : m_adaptedOutputFactoryComponentInfoList)
               {
                  if (!acomponentInfo->id().compare(model->id()))
                  {
                     delete acomponentInfo;
                     return true;
                  }
               }
               m_adaptedOutputFactoryComponentInfoList.append(acomponentInfo);
               emit adaptedOutputFactoryComponentInfoLoaded(acomponentInfo);
               return true;
            }

            delete component;
            message = "'/" + file.filePath() + "'/ is not a valid HydroCouple component";
            emit statusChangedMessage(message);
            emit statusChangedMessage(message ,  10);
            return false;
         }
         else
         {
            message = "'/" + file.filePath() + "'/ is not a valid Qt plugin";
            emit statusChangedMessage(message);
            emit statusChangedMessage(message, 10);
         }
      }
   }
   else
   {
      message = "File '" + file.filePath() + "' does not exist";
      emit statusChangedMessage(message);
      emit statusChangedMessage(message, 10);
   }

   return false;
}

QSet<QString> ComponentManager::componentFileExtensions() const
{
   return m_componentFileExtensions;
}

void ComponentManager::addComponentFileExtensions(const QString& extension)
{
   m_componentFileExtensions.insert(extension.toLower());
}

QList<QDir> ComponentManager::componentDirectories() const
{
   return m_componentDirectories;
}

void ComponentManager::addComponentDirectory(const QDir& directory)
{
   if (!m_componentDirectories.contains(directory))
   {
      m_componentDirectories.append(directory);

      QDirIterator it(directory.absolutePath(), QDirIterator::Subdirectories);

      QString message = "Checking '/" + directory.absolutePath() + "'/ for plugins";

      emit statusChangedMessage(message);
      emit statusChangedMessage(message, 0);

      while (it.hasNext())
      {
         it.next();
         if (it.fileInfo().isFile())
         {
            addComponent(it.fileInfo());
         }
      }
   }
}

bool ComponentManager::hasValidExtension(const QFileInfo& file)
{
   if (m_componentFileExtensions.contains("*." + file.suffix())
       || m_componentFileExtensions.contains("*." + file.completeSuffix()))
      return true;
   else
      return false;
}
