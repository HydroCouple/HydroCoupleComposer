#include "qtbridge/componentstatusobserver.h"

namespace HydroCouple::Composer
{

  namespace
  {
    using StatusEventArgs =
      const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> &;

    //! Selects the status signal from among a component's several ISignal bases.
    HydroCouple::ISignal<StatusEventArgs> *statusSignal(
      HydroCouple::IModelComponent *component)
    {
      return static_cast<HydroCouple::ISignal<StatusEventArgs> *>(component);
    }
  } // namespace

  ComponentStatusObserver::ComponentStatusObserver(
    HydroCouple::IModelComponent *component, QObject *parent)
    : QObject(parent),
      m_component(component)
  {
    if (!m_component)
    {
      return;
    }

    m_slot = std::make_shared<QueuedSlot<ComponentStatusUpdate, StatusArgs>>(
      this,
      // Snapshotter — runs on the emitting (worker) thread.
      [](StatusArgs args) -> ComponentStatusUpdate
      {
        ComponentStatusUpdate update;

        if (!args)
        {
          return update;
        }

        if (HydroCouple::IModelComponent *source = args->component())
        {
          update.componentId = QString::fromStdString(source->id());
        }

        update.message = QString::fromStdString(args->message());
        update.previousStatus = args->previousStatus();
        update.status = args->status();
        update.hasProgressMonitor = args->hasProgressMonitor();
        update.percentProgress =
          update.hasProgressMonitor ? args->percentProgress() : 0.0F;

        return update;
      },
      // Handler — runs on this object's thread.
      [this](const ComponentStatusUpdate &update)
      {
        Q_EMIT statusChanged(update);
      });

    // IModelComponent inherits two different ISignal instantiations — the
    // status signal, and ISignal<std::string> reached through
    // IIdentity -> IDescription -> IPropertyChanged — so an unqualified
    // connect() is ambiguous. Name the one being connected to.
    statusSignal(m_component)->connect(m_slot);
  }

  ComponentStatusObserver::~ComponentStatusObserver()
  {
    detach();
  }

  HydroCouple::IModelComponent *ComponentStatusObserver::component()
    const noexcept
  {
    return m_component;
  }

  void ComponentStatusObserver::detach()
  {
    if (m_component && m_slot)
    {
      statusSignal(m_component)->disconnect(m_slot);
    }

    // Releasing the slot also expires the signal's weak reference, so a
    // component that outlives this observer stops seeing it either way.
    m_slot.reset();
    m_component = nullptr;
  }

} // namespace HydroCouple::Composer
