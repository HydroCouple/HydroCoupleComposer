#include "stdafx.h"
#include "componentmanager.h"

using namespace HydroCouple;

ComponentManager::ComponentManager(QObject *parent)
   : QObject(parent)
{

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
   m_componentFileExtensions.insert("*.dll");
#elif __unix__ // all unices, not all compilers
   m_componentFileExtensions.insert("*.so");
#elif __linux__
   m_componentFileExtensions.insert("*.so");
#elif __APPLE__
   m_componentFileExtensions.insert("*.dylib");
#endif
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
            message = "\"" + file.filePath() + "\" is not a valid HydroCouple component library";
            emit postMessage(message);
            return false;
         }
         else
         {
            message = "'/" + file.filePath() + "'/ is not a valid Qt plugin library";
            emit postMessage(message);
         }

         pluginLoader->unload();

      }
   }
   else
   {
      message = "File '" + file.filePath() + "' does not exist";
      emit postMessage(message);
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

   QList<QFileInfo> files;

   if (!m_componentDirectories.contains(directory))
   {
      m_componentDirectories.append(directory);

      QDirIterator it(directory.absolutePath(), QDirIterator::Subdirectories);

      QString message = "Checking \"" + directory.absolutePath() + "\" for component libraries...";

      emit postMessage(message);


      while (it.hasNext())
      {
         it.next();

         if (it.fileInfo().isFile() && hasValidExtension(it.fileInfo()))
         {
            files.append(it.fileInfo());

         }
      }

      float count = 0;

      for(QFileInfo file : files)
      {
         emit postMessage("Loading component library \"" + file.absoluteFilePath() + "\" ...");

         if(loadComponent(file))
         {
         }
         count = count + 1;
         int progress = (int)(count * 100.0 /files.length());
         emit setProgress(true , progress);
      }

      emit setProgress(false , 0);

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
