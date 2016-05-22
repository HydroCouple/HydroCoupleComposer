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

   public:

      SimulationManager(HydroCoupleProject* project);

      ~SimulationManager();

      void runSimulation(bool background = false);

      bool isBusy() const;

   signals:

      void setProgress(bool visible, int progress, int min, int max);

      void postMessage(const QString &message);

   private:

      bool runModel();

      bool prepareModelForSimulation(GModelComponent *modelComponent);

      bool validateConnections() const;

   private slots:

      void onSimulationCompleted();

      void onComponentStatusChanged(const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> & status);

   private:
      HydroCoupleProject *m_project;
      bool m_isBusy;
      QFuture<bool> m_simFuture;
      QFutureWatcher<bool> *m_simFutureWater;
      QElapsedTimer m_timer;
      GModelComponent *m_triggerComponent;
};

#endif // SIMULATIONMANAGER

