/*!
 * \file   test_qtbridge.cpp
 * \brief  Phase A3 verification — ISignal callbacks reach the GUI thread.
 *
 * The component here is driven from a genuine worker thread, because the
 * property under test is thread affinity: a test that emitted on the GUI
 * thread would pass no matter how the bridge were written.
 */

#include "core/composerapplication.h"
#include "qtbridge/componentstatusobserver.h"

#include "hydrocouplesdk/component/abstractmodelcomponent.h"
#include "hydrocouplesdk/core/dimension.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <atomic>
#include <string>
#include <vector>

using namespace HydroCouple;
using namespace HydroCouple::Composer;

namespace
{
  //! A component whose only job is to emit a controllable run of statuses.
  class TickingComponent : public HydroCouple::SDK::AbstractModelComponent
  {
    public:
      TickingComponent()
        : AbstractModelComponent("bridge.test", "Bridge Test Component")
      {
      }

      std::vector<std::string> validate() override
      {
        setStatus(ComponentStatus::Validating);
        setStatus(ComponentStatus::Valid);
        return {};
      }

      void prepare() override
      {
        setStatus(ComponentStatus::Preparing);
        setStatus(ComponentStatus::Updated, "prepared");
      }

      void update(const std::vector<IOutput *> & = {}) override
      {
        setStatus(ComponentStatus::Updating);
      }

      void finish() override
      {
        setStatus(ComponentStatus::Finishing);
        setStatus(ComponentStatus::Finished);
      }

      //! Emits \a count numbered status changes, as a running model would.
      void emitTicks(int count)
      {
        for (int tick = 0; tick < count; ++tick)
        {
          setStatus(ComponentStatus::Updated,
                    "tick " + std::to_string(tick));
        }
      }

    protected:
      void createArguments() override {}

      bool initializeArguments(std::string &message) override
      {
        message.clear();
        return true;
      }

      void createInputs() override {}
      void createOutputs() override {}
      void initializeFailureCleanUp() override {}
  };

  //! Spins the event loop until \a predicate holds or the budget expires.
  bool pumpUntil(const std::function<bool()> &predicate, int timeoutMs = 5000)
  {
    QElapsedTimer timer;
    timer.start();

    while (!predicate() && timer.elapsed() < timeoutMs)
    {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    return predicate();
  }

  class BridgeTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_qtbridge";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *BridgeTest::s_app = nullptr;
}

// Events emitted on a worker thread must arrive on the observer's thread.
TEST_F(BridgeTest, DeliversWorkerThreadEventsOnTheGuiThread)
{
  TickingComponent component;
  ComponentStatusObserver observer(&component);

  const QThread *guiThread = QThread::currentThread();

  std::vector<QThread *> deliveryThreads;
  std::vector<QString> messages;

  // DirectConnection deliberately: it runs the handler on whichever thread
  // emitted statusChanged, so this observes the bridge's marshaling. With the
  // default AutoConnection Qt would re-queue onto the GUI thread by itself and
  // the assertion below would hold no matter how QueuedSlot were written.
  QObject::connect(&observer, &ComponentStatusObserver::statusChanged,
                   &observer,
                   [&](const ComponentStatusUpdate &update)
                   {
                     deliveryThreads.push_back(QThread::currentThread());
                     messages.push_back(update.message);
                   },
                   Qt::DirectConnection);

  std::atomic<QThread *> emittingThread{nullptr};

  QThread *worker = QThread::create(
    [&]()
    {
      emittingThread = QThread::currentThread();
      component.emitTicks(5);
    });

  worker->start();
  ASSERT_TRUE(worker->wait(5000));

  EXPECT_TRUE(pumpUntil([&] { return messages.size() >= 5; }));

  // The emission really did happen off the GUI thread, or this proves nothing.
  ASSERT_NE(emittingThread.load(), nullptr);
  EXPECT_NE(emittingThread.load(), guiThread);

  ASSERT_GE(deliveryThreads.size(), 5u);
  for (QThread *thread : deliveryThreads)
  {
    EXPECT_EQ(thread, guiThread);
  }

  worker->deleteLater();
  QCoreApplication::processEvents();
}

// Queued delivery must preserve emission order.
TEST_F(BridgeTest, PreservesEmissionOrder)
{
  TickingComponent component;
  ComponentStatusObserver observer(&component);

  std::vector<QString> messages;

  QObject::connect(&observer, &ComponentStatusObserver::statusChanged,
                   &observer,
                   [&](const ComponentStatusUpdate &update)
                   {
                     messages.push_back(update.message);
                   },
                   Qt::DirectConnection);

  QThread *worker = QThread::create([&] { component.emitTicks(20); });
  worker->start();
  ASSERT_TRUE(worker->wait(5000));

  EXPECT_TRUE(pumpUntil([&] { return messages.size() >= 20; }));

  ASSERT_GE(messages.size(), 20u);
  for (int tick = 0; tick < 20; ++tick)
  {
    EXPECT_EQ(messages[static_cast<size_t>(tick)],
              QStringLiteral("tick %1").arg(tick));
  }

  worker->deleteLater();
  QCoreApplication::processEvents();
}

// The snapshot must carry the values as they were at emit time.
TEST_F(BridgeTest, SnapshotCarriesStatusAndIdentity)
{
  TickingComponent component;
  ComponentStatusObserver observer(&component);

  std::vector<ComponentStatusUpdate> updates;

  QObject::connect(&observer, &ComponentStatusObserver::statusChanged,
                   &observer,
                   [&](const ComponentStatusUpdate &update)
                   {
                     updates.push_back(update);
                   },
                   Qt::DirectConnection);

  QThread *worker = QThread::create([&] { component.emitTicks(1); });
  worker->start();
  ASSERT_TRUE(worker->wait(5000));

  EXPECT_TRUE(pumpUntil([&] { return !updates.empty(); }));

  ASSERT_FALSE(updates.empty());
  EXPECT_EQ(updates.front().componentId, QStringLiteral("bridge.test"));
  EXPECT_EQ(updates.front().status,
            IModelComponent::ComponentStatus::Updated);
  EXPECT_EQ(updates.front().message, QStringLiteral("tick 0"));

  worker->deleteLater();
  QCoreApplication::processEvents();
}

// Detaching mid-flight must not deliver afterwards, and must not crash.
TEST_F(BridgeTest, DetachDuringEmissionIsSafe)
{
  TickingComponent component;
  auto observer = std::make_unique<ComponentStatusObserver>(&component);

  int delivered = 0;

  QObject::connect(observer.get(), &ComponentStatusObserver::statusChanged,
                   observer.get(),
                   [&](const ComponentStatusUpdate &) { ++delivered; },
                   Qt::DirectConnection);

  QThread *worker = QThread::create([&] { component.emitTicks(50); });
  worker->start();
  ASSERT_TRUE(worker->wait(5000));

  observer->detach();
  QCoreApplication::processEvents();

  const int afterDetach = delivered;

  // Nothing further may arrive, and the component may keep running.
  component.emitTicks(10);
  QCoreApplication::processEvents();

  EXPECT_EQ(delivered, afterDetach);

  worker->deleteLater();
  QCoreApplication::processEvents();
}

// Destroying the observer while queued events are outstanding must be safe:
// the context binding cancels them rather than calling into freed memory.
TEST_F(BridgeTest, DestroyingObserverCancelsQueuedDeliveries)
{
  TickingComponent component;

  int delivered = 0;
  {
    ComponentStatusObserver observer(&component);

    QObject::connect(&observer, &ComponentStatusObserver::statusChanged,
                     &observer,
                     [&](const ComponentStatusUpdate &) { ++delivered; },
                   Qt::DirectConnection);

    QThread *worker = QThread::create([&] { component.emitTicks(25); });
    worker->start();
    ASSERT_TRUE(worker->wait(5000));

    // Leave the queue full and let the observer die.
    worker->deleteLater();
  }

  QCoreApplication::processEvents();

  // Emitting into a signal whose only slot has expired must be harmless.
  component.emitTicks(5);
  QCoreApplication::processEvents();

  SUCCEED() << "delivered " << delivered
            << " event(s) before destruction without crashing";
}
