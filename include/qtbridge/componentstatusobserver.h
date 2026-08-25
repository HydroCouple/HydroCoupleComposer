/*!
 * \file   componentstatusobserver.h
 * \author Caleb Buahin
 * \brief  ComponentStatusObserver — a component's status changes as Qt signals.
 *
 * Wraps one HydroCouple::IModelComponent so the status panel, canvas, and
 * simulation manager can observe it with ordinary Qt connections while the
 * component itself runs on a worker thread.
 */

#ifndef HYDROCOUPLECOMPOSER_QTBRIDGE_COMPONENTSTATUSOBSERVER_H
#define HYDROCOUPLECOMPOSER_QTBRIDGE_COMPONENTSTATUSOBSERVER_H

#include "qtbridge/queuedslot.h"

#include <QObject>
#include <QString>

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief A thread-safe snapshot of one status change.
   *
   * Values are copied on the emitting thread. Deliberately carries no pointer
   * to the component: by the time this reaches the GUI thread the component
   * has usually moved on, and following a pointer would race it.
   */
  struct ComponentStatusUpdate
  {
      QString componentId;
      QString message;
      HydroCouple::IModelComponent::ComponentStatus previousStatus{};
      HydroCouple::IModelComponent::ComponentStatus status{};
      bool hasProgressMonitor = false;
      float percentProgress = 0.0F;
  };

  /*!
   * \brief Publishes an IModelComponent's status changes as Qt signals.
   *
   * Connects on construction and disconnects on destruction. Destroying the
   * observer while the component is mid-run is safe: the slot's context
   * binding cancels queued deliveries, and releasing the slot disconnects it
   * from the component's signal.
   */
  class ComponentStatusObserver : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Observes \a component.
       * \param component The component to watch; must outlive this observer.
       * \param parent Optional Qt parent.
       */
      explicit ComponentStatusObserver(HydroCouple::IModelComponent *component,
                                       QObject *parent = nullptr);

      ~ComponentStatusObserver() override;

      /*!
       * \brief The component being observed.
       */
      [[nodiscard]] HydroCouple::IModelComponent *component() const noexcept;

      /*!
       * \brief Stops observing; subsequent status changes are ignored.
       *
       * Idempotent, and safe to call from a status handler.
       */
      void detach();

    Q_SIGNALS:
      /*!
       * \brief Emitted on this object's thread for each status change.
       */
      void statusChanged(const HydroCouple::Composer::ComponentStatusUpdate &update);

    private:
      using StatusArgs =
        const std::shared_ptr<HydroCouple::IComponentStatusChangeEventArgs> &;

      HydroCouple::IModelComponent *m_component = nullptr;
      std::shared_ptr<QueuedSlot<ComponentStatusUpdate, StatusArgs>> m_slot;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_QTBRIDGE_COMPONENTSTATUSOBSERVER_H
