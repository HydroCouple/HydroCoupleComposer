/*!
 * \file   hydrocouple.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
 * This header file contains the core interface definitions for the
 * HydroCouple component-based modeling definitions.
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

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
