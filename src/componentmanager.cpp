#include "stdafx.h"
#include "componentmanager.h"
#include "hydrocoupleproject.h"
#include "gmodelcomponent.h"

#include <stdlib.h>
#include <QApplication>
#include <QDirIterator>
#include <QJsonDocument>

#include <QtDebug>

using namespace HydroCouple;

ComponentManager::ComponentManager(HydroCoupleProject *parent)
  : QObject(parent),
    m_project(parent)
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

  QString message;
  QList<QString> modelComponents = modelComponentInfoIdentifiers();
  for(QString identifier: modelComponents)
    unloadModelComponentInfo(identifier, message);

  QList<QString> adaptedFactoryComponents = adaptedFactoryComponentInfoIdentifiers();
  for(QString identifier: adaptedFactoryComponents)
    unloadAdaptedOutputFactoryComponentInfo(identifier, message);

  QList<QString> workflowComponents = workflowComponentInfoIdentifiers();
  for(QString identifier: workflowComponents)
    unloadWorkflowComponentInfo(identifier, message);
}

IComponentInfo* ComponentManager::loadComponent(const QFileInfo& file)
{
  QString message;

  if (file.isFile() && file.exists())
  {
    if (hasValidExtension(file))
    {

#ifdef __APPLE__
#elif __unix__
#elif __linux__
#elif _WIN32
#endif

      QString current = QDir::currentPath();
      QDir::setCurrent(file.absolutePath());

      QPluginLoader* pluginLoader = nullptr;

#ifdef USE_OPENMP
#pragma omp critical (ComponentManager)
#endif
      {
        pluginLoader = new QPluginLoader(file.absoluteFilePath(), this);
      }


      QJsonDocument jdoc(pluginLoader->metaData());

      QString strJSON(jdoc.toJson(QJsonDocument::Compact));

      printf("Component JSON: %s\n", strJSON.toStdString().c_str());

      QObject* component = pluginLoader->instance();

      QDir::setCurrent(current);

      if (component)
      {
        IComponentInfo *componentInfo =  qobject_cast<IComponentInfo*>(component);
        QString message;
        componentInfo->validateLicense(message);
        emit postMessage(message);

        if(componentInfo)
        {
          printf("loading component file: %s\n", qPrintable(file.absoluteFilePath()));

          //IModelComponentInfo
          {
            IModelComponentInfo* mcomponentInfo = qobject_cast<IModelComponentInfo*>(component);

            if (mcomponentInfo)
            {
              IModelComponentInfo *tempComponentInfo ;

              if((tempComponentInfo = findModelComponentInfo(mcomponentInfo->id())))
              {
                message = "Model component library " + tempComponentInfo->id() + "has already been loaded";
                emit postMessage(message);
                return tempComponentInfo ;
              }

              mcomponentInfo->setLibraryFilePath(file.absoluteFilePath());

              m_modelComponentInfoHash[mcomponentInfo] = QList<IModelComponent*>();
              m_modelComponentInfoPluginLoader[mcomponentInfo] = pluginLoader;

              emit modelComponentInfoLoaded(mcomponentInfo->id());
              return mcomponentInfo;
            }
          }

          //IAdaptedOutputFactoryComponentInfo
          {
            IAdaptedOutputFactoryComponentInfo* acomponentInfo = qobject_cast<IAdaptedOutputFactoryComponentInfo*>(component);

            if (acomponentInfo)
            {
              acomponentInfo->setLibraryFilePath(file.absoluteFilePath());

              IAdaptedOutputFactoryComponentInfo *tempAdaptedOutputComponentInfo = nullptr;

              if ((tempAdaptedOutputComponentInfo = findAdaptedOutputFactoryComponentInfo(acomponentInfo->id())))
              {
                //              delete acomponentInfo;

                message = "Adapted output factory component library " + tempAdaptedOutputComponentInfo->id() + "has already been loaded";
                emit postMessage(message);
                return tempAdaptedOutputComponentInfo;
              }

              IAdaptedOutputFactoryComponent* adaptedFactoryComponent = acomponentInfo->createComponentInstance();

              m_adaptedOutputFactoryComponentInfoHash[acomponentInfo] = adaptedFactoryComponent;
              m_adaptedOutputFactoryComponentInfoPluginLoader[acomponentInfo] = pluginLoader;

              emit adaptedOutputFactoryComponentInfoLoaded(acomponentInfo->id());
              return acomponentInfo;
            }
          }

          //IWorkflowComponentInfo
          {
            IWorkflowComponentInfo* wcomponentInfo = qobject_cast<IWorkflowComponentInfo*>(component);

            if (wcomponentInfo)
            {
              wcomponentInfo->setLibraryFilePath(file.absoluteFilePath());

              IWorkflowComponentInfo *tempWorkflowComponentInfo = nullptr;

              if ((tempWorkflowComponentInfo = findWorkflowComponentInfo(wcomponentInfo->id())))
              {
                //              delete wcomponentInfo;
                message = "Adapted output factory component library " + tempWorkflowComponentInfo->id() + "has already been loaded";
                emit postMessage(message);
                return tempWorkflowComponentInfo;
              }

              IWorkflowComponent *workflowComponent = wcomponentInfo->createComponentInstance();

              m_workflowComponentInfoHash[wcomponentInfo] = workflowComponent;
              m_workflowComponentInfoPluginLoader[wcomponentInfo] = pluginLoader;

              emit workflowComponentInfoLoaded(wcomponentInfo->id());
              return wcomponentInfo;
            }
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

        if(pluginLoader->errorString().size())
        {
          QString pluginError = pluginLoader->errorString();
          emit postMessage(pluginError);
          printf("%s\n", pluginError.toStdString().c_str());
        }
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

  if (directory.exists() && !m_componentDirectories.contains(directory))
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
      if(file.isSymLink())
      {
        QFileInfo tempFile(file.symLinkTarget());
        emit postMessage("Loading component library \"" + tempFile.absoluteFilePath() + "\" ...");
        loadComponent(tempFile);
      }
      else
      {
        emit postMessage("Loading component library \"" + file.absoluteFilePath() + "\" ...");
        loadComponent(file);
      }


      count = count + 1;
      int progress = (int)(count * 100.0 / files.length());
      emit setProgress(true, progress);
    }

    emit setProgress(false, 0);

  }
}

QList<QString> ComponentManager::modelComponentInfoIdentifiers() const
{
  QList<QString> identifiers;

  for(const IModelComponentInfo* modelComponent : m_modelComponentInfoHash.keys())
  {
    identifiers.append(modelComponent->id());
  }

  return identifiers;
}

QList<QString> ComponentManager::adaptedFactoryComponentInfoIdentifiers() const
{
  QList<QString> identifiers;

  for(const IAdaptedOutputFactoryComponentInfo* adaptedOutputFactoryComponentInfo : m_adaptedOutputFactoryComponentInfoHash.keys())
  {
    identifiers.append(adaptedOutputFactoryComponentInfo->id());
  }

  return identifiers;
}

QList<QString> ComponentManager::workflowComponentInfoIdentifiers() const
{
  QList<QString> identifiers;

  for(const IWorkflowComponentInfo* adaptedOutputFactoryComponentInfo : m_workflowComponentInfoHash.keys())
  {
    identifiers.append(adaptedOutputFactoryComponentInfo->id());
  }

  return identifiers;
}

bool ComponentManager::unloadModelComponentInfo(const QString &componentInfoId, QString &message)
{
  IModelComponentInfo *componentInfo = nullptr;

  if ((componentInfo = findModelComponentInfo(componentInfoId)))
  {
    QPluginLoader* plugin = m_modelComponentInfoPluginLoader[componentInfo];

    m_modelComponentInfoPluginLoader.remove(componentInfo);
    m_modelComponentInfoHash.remove(componentInfo);

    if(m_project)
    {
      m_project->deleteModelComponentsAssociatedWithComponentInfo(componentInfo);
    }

    emit modelComponentInfoUnloaded(componentInfo->id());
    emit postMessage("Model component library " + componentInfo->id() + " unloaded");

    delete componentInfo;

    plugin->unload();

    return true;
  }

  message = "Model Component: " + componentInfoId + " was not found!";
  emit postMessage(message);
  return false;
}

bool ComponentManager::unloadAdaptedOutputFactoryComponentInfo(const QString& componentInfoId, QString &message)
{
  IAdaptedOutputFactoryComponentInfo *componentInfo = nullptr;

  if((componentInfo = findAdaptedOutputFactoryComponentInfo(componentInfoId)))
  {
    QPluginLoader* plugin = m_adaptedOutputFactoryComponentInfoPluginLoader[componentInfo];

    m_adaptedOutputFactoryComponentInfoPluginLoader.remove(componentInfo);
    m_adaptedOutputFactoryComponentInfoHash.remove(componentInfo);
    m_adaptedFactoryComponentOutputs.remove(componentInfo);

    if(m_project)
    {
      m_project->deleteAdaptedOutputsAssociatedWithComponentInfo(componentInfo);
    }

    emit adaptedOutputFactoryComponentInfoUnloaded(componentInfo->id());
    emit postMessage("Adapted output factory component library " + componentInfo->id() + " unloaded");

    delete componentInfo;

    plugin->unload();

    return true;
  }


  message = "Adapted Output Factory Componnent: " +  componentInfoId + " was not found!";
  emit postMessage(message);
  return false;
}

bool ComponentManager::unloadWorkflowComponentInfo(const QString &componentInfoId, QString &message)
{
  IWorkflowComponentInfo* componentInfo = nullptr;

  if ((componentInfo = findWorkflowComponentInfo(componentInfoId)))
  {
    QPluginLoader* plugin = m_workflowComponentInfoPluginLoader[componentInfo];

    m_workflowComponentInfoPluginLoader.remove(componentInfo);
    IWorkflowComponent *workflowComponent = m_workflowComponentInfoHash[componentInfo];
    m_workflowComponentInfoHash.remove(componentInfo);

    emit workflowComponentInfoUnloaded(componentInfo->id());
    emit postMessage("Model component library " + componentInfo->id() + " unloaded");

    delete workflowComponent;
    delete componentInfo;

    plugin->unload();

    return true;
  }

  message = "Workflow Component: " + componentInfoId + " was not found!";
  emit postMessage(message);

  return false;
}

IModelComponent *ComponentManager::createModelComponentInstance(const QString &componentInfoId, QString &message)
{
  IModelComponentInfo *componentInfo = nullptr;

  if((componentInfo = findModelComponentInfo(componentInfoId)))
  {
    IModelComponent *component = componentInfo->createComponentInstance();
    m_modelComponentInfoHash[componentInfo].push_back(component);
    return component;
  }

  message = "Component with id: " + componentInfoId + " was not found !";

  return nullptr;
}

QList<IIdentity*> ComponentManager::getAvailableAdaptedOutputs(const QString &adaptedOutputFactoryComponentInfoId, IOutput *provider, IInput *consumer)
{
  QList<IIdentity*> foundAdaptedOutputs;
  IAdaptedOutputFactoryComponentInfo *componentInfo = nullptr;

  if((componentInfo = findAdaptedOutputFactoryComponentInfo(adaptedOutputFactoryComponentInfoId)))
  {
    IAdaptedOutputFactoryComponent *adaptedOutputFactoryComponent = m_adaptedOutputFactoryComponentInfoHash[componentInfo];
    foundAdaptedOutputs = adaptedOutputFactoryComponent->getAvailableAdaptedOutputIds(provider, consumer);
    return foundAdaptedOutputs;
  }

  return foundAdaptedOutputs;
}

IAdaptedOutput *ComponentManager::createAdaptedOutputInstance(const QString &adaptedOutputFactoryComponentInfoId, const QString &identity, IOutput *provider, IInput *consumer)
{
  IAdaptedOutputFactoryComponentInfo *componentInfo = nullptr;

  if((componentInfo = findAdaptedOutputFactoryComponentInfo(adaptedOutputFactoryComponentInfoId)))
  {
    IAdaptedOutputFactoryComponent *adaptedOutputFactoryComponent = m_adaptedOutputFactoryComponentInfoHash[componentInfo];
    QList<IIdentity*> availableAdaptors = adaptedOutputFactoryComponent->getAvailableAdaptedOutputIds(provider, consumer);

    for(IIdentity *adaptorId :  availableAdaptors)
    {
      if(!QString::compare(identity, adaptorId->id()))
      {
        IAdaptedOutput *adaptedOutput = adaptedOutputFactoryComponent->createAdaptedOutput(adaptorId, provider, consumer);
        m_adaptedFactoryComponentOutputs[componentInfo].append(adaptedOutput);
        return adaptedOutput;
      }
    }
  }

  return nullptr;
}

IWorkflowComponent *ComponentManager::getWorkflowComponent(const QString &componentInfoId)
{
  IWorkflowComponentInfo *componentInfo;

  if((componentInfo = findWorkflowComponentInfo(componentInfoId)))
  {
    IWorkflowComponent *workflowComponent = m_workflowComponentInfoHash[componentInfo];
    return workflowComponent;
  }

  return nullptr;
}

int ComponentManager::numModelComponentInstances(const QString &componentInfoId)
{
  IModelComponentInfo *componentInfo = findModelComponentInfo(componentInfoId);

  if(componentInfo)
  {
    return m_modelComponentInfoHash[componentInfo].size();
  }

  return 0;
}

int ComponentManager::numAdaptedOutputInstances(const QString &componentInfoId)
{
  IAdaptedOutputFactoryComponentInfo *componentInfo = findAdaptedOutputFactoryComponentInfo(componentInfoId);

  if(componentInfo)
  {
    return m_adaptedFactoryComponentOutputs[componentInfo].size();
  }

  return 0;
}

void ComponentManager::removeModelInstance(const QString &componentInfoId, IModelComponent *modelComponent)
{
  IModelComponentInfo *modelComponentInfo = findModelComponentInfo(componentInfoId);

  if(modelComponentInfo)
  {
    m_modelComponentInfoHash[modelComponentInfo].removeAll(modelComponent);
  }
}

void ComponentManager::removeAdaptedOutputInstance(const QString &componentInfoId, IAdaptedOutput *adaptedOutput)
{
  IAdaptedOutputFactoryComponentInfo *componentInfo = findAdaptedOutputFactoryComponentInfo(componentInfoId);

  if(componentInfo)
  {
    m_adaptedFactoryComponentOutputs[componentInfo].removeAll(adaptedOutput);
  }
}

IModelComponentInfo *ComponentManager::findModelComponentInfo(const QString &componentInfoId)
{
  for(IModelComponentInfo *componentInfo : m_modelComponentInfoHash.keys())
  {
    if(!QString::compare(componentInfo->id(), componentInfoId))
    {
      return componentInfo;
    }
  }

  return nullptr;
}

IAdaptedOutputFactoryComponentInfo *ComponentManager::findAdaptedOutputFactoryComponentInfo(const QString &componentInfoId)
{
  for(IAdaptedOutputFactoryComponentInfo *componentInfo : m_adaptedOutputFactoryComponentInfoHash.keys())
  {
    if(!QString::compare(componentInfo->id(), componentInfoId))
    {
      return componentInfo;
    }
  }

  return nullptr;
}

IAdaptedOutputFactoryComponent *ComponentManager::getAdaptedOutputFactoryComponent(const QString &componentInfoId)
{
  IAdaptedOutputFactoryComponentInfo *componentInfo = findAdaptedOutputFactoryComponentInfo(componentInfoId);

  if(componentInfo)
  {
    return m_adaptedOutputFactoryComponentInfoHash[componentInfo];
  }

  return nullptr;
}

IWorkflowComponentInfo *ComponentManager::findWorkflowComponentInfo(const QString &componentInfoId)
{
  for(IWorkflowComponentInfo *componentInfo : m_workflowComponentInfoHash.keys())
  {
    if(!QString::compare(componentInfo->id(), componentInfoId))
    {
      return componentInfo;
    }
  }

  return nullptr;
}

bool ComponentManager::hasValidExtension(const QFileInfo& file)
{
  if (m_componentFileExtensions.contains("*." + file.suffix())
      || m_componentFileExtensions.contains("*." + file.completeSuffix()))
    return true;
  else
    return false;
}
