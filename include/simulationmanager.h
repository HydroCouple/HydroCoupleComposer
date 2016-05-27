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

      Q_INVOKABLE void runComposition(bool background = false);

      bool isBusy() const;

      bool monitorComponentMessages() const;

      void setMonitorComponentMessages(bool monitor);

      bool monitorExchangeItemMessages() const;

      void setMonitorExchangeItemMessages(bool monitor);

   signals:

      void setProgress(bool visible, int progress, int min, int max);

      void postMessage(const QString &message);

      void postMessageToStatusBar(const QString &message);

      void propertyChanged(const QString &propertyName);

   private:

      bool initializeModels();

      void disestablishConnections();

      void establishConnections();

      bool validateModels();

      bool prepareModels();

      bool validateConnections();

      bool runModels();

   private slots:

      void onSimulationCompleted();

      void onComponentStatusChanged(const QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs> & status);

      void onExchangeItemStatusChanged(const QSharedPointer<HydroCouple::IExchangeItemChangeEventArgs> & status);

   private:
      HydroCoupleProject *m_project;
      bool m_monitorComponentMessages;
      bool m_monitorExchangeItemMessages;
      bool m_isBusy;
      QFuture<bool> m_simFuture;
      QFutureWatcher<bool> *m_simFutureWater;
      QElapsedTimer m_timer;
      GModelComponent *m_triggerComponent;
};

#endif // SIMULATIONMANAGER

