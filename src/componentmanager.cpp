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
   qDeleteAll(m_modelComponentInfoHash.keys());
   m_modelComponentInfoHash.clear();

   qDeleteAll(m_adaptedOutputFactoryComponentInfoHash.keys());
   m_adaptedOutputFactoryComponentInfoHash.clear();
}

QList<IModelComponentInfo*> ComponentManager::modelComponentInfoList() const
{
   return m_modelComponentInfoHash.keys();
}

IModelComponentInfo* ComponentManager::findModelComponentInfoById(const QString& id)
{
   for (QList<IModelComponentInfo*>::iterator it = m_modelComponentInfoHash.keys().begin();
        it != m_modelComponentInfoHash.keys().end(); it++)
   {
      IModelComponentInfo* modelComponentInfo = *it;

      if (!modelComponentInfo->id().compare(id))
      {
         return modelComponentInfo;
      }
   }

   return nullptr;
}

bool ComponentManager::unloadModelComponentInfoById(const QString& id)
{
   IModelComponentInfo* componentInfo = findModelComponentInfoById(id);

   emit postMessage("Unloading model component library " + componentInfo->name() + "...");

   if(componentInfo && m_modelComponentInfoHash.contains(componentInfo))
   {



      QPluginLoader* plugin = m_modelComponentInfoHash[componentInfo];
      m_modelComponentInfoHash.remove(componentInfo);

      emit modelComponentInfoUnloaded(componentInfo->id());
      emit postMessage("Model component library " + componentInfo->name() + " unloaded");

      plugin->unload();

      return true;
   }
   else
   {
      emit postMessage("Unable to unload model component library " + componentInfo->name() );
      return false;
   }
}

QList<IAdaptedOutputFactoryComponentInfo*> ComponentManager::adaptedOutputFactoryComponentInfoList() const
{
   return m_adaptedOutputFactoryComponentInfoHash.keys();
}

IAdaptedOutputFactoryComponentInfo* ComponentManager::findAdaptedOutputFactoryComponentInfoById(const QString& id)
{
   for (QList<IAdaptedOutputFactoryComponentInfo*>::iterator it = m_adaptedOutputFactoryComponentInfoHash.keys().begin();
        it != m_adaptedOutputFactoryComponentInfoHash.keys().end(); it++)
   {
      IAdaptedOutputFactoryComponentInfo*adaptedOutputFactoryComponentInfo = *it;

      if (!adaptedOutputFactoryComponentInfo->id().compare(id))
      {
         return adaptedOutputFactoryComponentInfo;
      }
   }

   return nullptr;
}

bool ComponentManager::unloadAdaptedOutputFactoryComponentInfoById(const QString& id)
{
   IAdaptedOutputFactoryComponentInfo* componentInfo = findAdaptedOutputFactoryComponentInfoById(id);

   emit postMessage("Unloading adapted output factory component library " + componentInfo->name() + "...");

   if(componentInfo && m_adaptedOutputFactoryComponentInfoHash.contains(componentInfo))
   {
      QPluginLoader* plugin = m_adaptedOutputFactoryComponentInfoHash[componentInfo];
      m_adaptedOutputFactoryComponentInfoHash.remove(componentInfo);

      emit adaptedOutputFactoryComponentUnloaded(componentInfo->id());
      emit postMessage("Adapted output factory component library " + componentInfo->name() + " unloaded");

      plugin->unload();

      return true;
   }
   else
   {
      emit postMessage("Unable to unload adapted output factory component library " + componentInfo->name());
      return false;
   }
}

bool ComponentManager::loadComponent(const QFileInfo& file)
{
   QString message;

   if (file.exists())
   {

      if (hasValidExtension(file))
      {
         QPluginLoader* pluginLoader = new QPluginLoader(file.filePath() , this);

         QObject* component = pluginLoader->instance();

         if (component)
         {
            IModelComponentInfo* mcomponentInfo = qobject_cast<IModelComponentInfo*>(component);

            if (mcomponentInfo)
            {
               emit postMessage("Loading model component library " + mcomponentInfo->name() + "...");

               for (IModelComponentInfo* model : m_modelComponentInfoHash.keys())
               {
                  if (!mcomponentInfo->id().compare(model->id()))
                  {
                     pluginLoader->unload();
                     emit postMessage("Model component library " + mcomponentInfo->name() + "has already been loaded");
                     return true;
                  }
               }

               mcomponentInfo->setLibraryFilePath(file.filePath());
               m_modelComponentInfoHash[mcomponentInfo] = pluginLoader;
               emit modelComponentInfoLoaded(mcomponentInfo);

               return true;
            }

            IAdaptedOutputFactoryComponentInfo* acomponentInfo = qobject_cast<IAdaptedOutputFactoryComponentInfo*>(component);

            if (acomponentInfo)
            {
               emit postMessage("Loading adapted output factor component library " + acomponentInfo->name() + "...");

               acomponentInfo->setLibraryFilePath(file.filePath());

               for (IAdaptedOutputFactoryComponentInfo* model : m_adaptedOutputFactoryComponentInfoHash.keys())
               {
                  if (!acomponentInfo->id().compare(model->id()))
                  {
                     pluginLoader->unload();
                     emit postMessage("Adapted output factory component library " + mcomponentInfo->name() + "has already been loaded");
                     return true;
                  }
               }


               m_adaptedOutputFactoryComponentInfoHash[acomponentInfo] = pluginLoader;
               acomponentInfo->setLibraryFilePath(file.filePath());
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

         pluginLoader->unload();

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


      while (it.hasNext())
      {
         it.next();

         if (it.fileInfo().isFile())
         {
            loadComponent(it.fileInfo());
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
