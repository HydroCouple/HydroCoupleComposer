#include "stdafx.h"
#include "simulationmanager.h"
#include "gmodelcomponent.h"
#include <QDebug>
#include <QFutureWatcher>

using namespace HydroCouple;

SimulationManager::SimulationManager(HydroCoupleProject *project)
   :QObject(project),
     m_isBusy(false)
{
   m_project = project;
   m_simFutureWater = new QFutureWatcher<bool>();
   connect(m_simFutureWater, SIGNAL(finished()), this, SLOT(onSimulationCompleted()));

}

SimulationManager::~SimulationManager()
{
   delete m_simFutureWater;
}

void SimulationManager::runSimulation(bool background)
{
   if(!m_isBusy)
   {

      m_isBusy = true;
      m_timer.start();

      m_triggerComponent = nullptr;

      for(GModelComponent *component : m_project->modelComponents())
      {
         component->setIgnoreSignalsFromComponent(true);

         if(component->trigger())
         {
            m_triggerComponent = component;

            QObject *compP = dynamic_cast<QObject*>(component->modelComponent());


            connect( compP,
                     SIGNAL(componentStatusChanged(const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> &)),
                     this,
                     SLOT(onComponentStatusChanged(const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> &)));
         }

         if(!prepareModelForSimulation(component))
         {
            postMessage("Could not initialize/validate " + component->id());
            onSimulationCompleted();
            return;
         }
      }

      if(validateConnections())
      {
         if(background)
         {
            m_simFuture = QtConcurrent::run(this,&SimulationManager::runModel);
            m_simFutureWater->setFuture(m_simFuture);
         }
         else
         {
            if(!runModel())
            {
               postMessage("Simulation Failed");
            }

            onSimulationCompleted();
         }
      }
   }
}

bool SimulationManager::isBusy() const
{
   return m_isBusy;
}

bool SimulationManager::runModel()
{
   if(m_triggerComponent)
   {
      while (m_triggerComponent->modelComponent()->status() != HydroCouple::Done &&
             m_triggerComponent->modelComponent()->status() != HydroCouple::Failed)
      {
         m_triggerComponent->modelComponent()->update();
      }

      for(GModelComponent *component : m_project->modelComponents())
      {
         component->m_inputs.clear();
         component->m_outputs.clear();
      }

      for(GModelComponent *component : m_project->modelComponents())
      {
         component->modelComponent()->finish();
         component->setIgnoreSignalsFromComponent(false);
      }
   }
   else
   {
      emit postMessage("Trigger component has not been set");
      m_isBusy = false;
      return false;
   }

   m_isBusy = false;
   return true;
}

bool SimulationManager::prepareModelForSimulation(GModelComponent *component)
{
   switch (component->modelComponent()->status())
   {
      case HydroCouple::Created:
         {
            component->modelComponent()->initialize();
         }
         break;
      case HydroCouple::Initialized:
         {
            QList<QString> validationMessages =  component->modelComponent()->validate();

            for(const QString &message : validationMessages)
            {
               postMessage(message);
            }
         }
         break;
      case HydroCouple::Valid:
         {
            component->modelComponent()->prepare();
         }
         break;
      case HydroCouple::Updated:
         {
            return true;
         }
         break;
   }

   if(component->modelComponent()->status() == HydroCouple::Failed ||
         component->modelComponent()->status() == HydroCouple::Invalid)
   {
      return false;
   }
   else
   {
      return prepareModelForSimulation(component);
   }
}

bool SimulationManager::validateConnections() const
{
   return true;
}

void SimulationManager::onSimulationCompleted()
{
   m_isBusy = false;
   float seconds = m_timer.elapsed() * 1.0 / 1000.0;
   emit setProgress(false,0,0,100);
   emit postMessage("Simulation run to completion in " +  QString::number(seconds) + " seconds");
}

void SimulationManager::onComponentStatusChanged(const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> &status)
{
   if(status->status() == HydroCouple::Finished || status->status() == HydroCouple::Created || status->status() == HydroCouple::Failed)
   {
      emit setProgress(false,0,0,100);
   }
   else if(status->hasProgressMonitor())
   {
      emit setProgress(true, status->percentProgress() , 0, 100);
   }
   else
   {
      emit setProgress(true,0,0,0);
   }

   emit postMessage(status->message());
}
