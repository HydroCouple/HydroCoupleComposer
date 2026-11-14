/*!
 * \file   test_executionpanel.cpp
 * \brief  ARGGEN S4.3 — the workflow/execution panel.
 *
 * Driven through the real main window: the panel binds to the whole
 * CompositionDocument (no selection routing), every edit goes back through
 * setWorkflow()/setComponentExecution() onto the undo stack, and external
 * edits reach the widgets through compositionChanged like every other view.
 * The document is pure data here — no component library is loaded, because
 * the panel must work on documents naming uninstalled components too.
 */

#include "core/composerapplication.h"
#include "simulation/executionpanel.h"
#include "ui/composermainwindow.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTest>
#include <QUndoStack>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::ExecutionMode;
using HydroCouple::SDK::IO::WorkflowSpec;
using HydroCouple::SDK::IO::WorkflowStrategy;

namespace
{
  //! Three components, one @from binding (b waits on a), a stepped workflow.
  QByteArray stagedDocument()
  {
    return R"({
      "schema_version": "1.1",
      "components": [
        { "id": "a" },
        { "id": "b",
          "arguments": {
            "x": { "@from": { "component": "a", "output": "out" } } } },
        { "id": "c" }
      ],
      "workflow": { "strategy": "time_stepped",
                    "iterations_per_group": 3,
                    "max_steps": 12 }
    })";
  }

  class ExecutionPanelTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_executionpanel";
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
        window = std::make_unique<ComposerMainWindow>();
        window->setAttribute(Qt::WA_QuitOnClose, false);

        QString message;
        ASSERT_TRUE(window->document()->loadFromJson(stagedDocument(),
                                                     message))
          << message.toStdString();

        panel = window->executionPanel();
        ASSERT_NE(panel, nullptr);
      }

      template <typename T>
      [[nodiscard]] T *widget(const QString &name) const
      {
        return panel->findChild<T *>(name);
      }

      std::unique_ptr<ComposerMainWindow> window;
      ExecutionPanel *panel = nullptr;

      static ComposerApplication *s_app;
  };

  ComposerApplication *ExecutionPanelTest::s_app = nullptr;
}

TEST_F(ExecutionPanelTest, ThePanelShowsTheDocumentsWorkflowAndFollowsExternalEdits)
{
  // Docked in the main window, beside the Arguments and Adapter docks.
  EXPECT_NE(window->findChild<QDockWidget *>(
              QStringLiteral("executionPanelDock")),
            nullptr);

  auto *strategy = widget<QComboBox>(QStringLiteral("workflowStrategy"));
  auto *iterations =
    widget<QSpinBox>(QStringLiteral("workflowIterationsPerGroup"));
  auto *maxSteps = widget<QSpinBox>(QStringLiteral("workflowMaxSteps"));
  ASSERT_NE(strategy, nullptr);
  ASSERT_NE(iterations, nullptr);
  ASSERT_NE(maxSteps, nullptr);

  EXPECT_EQ(strategy->currentData().toInt(),
            static_cast<int>(WorkflowStrategy::TimeStepped));
  EXPECT_EQ(iterations->value(), 3);
  EXPECT_TRUE(iterations->isEnabled());
  EXPECT_EQ(maxSteps->value(), 12);

  auto *modeOfA = widget<QComboBox>(QStringLiteral("execution_mode_a"));
  ASSERT_NE(modeOfA, nullptr);
  EXPECT_EQ(modeOfA->currentData().toInt(),
            static_cast<int>(ExecutionMode::Run));

  // An edit made through the document API — another view, a script —
  // reaches the widgets without this panel being told directly.
  WorkflowSpec workflow = window->document()->spec().workflow;
  workflow.strategy = WorkflowStrategy::PullDriven;
  workflow.triggerComponent = "a";
  workflow.triggerInput = "in";
  ASSERT_TRUE(window->document()->setWorkflow(workflow));

  auto *trigger =
    widget<QComboBox>(QStringLiteral("workflowTriggerComponent"));
  auto *triggerInput =
    widget<QLineEdit>(QStringLiteral("workflowTriggerInput"));
  ASSERT_NE(trigger, nullptr);
  ASSERT_NE(triggerInput, nullptr);

  EXPECT_EQ(strategy->currentData().toInt(),
            static_cast<int>(WorkflowStrategy::PullDriven));
  EXPECT_EQ(trigger->currentText(), QStringLiteral("a"));
  EXPECT_TRUE(trigger->isEnabled());
  EXPECT_EQ(triggerInput->text(), QStringLiteral("in"));
  EXPECT_FALSE(iterations->isEnabled())
    << "iterations are a stepped-mode knob";
}

TEST_F(ExecutionPanelTest, EditingTheWorkflowThroughThePanelIsUndoable)
{
  auto *strategy = widget<QComboBox>(QStringLiteral("workflowStrategy"));
  auto *iterations =
    widget<QSpinBox>(QStringLiteral("workflowIterationsPerGroup"));
  ASSERT_NE(strategy, nullptr);
  ASSERT_NE(iterations, nullptr);

  strategy->setCurrentIndex(
    strategy->findData(static_cast<int>(WorkflowStrategy::PullDriven)));

  EXPECT_EQ(window->document()->spec().workflow.strategy,
            WorkflowStrategy::PullDriven)
    << "the strategy edit never reached the document";
  EXPECT_EQ(window->document()->undoStack()->undoText(),
            QStringLiteral("Change the workflow"));

  window->document()->undoStack()->undo();
  EXPECT_EQ(window->document()->spec().workflow.strategy,
            WorkflowStrategy::TimeStepped);
  EXPECT_EQ(strategy->currentData().toInt(),
            static_cast<int>(WorkflowStrategy::TimeStepped))
    << "the panel did not follow the undo";

  // Spin fields commit on editing-finished, not per keystroke.
  iterations->setValue(5);
  QTest::keyClick(iterations, Qt::Key_Return);
  EXPECT_EQ(window->document()->spec().workflow.iterationsPerGroup, 5);
}

TEST_F(ExecutionPanelTest, OpenModeRequiresAManifestAndTheRefusalIsVisible)
{
  auto *mode = widget<QComboBox>(QStringLiteral("execution_mode_c"));
  auto *manifest =
    widget<QLineEdit>(QStringLiteral("execution_manifest_c"));
  ASSERT_NE(mode, nullptr);
  ASSERT_NE(manifest, nullptr);
  EXPECT_FALSE(manifest->isEnabled()) << "mode 'run' takes no manifest";

  // Open with nothing to open: the document refuses, and the refusal must
  // be visible where the user can fix it.
  mode->setCurrentIndex(mode->findData(static_cast<int>(ExecutionMode::Open)));

  ASSERT_TRUE(window->document()->component(QStringLiteral("c")).has_value());
  EXPECT_EQ(window->document()->component(QStringLiteral("c"))->mode,
            ExecutionMode::Run)
    << "an open block with no manifest reached the document";
  EXPECT_FALSE(manifest->styleSheet().isEmpty()) << "the refusal was invisible";
  EXPECT_TRUE(manifest->isEnabled());

  // Supplying the path completes the edit.
  manifest->setText(QStringLiteral("results/run_manifest.json"));
  QTest::keyClick(manifest, Qt::Key_Return);

  EXPECT_EQ(window->document()->component(QStringLiteral("c"))->mode,
            ExecutionMode::Open);
  EXPECT_EQ(window->document()->component(QStringLiteral("c"))->resultsManifest,
            std::string("results/run_manifest.json"));
  EXPECT_TRUE(manifest->styleSheet().isEmpty());

  // Back to run: the manifest is cleared with it.
  mode->setCurrentIndex(mode->findData(static_cast<int>(ExecutionMode::Run)));
  EXPECT_EQ(window->document()->component(QStringLiteral("c"))->mode,
            ExecutionMode::Run);
  EXPECT_TRUE(window->document()
                ->component(QStringLiteral("c"))
                ->resultsManifest.empty());
}

TEST_F(ExecutionPanelTest, TheStagePreviewOrdersProvidersBeforeConsumers)
{
  auto *stages = widget<QLabel>(QStringLiteral("executionStages"));
  ASSERT_NE(stages, nullptr);

  // b waits on a through its @from binding; c is free. Stage index is the
  // longest provider chain above the component.
  EXPECT_EQ(stages->text(), QStringLiteral("1. a, c\n2. b"));

  // A cycle cannot be loaded, but argument edits can create one; the panel
  // must then show the SDK's diagnosis instead of a bogus order.
  const nlohmann::json binding = {
    {"@from", {{"component", "b"}, {"output", "out"}}}};
  ASSERT_TRUE(window->document()->setArgument(QStringLiteral("a"),
                                              QStringLiteral("seed"),
                                              binding));

  EXPECT_TRUE(stages->text().contains(QStringLiteral("cycle")))
    << stages->text().toStdString();
}
