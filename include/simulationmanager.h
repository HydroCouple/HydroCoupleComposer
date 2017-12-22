#ifndef SIMULATIONMANAGER
#define SIMULATIONMANAGER

#include "hydrocoupleproject.h"
#include "hydrocouple.h"
#include <QtConcurrentRun>
#include <QElapsedTimer>

class GOutput;
class GModelComponent;


class SimulationManager : public QObject
{
      Q_OBJECT
      Q_PROPERTY(bool Busy READ isBusy NOTIFY propertyChanged)
      Q_PROPERTY(bool MonitorComponents READ monitorComponentMessages WRITE setMonitorComponentMessages NOTIFY propertyChanged)
      Q_PROPERTY(bool MonitorExchangeItems READ monitorExchangeItemMessages WRITE setMonitorExchangeItemMessages NOTIFY propertyChanged)

   public:

      SimulationManager(HydroCoupleProject* project);

      ~SimulationManager();

      /*!
       *\brief
       *\details
       *\todo Check to make sure project file exists and is saved before proceeding
       */
      Q_INVOKABLE void runComposition(bool background = false);

      bool isBusy() const;

      bool monitorComponentMessages() const;

      void setMonitorComponentMessages(bool monitor);

      bool monitorExchangeItemMessages() const;

      void setMonitorExchangeItemMessages(bool monitor);

  public slots:

      void onStopSimulation();

      void run();

      void onSimulationCompleted();

      void onWorkflowComponentStatusChanged(HydroCouple::IWorkflowComponent::WorkflowStatus status, const QString &message);

   signals:

      void setProgress(bool visible, int progress, int min, int max);

      void postMessage(const QString &message);

      void postMessageToStatusBar(const QString &message);

      void propertyChanged(const QString &propertyName);

      void simulationCompleted();

      void isBusy(bool isBusy);

   private:

      void assignComponentIndexes();

      void assignComputeResources();

      bool createComputeResources();

      bool initializeComputeResources();

      bool initializeModels();

      void disestablishConnections();

      void establishConnections();

      bool validateModels();

      bool prepareModels();

      bool validateConnections();

   private slots:

      void finalizeComputeResources();

      void onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> & status);

      void onExchangeItemStatusChanged(const QSharedPointer<HydroCouple::IExchangeItemChangeEventArgs> & status);

   private:
      HydroCoupleProject *m_project;
      bool m_monitorComponentMessages;
      bool m_monitorExchangeItemMessages;
      bool m_isBusy;
      bool m_stopSimulation;
      bool m_simulationFailed;
      QElapsedTimer m_timer;
      GModelComponent *m_triggerComponent;
      QThread m_runThread;

};

#endif // SIMULATIONMANAGER

