/*!
 * \file   test_selectionhub.cpp
 * \brief  One selection, seen from the canvas and from the layers.
 *
 * Both directions, the cases where there is nothing to say, and the
 * re-entrancy guard — which is gated by exit status as well as by
 * assertion, because a hub that recurses does not fail a test, it kills
 * the process (C4c's lesson, paid for once already).
 */

#include "canvas/canvasitems.h"
#include "canvas/compositionscene.h"
#include "core/composerapplication.h"
#include "layers/componentlayer.h"
#include "map/layerstackmodel.h"
#include "project/compositiondocument.h"
#include "ui/selectionhub.h"

#include "componentprobe.h"
#include "probelayer.h"
#include "vectorprobe.h"

#include <gtest/gtest.h>

#include <QSignalSpy>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  class SelectionHubTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_selectionhub";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static inline ComposerApplication *s_app = nullptr;
  };
}

// ── provenance ──────────────────────────────────────────────────────────

TEST_F(SelectionHubTest, AComponentIsAskedOfTheInterfaceNotOfALayerType)
{
  Testing::ComponentProbe fromComponent(QStringLiteral("runoff"),
                                        QStringLiteral("catchment"));
  Testing::VectorProbe fromFile(QStringLiteral("reaches.geojson"));

  EXPECT_EQ(SelectionHub::componentOf(&fromComponent),
            QStringLiteral("catchment"));
  EXPECT_EQ(fromComponent.dataItemId(), QStringLiteral("values"));

  // A layer read from a file belongs to no component, and so does nullptr.
  EXPECT_TRUE(SelectionHub::componentOf(&fromFile).isEmpty());
  EXPECT_TRUE(SelectionHub::componentOf(nullptr).isEmpty());
}

TEST_F(SelectionHubTest, LayersOfAComponentAreFoundTopFirstAndOnlyByAnId)
{
  LayerStackModel stack;
  SelectionHub hub(nullptr, &stack);

  auto *first = new Testing::ComponentProbe(QStringLiteral("runoff"),
                                            QStringLiteral("catchment"));
  auto *second = new Testing::ComponentProbe(QStringLiteral("depth"),
                                             QStringLiteral("catchment"));
  auto *other = new Testing::ComponentProbe(QStringLiteral("flow"),
                                            QStringLiteral("channel"));
  auto *file = new Testing::VectorProbe(QStringLiteral("reaches.geojson"));

  ASSERT_GE(stack.addLayer(first), 0);
  ASSERT_GE(stack.addLayer(second), 0);
  ASSERT_GE(stack.addLayer(other), 0);
  ASSERT_GE(stack.addLayer(file), 0);

  const QVector<MapLayer *> found = hub.layersOf(QStringLiteral("catchment"));
  ASSERT_EQ(found.size(), 2);

  // Top-first, as the stack lists them: the newest is on top, so "the
  // first" is the one the user can actually see.
  EXPECT_EQ(found.first(), second);
  EXPECT_EQ(found.last(), first);

  EXPECT_EQ(hub.layersOf(QStringLiteral("channel")).size(), 1);
  EXPECT_TRUE(hub.layersOf(QStringLiteral("nobody")).isEmpty());

  // An empty id matches nothing, rather than matching every layer that
  // never recorded one — which is every file layer on the map.
  EXPECT_TRUE(hub.layersOf(QString()).isEmpty());
}

// ── canvas → layers ─────────────────────────────────────────────────────

TEST_F(SelectionHubTest, SelectingAComponentBringsItsLayerForward)
{
  CompositionDocument document;
  LayerStackModel stack;
  CompositionScene scene(&document, nullptr);
  SelectionHub hub(&scene, &stack);

  auto *layer = new Testing::ComponentProbe(QStringLiteral("runoff"),
                                            QStringLiteral("catchment"));
  ASSERT_GE(stack.addLayer(layer), 0);

  auto *node = new ComponentNodeItem(QStringLiteral("catchment"),
                                     QStringLiteral("Catchment"), {}, {});
  scene.addItem(node);

  QSignalSpy asked(&hub, &SelectionHub::currentLayerRequested);
  node->setSelected(true);

  ASSERT_EQ(asked.count(), 1);
  EXPECT_EQ(asked.first().at(0).value<MapLayer *>(), layer);
}

TEST_F(SelectionHubTest, AComponentWithNoLayersAsksForNothing)
{
  CompositionDocument document;
  LayerStackModel stack;
  CompositionScene scene(&document, nullptr);
  SelectionHub hub(&scene, &stack);

  auto *node = new ComponentNodeItem(QStringLiteral("channel"),
                                     QStringLiteral("Channel"), {}, {});
  scene.addItem(node);

  QSignalSpy asked(&hub, &SelectionHub::currentLayerRequested);
  node->setSelected(true);

  // Silence, not a request for nullptr: clearing the tree's current row
  // because a component without layers was selected would throw away a
  // selection the user made on purpose.
  EXPECT_EQ(asked.count(), 0);
}

// ── layers → canvas ─────────────────────────────────────────────────────

TEST_F(SelectionHubTest, PickingAFeatureNamesTheComponentThatProducedIt)
{
  LayerStackModel stack;
  SelectionHub hub(nullptr, &stack);

  Testing::ComponentProbe fromComponent(QStringLiteral("runoff"),
                                        QStringLiteral("catchment"));
  Testing::VectorProbe fromFile(QStringLiteral("reaches.geojson"));

  QSignalSpy asked(&hub, &SelectionHub::componentSelectionRequested);

  hub.featurePicked(&fromComponent);
  ASSERT_EQ(asked.count(), 1);
  EXPECT_EQ(asked.first().at(0).toString(), QStringLiteral("catchment"));

  // A pick on a file layer says "no component" rather than staying quiet:
  // leaving the canvas lit on the last one that had a component would be
  // a highlight claiming a relationship that does not hold.
  hub.featurePicked(&fromFile);
  ASSERT_EQ(asked.count(), 2);
  EXPECT_TRUE(asked.at(1).at(0).toString().isEmpty());

  // And a click that missed everything says the same.
  hub.featurePicked(nullptr);
  ASSERT_EQ(asked.count(), 3);
  EXPECT_TRUE(asked.at(2).at(0).toString().isEmpty());
}

TEST_F(SelectionHubTest, TheTwoDirectionsDoNotChaseEachOther)
{
  CompositionDocument document;
  LayerStackModel stack;
  CompositionScene scene(&document, nullptr);
  SelectionHub hub(&scene, &stack);

  auto *layer = new Testing::ComponentProbe(QStringLiteral("runoff"),
                                            QStringLiteral("catchment"));
  ASSERT_GE(stack.addLayer(layer), 0);

  auto *node = new ComponentNodeItem(QStringLiteral("catchment"),
                                     QStringLiteral("Catchment"), {}, {});
  scene.addItem(node);

  // Wired head to tail, exactly as the window wires it: a pick selects the
  // component, and selecting a component asks for its layer. Without the
  // guard this does not fail — it recurses until the process dies, which
  // is why the falsifier judges this gate by exit status too.
  QObject::connect(&hub, &SelectionHub::componentSelectionRequested, &scene,
                   [&scene](const QString &id) { scene.selectComponent(id); });

  QSignalSpy layerAsked(&hub, &SelectionHub::currentLayerRequested);
  QSignalSpy componentAsked(&hub, &SelectionHub::componentSelectionRequested);

  hub.featurePicked(layer);

  EXPECT_EQ(componentAsked.count(), 1);

  // The canvas selection that the pick caused does not bounce back as a
  // second layer request: one hop, then it stops.
  EXPECT_LE(layerAsked.count(), 1);
  EXPECT_TRUE(scene.node(QStringLiteral("catchment"))->isSelected());
}

// ── the scene's own half ────────────────────────────────────────────────

TEST_F(SelectionHubTest, SelectingAComponentSelectsThatOneAndClearsTheRest)
{
  CompositionDocument document;
  CompositionScene scene(&document, nullptr);

  auto *first = new ComponentNodeItem(QStringLiteral("catchment"),
                                      QStringLiteral("Catchment"), {}, {});
  auto *second = new ComponentNodeItem(QStringLiteral("channel"),
                                       QStringLiteral("Channel"), {}, {});
  scene.addItem(first);
  scene.addItem(second);

  EXPECT_TRUE(scene.selectComponent(QStringLiteral("catchment")));
  EXPECT_TRUE(first->isSelected());
  EXPECT_FALSE(second->isSelected());

  // The second replaces the first rather than joining it: setSelected(true)
  // accumulates, and a canvas that lights one more box per click stops
  // meaning anything.
  EXPECT_TRUE(scene.selectComponent(QStringLiteral("channel")));
  EXPECT_FALSE(first->isSelected());
  EXPECT_TRUE(second->isSelected());

  // An id with no node clears, and says it found nothing.
  EXPECT_FALSE(scene.selectComponent(QStringLiteral("nobody")));
  EXPECT_FALSE(second->isSelected());
  EXPECT_FALSE(scene.selectComponent(QString()));
}

// ── additive selection ──────────────────────────────────────────────────

TEST_F(SelectionHubTest, ModifiersChooseReplaceAddAndToggle)
{
  EXPECT_EQ(selectionModeFor(Qt::NoModifier), SelectionMode::Replace);
  EXPECT_EQ(selectionModeFor(Qt::ShiftModifier), SelectionMode::Add);

  // ControlModifier is ⌘ on macOS, so one rule serves every platform.
  EXPECT_EQ(selectionModeFor(Qt::ControlModifier), SelectionMode::Toggle);

  // Shift wins when both are held: adding is the less destructive of the
  // two, and a user holding both has not said which they meant.
  EXPECT_EQ(selectionModeFor(Qt::ShiftModifier | Qt::ControlModifier),
            SelectionMode::Add);

  // A modifier nobody asked about does not quietly become a mode.
  EXPECT_EQ(selectionModeFor(Qt::AltModifier), SelectionMode::Replace);
}

TEST_F(SelectionHubTest, AddGrowsTheSelectionAndToggleTakesBackOut)
{
  LayerStackModel stack;

  auto *layer = new Testing::VectorProbe(QStringLiteral("conduits"));
  layer->addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });
  layer->addLine({ { 0.0, 5.0 }, { 10.0, 5.0 } });
  layer->addLine({ { 0.0, 9.0 }, { 10.0, 9.0 } });
  ASSERT_GE(stack.addLayer(layer), 0);

  stack.select(layer, QSet<int>{ 0 }, SelectionMode::Replace);
  EXPECT_EQ(layer->selection(), QSet<int>{ 0 });

  stack.select(layer, QSet<int>{ 1 }, SelectionMode::Add);
  EXPECT_EQ(layer->selection(), (QSet<int>{ 0, 1 }));

  // Toggling one that is in takes it out; one that is not puts it in.
  stack.select(layer, QSet<int>{ 0 }, SelectionMode::Toggle);
  EXPECT_EQ(layer->selection(), QSet<int>{ 1 });

  stack.select(layer, QSet<int>{ 2 }, SelectionMode::Toggle);
  EXPECT_EQ(layer->selection(), (QSet<int>{ 1, 2 }));

  // Replace is still replace, whatever was there before.
  stack.select(layer, QSet<int>{ 0 }, SelectionMode::Replace);
  EXPECT_EQ(layer->selection(), QSet<int>{ 0 });
}

TEST_F(SelectionHubTest, ALayerWithNoFeaturesIsNotSelectedInto)
{
  LayerStackModel stack;

  auto *features = new Testing::VectorProbe(QStringLiteral("conduits"));
  features->addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });
  auto *basemap = new Testing::ProbeLayer(QStringLiteral("basemap"),
                                          QRectF(0.0, 0.0, 10.0, 10.0));

  ASSERT_GE(stack.addLayer(features), 0);
  ASSERT_GE(stack.addLayer(basemap), 0);

  stack.select(features, QSet<int>{ 0 }, SelectionMode::Replace);
  ASSERT_EQ(features->selection().size(), 1);

  // A basemap has no features to add to. Reaching for its selection would
  // be a null dereference, not a no-op, so the guard is load-bearing —
  // and what it must do is clear, the same as clicking empty map.
  stack.select(basemap, QSet<int>{ 0 }, SelectionMode::Add);
  EXPECT_TRUE(features->selection().isEmpty());

  stack.select(basemap, QSet<int>{ 0 }, SelectionMode::Toggle);
  EXPECT_TRUE(features->selection().isEmpty());
}

TEST_F(SelectionHubTest, AddingAcrossLayersReplacesRatherThanSplitting)
{
  LayerStackModel stack;

  auto *first = new Testing::VectorProbe(QStringLiteral("conduits"));
  first->addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } });
  first->addLine({ { 0.0, 5.0 }, { 10.0, 5.0 } });

  auto *second = new Testing::VectorProbe(QStringLiteral("junctions"));
  second->addPoint({ 1.0, 1.0 });
  second->addPoint({ 2.0, 2.0 });

  ASSERT_GE(stack.addLayer(first), 0);
  ASSERT_GE(stack.addLayer(second), 0);

  stack.select(first, QSet<int>{ 0, 1 }, SelectionMode::Replace);
  ASSERT_EQ(first->selection().size(), 2);

  // Shift-clicking a feature on *another* layer replaces: the stack keeps
  // one selection on one layer, because that is what lets the attribute
  // table show it. A selection two thirds of which nothing can display is
  // a selection the user only half has.
  stack.select(second, QSet<int>{ 0 }, SelectionMode::Add);

  EXPECT_TRUE(first->selection().isEmpty());
  EXPECT_EQ(second->selection(), QSet<int>{ 0 });
}
