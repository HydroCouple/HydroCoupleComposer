/*!
 * \file   queuedslot.h
 * \author Caleb Buahin
 * \brief  QueuedSlot — the single crossing point between HydroCouple's
 *         ISignal callbacks and Qt's event loop.
 *
 * Components emit on whatever thread they are being driven on, which for a
 * running composition is a worker thread. Qt objects may only be touched on
 * the thread that owns them. `QueuedSlot` is the one adapter that bridges the
 * two, and nothing else in Composer implements `HydroCouple::ISlot` directly.
 *
 * Two properties make it safe:
 *
 *  - **The payload is snapshotted at emit time.** The callback receives a
 *    value the emitting thread built, never a pointer into component state.
 *    A callback that instead dereferenced the event's `component()` on the GUI
 *    thread would be racing the worker that is still driving it.
 *  - **Delivery is bound to a context QObject.** `QMetaObject::invokeMethod`
 *    with a context object cancels the queued call if that object dies first,
 *    so an observer destroyed mid-run cannot be called back into.
 *
 * The SDK's `Signal` holds slots as `std::weak_ptr` and purges expired ones on
 * emit, so dropping the last `shared_ptr` to a QueuedSlot disconnects it. It
 * also snapshots its slot list before firing, so disconnecting from inside a
 * callback is safe.
 */

#ifndef HYDROCOUPLECOMPOSER_QTBRIDGE_QUEUEDSLOT_H
#define HYDROCOUPLECOMPOSER_QTBRIDGE_QUEUEDSLOT_H

#include "hydrocouple.h"

#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <functional>
#include <memory>
#include <utility>

namespace HydroCouple::Composer
{

  /*!
   * \brief An ISlot that re-delivers each event on a QObject's thread.
   * \tparam Payload The snapshot type handed to the callback.
   * \tparam Args The signal's argument pack.
   */
  template <typename Payload, typename... Args>
  class QueuedSlot final : public HydroCouple::ISlot<Args...>
  {
    public:
      //! Builds the thread-safe snapshot, on the emitting thread.
      using Snapshotter = std::function<Payload(Args...)>;

      //! Consumes the snapshot, on the context object's thread.
      using Handler = std::function<void(const Payload &)>;

      /*!
       * \brief Creates a slot delivering to \a context's thread.
       * \param context Object whose thread receives the callback, and whose
       *        destruction cancels any still-queued delivery.
       * \param snapshotter Builds the payload on the emitting thread.
       * \param handler Consumes the payload on the context's thread.
       */
      QueuedSlot(QObject *context, Snapshotter snapshotter, Handler handler)
        : m_context(context),
          m_snapshotter(std::move(snapshotter)),
          m_handler(std::move(handler))
      {
      }

      void operator()(const HydroCouple::ISignal<Args...> &, Args... args) override
      {
        if (!m_context)
        {
          return;
        }

        // Snapshot here, on the emitting thread, while the state is still the
        // emitter's to read.
        Payload payload = m_snapshotter(args...);

        QObject *context = m_context;
        Handler handler = m_handler;

        QMetaObject::invokeMethod(
          context,
          [handler = std::move(handler), payload = std::move(payload)]()
          {
            handler(payload);
          },
          Qt::QueuedConnection);
      }

    private:
      QPointer<QObject> m_context;
      Snapshotter m_snapshotter;
      Handler m_handler;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_QTBRIDGE_QUEUEDSLOT_H
