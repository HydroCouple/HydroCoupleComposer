/*!
 * \file   test_compositiondocument.cpp
 * \brief  Phase A4 verification — the composition document, its undo stack,
 *         and its sidecar.
 *
 * The round-trip fixture is the SDK's own `serial_coupling` example rather
 * than a document written here, so the test cannot quietly agree with a
 * mistaken idea of the format.
 */

#include "core/composerapplication.h"
#include "project/compositiondocument.h"
#include "project/presentation.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUndoStack>

using namespace HydroCouple::Composer;
using CompositionSpec = HydroCouple::SDK::IO::CompositionSpec;
using ComponentSpec = HydroCouple::SDK::IO::ComponentSpec;
using ConnectionSpec = HydroCouple::SDK::IO::ConnectionSpec;

namespace
{
  QString sdkExampleComposition()
  {
    return QStringLiteral(COMPOSER_SDK_EXAMPLE_COMPOSITION);
  }

  //! A two-component composition with one connection.
  QByteArray minimalComposition()
  {
    return R"({
  "components": [
    { "id": "upstream", "arguments": {} },
    { "id": "downstream", "arguments": {} }
  ],
  "connections": [
    { "from": { "component": "upstream", "output": "flow" },
      "to": { "component": "downstream", "input": "inflow" } }
  ]
})";
  }

  ComponentSpec makeComponent(const std::string &id)
  {
    ComponentSpec component;
    component.id = id;
    return component;
  }

  ConnectionSpec makeConnection(const std::string &from, const std::string &out,
                                const std::string &to, const std::string &in)
  {
    ConnectionSpec connection;
    connection.fromComponent = from;
    connection.output = out;
    connection.toComponent = to;
    connection.input = in;
    return connection;
  }

  class DocumentTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_compositiondocument";
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

  ComposerApplication *DocumentTest::s_app = nullptr;
}

// ── Round-trip ────────────────────────────────────────────────────────────

TEST_F(DocumentTest, LoadsSdkExampleCompositionAndReSavesByteStably)
{
  if (sdkExampleComposition().isEmpty())
  {
    GTEST_SKIP() << "SDK example composition not available";
  }

  QTemporaryDir workspace;
  ASSERT_TRUE(workspace.isValid());

  CompositionDocument document;
  QString message;

  ASSERT_TRUE(document.load(sdkExampleComposition(), message))
    << message.toStdString();

  EXPECT_EQ(document.componentIds().size(), 2);
  EXPECT_EQ(document.connectionCount(), 1);

  // Byte-stability alone would still hold if the round-trip silently dropped
  // argument payloads — both saves would drop them identically. Assert the
  // content actually survived.
  const std::optional<ComponentSpec> catchment =
    document.component(QStringLiteral("catchment"));
  ASSERT_TRUE(catchment.has_value());
  ASSERT_TRUE(catchment->arguments.contains("coefficients"))
    << "argument payload lost on load: " << catchment->arguments.dump();
  EXPECT_EQ(catchment->arguments["coefficients"]["values"].size(), 3u);

  const QString first = QDir(workspace.path()).filePath("first.json");
  ASSERT_TRUE(document.save(first, message)) << message.toStdString();

  // Reload what we wrote and write it again: the second file must equal the
  // first, or every save would churn the user's diff.
  CompositionDocument reloaded;
  ASSERT_TRUE(reloaded.load(first, message)) << message.toStdString();

  const QString second = QDir(workspace.path()).filePath("second.json");
  ASSERT_TRUE(reloaded.save(second, message)) << message.toStdString();

  QFile firstFile(first);
  QFile secondFile(second);
  ASSERT_TRUE(firstFile.open(QIODevice::ReadOnly));
  ASSERT_TRUE(secondFile.open(QIODevice::ReadOnly));

  const QByteArray firstText = firstFile.readAll();
  EXPECT_EQ(firstText, secondFile.readAll());

  // ...and what was written is a real document, not an empty shell.
  EXPECT_TRUE(firstText.contains("coefficients"));
  EXPECT_TRUE(firstText.contains("lateral_inflow"));
}

TEST_F(DocumentTest, SurvivesMutationRoundTrip)
{
  QTemporaryDir workspace;
  ASSERT_TRUE(workspace.isValid());

  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message))
    << message.toStdString();

  ASSERT_TRUE(document.addComponent(makeComponent("gauge"), {{10.0, 20.0}}));
  ASSERT_TRUE(document.addConnection(
    makeConnection("downstream", "stage", "gauge", "level")));

  const QString path = QDir(workspace.path()).filePath("mutated.json");
  ASSERT_TRUE(document.save(path, message)) << message.toStdString();

  CompositionDocument reloaded;
  ASSERT_TRUE(reloaded.load(path, message)) << message.toStdString();

  EXPECT_EQ(reloaded.componentIds().size(), 3);
  EXPECT_EQ(reloaded.connectionCount(), 2);
  EXPECT_EQ(document.toJson(), reloaded.toJson());

  // The sidecar travelled alongside, and kept the placement.
  EXPECT_TRUE(QFile::exists(Presentation::sidecarPathFor(path)));
  EXPECT_TRUE(reloaded.presentation().hasComponent(QStringLiteral("gauge")));
  EXPECT_EQ(reloaded.presentation().component(QStringLiteral("gauge")).position,
            QPointF(10.0, 20.0));
}

// ── Validation ────────────────────────────────────────────────────────────

TEST_F(DocumentTest, RejectsInvalidJsonWithActionableMessage)
{
  CompositionDocument document;
  QString message;

  EXPECT_FALSE(document.loadFromJson("{ not json", message));
  EXPECT_FALSE(message.isEmpty());
  EXPECT_TRUE(message.contains(QStringLiteral("JSON")));
}

TEST_F(DocumentTest, RejectsDocumentWithDanglingConnectionEndpoint)
{
  CompositionDocument document;
  QString message;

  const QByteArray dangling = R"({
  "components": [ { "id": "only", "arguments": {} } ],
  "connections": [
    { "from": { "component": "only", "output": "flow" },
      "to": { "component": "ghost", "input": "inflow" } }
  ]
})";

  EXPECT_FALSE(document.loadFromJson(dangling, message));
  EXPECT_FALSE(message.isEmpty());
}

// A failed load must not disturb the document already open.
TEST_F(DocumentTest, FailedLoadLeavesExistingDocumentIntact)
{
  CompositionDocument document;
  QString message;

  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message));
  const QByteArray before = document.toJson();

  EXPECT_FALSE(document.loadFromJson("{ \"components\": 42 }", message));

  EXPECT_EQ(document.toJson(), before) << "a rejected load was partially applied";
  EXPECT_EQ(document.componentIds().size(), 2);
}

// ── Edits and undo ────────────────────────────────────────────────────────

TEST_F(DocumentTest, RejectsDuplicateAndDanglingEdits)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message));

  EXPECT_FALSE(document.addComponent(makeComponent("upstream")))
    << "duplicate id";
  EXPECT_FALSE(document.addComponent(makeComponent(""))) << "empty id";
  EXPECT_FALSE(document.addConnection(
    makeConnection("upstream", "flow", "ghost", "inflow")))
    << "unknown endpoint";
  EXPECT_FALSE(document.addConnection(
    makeConnection("upstream", "flow", "downstream", "inflow")))
    << "duplicate connection";

  // None of those may have consumed an undo slot.
  EXPECT_EQ(document.undoStack()->count(), 0);
}

TEST_F(DocumentTest, UndoRestoresExactDocumentText)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message));

  const QByteArray original = document.toJson();

  ASSERT_TRUE(document.addComponent(makeComponent("gauge")));
  ASSERT_TRUE(document.addConnection(
    makeConnection("upstream", "flow", "gauge", "level")));
  ASSERT_TRUE(document.setArgument(QStringLiteral("gauge"),
                                   QStringLiteral("scale"),
                                   nlohmann::json{{"values", {1.0, 2.0}}}));

  EXPECT_NE(document.toJson(), original);

  document.undoStack()->undo();
  document.undoStack()->undo();
  document.undoStack()->undo();

  EXPECT_EQ(document.toJson(), original);

  document.undoStack()->redo();
  document.undoStack()->redo();
  document.undoStack()->redo();

  EXPECT_EQ(document.componentIds().size(), 3);
  EXPECT_EQ(document.connectionCount(), 2);
}

// Removing a component must take its connections with it — and undo must
// bring them back, which is exactly what fine-grained inverses get wrong.
TEST_F(DocumentTest, RemovingComponentRemovesItsConnectionsAndUndoRestoresThem)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message));

  const QByteArray original = document.toJson();
  ASSERT_EQ(document.connectionCount(), 1);

  ASSERT_TRUE(document.removeComponent(QStringLiteral("downstream")));

  EXPECT_EQ(document.componentIds().size(), 1);
  EXPECT_EQ(document.connectionCount(), 0) << "the dangling connection remained";

  document.undoStack()->undo();

  EXPECT_EQ(document.toJson(), original);
  EXPECT_EQ(document.connectionCount(), 1);
}

TEST_F(DocumentTest, TracksModifiedState)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message));

  EXPECT_FALSE(document.isModified());

  ASSERT_TRUE(document.addComponent(makeComponent("gauge")));
  EXPECT_TRUE(document.isModified());

  document.undoStack()->undo();
  EXPECT_FALSE(document.isModified()) << "undoing back to the saved state";

  QTemporaryDir workspace;
  ASSERT_TRUE(workspace.isValid());
  ASSERT_TRUE(document.addComponent(makeComponent("gauge")));
  ASSERT_TRUE(
    document.save(QDir(workspace.path()).filePath("saved.json"), message));

  EXPECT_FALSE(document.isModified()) << "saving clears the dirty flag";
}

// A drag is one undo step, not one per mouse-move.
TEST_F(DocumentTest, ConsecutiveMovesMergeIntoOneUndoStep)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message));

  ASSERT_TRUE(document.moveComponent(QStringLiteral("upstream"), {1.0, 1.0}));
  ASSERT_TRUE(document.moveComponent(QStringLiteral("upstream"), {2.0, 2.0}));
  ASSERT_TRUE(document.moveComponent(QStringLiteral("upstream"), {3.0, 3.0}));

  EXPECT_EQ(document.undoStack()->count(), 1);
  EXPECT_EQ(document.presentation().component(QStringLiteral("upstream")).position,
            QPointF(3.0, 3.0));

  document.undoStack()->undo();
  EXPECT_EQ(document.presentation().component(QStringLiteral("upstream")).position,
            QPointF(0.0, 0.0));
}

// Views observe the document rather than holding copies (plan D8).
TEST_F(DocumentTest, EmitsChangeSignalsForViews)
{
  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(minimalComposition(), message));

  int compositionChanges = 0;
  int componentChanges = 0;

  QObject::connect(&document, &CompositionDocument::compositionChanged,
                   &document, [&] { ++compositionChanges; });
  QObject::connect(&document, &CompositionDocument::componentsChanged,
                   &document, [&] { ++componentChanges; });

  ASSERT_TRUE(document.addComponent(makeComponent("gauge")));

  EXPECT_GE(compositionChanges, 1);
  EXPECT_GE(componentChanges, 1);
}

TEST_F(DocumentTest, SidecarPathSitsBesideTheDocument)
{
  EXPECT_TRUE(Presentation::sidecarPathFor(QStringLiteral("/tmp/flow.json"))
                .endsWith(QStringLiteral("/flow.composer.json")));
  EXPECT_TRUE(Presentation::sidecarPathFor(QStringLiteral("/tmp/flow.yaml"))
                .endsWith(QStringLiteral("/flow.composer.json")));
}

// ── @from bindings and component removal ──────────────────────────────────

TEST(CompositionDocumentBindings, RemovingAProviderTakesItsBindingsAlongUndoably)
{
  using HydroCouple::Composer::CompositionDocument;

  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(R"({
    "schema_version": "1.1",
    "components": [
      { "id": "meshgen" },
      { "id": "gage" },
      { "id": "solver",
        "arguments": {
          "mesh": { "@from": { "component": "meshgen", "output": "mesh" } },
          "met": { "@from": { "component": "gage", "output": "flow" } },
          "dt": { "values": [60.0] } } }
    ]
  })", message))
    << message.toStdString();

  ASSERT_TRUE(document.removeComponent(QStringLiteral("meshgen")));

  // The binding went with its provider — a dangling @from would make the
  // saved document unparseable — while the unrelated argument stayed.
  {
    const auto solver = document.component(QStringLiteral("solver"));
    ASSERT_TRUE(solver.has_value());
    EXPECT_FALSE(solver->arguments.contains("mesh"));
    EXPECT_TRUE(solver->arguments.contains("dt"));
    // Only the REMOVED provider's bindings go; the gage's stays.
    EXPECT_TRUE(solver->arguments.contains("met"));
  }

  // And it comes back together on undo.
  document.undoStack()->undo();
  {
    const auto solver = document.component(QStringLiteral("solver"));
    ASSERT_TRUE(solver.has_value());
    ASSERT_TRUE(solver->arguments.contains("mesh"));
    EXPECT_TRUE(solver->arguments["mesh"].contains("@from"));
  }
}

// ── Workflow + execution blocks (the write side S4.3's panel drives) ──────

TEST(CompositionDocumentExecution, TheWorkflowBlockIsEditableAndUndoable)
{
  using HydroCouple::Composer::CompositionDocument;
  namespace IO = HydroCouple::SDK::IO;

  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(
    R"({"components": [{"id": "a"}]})", message))
    << message.toStdString();

  IO::WorkflowSpec workflow;
  workflow.strategy = IO::WorkflowStrategy::TimeStepped;
  workflow.iterationsPerGroup = 3;
  workflow.maxSteps = 50;
  ASSERT_TRUE(document.setWorkflow(workflow));

  EXPECT_EQ(document.spec().workflow.strategy,
            IO::WorkflowStrategy::TimeStepped);
  EXPECT_EQ(document.spec().workflow.iterationsPerGroup, 3);

  document.undoStack()->undo();
  EXPECT_EQ(document.spec().workflow.strategy, IO::WorkflowStrategy::None);
}

TEST(CompositionDocumentExecution, OpenModeNeedsAManifestAndRunClearsIt)
{
  using HydroCouple::Composer::CompositionDocument;
  namespace IO = HydroCouple::SDK::IO;

  CompositionDocument document;
  QString message;
  ASSERT_TRUE(document.loadFromJson(
    R"({"components": [{"id": "a"}]})", message))
    << message.toStdString();

  // Open with nothing to open would save an unloadable document.
  EXPECT_FALSE(document.setComponentExecution(
    QStringLiteral("a"), IO::ExecutionMode::Open, QString()));

  ASSERT_TRUE(document.setComponentExecution(
    QStringLiteral("a"), IO::ExecutionMode::Open,
    QStringLiteral("prior/run.json")));
  EXPECT_EQ(document.component(QStringLiteral("a"))->mode,
            IO::ExecutionMode::Open);
  EXPECT_EQ(document.component(QStringLiteral("a"))->resultsManifest,
            "prior/run.json");

  // Back to run: the stale manifest goes with the mode.
  ASSERT_TRUE(document.setComponentExecution(
    QStringLiteral("a"), IO::ExecutionMode::Run, QString()));
  EXPECT_TRUE(document.component(QStringLiteral("a"))
                ->resultsManifest.empty());
}
