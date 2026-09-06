/*!
 * \file   test_compositioncanvas.cpp
 * \brief  Phase B1 verification — the composition canvas.
 *
 * The decisive test builds a two-component composition through canvas
 * interaction — a palette drop and a port-to-port drag delivered as real mouse
 * events — and compares the resulting document with the hand-written
 * equivalent. Asserting on the *document* rather than on scene internals is
 * the point: it is the document every other view and every other host reads.
 */

#include "canvas/componentpalettemodel.h"
#include "canvas/compositioncanvas.h"
#include "canvas/compositionscene.h"
#include "core/composerapplication.h"
#include "plugins/componentregistry.h"
#include "project/componentinstances.h"
#include "project/compositiondocument.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>
#include <QUndoStack>

using namespace HydroCouple::Composer;
using ConnectionSpec = HydroCouple::SDK::IO::ConnectionSpec;

namespace
{
  QString fixturePath(const QString &stem)
  {
    return QDir(QStringLiteral(COMPOSER_FIXTURE_DIR))
      .absoluteFilePath(QStringLiteral("lib") + stem +
                        ComponentLibrary::librarySuffix());
  }

  //! Delivers a press/move/release triple to the scene, as a real drag does.
  void dragOnScene(QGraphicsScene *scene, const QPointF &from,
                   const QPointF &to)
  {
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setScenePos(from);
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    QCoreApplication::sendEvent(scene, &press);

    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    move.setScenePos(to);
    move.setButtons(Qt::LeftButton);
    QCoreApplication::sendEvent(scene, &move);

    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setScenePos(to);
    release.setButton(Qt::LeftButton);
    QCoreApplication::sendEvent(scene, &release);
  }

  class CanvasTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_compositioncanvas";
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

        instances = std::make_unique<ComponentInstances>(&document, &registry);
        scene = std::make_unique<CompositionScene>(&document, instances.get());
      }

      void TearDown() override
      {
        scene.reset();
        instances.reset();
      }

      ComponentRegistry registry;
      CompositionDocument document;
      std::unique_ptr<ComponentInstances> instances;
      std::unique_ptr<CompositionScene> scene;

      static ComposerApplication *s_app;
  };

  ComposerApplication *CanvasTest::s_app = nullptr;
}

// ── The verification the plan asks for ────────────────────────────────────

TEST_F(CanvasTest, BuildsCompositionByInteractionMatchingHandWrittenDocument)
{
  // Two palette drops...
  const QString upstream = scene->addComponentAt(
    QStringLiteral("composer.test.component"), QStringLiteral("upstream"),
    QPointF(0.0, 0.0));
  const QString downstream = scene->addComponentAt(
    QStringLiteral("composer.test.component"), QStringLiteral("downstream"),
    QPointF(400.0, 0.0));

  ASSERT_EQ(upstream, QStringLiteral("upstream"));
  ASSERT_EQ(downstream, QStringLiteral("downstream"));

  // ...then a genuine port-to-port drag, not a direct API call.
  ComponentNodeItem *fromNode = scene->node(upstream);
  ComponentNodeItem *toNode = scene->node(downstream);
  ASSERT_NE(fromNode, nullptr);
  ASSERT_NE(toNode, nullptr);

  PortItem *outputPort =
    fromNode->port(QStringLiteral("values"), PortItem::Direction::Output);
  PortItem *inputPort =
    toNode->port(QStringLiteral("inflow"), PortItem::Direction::Input);
  ASSERT_NE(outputPort, nullptr) << "the component's output port was not drawn";
  ASSERT_NE(inputPort, nullptr) << "the component's input port was not drawn";

  dragOnScene(scene.get(), outputPort->anchor(), inputPort->anchor());

  // The document must now equal the hand-written equivalent.
  CompositionDocument expected;
  QString message;

  const QByteArray handWritten = R"({
  "schema_version": "1.1",
  "components": [
    { "id": "upstream", "info": { "component_info_id": "composer.test.component" } },
    { "id": "downstream", "info": { "component_info_id": "composer.test.component" } }
  ],
  "connections": [
    { "from": { "component": "upstream", "output": "values" },
      "to": { "component": "downstream", "input": "inflow" } }
  ]
})";

  ASSERT_TRUE(expected.loadFromJson(handWritten, message))
    << message.toStdString();

  EXPECT_EQ(document.toJson(), expected.toJson());
}

// ── Ports come from live instances ────────────────────────────────────────

TEST_F(CanvasTest, DrawsPortsFromTheLiveComponent)
{
  const QString id = scene->addComponentAt(
    QStringLiteral("composer.test.component"), QStringLiteral("node"),
    QPointF(0.0, 0.0));

  ComponentNodeItem *node = scene->node(id);
  ASSERT_NE(node, nullptr);

  EXPECT_NE(node->port(QStringLiteral("inflow"), PortItem::Direction::Input),
            nullptr);
  EXPECT_NE(node->port(QStringLiteral("values"), PortItem::Direction::Output),
            nullptr);

  // Direction matters: an output is not reachable as an input.
  EXPECT_EQ(node->port(QStringLiteral("values"), PortItem::Direction::Input),
            nullptr);
}

TEST_F(CanvasTest, MarksComponentsWhoseLibraryIsMissing)
{
  HydroCouple::SDK::IO::ComponentSpec spec;
  spec.id = "ghost";
  spec.info.componentInfoId = "org.nowhere.missing";
  ASSERT_TRUE(document.addComponent(spec, {}));

  ComponentNodeItem *node = scene->node(QStringLiteral("ghost"));
  ASSERT_NE(node, nullptr) << "an uninstantiable component must still appear";

  EXPECT_FALSE(instances->failure(QStringLiteral("ghost")).isEmpty());
  EXPECT_FALSE(node->toolTip().isEmpty()) << "the reason must be visible";
}

// ── Edits route through the document ──────────────────────────────────────

TEST_F(CanvasTest, DraggingANodePushesAnUndoableMove)
{
  const QString id = scene->addComponentAt(
    QStringLiteral("composer.test.component"), QStringLiteral("node"),
    QPointF(0.0, 0.0));

  ComponentNodeItem *node = scene->node(id);
  ASSERT_NE(node, nullptr);

  const int before = document.undoStack()->count();

  // Drag the node the way a user does — press on its body, move, release —
  // so the scene's own item handling performs the move.
  const QPointF grab = node->sceneBoundingRect().center();
  dragOnScene(scene.get(), grab, grab + QPointF(120.0, 80.0));

  EXPECT_EQ(document.presentation().component(id).position, QPointF(120.0, 80.0));
  EXPECT_GT(document.undoStack()->count(), before);

  document.undoStack()->undo();
  EXPECT_EQ(document.presentation().component(id).position, QPointF(0.0, 0.0));
}

TEST_F(CanvasTest, RefusesSelfConnectionAndDuplicate)
{
  scene->addComponentAt(QStringLiteral("composer.test.component"),
                        QStringLiteral("a"), QPointF(0.0, 0.0));
  scene->addComponentAt(QStringLiteral("composer.test.component"),
                        QStringLiteral("b"), QPointF(300.0, 0.0));

  // A component feeding itself is not a composition edge.
  ComponentNodeItem *a = scene->node(QStringLiteral("a"));
  ASSERT_NE(a, nullptr);
  dragOnScene(scene.get(),
              a->port(QStringLiteral("values"), PortItem::Direction::Output)->anchor(),
              a->port(QStringLiteral("inflow"), PortItem::Direction::Input)->anchor());
  EXPECT_EQ(document.connectionCount(), 0);

  EXPECT_TRUE(scene->connectPorts(QStringLiteral("a"), QStringLiteral("values"),
                                  QStringLiteral("b"), QStringLiteral("inflow")));
  EXPECT_FALSE(scene->connectPorts(QStringLiteral("a"), QStringLiteral("values"),
                                   QStringLiteral("b"), QStringLiteral("inflow")))
    << "the same connection twice";
  EXPECT_EQ(document.connectionCount(), 1);
}

TEST_F(CanvasTest, DeletingASelectedComponentTakesItsConnections)
{
  scene->addComponentAt(QStringLiteral("composer.test.component"),
                        QStringLiteral("a"), QPointF(0.0, 0.0));
  scene->addComponentAt(QStringLiteral("composer.test.component"),
                        QStringLiteral("b"), QPointF(300.0, 0.0));
  ASSERT_TRUE(scene->connectPorts(QStringLiteral("a"), QStringLiteral("values"),
                                  QStringLiteral("b"), QStringLiteral("inflow")));
  ASSERT_EQ(document.connectionCount(), 1);

  ComponentNodeItem *node = scene->node(QStringLiteral("b"));
  ASSERT_NE(node, nullptr);
  node->setSelected(true);

  EXPECT_EQ(scene->removeSelection(), 1);
  EXPECT_EQ(document.componentIds().size(), 1);
  EXPECT_EQ(document.connectionCount(), 0);
}

// ── The scene follows the document, not the other way round ───────────────

TEST_F(CanvasTest, RebuildsWhenTheDocumentChangesElsewhere)
{
  scene->addComponentAt(QStringLiteral("composer.test.component"),
                        QStringLiteral("a"), QPointF(0.0, 0.0));
  ASSERT_NE(scene->node(QStringLiteral("a")), nullptr);

  // An edit made without touching the canvas must still reach it.
  ASSERT_TRUE(document.removeComponent(QStringLiteral("a")));
  EXPECT_EQ(scene->node(QStringLiteral("a")), nullptr);

  document.undoStack()->undo();
  EXPECT_NE(scene->node(QStringLiteral("a")), nullptr);
}

TEST_F(CanvasTest, DrawsAnEdgePerConnection)
{
  scene->addComponentAt(QStringLiteral("composer.test.component"),
                        QStringLiteral("a"), QPointF(0.0, 0.0));
  scene->addComponentAt(QStringLiteral("composer.test.component"),
                        QStringLiteral("b"), QPointF(300.0, 0.0));

  EXPECT_TRUE(scene->edges().isEmpty());

  ASSERT_TRUE(scene->connectPorts(QStringLiteral("a"), QStringLiteral("values"),
                                  QStringLiteral("b"), QStringLiteral("inflow")));

  ASSERT_EQ(scene->edges().size(), 1);
  EXPECT_EQ(scene->edges().first()->connection().toComponent, "b");
}

// ── Palette ───────────────────────────────────────────────────────────────

TEST_F(CanvasTest, PaletteListsRegistryComponentsAndCarriesTheDropPayload)
{
  ComponentPaletteModel palette(&registry);

  ASSERT_EQ(palette.rowCount(), 1);

  const QModelIndex index = palette.index(0, 0);
  EXPECT_EQ(palette.data(index, ComponentPaletteModel::ComponentIdRole).toString(),
            QStringLiteral("composer.test.component"));
  EXPECT_TRUE(palette.flags(index) & Qt::ItemIsDragEnabled);

  std::unique_ptr<QMimeData> mime(palette.mimeData({index}));
  ASSERT_NE(mime, nullptr);
  EXPECT_EQ(QString::fromUtf8(mime->data(QLatin1String(kComponentMimeType))),
            QStringLiteral("composer.test.component"));
}

// ── @from binding edges ───────────────────────────────────────────────────

namespace
{
  //! A provider + consumer whose "rating" argument is bound to the
  //! provider's "values" output.
  void loadBoundPair(CompositionDocument &document)
  {
    QString message;
    const QByteArray text = R"({
      "schema_version": "1.1",
      "components": [
        { "id": "prov",
          "info": { "component_info_id": "composer.test.component" } },
        { "id": "consumer",
          "info": { "component_info_id": "composer.test.component" },
          "arguments": { "rating": { "@from": {
              "component": "prov", "output": "values" } } } }
      ]
    })";
    ASSERT_TRUE(document.loadFromJson(text, message))
      << message.toStdString();
  }
}

TEST_F(CanvasTest, ABindingIsDrawnAsItsOwnKindOfEdge)
{
  loadBoundPair(document);

  // One binding edge, and no exchange edge pretending to be one.
  ASSERT_EQ(scene->bindingEdges().size(), 1);
  EXPECT_EQ(scene->edges().size(), 0);

  const BindingEdgeItem *edge = scene->bindingEdges().first();
  EXPECT_EQ(edge->binding().provider, "prov");
  EXPECT_EQ(edge->binding().component, "consumer");
  EXPECT_EQ(edge->binding().argument, "rating");
  EXPECT_NE(BindingEdgeItem::Type, ConnectionEdgeItem::Type);
}

TEST_F(CanvasTest, ABindingEdgeFollowsItsNodes)
{
  loadBoundPair(document);
  ASSERT_EQ(scene->bindingEdges().size(), 1);
  BindingEdgeItem *edge = scene->bindingEdges().first();

  const QRectF before = edge->boundingRect();
  document.moveComponent(QStringLiteral("prov"), QPointF(400.0, 250.0));
  const QRectF after = edge->boundingRect();

  EXPECT_NE(before, after) << "the edge did not follow the moved provider";
}

// ── Adapter factories in the palette and on the canvas (CONNECT B2) ─────────

#include <QSignalSpy>

TEST_F(CanvasTest, PaletteListsAdapterFactoriesUnderTheirOwnHeading)
{
  QString message;
  ASSERT_NE(registry.loadLibrary(
              fixturePath(QStringLiteral("testadapterfactory")), message),
            nullptr)
    << message.toStdString();

  ComponentPaletteModel palette(&registry);

  // Two sections: heading, model, heading, factory.
  ASSERT_EQ(palette.rowCount(), 4);
  EXPECT_TRUE(palette.data(palette.index(0, 0),
                           ComponentPaletteModel::IsHeaderRole).toBool());
  EXPECT_EQ(palette.data(palette.index(0, 0), Qt::DisplayRole).toString(),
            QStringLiteral("Model components"));
  EXPECT_EQ(palette.data(palette.index(1, 0),
                         ComponentPaletteModel::ComponentIdRole).toString(),
            QStringLiteral("composer.test.component"));
  EXPECT_TRUE(palette.data(palette.index(2, 0),
                           ComponentPaletteModel::IsHeaderRole).toBool());
  EXPECT_EQ(palette.data(palette.index(2, 0), Qt::DisplayRole).toString(),
            QStringLiteral("Adapter factories"));
  EXPECT_EQ(palette.data(palette.index(3, 0),
                         ComponentPaletteModel::ComponentIdRole).toString(),
            QStringLiteral("composer.test.adapterfactory"));

  // Headings are inert.
  EXPECT_EQ(palette.flags(palette.index(0, 0)), Qt::NoItemFlags);

  // A single-kind registry keeps the flat list it always had.
  ComponentRegistry plainRegistry;
  ASSERT_NE(plainRegistry.loadLibrary(
              fixturePath(QStringLiteral("testcomponent")), message),
            nullptr);
  ComponentPaletteModel flat(&plainRegistry);
  ASSERT_EQ(flat.rowCount(), 1);
  EXPECT_FALSE(flat.data(flat.index(0, 0),
                         ComponentPaletteModel::IsHeaderRole).toBool());
}

TEST_F(CanvasTest, AdapterFactoryRowsAreNotDraggable)
{
  QString message;
  ASSERT_NE(registry.loadLibrary(
              fixturePath(QStringLiteral("testadapterfactory")), message),
            nullptr)
    << message.toStdString();

  ComponentPaletteModel palette(&registry);
  ASSERT_EQ(palette.rowCount(), 4);

  const QModelIndex factoryRow = palette.index(3, 0);
  EXPECT_FALSE(palette.flags(factoryRow) & Qt::ItemIsDragEnabled);
  EXPECT_EQ(palette.mimeData({factoryRow}), nullptr)
    << "a factory row handed out a drop payload anyway";

  const QModelIndex modelRow = palette.index(1, 0);
  EXPECT_TRUE(palette.flags(modelRow) & Qt::ItemIsDragEnabled);
  std::unique_ptr<QMimeData> mime(palette.mimeData({modelRow}));
  ASSERT_NE(mime, nullptr);
  EXPECT_EQ(QString::fromUtf8(mime->data(QLatin1String(kComponentMimeType))),
            QStringLiteral("composer.test.component"));
}

TEST_F(CanvasTest, DroppingAFactoryOnTheCanvasLeavesTheDocumentUntouched)
{
  QString message;
  ASSERT_NE(registry.loadLibrary(
              fixturePath(QStringLiteral("testadapterfactory")), message),
            nullptr)
    << message.toStdString();

  QSignalSpy refused(scene.get(), &CompositionScene::componentRefused);

  EXPECT_TRUE(scene->addComponentAt(
                QStringLiteral("composer.test.adapterfactory"),
                QStringLiteral("factory"), QPointF(0, 0)).isEmpty());
  EXPECT_TRUE(document.componentIds().isEmpty());
  ASSERT_EQ(refused.count(), 1);
  EXPECT_TRUE(refused.first().first().toString().contains(
                QStringLiteral("attach to connections")))
    << refused.first().first().toString().toStdString();

  // A model component from the same registry still goes in.
  EXPECT_FALSE(scene->addComponentAt(
                 QStringLiteral("composer.test.component"),
                 QStringLiteral("solver"), QPointF(0, 0)).isEmpty());
  EXPECT_EQ(document.componentIds().size(), 1);
}

// ── Spliced adapter nodes (CONNECT C3) ──────────────────────────────────────

namespace
{
  //! A provider→consumer document whose connection carries a two-step chain.
  void loadAdaptedPair(CompositionDocument &document)
  {
    QString message;
    const QByteArray text = R"({
      "schema_version": "1.1",
      "components": [
        { "id": "prov",
          "info": { "component_info_id": "composer.test.component" } },
        { "id": "consumer",
          "info": { "component_info_id": "composer.test.component" } }
      ],
      "connections": [
        { "from": { "component": "prov", "output": "values",
                    "adapted_outputs": [
                      { "id": "scale" },
                      { "id": "shift" } ] },
          "to": { "component": "consumer", "input": "inflow" } }
      ]
    })";
    ASSERT_TRUE(document.loadFromJson(text, message)) << message.toStdString();
  }
}

TEST_F(CanvasTest, AnAdaptedConnectionGrowsASplicedAdapterNodePerChainStep)
{
  ASSERT_NO_FATAL_FAILURE(loadAdaptedPair(document));

  const QList<AdapterNodeItem *> adapters = scene->adapterNodes();
  ASSERT_EQ(adapters.size(), 2);
  EXPECT_EQ(adapters[0]->step().id, "scale");
  EXPECT_EQ(adapters[0]->stepIndex(), 0);
  EXPECT_EQ(adapters[1]->step().id, "shift");
  EXPECT_EQ(adapters[1]->stepIndex(), 1);

  // Still ONE edge: the connection keeps a single selectable identity.
  EXPECT_EQ(scene->edges().size(), 1);

  // Unplaced steps sit spread along the edge, not stacked on one point.
  EXPECT_NE(adapters[0]->pos(), adapters[1]->pos());
}

TEST_F(CanvasTest, TheEdgeRoutesThroughItsAdapterNodesInChainOrder)
{
  ASSERT_NO_FATAL_FAILURE(loadAdaptedPair(document));

  const QList<AdapterNodeItem *> adapters = scene->adapterNodes();
  ASSERT_EQ(adapters.size(), 2);
  ConnectionEdgeItem *edge = scene->edges().first();

  // The stroked shape passes through every adapter's anchors — the legs
  // really do route through the spliced nodes.
  for (AdapterNodeItem *adapter : adapters)
  {
    EXPECT_TRUE(edge->shape().contains(edge->mapFromScene(adapter->anchorIn())))
      << "the edge does not reach " << adapter->step().id << "'s inlet";
    EXPECT_TRUE(edge->shape().contains(edge->mapFromScene(adapter->anchorOut())))
      << "the edge does not leave " << adapter->step().id << "'s outlet";
  }

  // And it follows a node that moves.
  adapters[0]->setPos(adapters[0]->pos() + QPointF(0.0, 140.0));
  edge->refresh();
  EXPECT_TRUE(edge->shape().contains(edge->mapFromScene(adapters[0]->anchorIn())))
    << "the edge did not follow the moved adapter";
}

TEST_F(CanvasTest, DraggingAnAdapterNodePersistsItsPositionInTheSidecar)
{
  ASSERT_NO_FATAL_FAILURE(loadAdaptedPair(document));

  const QList<AdapterNodeItem *> adapters = scene->adapterNodes();
  ASSERT_EQ(adapters.size(), 2);
  AdapterNodeItem *adapter = adapters[1];
  const ConnectionSpec identity = adapter->connection();
  const QPointF target = adapter->pos() + QPointF(60.0, 90.0);

  adapter->setSelected(true);
  dragOnScene(scene.get(), adapter->pos(), target);

  const QList<QPointF> saved =
    document.presentation().adapterChain(identity);
  ASSERT_GT(saved.size(), 1);
  EXPECT_EQ(saved[1], target) << "the drag did not reach the sidecar";

  // Through the document means undoable.
  document.undoStack()->undo();
  EXPECT_NE(document.presentation().adapterChain(identity).value(1), target);

  // And a selected adapter survives a rebuild BY IDENTITY — in-place chain
  // edits rebuild the scene mid-interaction, and a selection held by item
  // pointer would be gone.
  adapter = scene->adapterNodes()[1];
  adapter->setSelected(true);
  scene->rebuild();
  bool reselected = false;
  for (AdapterNodeItem *rebuilt : scene->adapterNodes())
  {
    if (rebuilt->stepIndex() == 1 && rebuilt->isSelected())
    {
      reselected = true;
    }
  }
  EXPECT_TRUE(reselected) << "selection did not survive the rebuild";
}

// ── Adapter insertion through the menu seams (CONNECT C4) ───────────────────

namespace
{
  //! A resolvable provider→consumer pair joined values→inflow.
  void loadConnectedPair(CompositionDocument &document)
  {
    QString message;
    const QByteArray text = R"({
      "components": [
        { "id": "prov",
          "info": { "component_info_id": "composer.test.component" } },
        { "id": "consumer",
          "info": { "component_info_id": "composer.test.component" } }
      ],
      "connections": [
        { "from": { "component": "prov", "output": "values" },
          "to": { "component": "consumer", "input": "inflow" } }
      ]
    })";
    ASSERT_TRUE(document.loadFromJson(text, message)) << message.toStdString();
  }

  ConnectionSpec pairIdentity()
  {
    ConnectionSpec identity;
    identity.fromComponent = "prov";
    identity.output = "values";
    identity.toComponent = "consumer";
    identity.input = "inflow";
    return identity;
  }
}

TEST_F(CanvasTest, TheInsertMenuOffersOnlyAdaptersTheFactoriesReportAvailable)
{
  QString message;
  ASSERT_NE(registry.loadLibrary(
              fixturePath(QStringLiteral("testadapterfactory")), message),
            nullptr)
    << message.toStdString();
  ASSERT_NO_FATAL_FAILURE(loadConnectedPair(document));

  const QList<CompositionScene::AdapterOffering> offerings =
    scene->adapterOfferings(pairIdentity());

  QStringList ids;
  for (const CompositionScene::AdapterOffering &offering : offerings)
  {
    ids.append(offering.factoryId + QStringLiteral("/") + offering.adapterId);
  }

  // The SDK's factory rides along for every session; the standalone plugin
  // adds its own; and what a factory does NOT offer for this output — the
  // temporal adapter wants a time series, 'values' is a plain slab — never
  // shows up.
  EXPECT_TRUE(ids.contains(
    QStringLiteral("hydrocouple.sdk.adapters/linear_transform")))
    << ids.join(QStringLiteral(", ")).toStdString();
  EXPECT_TRUE(ids.contains(QStringLiteral("hydrocouple.sdk.adapters/relaxation")));
  EXPECT_TRUE(ids.contains(
    QStringLiteral("composer.test.adapterfactory/double_it")));
  EXPECT_FALSE(ids.join(QStringLiteral(",")).contains(
    QStringLiteral("temporal_interpolation")))
    << "an adapter the factory does not offer for this output was listed";
}

TEST_F(CanvasTest, InsertingAnAdapterThroughTheMenuMatchesTheHandWrittenDocument)
{
  ASSERT_NO_FATAL_FAILURE(loadConnectedPair(document));

  ASSERT_TRUE(scene->insertAdapter(pairIdentity(), 0,
                                   QStringLiteral("hydrocouple.sdk.adapters"),
                                   QStringLiteral("linear_transform")));

  const auto &chain = document.spec().connections[0].adaptedOutputs;
  ASSERT_EQ(chain.size(), 1u);
  EXPECT_EQ(chain[0].id, "linear_transform");
  EXPECT_EQ(chain[0].factory, "hydrocouple.sdk.adapters");

  // The scratch instance captured the argument keys and their defaults, so
  // the inspector has editable payloads from the very first save.
  ASSERT_TRUE(chain[0].arguments.contains("multiplier"))
    << chain[0].arguments.dump();
  EXPECT_EQ(chain[0].arguments["multiplier"]["values"][0].get<double>(), 1.0);
  ASSERT_TRUE(chain[0].arguments.contains("offset"));
  EXPECT_EQ(chain[0].arguments["offset"]["values"][0].get<double>(), 0.0);

  // And the scratch is GONE: the live output's non-owning registry holds
  // nothing, or the next refresh would chase a destroyed adapter.
  HydroCouple::IModelComponent *prov =
    instances->instance(QStringLiteral("prov"));
  ASSERT_NE(prov, nullptr);
  for (HydroCouple::IOutput *output : prov->outputs())
  {
    if (output && output->id() == "values")
    {
      EXPECT_TRUE(output->adaptedOutputs().empty())
        << "the scratch adapted output was left registered";
    }
  }

  // The canvas grew the spliced node.
  ASSERT_EQ(scene->adapterNodes().size(), 1);
  EXPECT_EQ(scene->adapterNodes().first()->step().id, "linear_transform");
}

TEST_F(CanvasTest, RemovingAnAdapterNodeShortensTheChainByOne)
{
  ASSERT_NO_FATAL_FAILURE(loadAdaptedPair(document));
  ASSERT_EQ(scene->adapterNodes().size(), 2);

  // What the node's Remove adapter action performs.
  const AdapterNodeItem *first = scene->adapterNodes().first();
  ASSERT_TRUE(document.removeConnectionAdapter(first->connection(),
                                               first->stepIndex()));

  ASSERT_EQ(scene->adapterNodes().size(), 1);
  EXPECT_EQ(scene->adapterNodes().first()->step().id, "shift");
  EXPECT_EQ(scene->adapterNodes().first()->stepIndex(), 0)
    << "the surviving step did not renumber";
}
