/*!
 * \file   test_runbrowser.cpp
 * \brief  Phase D1 verification — reopening a finished run.
 *
 * The claim under test is that the browser reports what the manifest says,
 * not what a viewer inferred. So the gates compare the tree against the
 * manifest entry by entry rather than checking that rows appeared — a model
 * that invented plausible shapes and units would satisfy every count.
 *
 * The second claim is that none of this needs the model libraries that
 * produced the run. That is what the manifest exists for, and it is why the
 * fixture is a recorded run with no component binary anywhere near it.
 */

#include "core/composerapplication.h"
#include "layers/dataitemlayer.h"
#include "layers/differencelayer.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "results/runbrowsermodel.h"
#include "results/runsession.h"
#include "ui/composermainwindow.h"
#include "ui/panels/runbrowserpanel.h"

#include "hydrocouplesdk/io/runmanifest.h"
#include "hydrocoupletemporal.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSignalSpy>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <filesystem>

using namespace HydroCouple::Composer;
namespace SDK = HydroCouple::SDK;

namespace
{
  /*!
   * \brief Writes a two-component run into the fixture directory.
   *
   * Written rather than checked in, so what the test reads is what the SDK's
   * own writer produces today — a hand-edited manifest would drift from the
   * schema the moment it changed. It lands under tests/fixtures/results so
   * it can be opened and read by hand.
   *
   * \returns The manifest's path.
   */
  QString writeFixtureRun()
  {
    const QString directory =
      QStringLiteral(COMPOSER_RESULTS_FIXTURE_DIR) + QStringLiteral("/generated");

    QDir().mkpath(directory);

    // A tiny CSV artifact, so opening the component has something real to
    // read: three elements over four times.
    const QString csvPath = directory + QStringLiteral("/flow.csv");
    QFile csv(csvPath);

    if (csv.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      QTextStream out(&csv);
      out << "time,flow_0,flow_1,flow_2\n";

      for (int step = 0; step < 4; ++step)
      {
        out << 1000.0 + step << ',' << step << ',' << step * 2 << ','
            << step * 3 << '\n';
      }
    }

    SDK::IO::RunManifest manifest;
    manifest.id = "fixture-run";
    manifest.caption = "Fixture Run";
    manifest.started = "2026-08-26T00:00:00Z";
    manifest.finished = "2026-08-26T00:00:05Z";
    manifest.status = SDK::IO::RunStatus::Completed;

    SDK::IO::ResultEntry flow;
    flow.componentId = "channel";
    flow.itemId = "flow";
    flow.artifact = "flow.csv";
    flow.format = "csv";
    flow.variable = "flow";
    flow.kind = HydroCouple::DataKind::Float64;
    flow.shape = {4, 3};
    flow.dimensions = {"time", "element"};
    flow.units = "m3/s";
    flow.description = "Channel discharge";
    flow.time = SDK::IO::ResultTimeAxis{4, 1000.0, 1003.0, "julian_day"};

    SDK::IO::ResultEntry depth;
    depth.componentId = "channel";
    depth.itemId = "depth";
    depth.artifact = "flow.csv";
    depth.format = "csv";
    depth.variable = "depth";
    depth.kind = HydroCouple::DataKind::Float64;
    depth.shape = {4, 3};
    depth.dimensions = {"time", "element"};
    depth.units = "m";

    // A second component, so the tree has more than one branch to get wrong.
    SDK::IO::ResultEntry area;
    area.componentId = "catchment";
    area.itemId = "area";
    area.artifact = "flow.csv";
    area.format = "csv";
    area.variable = "area";
    area.kind = HydroCouple::DataKind::Float64;
    area.shape = {3};
    area.dimensions = {"element"};
    area.units = "m2";

    manifest.results = {flow, depth, area};

    const QString manifestPath = directory + QStringLiteral("/run.json");
    std::string message;

    manifest.write(std::filesystem::path(manifestPath.toStdString()),
                   message);

    return manifestPath;
  }

  class RunBrowserTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_runbrowser";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }

        s_manifestPath = writeFixtureRun();
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static ComposerApplication *s_app;
      static QString s_manifestPath;
  };

  ComposerApplication *RunBrowserTest::s_app = nullptr;
  QString RunBrowserTest::s_manifestPath;
}

TEST_F(RunBrowserTest, ReadsTheCatalogWithoutAnyModelLibrary)
{
  QString message;
  const std::unique_ptr<RunSession> session =
    RunSession::open(s_manifestPath, message);

  ASSERT_NE(session, nullptr) << message.toStdString();

  EXPECT_EQ(session->title(), QStringLiteral("Fixture Run"));

  // Catalog order, deduplicated — the order things were recorded in, which
  // is more use than alphabetical.
  const QStringList components = session->componentIds();
  ASSERT_EQ(components.size(), 2);
  EXPECT_EQ(components.at(0), QStringLiteral("channel"));
  EXPECT_EQ(components.at(1), QStringLiteral("catchment"));

  const QVector<const SDK::IO::ResultEntry *> channel =
    session->entriesFor(QStringLiteral("channel"));
  ASSERT_EQ(channel.size(), 2);
  EXPECT_EQ(channel.at(0)->itemId, "flow");
  EXPECT_EQ(channel.at(1)->itemId, "depth");

  EXPECT_TRUE(session->entriesFor(QStringLiteral("nobody")).isEmpty());
}

TEST_F(RunBrowserTest, TheTreeIsRunThenComponentThenItem)
{
  RunBrowserModel model;

  QString message;
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr)
    << message.toStdString();

  ASSERT_EQ(model.runCount(), 1);
  ASSERT_EQ(model.rowCount(), 1);

  const QModelIndex run = model.index(0, 0);
  ASSERT_TRUE(run.isValid());
  EXPECT_FALSE(model.parent(run).isValid());
  EXPECT_EQ(model.rowCount(run), 2) << "the run's components";

  const QModelIndex component = model.index(0, 0, run);
  ASSERT_TRUE(component.isValid());
  EXPECT_EQ(model.parent(component), run)
    << "a component's parent is not the run it belongs to";
  EXPECT_EQ(model.rowCount(component), 2) << "the component's items";

  const QModelIndex item = model.index(1, 0, component);
  ASSERT_TRUE(item.isValid());
  EXPECT_EQ(model.parent(item), component);
  EXPECT_EQ(model.rowCount(item), 0) << "a recorded value has no parts";

  // The second component has one item, so a tree that confused the two
  // branches would show two here.
  const QModelIndex second = model.index(1, 0, run);
  ASSERT_TRUE(second.isValid());
  EXPECT_EQ(model.rowCount(second), 1);
}

TEST_F(RunBrowserTest, EveryColumnComesFromTheCatalog)
{
  RunBrowserModel model;

  QString message;
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr);

  const QModelIndex run = model.index(0, 0);
  const QModelIndex component = model.index(0, 0, run);
  const QModelIndex flow = model.index(0, 0, component);

  ASSERT_TRUE(flow.isValid());

  const auto text = [&model, &component](int row, int column)
  {
    return model.data(model.index(row, column, component)).toString();
  };

  EXPECT_EQ(text(0, RunBrowserModel::NameColumn), QStringLiteral("flow"));
  EXPECT_EQ(text(0, RunBrowserModel::KindColumn), QStringLiteral("Float64"));

  // The shape as the manifest gives it: four times of three elements, not a
  // count of values and not the two numbers the other way round.
  EXPECT_EQ(text(0, RunBrowserModel::ShapeColumn), QStringLiteral("4 × 3"));
  EXPECT_EQ(text(0, RunBrowserModel::UnitsColumn), QStringLiteral("m3/s"));

  EXPECT_TRUE(text(0, RunBrowserModel::TimeColumn).contains(QStringLiteral("4")))
    << text(0, RunBrowserModel::TimeColumn).toStdString();
  EXPECT_TRUE(
    text(0, RunBrowserModel::TimeColumn).contains(QStringLiteral("julian_day")));

  // An item with no time axis says so rather than showing an empty one.
  const QModelIndex catchment = model.index(1, 0, run);
  const QString staticTime =
    model
      .data(model.index(0, RunBrowserModel::TimeColumn, catchment))
      .toString();

  EXPECT_FALSE(staticTime.isEmpty());
  EXPECT_FALSE(staticTime.contains(QStringLiteral("julian_day")))
    << "a static field was given a time axis it never had";

  // The roles a view acts through carry identity, not display text.
  EXPECT_EQ(model.data(flow, RunBrowserModel::ComponentIdRole).toString(),
            QStringLiteral("channel"));
  EXPECT_EQ(model.data(flow, RunBrowserModel::ItemIdRole).toString(),
            QStringLiteral("flow"));
}

TEST_F(RunBrowserTest, TwoRunsAreOpenAtOnceAndStaySeparate)
{
  RunBrowserModel model;

  QString message;
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr);
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr);

  ASSERT_EQ(model.runCount(), 2);

  const QModelIndex first = model.index(0, 0);
  const QModelIndex second = model.index(1, 0);

  ASSERT_TRUE(first.isValid());
  ASSERT_TRUE(second.isValid());

  // Each row resolves to its own session — comparing a run against another
  // is the reason two are open, and a tree that returned the first for both
  // would compare a run against itself without saying so.
  EXPECT_NE(model.runFor(first), model.runFor(second));
  EXPECT_EQ(model.runFor(model.index(0, 0, second)), model.runFor(second));

  model.removeRun(0);

  ASSERT_EQ(model.runCount(), 1);
  EXPECT_EQ(model.runFor(model.index(0, 0)), model.run(0));
}

TEST_F(RunBrowserTest, ARunThatCannotBeReadIsRefusedWithAReason)
{
  RunBrowserModel model;

  QString message;

  EXPECT_EQ(model.addRun(QStringLiteral("/nowhere/run.json"), message),
            nullptr);
  EXPECT_FALSE(message.isEmpty()) << "refused without saying why";
  EXPECT_EQ(model.runCount(), 0);
}

TEST_F(RunBrowserTest, TheSdkFixtureIsReportedExactlyAsItsManifestReadsIt)
{
  const QString sdkManifest = QStringLiteral(COMPOSER_SDK_REOPEN_MANIFEST);

  if (sdkManifest.isEmpty() || !QFile::exists(sdkManifest))
  {
    GTEST_SKIP() << "the SDK's reopen fixture is not in this checkout";
  }

  QString message;
  const std::unique_ptr<RunSession> session =
    RunSession::open(sdkManifest, message);

  ASSERT_NE(session, nullptr) << message.toStdString();

  // Read a second time, independently of the session, and compare what the
  // browser reports against what the file says. This is the gate the phase
  // asks for: the catalog shown *is* the catalog recorded, across all four
  // artifact formats the fixture carries.
  SDK::IO::RunManifest direct;
  std::string reason;

  ASSERT_TRUE(SDK::IO::RunManifest::read(
    std::filesystem::path(sdkManifest.toStdString()), direct, reason))
    << reason;

  ASSERT_FALSE(direct.results.empty());

  // Field by field, in order. This is the phase's own gate: the catalog the
  // session holds is the catalog the file records, not a re-derivation of it.
  const std::vector<SDK::IO::ResultEntry> &held = session->manifest().results;

  ASSERT_EQ(held.size(), direct.results.size());

  for (size_t i = 0; i < held.size(); ++i)
  {
    EXPECT_EQ(held[i].componentId, direct.results[i].componentId) << i;
    EXPECT_EQ(held[i].itemId, direct.results[i].itemId) << i;
    EXPECT_EQ(held[i].artifact, direct.results[i].artifact) << i;
    EXPECT_EQ(held[i].format, direct.results[i].format) << i;
    EXPECT_EQ(held[i].kind, direct.results[i].kind) << i;
    EXPECT_EQ(held[i].shape, direct.results[i].shape) << i;
    EXPECT_EQ(held[i].units, direct.results[i].units) << i;
    EXPECT_EQ(held[i].mesh, direct.results[i].mesh) << i;
    EXPECT_EQ(held[i].time.has_value(), direct.results[i].time.has_value())
      << i;
  }

  // And the browser reports each of them once. Counting rows would not show
  // this: the fixture records the same item into four different artifacts,
  // so an entry cannot be identified by component and item alone — which is
  // exactly the mistake this assertion is built to catch. The pointers come
  // from the manifest's own vector, so their offsets name the entries.
  std::vector<int> reported(held.size(), 0);

  for (const QString &componentId : session->componentIds())
  {
    for (const SDK::IO::ResultEntry *entry : session->entriesFor(componentId))
    {
      ASSERT_NE(entry, nullptr);

      const auto offset = size_t(entry - held.data());

      ASSERT_LT(offset, held.size())
        << "an entry that is not in the manifest was reported";

      ++reported[offset];
    }
  }

  for (size_t i = 0; i < reported.size(); ++i)
  {
    EXPECT_EQ(reported[i], 1)
      << "entry " << i << " (" << held[i].componentId << '/'
      << held[i].itemId << ", " << held[i].format << ") was reported "
      << reported[i] << " times";
  }
}

TEST_F(RunBrowserTest, OpeningAComponentReadsItsArtifacts)
{
  QString message;
  const std::unique_ptr<RunSession> session =
    RunSession::open(s_manifestPath, message);

  ASSERT_NE(session, nullptr) << message.toStdString();

  // The catalog opened without touching a file; this is where the artifact
  // is actually read, and with no model library involved.
  HydroCouple::IComponentDataItem *item =
    session->item(QStringLiteral("channel"), QStringLiteral("flow"), message);

  ASSERT_NE(item, nullptr) << message.toStdString();
  EXPECT_EQ(item->id(), "flow");

  const std::vector<int64_t> shape = item->shape();
  ASSERT_EQ(shape.size(), 2u);
  EXPECT_EQ(shape[0], 4);
  EXPECT_EQ(shape[1], 3);

  // Asked for twice, opened once: a browser asks repeatedly and reopening
  // re-reads every artifact.
  HydroCouple::IComponentDataItem *again =
    session->item(QStringLiteral("channel"), QStringLiteral("flow"), message);

  EXPECT_EQ(again, item);

  EXPECT_EQ(session->item(QStringLiteral("channel"),
                          QStringLiteral("nothing"), message),
            nullptr);
  EXPECT_FALSE(message.isEmpty());
}

TEST_F(RunBrowserTest, AMovedArtifactIsReportedRatherThanReturnedEmpty)
{
  const QString directory =
    QStringLiteral(COMPOSER_RESULTS_FIXTURE_DIR) + QStringLiteral("/missing");

  QDir().mkpath(directory);

  SDK::IO::RunManifest manifest;
  manifest.id = "moved-artifact";
  manifest.caption = "Moved Artifact";

  SDK::IO::ResultEntry entry;
  entry.componentId = "channel";
  entry.itemId = "flow";
  entry.artifact = "not-here.csv";
  entry.format = "csv";
  entry.variable = "flow";
  entry.kind = HydroCouple::DataKind::Float64;
  entry.shape = {4, 3};
  entry.dimensions = {"time", "element"};

  manifest.results = {entry};

  const QString manifestPath = directory + QStringLiteral("/run.json");
  std::string writeMessage;
  ASSERT_TRUE(manifest.write(
    std::filesystem::path(manifestPath.toStdString()), writeMessage))
    << writeMessage;

  QString message;
  const std::unique_ptr<RunSession> session =
    RunSession::open(manifestPath, message);

  // The catalog still reads: a manifest whose files have moved is still a
  // record of what was run, and refusing to open it would hide that.
  ASSERT_NE(session, nullptr) << message.toStdString();
  EXPECT_EQ(session->entriesFor(QStringLiteral("channel")).size(), 1);

  // Opening the artifacts is where it fails, and the component is where to
  // assert it: a component that failed to open still answers an empty
  // results() list, so asking for a missing item gives nullptr either way —
  // "could not open" and "no such item" would look identical from there.
  message.clear();

  EXPECT_EQ(session->component(QStringLiteral("channel"), message), nullptr)
    << "a component whose artifact is missing reported itself opened";
  EXPECT_FALSE(message.isEmpty()) << "it failed without saying why";

  message.clear();

  EXPECT_EQ(session->item(QStringLiteral("channel"),
                          QStringLiteral("flow"), message),
            nullptr);
  EXPECT_FALSE(message.isEmpty());
}

TEST_F(RunBrowserTest, TheWindowOpensARunIntoItsBrowser)
{
  ComposerMainWindow window;

  ASSERT_NE(window.runs(), nullptr);
  ASSERT_NE(window.runBrowser(), nullptr);
  EXPECT_EQ(window.runBrowser()->model(), window.runs())
    << "the panel is browsing a different set of runs than the window holds";

  QString message;
  ASSERT_TRUE(window.openRun(s_manifestPath, message))
    << message.toStdString();

  EXPECT_EQ(window.runs()->runCount(), 1);

  auto *tree = window.runBrowser()->findChild<QTreeView *>(
    QStringLiteral("runTree"));
  ASSERT_NE(tree, nullptr);
  EXPECT_EQ(tree->model(), window.runs());

  // Closing acts on the run the selection is inside, so it works from a
  // selected item as well as from the run's own row.
  const QModelIndex run = window.runs()->index(0, 0);
  const QModelIndex component = window.runs()->index(0, 0, run);

  // The *second* item, deliberately: its own row is 1, so a panel that read
  // the highlighted row instead of walking up to the run would answer 1 and
  // close a run that is not open. The first item's row is 0, which is also
  // the run's, and would have hidden exactly that.
  const QModelIndex item = window.runs()->index(1, 0, component);
  ASSERT_TRUE(item.isValid());

  tree->setCurrentIndex(item);
  EXPECT_EQ(window.runBrowser()->currentRunRow(), 0)
    << "closing would have acted on the wrong run";

  auto *close = window.runBrowser()->findChild<QToolButton *>(
    QStringLiteral("closeRunButton"));
  ASSERT_NE(close, nullptr);
  EXPECT_TRUE(close->isEnabled());

  close->click();

  EXPECT_EQ(window.runs()->runCount(), 0);
  EXPECT_FALSE(close->isEnabled())
    << "the close button offers to close a run that is no longer open";
}

// ── D2a: a recorded item becomes a layer ─────────────────────────────────
//
// Browsing a run and seeing it are two different things, and until this
// slice the browser could do only the first. The gate is that the layer the
// map gets is built from what the run recorded -- the geometry and the
// values -- rather than from a plausible reconstruction of either.

TEST_F(RunBrowserTest, ARecordedItemBecomesALayerOnTheMap)
{
  const QString sdkManifest = QStringLiteral(COMPOSER_SDK_REOPEN_MANIFEST);

  if (sdkManifest.isEmpty() || !QFile::exists(sdkManifest))
  {
    GTEST_SKIP() << "the SDK's reopen fixture is not in this checkout";
  }

  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  QString message;
  ASSERT_TRUE(window.openRun(sdkManifest, message)) << message.toStdString();

  RunSession *session = window.runs()->run(0);
  ASSERT_NE(session, nullptr);

  const QStringList components = session->componentIds();
  ASSERT_FALSE(components.isEmpty());

  const QVector<const SDK::IO::ResultEntry *> entries =
    session->entriesFor(components.first());
  ASSERT_FALSE(entries.isEmpty());

  // The entry that names a mesh: the fixture records one item into four
  // artifacts and only some of them carry geometry, which is the whole
  // reason the catalog names the attachment per entry.
  const auto spatial =
    std::find_if(entries.begin(), entries.end(),
                 [](const SDK::IO::ResultEntry *entry)
                 { return !entry->mesh.empty(); });

  if (spatial == entries.end())
  {
    GTEST_SKIP() << "this build recorded no format that carries geometry";
  }

  const int before = window.layerStack()->rowCount();

  ASSERT_TRUE(window.showRunItem(
    0, components.first(), QString::fromStdString((*spatial)->itemId),
    message))
    << message.toStdString();

  ASSERT_EQ(window.layerStack()->rowCount(), before + 1);

  auto *layer = dynamic_cast<DataItemLayer *>(
    window.layerStack()->layerAt(window.layerStack()->rowCount() - 1));
  ASSERT_NE(layer, nullptr) << "what was added is not a data-item layer";

  // One feature per recorded entity, and one time level per recorded step:
  // both come from the manifest, so a layer that agreed with neither would
  // still have drawn something.
  ASSERT_EQ((*spatial)->shape.size(), 2u);
  EXPECT_EQ(layer->featureCount(), static_cast<int>((*spatial)->shape[1]));
  EXPECT_EQ(layer->timeCount(), static_cast<int>((*spatial)->shape[0]));

  // The run is in the name, because holding two runs open to compare them is
  // what the browser is for, and two layers called "flow" are not comparable.
  EXPECT_TRUE(layer->name().contains(session->title()))
    << "the layer does not say which run it came from: "
    << layer->name().toStdString();

  // Brought forward. A layer added from a dock at the bottom of the window
  // is otherwise drawn behind whichever tab happens to be showing, and the
  // user is left looking at the tab they were already on wondering whether
  // anything happened.
  auto *workspace =
    window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  ASSERT_NE(workspace, nullptr);
  EXPECT_EQ(workspace->currentWidget(),
            static_cast<QWidget *>(window.mapCanvas()));

  // The values on the map are the values in the artifact, read a second time
  // through the item itself rather than through the layer.
  auto *item = dynamic_cast<HydroCouple::Temporal::ITimeSeriesComponentDataItem *>(
    layer->dataItem());
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->timeCount(), layer->timeCount());
}

TEST_F(RunBrowserTest, AnItemRecordedWithoutGeometrySaysSoRatherThanFailing)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  QString message;
  ASSERT_TRUE(window.openRun(s_manifestPath, message))
    << message.toStdString();

  const int before = window.layerStack()->rowCount();

  // The local fixture is a CSV of values with no mesh anywhere -- a complete
  // recording, and nothing to draw.
  EXPECT_FALSE(window.showRunItem(0, QStringLiteral("channel"),
                                  QStringLiteral("flow"), message));

  EXPECT_EQ(window.layerStack()->rowCount(), before)
    << "a layer was added for an item with no geometry";

  // And it says which item and why, rather than failing silently or
  // reporting something that reads like a broken file.
  EXPECT_TRUE(message.contains(QStringLiteral("flow")))
    << message.toStdString();
  EXPECT_TRUE(message.contains(QStringLiteral("geometry")))
    << message.toStdString();
}

TEST_F(RunBrowserTest, ShowingIsOfferedOnlyForRowsThatNameAnItem)
{
  RunBrowserModel model;
  QString message;
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr)
    << message.toStdString();

  RunBrowserPanel panel;
  panel.setModel(&model);

  auto *tree = panel.findChild<QTreeView *>(QStringLiteral("runTree"));
  auto *show = panel.findChild<QToolButton *>(QStringLiteral("showItemButton"));
  ASSERT_NE(tree, nullptr);
  ASSERT_NE(show, nullptr);

  const QModelIndex run = model.index(0, 0);
  ASSERT_TRUE(run.isValid());

  tree->selectionModel()->setCurrentIndex(run,
                                          QItemSelectionModel::ClearAndSelect);
  EXPECT_FALSE(show->isEnabled()) << "a run row offered to be drawn";

  const QModelIndex component = model.index(0, 0, run);
  ASSERT_TRUE(component.isValid());

  // A component row carries a component id too, so a check that only looked
  // for one would light this up and then have no item to ask for.
  tree->selectionModel()->setCurrentIndex(component,
                                          QItemSelectionModel::ClearAndSelect);
  EXPECT_FALSE(show->isEnabled()) << "a component row offered to be drawn";

  const QModelIndex item = model.index(0, 0, component);
  ASSERT_TRUE(item.isValid());

  tree->selectionModel()->setCurrentIndex(item,
                                          QItemSelectionModel::ClearAndSelect);
  EXPECT_TRUE(show->isEnabled());

  QSignalSpy asked(&panel, &RunBrowserPanel::showItemRequested);
  show->click();

  ASSERT_EQ(asked.size(), 1);
  EXPECT_EQ(asked.at(0).at(0).toInt(), 0);
  EXPECT_FALSE(asked.at(0).at(1).toString().isEmpty())
    << "the request named no component";
  EXPECT_FALSE(asked.at(0).at(2).toString().isEmpty())
    << "the request named no item";
}

// ── D4a — comparing two runs ────────────────────────────────────────────────

TEST_F(RunBrowserTest, TheModelSaysWhichOpenRunsCarryTheSameItem)
{
  // From the manifests, so asking costs no artifact reads: a viewer holding
  // ten runs open should not have to read forty files to grey out a button.
  RunBrowserModel model;
  QString message;
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr)
    << message.toStdString();
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr);

  RunSession *session = model.run(0);
  ASSERT_NE(session, nullptr);

  const QStringList components = session->componentIds();
  ASSERT_FALSE(components.isEmpty());

  const QVector<const SDK::IO::ResultEntry *> entries =
    session->entriesFor(components.first());
  ASSERT_FALSE(entries.isEmpty());

  const QString itemId = QString::fromStdString(entries.first()->itemId);

  EXPECT_EQ(model.runsCarrying(components.first(), itemId),
            (QVector<int>{0, 1}));

  // An item neither run recorded, said as an empty answer rather than as a
  // run that happens to hold something with the same component id.
  EXPECT_TRUE(
    model.runsCarrying(components.first(), QStringLiteral("nothing-like-it"))
      .isEmpty());
}

TEST_F(RunBrowserTest, ComparingIsOfferedOnlyOnceASecondRunIsOpen)
{
  RunBrowserModel model;
  QString message;
  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr)
    << message.toStdString();

  RunBrowserPanel panel;
  panel.setModel(&model);

  auto *tree = panel.findChild<QTreeView *>(QStringLiteral("runTree"));
  auto *compare =
    panel.findChild<QToolButton *>(QStringLiteral("compareItemButton"));
  ASSERT_NE(tree, nullptr);
  ASSERT_NE(compare, nullptr);

  const QModelIndex item =
    model.index(0, 0, model.index(0, 0, model.index(0, 0)));
  ASSERT_TRUE(item.isValid());

  tree->selectionModel()->setCurrentIndex(item,
                                          QItemSelectionModel::ClearAndSelect);

  // One run open: an item is selected and drawable, and there is still
  // nothing to compare it against. Offered and then refused is worse.
  EXPECT_FALSE(compare->isEnabled())
    << "a comparison was offered with only one run open";

  ASSERT_NE(model.addRun(s_manifestPath, message), nullptr);

  EXPECT_TRUE(compare->isEnabled())
    << "a second run did not enable the comparison";

  QSignalSpy asked(&panel, &RunBrowserPanel::compareItemRequested);
  compare->click();

  ASSERT_EQ(asked.size(), 1);
  EXPECT_EQ(asked.at(0).at(0).toInt(), 0);
  EXPECT_FALSE(asked.at(0).at(2).toString().isEmpty())
    << "the request named no item";
}

TEST_F(RunBrowserTest, ARunComparedAgainstACopyOfItselfDrawsZeroEverywhere)
{
  // The plan's gate, end to end: the same manifest opened twice, differenced
  // through the window. Every feature reads exactly zero, and a comparison
  // that had matched the wrong instants or the wrong entities would read
  // nearly zero and look completely convincing.
  const QString sdkManifest = QStringLiteral(COMPOSER_SDK_REOPEN_MANIFEST);

  if (sdkManifest.isEmpty() || !QFile::exists(sdkManifest))
  {
    GTEST_SKIP() << "the SDK's reopen fixture is not in this checkout";
  }

  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  QString message;
  ASSERT_TRUE(window.openRun(sdkManifest, message)) << message.toStdString();
  ASSERT_TRUE(window.openRun(sdkManifest, message)) << message.toStdString();

  RunSession *session = window.runs()->run(0);
  ASSERT_NE(session, nullptr);

  const QStringList components = session->componentIds();
  ASSERT_FALSE(components.isEmpty());

  const QVector<const SDK::IO::ResultEntry *> entries =
    session->entriesFor(components.first());

  const auto spatial =
    std::find_if(entries.begin(), entries.end(),
                 [](const SDK::IO::ResultEntry *entry)
                 { return !entry->mesh.empty(); });

  if (spatial == entries.end())
  {
    GTEST_SKIP() << "this build recorded no format that carries geometry";
  }

  const QString itemId = QString::fromStdString((*spatial)->itemId);
  const int before = window.layerStack()->rowCount();

  ASSERT_TRUE(
    window.compareRunItem(0, 1, components.first(), itemId, message))
    << message.toStdString();

  ASSERT_EQ(window.layerStack()->rowCount(), before + 1);

  auto *layer = dynamic_cast<DifferenceLayer *>(
    window.layerStack()->layerAt(window.layerStack()->rowCount() - 1));
  ASSERT_NE(layer, nullptr) << "what was added is not a difference layer";

  ASSERT_GT(layer->featureCount(), 0);

  for (int level = 0; level < layer->timeCount(); ++level)
  {
    ASSERT_TRUE(layer->setTimeIndex(level));

    for (int feature = 0; feature < layer->featureCount(); ++feature)
    {
      const QVariant value =
        layer->attributeValue(feature, layer->valueAttribute());

      ASSERT_TRUE(value.isValid())
        << "feature " << feature << " has no difference at level " << level;
      EXPECT_DOUBLE_EQ(value.toDouble(), 0.0)
        << "feature " << feature << " at level " << level;
    }
  }

  // Both runs named, in the order they were subtracted: a difference map
  // whose title does not say which way round it was taken has a sign nobody
  // can read.
  EXPECT_TRUE(layer->name().contains(QStringLiteral("−")))
    << layer->name().toStdString();
}

TEST_F(RunBrowserTest, ARunComparedAgainstItselfIsRefusedRatherThanDrawn)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  QString message;
  ASSERT_TRUE(window.openRun(s_manifestPath, message))
    << message.toStdString();

  RunSession *session = window.runs()->run(0);
  ASSERT_NE(session, nullptr);

  const QStringList components = session->componentIds();
  ASSERT_FALSE(components.isEmpty());

  const QVector<const SDK::IO::ResultEntry *> entries =
    session->entriesFor(components.first());
  ASSERT_FALSE(entries.isEmpty());

  const int before = window.layerStack()->rowCount();

  EXPECT_FALSE(window.compareRunItem(
    0, 0, components.first(),
    QString::fromStdString(entries.first()->itemId), message));
  EXPECT_TRUE(message.contains(QStringLiteral("itself")))
    << message.toStdString();
  EXPECT_EQ(window.layerStack()->rowCount(), before)
    << "a comparison of a run with itself still added a layer";
}
