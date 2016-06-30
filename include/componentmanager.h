#ifndef COMPONENTMANAGER_H
#define COMPONENTMANAGER_H

#include <QObject>
#include <QDir>
#include <hydrocouple.h>
#include <QSet>
#include <QPluginLoader>

class ComponentManager : public QObject
{
      Q_OBJECT

   public:
      ComponentManager(QObject *parent);

      virtual ~ComponentManager();

      QHash<QString, HydroCouple::IModelComponentInfo*> modelComponentInfoById() const;

      bool unloadModelComponentInfoById(const QString& id);

      QHash<QString,HydroCouple::IAdaptedOutputFactoryComponentInfo*> adaptedOutputFactoryComponentInfoById() const;

      QHash<QString,HydroCouple::IAdaptedOutputFactoryComponent*> adaptedOutputFactoryComponentById() const;

      QHash<HydroCouple::IAdaptedOutputFactoryComponentInfo*,HydroCouple::IAdaptedOutputFactoryComponent*> adaptedOutputFactoryComponentByComponentInfo() const;

      bool unloadAdaptedOutputFactoryComponentInfoById(const QString& id);

      HydroCouple::IComponentInfo* findComponentInfoById(const QString& id);

      HydroCouple::IComponentInfo* loadComponent(const QFileInfo& file);

      QSet<QString> componentFileExtensions() const;

      void addComponentFileExtensions(const QString& extension);

      QList<QDir> componentDirectories() const;

      void addComponentDirectory(const QDir& directory);

   signals:

      //! Emitted when a component is loaded successfully
      void modelComponentInfoLoaded(const HydroCouple::IModelComponentInfo* modelComponentInfo);

      void modelComponentInfoUnloaded(const QString& id);

      /*!
       * \brief Emitted when a component is loaded successfully
       */
      void adaptedOutputFactoryComponentInfoLoaded(const HydroCouple::IAdaptedOutputFactoryComponentInfo*  adaptedOutputComponentInfo);

      /*!
       * \brief adaptedOutputFactoryComponentInfoLoaded
       * \param adaptedOutputComponentInfo
       */
      void adaptedOutputFactoryComponentUnloaded(const QString&  id);

      void postMessage(const QString& message);

      void setProgress(bool visible, int progressvalue , int min = 0, int max = 100);

   private:
      bool hasValidExtension(const QFileInfo& file);

   private:
      QSet<QString> m_componentFileExtensions;
      QList<QDir> m_componentDirectories;

      QHash<HydroCouple::IModelComponentInfo*,QPluginLoader*> m_modelComponentInfoHash;
      QHash<QString,HydroCouple::IModelComponentInfo*> m_modelComponentInfoById;

      QHash<HydroCouple::IAdaptedOutputFactoryComponentInfo*,QPluginLoader*> m_adaptedOutputFactoryComponentInfoHash;
      QHash<QString,HydroCouple::IAdaptedOutputFactoryComponentInfo*> m_adaptedOutputFactoryComponentInfoById;
      QHash<HydroCouple::IAdaptedOutputFactoryComponentInfo*,HydroCouple::IAdaptedOutputFactoryComponent*> m_adaptedOutputFactoryComponentByComponentInfo;
      QHash<QString,HydroCouple::IAdaptedOutputFactoryComponent*> m_adaptedOutputFactoryComponentById;
};

#endif // COMPONENTMANAGER_H
