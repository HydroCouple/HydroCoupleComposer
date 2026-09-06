#include "simulation/simulationmanager.h"

#include "simulation/runrecording.h"

#include "hydrocouplesdk/component/pulldrivenworkflow.h"
#include "hydrocouplesdk/component/timesteppedworkflow.h"
#include "hydrocouplesdk/data/sdkadaptedoutputfactory.h"
#include "hydrocouplesdk/io/iothread.h"
#include "hydrocouplesdk/io/modelinitializer.h"
#include "hydrocouplesdk/io/runmanifest.h"
#include "hydrocouplesdk/io/snapshot.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QMutex>
#include <QDir>
#include <QMutexLocker>
#include <QThread>

#include <atomic>
#include <exception>

namespace HydroCouple::Composer
{

  using WorkflowStatus = HydroCouple::IWorkflowComponent::WorkflowStatus;
  using WorkflowStrategy = HydroCouple::SDK::IO::WorkflowStrategy;

  namespace
  {
    //! A guard against a composition that never reports Done.
    constexpr int kMaximumSteps = 1000000;

    QString describe(WorkflowStatus status)
    {
      return QString::fromStdString(
        HydroCouple::SDK::AbstractWorkflowComponent::statusToString(status));
    }
  } // namespace

  // ── Private ───────────────────────────────────────────────────────────────

  class SimulationManager::Private
  {
    public:
      explicit Private(ComponentRegistry *registry) : registry(registry) {}

      ComponentRegistry *registry = nullptr;

      // Instances owned by the run; never shared with the GUI's introspection
      // instances.
      std::vector<std::unique_ptr<HydroCouple::IModelComponent>> components;
      std::unique_ptr<HydroCouple::SDK::AbstractWorkflowComponent> workflow;
      std::unique_ptr<HydroCouple::SDK::IO::ModelInitializer> initializer;

      // Standalone adapter factories realised for this run; the initializer's
      // adapted outputs reference them, so they die after the initializer
      // and before the component instances (see dispose()).
      std::vector<std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent>>
        adapterFactories;

      // Recording is optional: a composition that names no writers simply
      // runs without producing artefacts.
      std::unique_ptr<HydroCouple::SDK::IO::IOThread> ioThread;
      std::unique_ptr<HydroCouple::SDK::IO::RunRecorder> recorder;
      QString manifestDestination;

      QThread *thread = nullptr;

      /*!
       * \brief Whether the user currently wants the run paused.
       *
       * The workflow's own pause is a request honoured at the next
       * synchronisation point, so a resume can arrive *before* the workflow
       * has entered Paused — consuming the resume and leaving the run wedged.
       * Tracking the user's intent separately lets the worker re-apply it
       * whenever it observes Paused, which makes resume idempotent and the
       * ordering irrelevant.
       */
      std::atomic<bool> pauseIntent{false};

      mutable QMutex mutex;
      SimulationState state = SimulationState::Idle;
      QStringList errors;
      QString manifestPath;

      void setState(SimulationManager *owner, SimulationState next)
      {
        {
          const QMutexLocker locker(&mutex);

          if (state == next)
          {
            return;
          }

          state = next;
        }

        // The worker thread reports; the GUI thread hears about it.
        QMetaObject::invokeMethod(
          owner, [owner, next] { Q_EMIT owner->stateChanged(next); },
          Qt::QueuedConnection);
      }

      void recordErrors()
      {
        if (!workflow)
        {
          return;
        }

        const QMutexLocker locker(&mutex);

        for (const HydroCouple::ErrorEntry &entry : workflow->errors(true))
        {
          errors.append(QString::fromStdString(entry.message));
        }
      }

      void dispose()
      {
        ioThread.reset();
        recorder.reset();
        workflow.reset();
        initializer.reset();       // owns the run's adapted outputs
        adapterFactories.clear();  // referenced by those adapted outputs
        components.clear();
      }
  };

  // ── SimulationManager ─────────────────────────────────────────────────────

  SimulationManager::SimulationManager(ComponentRegistry *registry,
                                       QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(registry))
  {
    qRegisterMetaType<HydroCouple::Composer::SimulationState>();
  }

  SimulationManager::~SimulationManager()
  {
    if (d->thread && d->thread->isRunning())
    {
      requestStop();
      d->thread->wait(10000);
    }

    d->dispose();
  }

  bool SimulationManager::start(const CompositionDocument &document,
                                QString &message)
  {
    if (isRunning())
    {
      message = tr("a run is already in progress");
      return false;
    }

    if (!d->registry)
    {
      message = tr("no component registry");
      return false;
    }

    d->dispose();

    {
      const QMutexLocker locker(&d->mutex);
      d->errors.clear();
      d->manifestPath.clear();
    }

    d->setState(this, SimulationState::Preparing);

    const HydroCouple::SDK::IO::CompositionSpec &spec = document.spec();

    if (spec.components.empty())
    {
      message = tr("the composition has no components");
      d->setState(this, SimulationState::Failed);
      return false;
    }

    // ── Realise this run's own components ────────────────────────────────
    std::map<std::string, HydroCouple::IModelComponent *> byId;

    for (const HydroCouple::SDK::IO::ComponentSpec &componentSpec :
         spec.components)
    {
      const QString registryId =
        componentSpec.info.componentInfoId.empty()
          ? QString::fromStdString(componentSpec.id)
          : QString::fromStdString(componentSpec.info.componentInfoId);

      QString failure;
      std::unique_ptr<HydroCouple::IModelComponent> instance =
        d->registry->createInstance(registryId, failure);

      if (!instance)
      {
        message = tr("cannot create '%1': %2")
                    .arg(QString::fromStdString(componentSpec.id), failure);
        d->setState(this, SimulationState::Failed);
        return false;
      }

      byId.emplace(componentSpec.id, instance.get());
      d->components.push_back(std::move(instance));
    }

    // ── Apply the document to them ───────────────────────────────────────
    d->initializer = std::make_unique<HydroCouple::SDK::IO::ModelInitializer>(
      [&byId](const std::string &id) -> HydroCouple::IModelComponent *
      {
        const auto it = byId.find(id);
        return it == byId.end() ? nullptr : it->second;
      });

    // Standalone adapter factories: the SDK's own plus one instance per
    // loaded factory library, so a document's adapted_outputs can name
    // them (the A4 resolver seam).
    d->adapterFactories.clear();

    for (HydroCouple::IComponentInfo *info : d->registry->entries(
           ComponentRegistry::ComponentKind::AdapterFactory))
    {
      QString failure;
      std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent> factory =
        d->registry->createAdaptedOutputFactory(
          QString::fromStdString(info->id()), failure);

      if (factory)
      {
        d->adapterFactories.push_back(std::move(factory));
      }
    }

    // The staged apply runs synchronously below, so the callback fires on
    // the GUI thread and a plain emit is thread-correct.
    d->initializer->setProgressCallback(
      [this](int stage, int stageCount, const std::string &componentId,
             const std::string &activity)
      {
        // The SDK reports the 0-based bindingStages() index; the signal
        // counts from one, as a person reads "stage 1 of 2".
        Q_EMIT initializationProgressed(
          stage + 1, stageCount, QString::fromStdString(componentId),
          QString::fromStdString(activity));
      });

    d->initializer->setAdapterFactoryResolver(
      [priv = d.get()]
      {
        std::vector<HydroCouple::IAdaptedOutputFactory *> factories;
        factories.push_back(
          HydroCouple::SDK::SdkAdaptedOutputFactory::instance());

        for (const std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent>
               &factory : priv->adapterFactories)
        {
          factories.push_back(factory.get());
        }

        return factories;
      });

    std::string initializerMessage;

    try
    {
      const nlohmann::json documentJson =
        nlohmann::json::parse(document.toJson().toStdString());

      if (!d->initializer->initialize(documentJson, initializerMessage))
      {
        message = tr("the composition could not be applied: %1")
                    .arg(QString::fromStdString(initializerMessage));
        d->setState(this, SimulationState::Failed);
        return false;
      }
    }
    catch (const std::exception &error)
    {
      message = tr("the composition could not be applied: %1")
                  .arg(QString::fromUtf8(error.what()));
      d->setState(this, SimulationState::Failed);
      return false;
    }

    // ── Choose and wire the workflow ─────────────────────────────────────
    if (spec.workflow.strategy == WorkflowStrategy::PullDriven)
    {
      auto pull = std::make_unique<HydroCouple::SDK::PullDrivenWorkflow>(
        "composer-run");
      d->workflow = std::move(pull);
    }
    else
    {
      // TimeStepped derives its schedule from the connection graph, which is
      // the right default for a composition that names no strategy.
      d->workflow = std::make_unique<HydroCouple::SDK::TimeSteppedWorkflow>(
        "composer-run");
    }

    for (const auto &component : d->components)
    {
      std::string wiringMessage;

      if (!d->workflow->addModelComponent(component.get(), nullptr,
                                          &wiringMessage))
      {
        message = tr("workflow wiring failed: %1")
                    .arg(QString::fromStdString(wiringMessage));
        d->setState(this, SimulationState::Failed);
        return false;
      }
    }

    // ── Recording, when the composition asks for it ──────────────────────
    const QString documentDirectory =
      document.filePath().isEmpty()
        ? QString()
        : QFileInfo(document.filePath()).absolutePath();

    if (!spec.writers.empty() || !spec.runManifest.empty())
    {
      d->ioThread = std::make_unique<HydroCouple::SDK::IO::IOThread>(32);
      d->recorder = std::make_unique<HydroCouple::SDK::IO::RunRecorder>();

      if (!document.filePath().isEmpty())
      {
        d->recorder->setComposition(
          std::filesystem::path(document.filePath().toStdString()));
      }

      for (const HydroCouple::SDK::IO::WriterSpec &writerSpec : spec.writers)
      {
        QString writerMessage;
        std::shared_ptr<HydroCouple::SDK::IO::IOutputWriter> writer =
          makeOutputWriter(writerSpec, documentDirectory, writerMessage);

        if (!writer)
        {
          message = tr("cannot record output: %1").arg(writerMessage);
          d->setState(this, SimulationState::Failed);
          return false;
        }

        d->ioThread->addWriter(writer);
        d->recorder->addWriter(writer);
      }

      for (const auto &component : d->components)
      {
        d->recorder->addComponent(component.get());
      }

      if (!spec.runManifest.empty())
      {
        const QString manifest = QString::fromStdString(spec.runManifest);

        d->manifestDestination =
          (documentDirectory.isEmpty() || QFileInfo(manifest).isAbsolute())
            ? manifest
            : QDir(documentDirectory).absoluteFilePath(manifest);
      }
    }

    // ── Everything that can fail early, fails here on the calling thread ──
    d->workflow->initialize();

    const std::vector<std::string> problems = d->workflow->validate();

    if (!problems.empty())
    {
      QStringList reported;

      for (const std::string &problem : problems)
      {
        reported.append(QString::fromStdString(problem));
      }

      {
        const QMutexLocker locker(&d->mutex);
        d->errors.append(reported);
      }

      message = tr("the composition is not valid: %1")
                  .arg(reported.join(QStringLiteral("; ")));
      d->setState(this, SimulationState::Failed);
      return false;
    }

    d->workflow->prepare();

    if (d->ioThread && !d->ioThread->start())
    {
      message = tr("the output writers could not be started: %1")
                  .arg(QString::fromStdString(d->ioThread->errorMessage()));
      d->setState(this, SimulationState::Failed);
      return false;
    }

    // ── Drive it on a worker thread ──────────────────────────────────────
    d->thread = QThread::create(
      [this]
      {
        d->setState(this, SimulationState::Running);

        int step = 0;
        bool failed = false;

        try
        {
          while (d->workflow->status() != WorkflowStatus::Done &&
                 d->workflow->status() != WorkflowStatus::Failed &&
                 step < kMaximumSteps)
          {
            d->workflow->update();

            const WorkflowStatus status = d->workflow->status();

            if (status == WorkflowStatus::Failed)
            {
              failed = true;
              break;
            }

            // Paused and Done are reported, not stepped past: the workflow
            // honours pause and stop between steps, so the loop simply
            // observes what it decided.
            if (status == WorkflowStatus::Paused)
            {
              d->setState(this, SimulationState::Paused);

              while (d->workflow->status() == WorkflowStatus::Paused &&
                     d->pauseIntent.load())
              {
                QThread::msleep(20);
              }

              // Re-apply the intent rather than trusting that an earlier
              // resume() was observed: it may have arrived before the
              // workflow reached Paused at all.
              if (d->workflow->status() == WorkflowStatus::Paused)
              {
                d->workflow->resume();
              }

              if (d->workflow->status() != WorkflowStatus::Done)
              {
                d->setState(this, SimulationState::Running);
              }

              continue;
            }

            ++step;

            // Record the step. Capturing every component's outputs keeps this
            // model-agnostic: Snapshot::capture takes a plain
            // IComponentDataItem, so no component-specific knowledge is
            // needed to record a run.
            if (d->ioThread)
            {
              HydroCouple::SDK::IO::Snapshot snapshot(
                static_cast<double>(step));

              std::string captureMessage;
              bool captured = false;

              for (const auto &component : d->components)
              {
                for (HydroCouple::IOutput *output : component->outputs())
                {
                  if (output && snapshot.capture(*output, captureMessage))
                  {
                    captured = true;
                  }
                }
              }

              if (captured && !d->ioThread->enqueue(std::move(snapshot)))
              {
                const QMutexLocker locker(&d->mutex);
                d->errors.append(
                  QStringLiteral("writer error (code %1): %2")
                    .arg(d->ioThread->errorCode())
                    .arg(QString::fromStdString(d->ioThread->errorMessage())));
              }
            }

            const QString statusText = describe(status);

            QMetaObject::invokeMethod(
              this,
              [this, step, statusText]
              {
                Q_EMIT stepCompleted(step);
                Q_EMIT statusChanged(statusText, QString());
              },
              Qt::QueuedConnection);
          }
        }
        catch (const std::exception &error)
        {
          failed = true;
          const QMutexLocker locker(&d->mutex);
          d->errors.append(QString::fromUtf8(error.what()));
        }

        d->recordErrors();

        if (!failed)
        {
          d->workflow->finish();
        }

        // Writer catalogs are only valid once the IO thread has finalized,
        // so the manifest must be written after finish(), never before.
        if (d->ioThread)
        {
          d->ioThread->finish();

          if (d->ioThread->errorCode() != 0)
          {
            failed = true;
            const QMutexLocker locker(&d->mutex);
            d->errors.append(
              QStringLiteral("output writing failed (code %1): %2")
                .arg(d->ioThread->errorCode())
                .arg(QString::fromStdString(d->ioThread->errorMessage())));
          }
        }

        if (d->recorder && !d->manifestDestination.isEmpty())
        {
          std::string manifestMessage;

          const bool wrote = d->recorder->finishToFile(
            std::filesystem::path(d->manifestDestination.toStdString()),
            manifestMessage,
            failed ? HydroCouple::SDK::IO::RunStatus::Failed
                   : HydroCouple::SDK::IO::RunStatus::Completed);

          if (wrote)
          {
            const QMutexLocker locker(&d->mutex);
            d->manifestPath = d->manifestDestination;
          }
          else
          {
            failed = true;
            const QMutexLocker locker(&d->mutex);
            d->errors.append(QStringLiteral("run manifest failed: %1")
                               .arg(QString::fromStdString(manifestMessage)));
          }
        }

        d->setState(this, failed ? SimulationState::Failed
                                 : SimulationState::Finished);

        const QString summary =
          failed ? tr("the run failed")
                 : tr("completed %1 step(s)").arg(step);

        QMetaObject::invokeMethod(
          this, [this, failed, summary] { Q_EMIT finished(!failed, summary); },
          Qt::QueuedConnection);
      });

    d->thread->setObjectName(QStringLiteral("composer-simulation"));

    connect(d->thread, &QThread::finished, d->thread, &QObject::deleteLater);
    connect(d->thread, &QObject::destroyed, this,
            [this] { d->thread = nullptr; });

    d->thread->start();

    return true;
  }

  void SimulationManager::requestPause()
  {
    d->pauseIntent.store(true);

    if (d->workflow)
    {
      d->workflow->requestPause();
    }
  }

  void SimulationManager::requestResume()
  {
    d->pauseIntent.store(false);

    if (d->workflow)
    {
      d->workflow->resume();
    }
  }

  void SimulationManager::requestStop()
  {
    if (!d->workflow)
    {
      return;
    }

    d->setState(this, SimulationState::Stopping);

    // A paused workflow must be released before it can observe the stop, and
    // the intent must be cleared or the worker would pause itself again.
    d->pauseIntent.store(false);
    d->workflow->resume();
    d->workflow->requestStop();
  }

  bool SimulationManager::wait(int milliseconds)
  {
    QThread *thread = d->thread;

    if (!thread)
    {
      return true;
    }

    return thread->wait(milliseconds);
  }

  SimulationState SimulationManager::state() const
  {
    const QMutexLocker locker(&d->mutex);
    return d->state;
  }

  bool SimulationManager::isRunning() const
  {
    const SimulationState current = state();

    return current == SimulationState::Preparing ||
           current == SimulationState::Running ||
           current == SimulationState::Paused ||
           current == SimulationState::Stopping;
  }

  QStringList SimulationManager::errors() const
  {
    const QMutexLocker locker(&d->mutex);
    return d->errors;
  }

  QString SimulationManager::runManifestPath() const
  {
    const QMutexLocker locker(&d->mutex);
    return d->manifestPath;
  }

} // namespace HydroCouple::Composer
