#ifndef COMPONENTMANAGER_H
#define COMPONENTMANAGER_H

#include <QObject>
#include <QDir>
#include <hydrocouple.h>
#include <QSet>
#include <QPluginLoader>

class HydroCoupleProject;

class ComponentManager : public QObject
{
      Q_OBJECT

      friend class HydroCoupleComposer;
      friend class GAdaptedOutput;

   public:

      ComponentManager(HydroCoupleProject *parent);

      virtual ~ComponentManager();

      HydroCouple::IComponentInfo* loadComponent(const QFileInfo& file);

      QSet<QString> componentFileExtensions() const;

      void addComponentFileExtensions(const QString& extension);

      QList<QDir> componentDirectories() const;

      void addComponentDirectory(const QDir& directory);


      QList<QString> modelComponentInfoIdentifiers() const;

      QList<QString> adaptedFactoryComponentInfoIdentifiers() const;

      QList<QString> workflowComponentInfoIdentifiers() const;


      bool unloadModelComponentInfo(const QString &componentInfoId, QString &message);

      bool unloadAdaptedOutputFactoryComponentInfo(const QString& componentInfoId, QString &message);

      bool unloadWorkflowComponentInfo(const QString& componentInfoId, QString &message);


      HydroCouple::IModelComponent *createModelComponentInstance(const QString& componentInfoId, QString &message);

      QList<HydroCouple::IIdentity*> getAvailableAdaptedOutputs(const QString &adaptedOutputFactoryComponentInfoId, HydroCouple::IOutput *provider, HydroCouple::IInput *consumer);

      HydroCouple::IAdaptedOutput *createAdaptedOutputInstance(const QString &adaptedOutputFactoryComponentInfoId, const QString &identity, HydroCouple::IOutput *provider, HydroCouple::IInput *consumer);

      HydroCouple::IWorkflowComponent *getWorkflowComponent(const QString &componentInfoId);


      int numModelComponentInstances(const QString &componentInfoId);

      int numAdaptedOutputInstances(const QString &componentInfoId);


      void removeModelInstance(const QString &componentInfoId, HydroCouple::IModelComponent *modelComponent);

      void removeAdaptedOutputInstance(const QString &componentInfoId, HydroCouple::IAdaptedOutput *adaptedOutput);

   signals:

      void modelComponentInfoLoaded(const QString &componentInfoId);

      void modelComponentInfoUnloaded(const QString &componentInfoId);


      void adaptedOutputFactoryComponentInfoLoaded(const QString &componentInfoId);

      void adaptedOutputFactoryComponentInfoUnloaded(const QString &componentInfoId);


      void workflowComponentInfoLoaded(const QString &componentInfoId);

      void workflowComponentInfoUnloaded(const QString &componentInfoId);


      void postMessage(const QString& message);

      void setProgress(bool visible, int progressvalue , int min = 0, int max = 100);

   private:

      HydroCouple::IModelComponentInfo *findModelComponentInfo(const QString &componentInfoId);

      HydroCouple::IAdaptedOutputFactoryComponentInfo *findAdaptedOutputFactoryComponentInfo(const QString &componentInfoId);

      HydroCouple::IAdaptedOutputFactoryComponent *getAdaptedOutputFactoryComponent(const QString &componentInfoId);

      HydroCouple::IWorkflowComponentInfo *findWorkflowComponentInfo(const QString &componentId);

      bool hasValidExtension(const QFileInfo& file);

  private:

      QSet<QString> m_componentFileExtensions;
      QList<QDir> m_componentDirectories;

      QHash<HydroCouple::IModelComponentInfo*, QList<HydroCouple::IModelComponent*>> m_modelComponentInfoHash;
      QHash<HydroCouple::IModelComponentInfo*, QPluginLoader*> m_modelComponentInfoPluginLoader;

      QHash<HydroCouple::IAdaptedOutputFactoryComponentInfo*, HydroCouple::IAdaptedOutputFactoryComponent*> m_adaptedOutputFactoryComponentInfoHash;
      QHash<HydroCouple::IAdaptedOutputFactoryComponentInfo*, QList<HydroCouple::IAdaptedOutput*>> m_adaptedFactoryComponentOutputs;
      QHash<HydroCouple::IAdaptedOutputFactoryComponentInfo*, QPluginLoader*> m_adaptedOutputFactoryComponentInfoPluginLoader;

      QHash<HydroCouple::IWorkflowComponentInfo*, HydroCouple::IWorkflowComponent*> m_workflowComponentInfoHash;
      QHash<HydroCouple::IWorkflowComponentInfo*, QPluginLoader*> m_workflowComponentInfoPluginLoader;

      HydroCoupleProject *m_project;

};

#endif // COMPONENTMANAGER_H
