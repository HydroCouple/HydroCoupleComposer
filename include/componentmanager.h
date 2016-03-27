#ifndef COMPONENTMANAGER_H
#define COMPONENTMANAGER_H

#include <QObject>
#include <QDir>
#include <hydrocouple.h>
#include <QSet>

class ComponentManager : public QObject
{
      Q_OBJECT

   public:
      ComponentManager(QObject *parent);

      ~ComponentManager();

      //!Loaded model components
      QList<HydroCouple::IModelComponentInfo*> modelComponentInfoList() const;

      HydroCouple::IModelComponentInfo* findModelComponentInfoById(const QString& id);

      bool removeModelComponentInfoById(const QString& id);

      //!Loaded model components
      QList<HydroCouple::IAdaptedOutputFactoryComponentInfo*> adaptedOutputFactoryComponentInfoList() const;

      HydroCouple::IAdaptedOutputFactoryComponentInfo* findAdaptedOutputFactoryComponentInfoById(const QString& id);

      bool removeAdaptedOutputFactoryComponentInfoById(const QString& id);

      bool addComponent(const QFileInfo& file);

      QSet<QString> componentFileExtensions() const;

      void addComponentFileExtensions(const QString& extension);

      QList<QDir> componentDirectories() const;

      void addComponentDirectory(const QDir& directory);

   signals:
      void statusChangedMessage(const QString& message);

      void statusChangedMessage(const QString& message, int timeout);

      //! Emitted when a component is loaded successfully
      void modelComponentInfoLoaded(const HydroCouple::IModelComponentInfo* modelComponentInfo);

      void modelComponentInfoRemoved(const HydroCouple::IModelComponentInfo* modelComponentInfo);

      /*!
       * \brief Emitted when a component is loaded successfully
       */
      void adaptedOutputFactoryComponentInfoLoaded(const HydroCouple::IAdaptedOutputFactoryComponentInfo*  adaptedOutputComponentInfo);

      /*!
       * \brief adaptedOutputFactoryComponentInfoLoaded
       * \param adaptedOutputComponentInfo
       */
      void adaptedOutputFactoryComponentRemoved(const HydroCouple::IAdaptedOutputFactoryComponentInfo*  adaptedOutputComponentInfo);

   private:
      bool hasValidExtension(const QFileInfo& file);

   private:
      QList<HydroCouple::IModelComponentInfo*> m_modelComponentInfoList;
      QList<HydroCouple::IAdaptedOutputFactoryComponentInfo*> m_adaptedOutputFactoryComponentInfoList;
      QSet<QString> m_componentFileExtensions;
      QList<QDir> m_componentDirectories;
};

#endif // COMPONENTMANAGER_H
