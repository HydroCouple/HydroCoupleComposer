/*!
 * \file   test_hcpimporter.cpp
 * \brief  Phase A5 verification — one-way `.hcp` import.
 *
 * The fixtures live in tests/fixtures/legacy and are reconstructed from the v1
 * writer, since no `.hcp` files survived in the repository. That is a real
 * limitation on confidence and is recorded in the plan: the importer is proven
 * against the format as the v1 code emits it, not against field projects.
 */

#include "core/composerapplication.h"
#include "project/compositiondocument.h"
#include "project/hcpimporter.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QString>

using namespace HydroCouple::Composer;

namespace
{
  QString fixture(const QString &name)
  {
    return QDir(QStringLiteral(COMPOSER_LEGACY_FIXTURE_DIR)).filePath(name);
  }

  bool anyIssueContains(const ImportResult &result, const QString &needle)
  {
    for (const ImportIssue &issue : result.issues)
    {
      if (issue.toString().contains(needle, Qt::CaseInsensitive))
      {
        return true;
      }
    }
    return false;
  }

  class ImporterTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_hcpimporter";
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

  ComposerApplication *ImporterTest::s_app = nullptr;
}

TEST_F(ImporterTest, ImportsComponentsWithSynthesisedIds)
{
  const ImportResult result = HcpImporter::importFile(fixture("example.hcp"));

  ASSERT_TRUE(result.succeeded);
  ASSERT_EQ(result.spec.components.size(), 2u);

  // v1 had no instance ids — they are derived from the captions.
  EXPECT_EQ(result.spec.components[0].id, "upper_catchment");
  EXPECT_EQ(result.spec.components[1].id, "main_channel");

  EXPECT_EQ(result.spec.components[0].caption, "Upper Catchment");
  EXPECT_EQ(result.spec.components[0].info.componentInfoId,
            "org.hydrocouple.catchment");
  EXPECT_EQ(result.spec.components[0].info.library,
            "../components/libCatchment.dylib");
}

TEST_F(ImporterTest, ResolvesIndexBasedConnectionsToIds)
{
  const ImportResult result = HcpImporter::importFile(fixture("example.hcp"));

  ASSERT_TRUE(result.succeeded);
  ASSERT_EQ(result.spec.connections.size(), 2u);

  const auto &direct = result.spec.connections[0];
  EXPECT_EQ(direct.fromComponent, "upper_catchment");
  EXPECT_EQ(direct.output, "runoff");
  EXPECT_EQ(direct.toComponent, "main_channel");
  EXPECT_EQ(direct.input, "lateral_inflow");
  EXPECT_TRUE(direct.adaptedOutputs.empty());

  // The adapted-output chain survives as a chain, not as a plain connection.
  const auto &adapted = result.spec.connections[1];
  ASSERT_EQ(adapted.adaptedOutputs.size(), 1u);
  EXPECT_EQ(adapted.adaptedOutputs[0].id, "timeInterpolator");
  EXPECT_EQ(adapted.adaptedOutputs[0].factory, "org.hydrocouple.temporal");
  EXPECT_EQ(adapted.toComponent, "main_channel");
  EXPECT_EQ(adapted.input, "upstream_flow");
}

TEST_F(ImporterTest, PreservesCanvasPositions)
{
  const ImportResult result = HcpImporter::importFile(fixture("example.hcp"));

  ASSERT_TRUE(result.succeeded);
  EXPECT_EQ(result.presentation.component(QStringLiteral("upper_catchment")).position,
            QPointF(-120.5, 40.0));
  EXPECT_EQ(result.presentation.component(QStringLiteral("main_channel")).position,
            QPointF(180.0, 42.25));
}

// A pull-driven workflow needs a trigger *input*, which v1 never recorded.
// Selecting the strategy anyway would emit a document that fails to load, so
// the trigger is reported rather than guessed.
TEST_F(ImporterTest, ReportsTriggerWithoutInventingAWorkflowStrategy)
{
  const ImportResult result = HcpImporter::importFile(fixture("example.hcp"));

  ASSERT_TRUE(result.succeeded);
  EXPECT_EQ(result.spec.workflow.strategy,
            HydroCouple::SDK::IO::WorkflowStrategy::None);
  EXPECT_TRUE(result.spec.workflow.triggerComponent.empty());

  EXPECT_TRUE(anyIssueContains(result, QStringLiteral("trigger component")));
  EXPECT_TRUE(anyIssueContains(result, QStringLiteral("upper_catchment")));
}

// The whole point of the importer's contract: nothing vanishes quietly.
TEST_F(ImporterTest, ReportsEveryArgumentRatherThanDroppingIt)
{
  const ImportResult result = HcpImporter::importFile(fixture("example.hcp"));

  ASSERT_TRUE(result.succeeded);

  // Three arguments in the fixture: two on the catchment, one on the channel,
  // plus one on the adapted output.
  int argumentIssues = 0;
  for (const ImportIssue &issue : result.issues)
  {
    if (issue.message.contains(QStringLiteral("argument not carried across")))
    {
      ++argumentIssues;
      EXPECT_FALSE(issue.detail.isEmpty())
        << "the original value must be preserved for re-entry";
    }
  }

  EXPECT_EQ(argumentIssues, 4);

  // And the original values really are recoverable from the report.
  EXPECT_TRUE(anyIssueContains(result, QStringLiteral("0.35 0.20 0.55")));
  EXPECT_TRUE(anyIssueContains(result, QStringLiteral("inputs/catchment.inp")));

  // Arguments must not have been invented into the composition.
  EXPECT_TRUE(result.spec.components[0].arguments.empty())
    << "the importer fabricated an argument payload";
}

TEST_F(ImporterTest, ReportsConstructsWithNoV2Equivalent)
{
  const ImportResult result = HcpImporter::importFile(fixture("example.hcp"));

  EXPECT_TRUE(anyIssueContains(result, QStringLiteral("compute resource")));
}

TEST_F(ImporterTest, ReportsDanglingConnectionAsErrorPerNode)
{
  const ImportResult result = HcpImporter::importFile(fixture("dangling.hcp"));

  EXPECT_FALSE(result.succeeded);
  EXPECT_TRUE(result.hasErrors());

  const QList<ImportIssue> errors =
    result.issuesOfAtLeast(ImportIssue::Severity::Error);
  ASSERT_FALSE(errors.isEmpty());
  EXPECT_TRUE(errors.first().message.contains(QStringLiteral("component index")));
}

TEST_F(ImporterTest, RejectsMalformedAndForeignDocuments)
{
  const ImportResult malformed =
    HcpImporter::importXml("<HydroCoupleProject><oops", QString());
  EXPECT_FALSE(malformed.succeeded);
  EXPECT_TRUE(anyIssueContains(malformed, QStringLiteral("malformed XML")));

  const ImportResult foreign =
    HcpImporter::importXml("<?xml version=\"1.0\"?><something/>", QString());
  EXPECT_FALSE(foreign.succeeded);
  EXPECT_TRUE(anyIssueContains(foreign, QStringLiteral("HydroCouple 1.x")));

  const ImportResult missing = HcpImporter::importFile(fixture("nope.hcp"));
  EXPECT_FALSE(missing.succeeded);
  EXPECT_TRUE(missing.hasErrors());
}

// The imported composition must be a document the rest of the program accepts.
TEST_F(ImporterTest, ImportedCompositionIsValidCompositionSpecV1)
{
  const ImportResult result = HcpImporter::importFile(fixture("example.hcp"));
  ASSERT_TRUE(result.succeeded);

  CompositionDocument document;
  QString message;

  const QByteArray json =
    QByteArray::fromStdString(result.spec.toJson().dump(2) + "\n");

  ASSERT_TRUE(document.loadFromJson(json, message))
    << "the importer produced a document the SDK rejects: "
    << message.toStdString();

  EXPECT_EQ(document.componentIds().size(), 2);
  EXPECT_EQ(document.connectionCount(), 2);
}
