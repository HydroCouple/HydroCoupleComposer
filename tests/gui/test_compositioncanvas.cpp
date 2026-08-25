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
