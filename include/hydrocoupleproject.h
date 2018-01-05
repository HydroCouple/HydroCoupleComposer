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

#ifndef HYDROCOUPLEPROJECT_H
#define HYDROCOUPLEPROJECT_H

#include <QObject>
#include <QFileInfo>
#include "componentmanager.h"
#include <exception>

class GModelComponent;
class GConnection;
class GAdaptedOutput;
class GNode;
class GOutput;
class GInput;

class HydroCoupleProject : public QObject
{
      friend class GModelComponent;
      friend class GAdaptedOutput;

      Q_OBJECT
      Q_PROPERTY(QFileInfo ProjectFile READ projectFile)
      Q_PROPERTY(ComponentManager* Componentmanager READ componentManager)
      Q_PROPERTY(QList<GModelComponent*> ModelComponents READ modelComponents)

   public:

      HydroCoupleProject(QObject *parent);

      virtual ~HydroCoupleProject();

      QFileInfo projectFile() const;

      ComponentManager* componentManager() const;

      QList<GModelComponent*> modelComponents() const;

      HydroCouple::IWorkflowComponent *workflowComponent() const;

      void setWorkflowComponent(HydroCouple::IWorkflowComponent *workflowComponent);

      GModelComponent* findModelComponentById(const QString& id);

      void addModelComponent(GModelComponent *component);

      bool deleteModelComponent(GModelComponent *component);

      void deleteModelComponentsAssociatedWithComponentInfo(HydroCouple::IModelComponentInfo *modelComponentInfo);

      void deleteAdaptedOutputsAssociatedWithComponentInfo(HydroCouple::IAdaptedOutputFactoryComponentInfo *adaptedOutputFactoryComponentInfo);

      bool hasChanges() const;

      bool hasGraphics() const;

      static HydroCoupleProject* readProjectFile(const QFileInfo& fileInfo, QList<QString>& errorMessages, bool initializeComponents = true, bool hasGraphics = true);

      static GModelComponent* readModelComponent(const QFileInfo &fileInfo, QList<QString> &errorMessages, int componentIndex);

      static int mpiProcess;

      static int numMPIProcesses;

   signals:

      void componentAdded(GModelComponent *component);

      void componentDeleting(GModelComponent *component);

      void workflowComponentChanged(HydroCouple::IWorkflowComponent *workflowComponent);

      void stateModified(bool hasChanges);

      void postMessage(const QString& message);

      void setProgress(bool visible, int progressvalue , int min = 0, int max = 100);

   public slots:

      void onTriggerChanged(GModelComponent *triggerModelComponent);

      void onSaveProject();

      void onSaveProjectAs(const QFileInfo& file);

      void onSetHasChanges(bool hasChanges = true);

      void onReloadConnections();

   private:

      bool contains(GModelComponent *component) const;

   private slots:

      void onPostMessage(const QString& message);

   private:

      QList<GModelComponent*> m_modelComponents;
      QFileInfo m_projectFile;
      bool m_hasChanges, m_hasGraphics;
      ComponentManager* m_componentManager;
      HydroCouple::IWorkflowComponent *m_workflowComponent;

};

Q_DECLARE_METATYPE(HydroCoupleProject*)

#endif // HYDROCOUPLEPROJECT_H
