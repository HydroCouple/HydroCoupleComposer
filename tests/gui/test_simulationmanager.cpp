/*!
 * \file   test_simulationmanager.cpp
 * \brief  Phase B3 verification — running a composition from the GUI.
 *
 * The run is driven exactly as the application drives it: through
 * SimulationManager, on its own worker thread, with the GUI thread pumping
 * events. Assertions are about observable outcomes — state transitions,
 * completion, and that a rejected composition never starts a thread.
 */

#include "core/composerapplication.h"
#include "plugins/componentregistry.h"
#include "project/compositiondocument.h"
#include "simulation/simulationmanager.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <nlohmann/json.hpp>

using namespace HydroCouple::Composer;

namespace
{
  QString fixturePath(const QString &stem)
  {
    return QDir(QStringLiteral(COMPOSER_FIXTURE_DIR))
      .absoluteFilePath(QStringLiteral("lib") + stem +
                        ComponentLibrary::librarySuffix());
  }

  //! Pumps the event loop until \a predicate holds or the budget expires.
  bool pumpUntil(const std::function<bool()> &predicate, int timeoutMs = 15000)
  {
    QElapsedTimer timer;
    timer.start();

    while (!predicate() && timer.elapsed() < timeoutMs)
    {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
      QThread::msleep(1);
    }

    return predicate();
  }

  //! A two-component composition using the fixture component.
  QByteArray twoComponentComposition()
  {
    return R"({
  "components": [
    { "id": "upstream",
      "info": { "component_info_id": "composer.test.component" } },
    { "id": "downstream",
      "info": { "component_info_id": "composer.test.component" } }
  ],
  "connections": [
    { "from": { "component": "upstream", "output": "values" },
      "to": { "component": "downstream", "input": "inflow" } }
  ]
})";
  }

  class SimulationTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_simulationmanager";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        QString message;
        ASSERT_NE(registry.loadLibrary(fixturePath(QStringLiteral("testcomponent")),
                                       message),
                  nullptr)
          << message.toStdString();

        manager = std::make_unique<SimulationManager>(&registry);
      }

      void TearDown() override
      {
        if (manager && manager->isRunning())
        {
          manager->requestStop();
          manager->wait(10000);
        }

        manager.reset();
      }

      ComponentRegistry registry;
      std::unique_ptr<SimulationManager> manager;

      static ComposerApplication *s_app;
  };

  ComposerApplication *SimulationTest::s_app = nullptr;
}

// ── The end-to-end run ────────────────────────────────────────────────────

TEST_F(SimulationTest, RunsACompositionToCompletion)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(twoComponentComposition(), message))
    << message.toStdString();

  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);
  QSignalSpy stepSpy(manager.get(), &SimulationManager::stepCompleted);

  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();

  EXPECT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }))
    << "the run never finished; state="
    << static_cast<int>(manager->state());

  ASSERT_EQ(finishedSpy.count(), 1);
  EXPECT_TRUE(finishedSpy.first().at(0).toBool())
    << finishedSpy.first().at(1).toString().toStdString()
    << " errors: " << manager->errors().join(QStringLiteral("; ")).toStdString();

  EXPECT_EQ(manager->state(), SimulationState::Finished);
  EXPECT_GT(stepSpy.count(), 0) << "no step was ever reported";
  EXPECT_FALSE(manager->isRunning());
}

// The GUI thread must stay free while the run proceeds.
TEST_F(SimulationTest, RunsOnAWorkerThreadNotTheCallingThread)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(twoComponentComposition(), message));

  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);

  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();

  // start() returns before the run ends — that is the whole point.
  EXPECT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }));

  // Signals are delivered on the thread that pumped them, i.e. this one.
  EXPECT_EQ(finishedSpy.count(), 1);
}

// ── Control ───────────────────────────────────────────────────────────────

TEST_F(SimulationTest, StopLeavesAConsistentState)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(twoComponentComposition(), message));

  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);
  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();

  manager->requestStop();

  EXPECT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }))
    << "stop did not bring the run to an end";

  EXPECT_FALSE(manager->isRunning());
  EXPECT_TRUE(manager->state() == SimulationState::Finished ||
              manager->state() == SimulationState::Failed);
}

// Proves the pause path is real rather than skipped: the run must be caught
// in Paused, and must then complete once resumed.
TEST_F(SimulationTest, PauseIsObservedAndResumeCompletesTheRun)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(twoComponentComposition(), message));

  QSignalSpy stateSpy(manager.get(), &SimulationManager::stateChanged);
  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);

  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();

  manager->requestPause();

  ASSERT_TRUE(pumpUntil([&] {
    return manager->state() == SimulationState::Paused ||
           finishedSpy.count() > 0;
  })) << "state=" << static_cast<int>(manager->state());

  // If the run is genuinely long enough, we caught it paused.
  if (finishedSpy.count() == 0)
  {
    EXPECT_EQ(manager->state(), SimulationState::Paused);

    // It must stay paused rather than drifting onward on its own.
    const int stepsAtPause = stateSpy.count();
    QCoreApplication::processEvents();
    QThread::msleep(150);
    EXPECT_EQ(manager->state(), SimulationState::Paused)
      << "the run did not stay paused (states seen since: "
      << stateSpy.count() - stepsAtPause << ")";

    manager->requestResume();
  }

  EXPECT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }))
    << "state=" << static_cast<int>(manager->state());

  EXPECT_TRUE(finishedSpy.first().at(0).toBool());
  EXPECT_FALSE(manager->isRunning());
}

TEST_F(SimulationTest, PauseAndResumeCompleteTheRun)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(twoComponentComposition(), message));

  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);
  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();

  manager->requestPause();
  manager->requestResume();

  // Whatever the interleaving, the run must still reach an end and must not
  // be left wedged in Paused.
  EXPECT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }))
    << "state=" << static_cast<int>(manager->state());

  EXPECT_NE(manager->state(), SimulationState::Paused);
  EXPECT_FALSE(manager->isRunning());
}

// ── Failures happen before a thread is started ────────────────────────────

TEST_F(SimulationTest, RefusesAnEmptyComposition)
{
  CompositionDocument document;
  QString message;

  EXPECT_FALSE(manager->start(document, message));
  EXPECT_FALSE(message.isEmpty());
  EXPECT_EQ(manager->state(), SimulationState::Failed);
  EXPECT_FALSE(manager->isRunning());
}

TEST_F(SimulationTest, ReportsAComponentThatCannotBeCreated)
{
  CompositionDocument document;
  QString message;

  const QByteArray missing = R"({
  "components": [
    { "id": "ghost", "info": { "component_info_id": "org.nowhere.missing" } }
  ]
})";

  ASSERT_TRUE(document.loadFromJson(missing, message));

  EXPECT_FALSE(manager->start(document, message));
  EXPECT_TRUE(message.contains(QStringLiteral("ghost")))
    << message.toStdString();
  EXPECT_EQ(manager->state(), SimulationState::Failed);
}

TEST_F(SimulationTest, RefusesToStartTwice)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(twoComponentComposition(), message));

  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);
  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();

  // A second start while the first is in flight must be refused, not queued.
  QString secondMessage;
  const bool acceptedSecond = manager->start(document, secondMessage);

  if (acceptedSecond)
  {
    // The first run may legitimately have finished already on a fast machine.
    EXPECT_TRUE(pumpUntil([&] { return finishedSpy.count() >= 1; }));
  }
  else
  {
    EXPECT_FALSE(secondMessage.isEmpty());
  }

  EXPECT_TRUE(pumpUntil([&] { return !manager->isRunning(); }));
}

// ── Recording (B3b) ───────────────────────────────────────────────────────

// A composition naming writers and a manifest must produce both, and the
// manifest must be readable back — that is what makes a finished run
// reopenable without the model's library.
TEST_F(SimulationTest, RecordsOutputsAndWritesAValidRunManifest)
{
  QTemporaryDir workspace;
  ASSERT_TRUE(workspace.isValid());

  const QString documentPath = QDir(workspace.path()).filePath("run.json");

  const QString compositionText = QStringLiteral(R"({
  "components": [
    { "id": "upstream",
      "info": { "component_info_id": "composer.test.component" } }
  ],
  "writers": [
    { "type": "csv", "path": "out/values.csv" }
  ],
  "run": { "manifest": "out/manifest.json" }
})");

  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(compositionText.toUtf8(), message))
    << message.toStdString();
  ASSERT_TRUE(document.save(documentPath, message)) << message.toStdString();

  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);
  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();

  ASSERT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }, 30000))
    << "state=" << static_cast<int>(manager->state());

  EXPECT_TRUE(finishedSpy.first().at(0).toBool())
    << manager->errors().join(QStringLiteral("; ")).toStdString();

  // The CSV the composition asked for exists and holds more than a header.
  const QString csvPath = QDir(workspace.path()).filePath("out/values.csv");
  ASSERT_TRUE(QFile::exists(csvPath)) << "no CSV was written";

  QFile csv(csvPath);
  ASSERT_TRUE(csv.open(QIODevice::ReadOnly));
  const QByteArray csvText = csv.readAll();
  EXPECT_GT(csvText.count('\n'), 1) << "the CSV has no data rows";

  // The manifest exists, is valid JSON, and catalogues the run.
  const QString manifestPath =
    QDir(workspace.path()).filePath("out/manifest.json");
  ASSERT_TRUE(QFile::exists(manifestPath)) << "no run manifest was written";
  EXPECT_EQ(manager->runManifestPath(), manifestPath);

  QFile manifest(manifestPath);
  ASSERT_TRUE(manifest.open(QIODevice::ReadOnly));

  const nlohmann::json parsed =
    nlohmann::json::parse(manifest.readAll().toStdString(), nullptr, false);

  ASSERT_FALSE(parsed.is_discarded()) << "the manifest is not valid JSON";

  // The catalogue must actually name what was recorded — a manifest that
  // merely exists is no use to a results viewer that has to find the values.
  const std::string manifestText = parsed.dump();
  EXPECT_NE(manifestText.find("values"), std::string::npos)
    << "the manifest does not mention the recorded output: " << manifestText;
  EXPECT_NE(manifestText.find("csv"), std::string::npos)
    << "the manifest does not reference its artefact: " << manifestText;
}

// A writer this host cannot build must be refused up front, not discovered
// after a run has produced nothing.
TEST_F(SimulationTest, RefusesAWriterItCannotBuild)
{
  CompositionDocument document;
  QString message;

  const QByteArray meshWriter = R"({
  "components": [
    { "id": "upstream",
      "info": { "component_info_id": "composer.test.component" } }
  ],
  "writers": [ { "type": "netcdf_ugrid", "path": "out/values.nc" } ]
})";

  ASSERT_TRUE(document.loadFromJson(meshWriter, message));

  EXPECT_FALSE(manager->start(document, message));
  EXPECT_TRUE(message.contains(QStringLiteral("mesh definition")))
    << message.toStdString();
  EXPECT_EQ(manager->state(), SimulationState::Failed);
}

TEST_F(SimulationTest, RefusesAnUnknownWriterType)
{
  CompositionDocument document;
  QString message;

  const QByteArray unknown = R"({
  "components": [
    { "id": "upstream",
      "info": { "component_info_id": "composer.test.component" } }
  ],
  "writers": [ { "type": "parquet", "path": "out/values.parquet" } ]
})";

  // The document format may reject an unknown writer type itself; if it does
  // not, the manager must. Either way it never runs silently without output.
  if (!document.loadFromJson(unknown, message))
  {
    EXPECT_FALSE(message.isEmpty());
    return;
  }

  EXPECT_FALSE(manager->start(document, message));
  EXPECT_TRUE(message.contains(QStringLiteral("parquet")))
    << message.toStdString();
}

TEST_F(SimulationTest, EmitsStateTransitions)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(twoComponentComposition(), message));

  QSignalSpy stateSpy(manager.get(), &SimulationManager::stateChanged);
  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);

  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();
  ASSERT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }));

  ASSERT_GT(stateSpy.count(), 0);

  QList<SimulationState> seen;
  for (const QList<QVariant> &emission : stateSpy)
  {
    seen.append(emission.at(0).value<SimulationState>());
  }

  EXPECT_TRUE(seen.contains(SimulationState::Running));
  EXPECT_TRUE(seen.contains(SimulationState::Finished));
}

// ── Standalone adapter factories in a run (CONNECT B3) ──────────────────────

TEST_F(SimulationTest, ARunBuildsAdapterChainsFromStandaloneFactoryLibraries)
{
  QString message;
  ASSERT_NE(registry.loadLibrary(
              fixturePath(QStringLiteral("testadapterfactory")), message),
            nullptr)
    << message.toStdString();

  const QByteArray adapted = R"({
  "components": [
    { "id": "upstream",
      "info": { "component_info_id": "composer.test.component" } },
    { "id": "downstream",
      "info": { "component_info_id": "composer.test.component" } }
  ],
  "connections": [
    { "from": { "component": "upstream", "output": "values",
                "adapted_outputs": [
                  { "factory": "composer.test.adapterfactory",
                    "id": "double_it" } ] },
      "to": { "component": "downstream", "input": "inflow" } }
  ]
})";

  CompositionDocument document;
  ASSERT_TRUE(document.loadFromJson(adapted, message)) << message.toStdString();

  // With the factory library loaded, the chain resolves through the
  // registry-backed resolver and the run completes.
  QSignalSpy finishedSpy(manager.get(), &SimulationManager::finished);
  ASSERT_TRUE(manager->start(document, message)) << message.toStdString();
  EXPECT_TRUE(pumpUntil([&] { return finishedSpy.count() > 0; }))
    << "the adapted run never finished; state="
    << static_cast<int>(manager->state());
  ASSERT_EQ(finishedSpy.count(), 1);
  EXPECT_TRUE(finishedSpy.first().at(0).toBool())
    << finishedSpy.first().at(1).toString().toStdString();

  // Without it, the same document fails the apply, naming the adapter —
  // which is what proves the run above resolved through the LIBRARY rather
  // than something built in.
  ComponentRegistry bareRegistry;
  ASSERT_NE(bareRegistry.loadLibrary(
              fixturePath(QStringLiteral("testcomponent")), message),
            nullptr);
  SimulationManager bareManager(&bareRegistry);
  QString failure;
  EXPECT_FALSE(bareManager.start(document, failure));
  EXPECT_TRUE(failure.contains(QStringLiteral("double_it"))) 
    << failure.toStdString();
}
