#include "stdafx.h"
#include "simulationmanager.h"
#include "gmodelcomponent.h"
#include "commandlineparser.h"
#include "cpugpuallocation.h"

#include <QDebug>
//#include <QFutureWatcher>
#include <tuple>
#include <thread>
#include <cstring>

#ifdef USE_MPI
#include <mpi.h>
#endif

using namespace HydroCouple;

SimulationManager::SimulationManager(HydroCoupleProject *project)
  :QObject(project),
    m_monitorComponentMessages(false),
    m_monitorExchangeItemMessages(false),
    m_isBusy(false),
    m_stopSimulation(false)
{
  m_project = project;
  connect(this, &SimulationManager::simulationCompleted, this, &SimulationManager::onSimulationCompleted);
}

SimulationManager::~SimulationManager()
{
  //  delete m_simFutureWater;
}

void SimulationManager::runComposition(bool background)
{
  if(!m_isBusy)
  {
    m_isBusy = true;
    emit isBusy(true);

    m_timer.start();

    try
    {
      assignComponentIndexes();

      assignComputeResources();

      if(createComputeResources()  && initializeComputeResources() && initializeModels())
      {
        disestablishConnections();

        establishConnections();

        if(validateModels() && prepareModels() && validateConnections())
        {
          if(background)
          {

//            std::thread backgrounThread(&SimulationManager::run, this);
            //           run();
            QtConcurrent::run(this,&SimulationManager::run);
          }
          else
          {
            run();
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
    catch(const std::exception &exception)
    {
      emit postMessage(QString(exception.what()));
      printf("%s\n", exception.what());
      onSimulationCompleted();
    }
    catch(const std::string &exception)
    {
      emit postMessage(QString::fromStdString(exception.data()));
      printf("%s\n", exception.c_str());
      onSimulationCompleted();
    }
    catch(...)
    {
      emit postMessage("Untitled exception caught. Simulation Failed");
      printf("Untitled exception caught. Simulation Failed\n");
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

void SimulationManager::onStopSimulation()
{
  if(m_isBusy)
  {
    emit postMessage("Canceling simulation...");
    printf("Canceling simulation...\n");
    m_stopSimulation = true;
  }
}

void SimulationManager::assignComponentIndexes()
{
  for(int i = 0; i < m_project->modelComponents().length() ; i++)
  {
    IModelComponent *component = m_project->modelComponents()[i]->modelComponent();
    component->setIndex(i);
    component->mpiSetProcessRank(HydroCoupleProject::mpiProcess);
  }
}

void SimulationManager::assignComputeResources()
{
  QString info = "Assigning compute resources...";
  emit postMessage(info);
  printf("%s\n",qPrintable(info));

#ifdef USE_MPI
  QSet<int> allocatedComputeResources;

  for(GModelComponent *component : m_project->modelComponents())
  {
    component->modelComponent()->gpuClearAllocatedResources();
    component->modelComponent()->mpiClearAllocatedProcesses();

    QSet<int> componentMPIProcesses;
    componentMPIProcesses << 0;

    QHash<int, std::tuple<int,int,int>> cpugpuAllocations;

    for(CPUGPUAllocation *allocation : component->allocatedComputeResources())
    {
      if(allocation->mpiProcess() != 0)
      {
        if(allocatedComputeResources.contains(allocation->mpiProcess()))
        {
          QString message = "MPI Process:" + QString::number(allocation->mpiProcess()) + " allocated to Component: " + component->modelComponent()->id() +
                            " has already been allocated to a different component";
          emit postMessage(message);
          printf("%s\n", qPrintable(message));
        }
        else if(allocation->mpiProcess() >= HydroCoupleProject::numMPIProcesses)
        {
          QString message = "MPI Process:" + QString::number(allocation->mpiProcess()) + " allocated to Component: " + component->modelComponent()->id() +
                            " exceeds the total number of MPI processes available";
          emit postMessage(message);
          printf("%s\n", qPrintable(message));
        }
        else
        {
          allocatedComputeResources.insert(allocation->mpiProcess());
          componentMPIProcesses.insert(allocation->mpiProcess());
          cpugpuAllocations[allocation->mpiProcess()] = std::make_tuple(allocation->gpuPlatform() , allocation->gpuDevice(), allocation->gpuMaxNumBlocksOrWorkGrps());
        }
      }
      else
      {
        componentMPIProcesses << allocation->mpiProcess();
        cpugpuAllocations[allocation->mpiProcess()] = std::make_tuple(allocation->gpuPlatform() , allocation->gpuDevice(), allocation->gpuMaxNumBlocksOrWorkGrps());
      }
    }

    component->modelComponent()->mpiAllocateProcesses(componentMPIProcesses);

    QHash<int, std::tuple<int,int,int> >::iterator it;

    for(it = cpugpuAllocations.begin(); it !=  cpugpuAllocations.end(); it++)
    {
      int mpiProcess = it.key();
      std::tuple<int, int, int> gpuAlloc = it.value();
      component->modelComponent()->gpuAllocatedResources(mpiProcess, std::get<0>(gpuAlloc), std::get<1>(gpuAlloc), std::get<2>(gpuAlloc));
    }
  }
#endif

  info = "Finished assigning compute resources...";
  emit postMessage(info);
  printf("%s\n",qPrintable(info));

}

bool SimulationManager::createComputeResources()
{

  QString info = "Creating Compute Resources...";
  emit postMessage(info);
  printf("%s\n",qPrintable(info));

#ifdef USE_MPI

  for(GModelComponent *component : m_project->modelComponents())
  {
    QSet<int> allocatedMPINodes = component->modelComponent()->mpiAllocatedProcesses();

    for(int process : allocatedMPINodes)
    {
      if(process != 0)
      {
        std::string message = m_project->projectFile().absoluteFilePath().toStdString() + "," + std::to_string(component->modelComponent()->index());

        char *initializeArgs = new char[message.size() + 1] ;
        std::strcpy (initializeArgs, message.c_str());

        MPI_Send(&initializeArgs[0],message.size() + 1,MPI_CHAR, process, CommandLineParser::CreateComponent, MPI_COMM_WORLD);

        delete[] initializeArgs;

      }
    }
  }

#endif

  info = "Finished Creating Compute Resources...";
  emit postMessage(info);
  printf("%s\n",qPrintable(info));

  return true;
}

bool SimulationManager::initializeComputeResources()
{
  QString info = "Initializing Compute Resources...";
  emit postMessage(info);
  printf("%s\n",qPrintable(info));

#ifdef USE_MPI

  for(GModelComponent *component : m_project->modelComponents())
  {
    component->modelComponent()->mpiSetProcessRank(0);
    QSet<int> allocatedMPINodes = component->modelComponent()->mpiAllocatedProcesses();

    for(int process : allocatedMPINodes)
    {
      if(process != 0)
      {
        int componentIndex = component->modelComponent()->index();
        MPI_Send(&componentIndex,1,MPI_INT, process, CommandLineParser::InitializeAndPrepareComponent, MPI_COMM_WORLD);
      }
    }
  }

#endif

  info = "Finished Initializing Compute Resources...";
  emit postMessage(info);
  printf("%s\n",qPrintable(info));

  return true;
}

bool SimulationManager::initializeModels()
{
  bool initialized = true;

  for(GModelComponent *component : m_project->modelComponents())
  {
//    if(component->modelComponent()->status() != IModelComponent::Initialized)
    {
      printf("start initializing\n");

      component->modelComponent()->initialize();

      printf("finished initializing\n");

      if(component->modelComponent()->status() == IModelComponent::Failed)
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
    if(component->modelComponent()->status() == IModelComponent::Initialized)
    {
      QList<QString> validationMessages = component->modelComponent()->validate();

      if(component->modelComponent()->status() == IModelComponent::Failed)
      {
        validated = false;
        QString message = "Model component with id => " + component->modelComponent()->id() + " is invalid";
        postMessage(message);
        printf("%s\n", message.toStdString().c_str());
      }

      for(const QString &message : validationMessages)
      {
        postMessage(message);
        printf("%s\n", message.toStdString().c_str());
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
    if(component->modelComponent()->status() == IModelComponent::Initialized ||
       component->modelComponent()->status() == IModelComponent::Valid)
    {
      component->modelComponent()->prepare();

      if(component->modelComponent()->status() == IModelComponent::Failed)
      {
        prepared = false;
        QString message = "Model component with id => " + component->modelComponent()->id() + " could not be prepared";
        postMessage(message);
        printf("%s\n", message.toStdString().c_str());
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

void SimulationManager::run()
{
  try
  {
    m_triggerComponent = nullptr;

    for(GModelComponent *component : m_project->modelComponents())
    {
      if(component->trigger())
      {
        m_triggerComponent = component;

        QObject *compObj = dynamic_cast<QObject*>(component->modelComponent());
        connect(compObj, SIGNAL(componentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)),
                this, SLOT(onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &)));
        break;
      }
    }

    if(m_triggerComponent)
    {
      while (m_triggerComponent->modelComponent()->status() != IModelComponent::Done &&
             m_triggerComponent->modelComponent()->status() != IModelComponent::Failed)
      {
        m_triggerComponent->modelComponent()->update();

        if(m_stopSimulation)
        {
          break;
        }
      }

      for(GModelComponent *component : m_project->modelComponents())
      {
        component->modelComponent()->finish();
      }
    }
    else
    {
      emit postMessage("Trigger component has not been set");
      printf("Trigger component has not been set\n");
    }
  }
  catch(const std::exception &exception)
  {
    emit postMessage(QString(exception.what()));
    printf("%s\n",exception.what());
  }
  catch(const std::string &exception)
  {
    emit postMessage(QString::fromStdString(exception.data()));
    printf("%s\n",exception.c_str());
  }
  catch(...)
  {
    emit postMessage("Untitled exception caught. Simulation Failed");
    printf("Untitled exception caught. Simulation Failed\n");
  }

  emit simulationCompleted();
}

void SimulationManager::onSimulationCompleted()
{
  finalizeComputeResources();

  float seconds = m_timer.elapsed() * 1.0 / 1000.0;
  m_isBusy = false;

  if(m_stopSimulation)
  {
    emit postMessage("Simulation canceled.");
    printf("Simulation canceled\n");
    m_stopSimulation = false;
  }


  emit isBusy(false);
  emit setProgress(false,0,0,100);
  emit postMessage("Simulation Time (s): " +  QString::number(seconds));
  printf("Simulation Time (s): %f\n", seconds);

}

void SimulationManager::finalizeComputeResources()
{
#ifdef USE_MPI
  if(HydroCoupleProject::numMPIProcesses > 1)
  {
    int sendIndex = 0;

    for(int i = 1 ; i < HydroCoupleProject::numMPIProcesses; i++)
    {
      MPI_Send(&sendIndex, 1, MPI_INT, i, CommandLineParser::FinishComponent, MPI_COMM_WORLD);
    }
  }
#endif
}

void SimulationManager::onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> &status)
{
  if(status->status() == IModelComponent::Finished || status->status() == IModelComponent::Created ||
     status->status() == IModelComponent::Failed   ||  status->status() == IModelComponent::Initialized)
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
  {
    emit postMessage(status->message());
    printf("%s\n",status->message().toStdString().c_str());
  }
}

void SimulationManager::onExchangeItemStatusChanged(const QSharedPointer<HydroCouple::IExchangeItemChangeEventArgs> & status)
{

}
