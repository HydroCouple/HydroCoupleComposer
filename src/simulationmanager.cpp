#include "stdafx.h"
#include "simulationmanager.h"
#include "gmodelcomponent.h"
#include <QDebug>
#include <QFutureWatcher>

using namespace HydroCouple;

SimulationManager::SimulationManager(HydroCoupleProject *project)
   :QObject(project),
     m_monitorComponentMessages(false),
     m_monitorExchangeItemMessages(false),
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

void SimulationManager::runComposition(bool background)
{
   if(!m_isBusy)
   {

      m_isBusy = true;

      m_timer.start();

      m_triggerComponent = nullptr;

      try
      {
         if(initializeModels())
         {
            disestablishConnections();

            establishConnections();

            if(validateModels())
            {
               if(prepareModels())
               {
                  if(validateConnections())
                  {
                     for(GModelComponent *component : m_project->modelComponents())
                     {
                        component->blockSignals(true);

                        if(component->trigger())
                        {
                           m_triggerComponent = component;


//                           if(m_monitorComponentMessages)
                           {
                              QObject *compP = dynamic_cast<QObject*>(component->modelComponent());

                              connect( compP,
                                       SIGNAL(componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)),
                                       this,
                                       SLOT(onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)));
                           }
                        }
                     }

                     if(validateConnections())
                     {
                        if(background)
                        {
                           m_simFuture = QtConcurrent::run(this,&SimulationManager::runModels);
                           m_simFutureWater->setFuture(m_simFuture);
                        }
                        else
                        {
                           if(!runModels())
                           {
                              postMessage("Simulation Failed");
                           }

                           onSimulationCompleted();
                        }
                     }
                     else
                     {
                        onSimulationCompleted();
                     }
                  }
                  else
                  {
                     onSimulationCompleted();
                  }
               }
               else
               {
                  onSimulationCompleted();
               }
            }
            else
            {
               onSimulationCompleted();
            }
         }
      }
      catch(const std::exception &exception)
      {
         emit postMessage(QString(exception.what()));
         onSimulationCompleted();
      }
      catch(const std::string &exception)
      {
         emit postMessage(QString::fromStdString(exception.data()));
         onSimulationCompleted();
      }
      catch(...)
      {
         emit postMessage("Untitled exception caught. Simulation Failed");
         onSimulationCompleted();
      }
   }
}

bool SimulationManager::isBusy() const
{
   return m_isBusy;
}

bool SimulationManager::monitorComponentMessages() const
{
   return m_monitorComponentMessages;
}

void SimulationManager::setMonitorComponentMessages(bool monitor)
{
   m_monitorComponentMessages = monitor;
}

bool SimulationManager::monitorExchangeItemMessages() const
{
   return m_monitorExchangeItemMessages;
}

void SimulationManager::setMonitorExchangeItemMessages(bool monitor)
{
   m_monitorExchangeItemMessages = monitor;
}

bool SimulationManager::initializeModels()
{
   bool initialized = true;

   for(GModelComponent *component : m_project->modelComponents())
   {
      if(component->modelComponent()->status() != HydroCouple::Initialized)
      {
         component->modelComponent()->initialize();

         if(component->modelComponent()->status() == HydroCouple::Failed)
         {
            initialized = false;
            emit postMessage("Model component with id => " + component->modelComponent()->id() + " failed to initialize");
         }
      }
   }

   return initialized;
}

void SimulationManager::disestablishConnections()
{
   for(GModelComponent *component : m_project->modelComponents())
   {
      for(GOutput* output : component->outputGraphicObjects().values())
      {
         output->disestablishConnections();
      }
   }

   for(GModelComponent *component : m_project->modelComponents())
   {
      for(GInput* input : component->inputGraphicObjects().values())
      {
         input->disestablishConnections();
      }
   }
}

void SimulationManager::establishConnections()
{
   for(GModelComponent *component : m_project->modelComponents())
   {
      for(GOutput* output : component->outputGraphicObjects().values())
      {
         output->reestablishConnections();
      }
   }

   for(GModelComponent *component : m_project->modelComponents())
   {
      for(GInput* input : component->inputGraphicObjects().values())
      {
         input->reestablishConnections();
      }
   }
}

bool SimulationManager::validateModels()
{
   bool validated = true;

   for(GModelComponent *component : m_project->modelComponents())
   {
      if(component->modelComponent()->status() == HydroCouple::Initialized)
      {
         QList<QString> validationMessages = component->modelComponent()->validate();

         if(component->modelComponent()->status() == HydroCouple::Failed)
         {
            validated = false;
            postMessage("Model component with id => " + component->modelComponent()->id() + " is invalid");
         }

         for(const QString &message : validationMessages)
         {
            postMessage(message);
         }
      }
   }

   return validated;
}

bool SimulationManager::prepareModels()
{
   bool prepared = true;

   for(GModelComponent *component : m_project->modelComponents())
   {
      if(component->modelComponent()->status() == HydroCouple::Initialized ||
            component->modelComponent()->status() == HydroCouple::Valid)
      {
         component->modelComponent()->prepare();

         if(component->modelComponent()->status() == HydroCouple::Failed)
         {
            prepared = false;
            postMessage("Model component with id => " + component->modelComponent()->id() + " could not be prepared");
         }
      }
   }

   return prepared;
}

bool SimulationManager::validateConnections()
{
   bool validated = true;

   //   for(GModelComponent *component : m_project->modelComponents())
   //   {
   //      if(component->modelComponent()->status() == HydroCouple::Initialized ||
   //            component->modelComponent()->status() == HydroCouple::Valid ||
   //            component->modelComponent()->status() == HydroCouple::Updated)
   //      {
   //         component->modelComponent()->prepare();

   //         if(component->modelComponent()->status() == HydroCouple::Failed)
   //         {
   //            prepared = false;
   //            postMessage("Model component with id => " + component->modelComponent()->id() + " could not be prepared");
   //         }
   //      }
   //   }

   return validated;
}

bool SimulationManager::runModels()
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
         component->modelComponent()->finish();
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

void SimulationManager::onSimulationCompleted()
{
   for(GModelComponent *component : m_project->modelComponents())
   {
      component->blockSignals(false);
   }

   m_isBusy = false;
   float seconds = m_timer.elapsed() * 1.0 / 1000.0;
   emit setProgress(false,0,0,100);
   emit postMessage("Simulation run to completion in " +  QString::number(seconds) + " seconds");
}

void SimulationManager::onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &status)
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

   if(m_monitorComponentMessages)
   emit postMessage(status->message());
}

void SimulationManager::onExchangeItemStatusChanged(const QSharedPointer<HydroCouple::IExchangeItemChangeEventArgs> & status)
{

}
