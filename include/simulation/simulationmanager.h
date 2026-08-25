/*!
 * \file   simulationmanager.h
 * \author Caleb Buahin
 * \brief  SimulationManager — runs a composition without freezing the GUI.
 *
 * Realises a composition's components, applies the document to them through
 * the SDK's `ModelInitializer`, wires them into a workflow engine, and drives
 * that engine on a worker thread. Pause, resume and stop are the workflow's own
 * cooperative requests, so they take effect at a synchronisation point rather
 * than by killing a thread mid-step.
 *
 * \par The run owns its own component instances
 * A run never drives the instances the canvas and configurator introspect.
 * Those exist to be read on the GUI thread; a running workflow mutates its
 * components continuously, and sharing them would mean the canvas reading a
 * model mid-step. The manager therefore creates a fresh set from the registry
 * for each run and disposes of them afterwards.
 *
 * \par Threading
 * Everything the workflow reports crosses to the GUI thread as Qt signals.
 * Callers only ever touch this object from the GUI thread; the worker thread
 * is private to it.
 */

#ifndef HYDROCOUPLECOMPOSER_SIMULATION_SIMULATIONMANAGER_H
#define HYDROCOUPLECOMPOSER_SIMULATION_SIMULATIONMANAGER_H

#include "plugins/componentregistry.h"
#include "project/compositiondocument.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief What the manager is doing.
   */
  enum class SimulationState
  {
    Idle,
    Preparing,
    Running,
    Paused,
    Stopping,
    Finished,
    Failed
  };

  /*!
   * \brief Runs one composition at a time on a worker thread.
   */
  class SimulationManager : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds a manager drawing components from \a registry.
       * \param registry Source of component libraries.
       * \param parent Optional Qt parent.
       */
      explicit SimulationManager(ComponentRegistry *registry,
                                 QObject *parent = nullptr);

      ~SimulationManager() override;

      /*!
       * \brief Starts running \a document.
       *
       * Everything that can fail before the first step — instantiating
       * components, applying arguments and connections, validating the
       * composition — happens synchronously here, so a composition that cannot
       * run reports why immediately instead of failing on a worker thread.
       *
       * \param document The composition to run.
       * \param[out] message Diagnostic when the run cannot be started.
       * \returns true when the worker thread was started.
       */
      bool start(const CompositionDocument &document, QString &message);

      /*!
       * \brief Requests a cooperative pause; honoured after the current step.
       */
      void requestPause();

      /*!
       * \brief Resumes a paused run.
       */
      void requestResume();

      /*!
       * \brief Requests a cooperative stop; honoured after the current step.
       */
      void requestStop();

      /*!
       * \brief Blocks until the run finishes.
       * \param milliseconds How long to wait.
       * \returns true when the run finished within the timeout.
       */
      bool wait(int milliseconds = 30000);

      [[nodiscard]] SimulationState state() const;

      [[nodiscard]] bool isRunning() const;

      /*!
       * \brief Diagnostics drained from the workflow and its components.
       */
      [[nodiscard]] QStringList errors() const;

      /*!
       * \brief Path of the run manifest written by the last run, if any.
       */
      [[nodiscard]] QString runManifestPath() const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when the manager's state changes.
       * \param state The new state.
       */
      void stateChanged(HydroCouple::Composer::SimulationState state);

      /*!
       * \brief Emitted for each workflow status transition.
       * \param status Human-readable workflow status.
       * \param message Accompanying message; may be empty.
       */
      void statusChanged(const QString &status, const QString &message);

      /*!
       * \brief Emitted once per completed step.
       * \param step The step just completed, counting from one.
       */
      void stepCompleted(int step);

      /*!
       * \brief Emitted when the run ends, successfully or not.
       * \param succeeded Whether the run completed without failing.
       * \param message Summary or failure description.
       */
      void finished(bool succeeded, const QString &message);

    private:
      class Private;
      std::unique_ptr<Private> d;
  };

} // namespace HydroCouple::Composer

Q_DECLARE_METATYPE(HydroCouple::Composer::SimulationState)

#endif // HYDROCOUPLECOMPOSER_SIMULATION_SIMULATIONMANAGER_H
