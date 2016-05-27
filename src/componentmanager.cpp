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


#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
   // Windows (x64 and x86)
   addComponentDirectory(QDir("./"));
   manager->addComponentDirectory(QDir("./Components"));

#elif __unix__ // all unices, not all compilers
   // Unix
   addComponentDirectory(QDir("./"));
   addComponentDirectory(QDir("./Components"));
#elif __linux__
   addComponentDirectory(QDir("./"));
   addComponentDirectory(QDir("./Components"));
   // linux
#elif __APPLE__
   addComponentDirectory(QDir("./"));
   addComponentDirectory(QDir("./Components"));
   addComponentDirectory(QDir("./../../../"));
   addComponentDirectory(QDir("./../../../Components"));
#endif

}

ComponentManager::~ComponentManager()
{

   for(IModelComponentInfo* cinfo : m_modelComponentInfoHash.keys())
   {
      emit modelComponentInfoUnloaded(cinfo->id());
   }

   for(QPluginLoader* pl : m_modelComponentInfoHash.values())
   {
      pl->unload();
   }

   //   qDeleteAll(m_modelComponentInfoHash.keys());
   qDeleteAll(m_modelComponentInfoHash.values());
   m_modelComponentInfoHash.clear();


   qDeleteAll(m_adaptedOutputFactoriesHash.values());
   m_adaptedOutputFactoriesHash.clear();

   for(IAdaptedOutputFactoryComponentInfo* cinfo : m_adaptedOutputFactoryComponentInfoHash.keys())
   {
      emit adaptedOutputFactoryComponentUnloaded(cinfo->id());
   }

   for(QPluginLoader* pl : m_adaptedOutputFactoryComponentInfoHash.values())
   {
      pl->unload();
   }

   //   qDeleteAll(m_adaptedOutputFactoryComponentInfoHash.keys());
   qDeleteAll(m_adaptedOutputFactoryComponentInfoHash.values());
   m_adaptedOutputFactoryComponentInfoHash.clear();
}

QList<IModelComponentInfo*> ComponentManager::modelComponentInfoList() const
{
   return m_modelComponentInfoHash.keys();
}

IModelComponentInfo* ComponentManager::findModelComponentInfoById(const QString& id)
{
   QList<IModelComponentInfo*> componentInfos = m_modelComponentInfoHash.keys();

   for (int i = 0; i < componentInfos.length(); i++)
   {
      IModelComponentInfo* modelComponentInfo = componentInfos[i];

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

   if (componentInfo && m_modelComponentInfoHash.contains(componentInfo))
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
      emit postMessage("Unable to unload model component library " + componentInfo->name());
      return false;
   }
}

QList<IAdaptedOutputFactoryComponentInfo*> ComponentManager::adaptedOutputFactoryComponentInfoList() const
{
   return m_adaptedOutputFactoryComponentInfoHash.keys();
}

IAdaptedOutputFactoryComponentInfo* ComponentManager::findAdaptedOutputFactoryComponentInfoById(const QString& id)
{
   QList<IAdaptedOutputFactoryComponentInfo*> componentInfos = m_adaptedOutputFactoryComponentInfoHash.keys();

   for (int i = 0; i < componentInfos.length(); i++)
   {
      IAdaptedOutputFactoryComponentInfo* componentInfo = componentInfos[i];

      if (!componentInfo->id().compare(id))
      {
         return componentInfo;
      }
   }

   return nullptr;
}

QHash<IAdaptedOutputFactoryComponentInfo* , IAdaptedOutputFactoryComponent*> ComponentManager::adaptedOutputFactories() const
{
   return m_adaptedOutputFactoriesHash;
}

bool ComponentManager::unloadAdaptedOutputFactoryComponentInfoById(const QString& id)
{
   IAdaptedOutputFactoryComponentInfo* componentInfo = findAdaptedOutputFactoryComponentInfoById(id);

   if (componentInfo && m_adaptedOutputFactoryComponentInfoHash.contains(componentInfo))
   {
      QPluginLoader* plugin = m_adaptedOutputFactoryComponentInfoHash[componentInfo];
      m_adaptedOutputFactoryComponentInfoHash.remove(componentInfo);

      delete m_adaptedOutputFactoriesHash[componentInfo];
      m_adaptedOutputFactoriesHash.remove(componentInfo);

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

IComponentInfo* ComponentManager::findComponentInfoById(const QString &id)
{
   IComponentInfo* componentInfo = nullptr;

   if( (componentInfo = findModelComponentInfoById(id)))
      return componentInfo;
   else
      return findAdaptedOutputFactoryComponentInfoById(id);

}

IComponentInfo* ComponentManager::loadComponent(const QFileInfo& file)
{
   QString message;

   if (file.exists())
   {

      if (hasValidExtension(file))
      {

         QPluginLoader* pluginLoader = new QPluginLoader(file.absoluteFilePath(), this);

         QObject* component = pluginLoader->instance();

         if (component)
         {

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
                        return model;
                     }
                  }

                  mcomponentInfo->setLibraryFilePath(file.absoluteFilePath());
                  m_modelComponentInfoHash[mcomponentInfo] = pluginLoader;
                  emit modelComponentInfoLoaded(mcomponentInfo);

                  return mcomponentInfo;
               }
            }

            {
               IAdaptedOutputFactoryComponentInfo* acomponentInfo = qobject_cast<IAdaptedOutputFactoryComponentInfo*>(component);

               if (acomponentInfo)
               {
                  acomponentInfo->setLibraryFilePath(file.absoluteFilePath());

                  for (IAdaptedOutputFactoryComponentInfo* model : m_adaptedOutputFactoryComponentInfoHash.keys())
                  {
                     if (!acomponentInfo->id().compare(model->id()))
                     {
                        pluginLoader->unload();
                        emit postMessage("Adapted output factory component library " + acomponentInfo->name() + "has already been loaded");
                        return model;
                     }
                  }


                  m_adaptedOutputFactoryComponentInfoHash[acomponentInfo] = pluginLoader;
                  m_adaptedOutputFactoriesHash[acomponentInfo] = acomponentInfo->createComponentInstance();

                  acomponentInfo->setLibraryFilePath(file.absoluteFilePath());
                  emit adaptedOutputFactoryComponentInfoLoaded(acomponentInfo);
                  return acomponentInfo;
               }
            }

            delete component;
            message = "\"" + file.filePath() + "\" is not a valid HydroCouple component library";
            emit postMessage(message);
            return nullptr;
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

   return nullptr;
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

      for (QFileInfo file : files)
      {
         emit postMessage("Loading component library \"" + file.absoluteFilePath() + "\" ...");

         if (loadComponent(file))
         {
         }
         count = count + 1;
         int progress = (int)(count * 100.0 / files.length());
         emit setProgress(true, progress);
      }

      emit setProgress(false, 0);

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
