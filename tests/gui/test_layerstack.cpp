/*!
 * \file   test_layerstack.cpp
 * \brief  Phase C1b verification — the layer stack and the layer tree.
 *
 * The stack is the single owner of the session's layers, so these tests care
 * about two things a weaker suite would miss: that ownership is real (a
 * removed layer is destroyed, not leaked), and that every change which alters
 * the drawn result reaches the one signal a view subscribes to. A view that
 * missed a change would show a stale map with no error anywhere.
 */

#include "core/composerapplication.h"
#include "map/layerstackmodel.h"
#include "layers/meshlayer.h"
#include "map/maplayer.h"
#include "probelayer.h"
#include "render/layerstyle.h"
#include "ui/panels/layertreepanel.h"

#include <gtest/gtest.h>

#include <QPointer>
#include <QSignalSpy>
#include <QToolButton>
#include <QTreeView>

using namespace HydroCouple::Composer;
using HydroCouple::Composer::Testing::ProbeLayer;

namespace
{
  class LayerStackTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_layerstack";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      ProbeLayer *addProbe(LayerStackModel &stack, const QString &name)
      {
        auto *layer = new ProbeLayer(name, QRectF(0.0, 0.0, 10.0, 10.0));
        stack.addLayer(layer);

        return layer;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *LayerStackTest::s_app = nullptr;
}

TEST_F(LayerStackTest, AddsLayersOnTopOfTheStack)
{
  LayerStackModel stack;

  ProbeLayer *first = addProbe(stack, QStringLiteral("first"));
  ProbeLayer *second = addProbe(stack, QStringLiteral("second"));

  ASSERT_EQ(stack.rowCount(), 2);

  // Row 0 is the top of the stack, so the most recently added layer is the
  // one the user sees at the top of the tree.
  EXPECT_EQ(stack.layerAt(0), second);
  EXPECT_EQ(stack.layerAt(1), first);
  EXPECT_EQ(stack.rowOf(first), 1);
}

TEST_F(LayerStackTest, RenderOrderIsTheStackUpsideDown)
{
  LayerStackModel stack;

  ProbeLayer *bottom = addProbe(stack, QStringLiteral("bottom"));
  ProbeLayer *top = addProbe(stack, QStringLiteral("top"));

  const QVector<MapLayer *> order = stack.renderOrder();

  ASSERT_EQ(order.size(), 2);
  EXPECT_EQ(order.at(0), bottom) << "the bottom layer must be drawn first";
  EXPECT_EQ(order.at(1), top);
}

TEST_F(LayerStackTest, RefusesTheSameLayerTwice)
{
  LayerStackModel stack;

  ProbeLayer *layer = addProbe(stack, QStringLiteral("once"));

  // Adding a layer a second time would give the stack two rows sharing one
  // object, and destroying the row would leave the other dangling.
  EXPECT_EQ(stack.addLayer(layer), -1);
  EXPECT_EQ(stack.rowCount(), 1);
  EXPECT_EQ(stack.addLayer(nullptr), -1);
}

TEST_F(LayerStackTest, ExposesNameVisibilityAndCrsThroughRoles)
{
  LayerStackModel stack;
  ProbeLayer *layer = addProbe(stack, QStringLiteral("probe"));

  const QModelIndex index = stack.index(0, 0);

  EXPECT_EQ(index.data(Qt::DisplayRole).toString(), QStringLiteral("probe"));
  EXPECT_EQ(index.data(Qt::CheckStateRole).toInt(), Qt::Checked);
  EXPECT_EQ(index.data(LayerStackModel::LayerIdRole).toString(), layer->id());
  EXPECT_DOUBLE_EQ(index.data(LayerStackModel::OpacityRole).toDouble(), 1.0);

  // No CRS set yet, and a layer without one must say so rather than invent a
  // default that would silently misplace it on the map.
  EXPECT_TRUE(
    index.data(LayerStackModel::CrsDescriptionRole).toString().isEmpty());
}

TEST_F(LayerStackTest, CheckStateEditsVisibilityAndAsksForARepaint)
{
  LayerStackModel stack;
  ProbeLayer *layer = addProbe(stack, QStringLiteral("probe"));

  QSignalSpy renderSpy(&stack, &LayerStackModel::renderChanged);
  QSignalSpy dataSpy(&stack, &LayerStackModel::dataChanged);

  ASSERT_TRUE(stack.setData(stack.index(0, 0), Qt::Unchecked,
                            Qt::CheckStateRole));

  EXPECT_FALSE(layer->isVisible());
  EXPECT_EQ(renderSpy.count(), 1);
  EXPECT_EQ(dataSpy.count(), 1);
}

TEST_F(LayerStackTest, ReportsChangesMadeOnTheLayerItself)
{
  LayerStackModel stack;
  ProbeLayer *layer = addProbe(stack, QStringLiteral("probe"));

  QSignalSpy renderSpy(&stack, &LayerStackModel::renderChanged);
  QSignalSpy dataSpy(&stack, &LayerStackModel::dataChanged);

  // The point of the model owning the layers is that an edit made anywhere —
  // a dialog, a script, a component — reaches the map. Going through setData
  // only would leave that untested.
  layer->setOpacity(0.4);
  layer->setName(QStringLiteral("renamed"));
  layer->setExtent(QRectF(0.0, 0.0, 50.0, 50.0));

  EXPECT_EQ(stack.index(0, 0).data(Qt::DisplayRole).toString(),
            QStringLiteral("renamed"));
  EXPECT_EQ(dataSpy.count(), 2) << "opacity and name each refresh the row";
  EXPECT_EQ(renderSpy.count(), 2) << "opacity and extent each need a repaint";
}

TEST_F(LayerStackTest, RenamesThroughTheModelButRefusesAnEmptyName)
{
  LayerStackModel stack;
  ProbeLayer *layer = addProbe(stack, QStringLiteral("probe"));

  EXPECT_TRUE(stack.setData(stack.index(0, 0), QStringLiteral("  renamed  "),
                            Qt::EditRole));
  EXPECT_EQ(layer->name(), QStringLiteral("renamed"));

  EXPECT_FALSE(
    stack.setData(stack.index(0, 0), QStringLiteral("   "), Qt::EditRole));
  EXPECT_EQ(layer->name(), QStringLiteral("renamed"));
}

TEST_F(LayerStackTest, ClampsOpacityToWhatAPainterCanUse)
{
  LayerStackModel stack;
  ProbeLayer *layer = addProbe(stack, QStringLiteral("probe"));

  layer->setOpacity(4.0);
  EXPECT_DOUBLE_EQ(layer->opacity(), 1.0);

  layer->setOpacity(-1.0);
  EXPECT_DOUBLE_EQ(layer->opacity(), 0.0);
}

TEST_F(LayerStackTest, MovesRowsAndRejectsMovesThatChangeNothing)
{
  LayerStackModel stack;

  ProbeLayer *a = addProbe(stack, QStringLiteral("a"));
  ProbeLayer *b = addProbe(stack, QStringLiteral("b"));
  ProbeLayer *c = addProbe(stack, QStringLiteral("c"));

  // Stack is c, b, a from the top.
  ASSERT_EQ(stack.layerAt(0), c);

  QSignalSpy renderSpy(&stack, &LayerStackModel::renderChanged);

  // Send the top layer to the bottom.
  ASSERT_TRUE(stack.moveRows(QModelIndex(), 0, 1, QModelIndex(), 3));

  EXPECT_EQ(stack.layerAt(0), b);
  EXPECT_EQ(stack.layerAt(1), a);
  EXPECT_EQ(stack.layerAt(2), c);
  EXPECT_EQ(renderSpy.count(), 1);

  // A move onto itself is what dropping a row back where it started produces;
  // Qt asserts on the corresponding beginMoveRows, so it must be refused here.
  EXPECT_FALSE(stack.moveRows(QModelIndex(), 1, 1, QModelIndex(), 1));
  EXPECT_FALSE(stack.moveRows(QModelIndex(), 1, 1, QModelIndex(), 2));
  EXPECT_FALSE(stack.moveRows(QModelIndex(), 0, 1, QModelIndex(), 9));
  EXPECT_EQ(renderSpy.count(), 1);
}

TEST_F(LayerStackTest, OwnsItsLayersAndDestroysWhatItRemoves)
{
  LayerStackModel stack;

  QPointer<MapLayer> kept = addProbe(stack, QStringLiteral("kept"));
  QPointer<MapLayer> removed = addProbe(stack, QStringLiteral("removed"));

  ASSERT_EQ(removed->parent(), &stack) << "the stack must adopt the layer";

  ASSERT_TRUE(stack.removeLayer(stack.rowOf(removed)));

  EXPECT_TRUE(removed.isNull()) << "removing a layer must destroy it";
  EXPECT_FALSE(kept.isNull());
  EXPECT_EQ(stack.rowCount(), 1);
}

TEST_F(LayerStackTest, ClearsEverythingAtOnce)
{
  LayerStackModel stack;

  QPointer<MapLayer> first = addProbe(stack, QStringLiteral("a"));
  QPointer<MapLayer> second = addProbe(stack, QStringLiteral("b"));

  stack.clear();

  EXPECT_EQ(stack.rowCount(), 0);
  EXPECT_TRUE(first.isNull());
  EXPECT_TRUE(second.isNull());
}

TEST_F(LayerStackTest, FindsLayersByIdentityNotByName)
{
  LayerStackModel stack;

  // Two layers may legitimately share a name — the same file opened twice —
  // so lookup keys on the id.
  ProbeLayer *first = addProbe(stack, QStringLiteral("same"));
  ProbeLayer *second = addProbe(stack, QStringLiteral("same"));

  ASSERT_NE(first->id(), second->id());
  EXPECT_EQ(stack.layerById(first->id()), first);
  EXPECT_EQ(stack.layerById(second->id()), second);
  EXPECT_EQ(stack.layerById(QStringLiteral("nope")), nullptr);
}

// ── Layer tree panel ────────────────────────────────────────────────────────

TEST_F(LayerStackTest, TreePanelMovesTheSelectedLayerAndFollowsIt)
{
  LayerStackModel stack;
  LayerTreePanel panel;
  panel.setModel(&stack);

  ProbeLayer *bottom = addProbe(stack, QStringLiteral("bottom"));
  ProbeLayer *top = addProbe(stack, QStringLiteral("top"));

  panel.view()->setCurrentIndex(stack.index(0, 0));
  ASSERT_EQ(panel.currentLayer(), top);

  panel.moveCurrentDown();

  EXPECT_EQ(stack.layerAt(0), bottom);
  EXPECT_EQ(stack.layerAt(1), top);
  EXPECT_EQ(panel.currentLayer(), top)
    << "the selection must follow the layer, not stay on the row";

  panel.moveCurrentUp();

  EXPECT_EQ(stack.layerAt(0), top);
  EXPECT_EQ(panel.currentLayer(), top);
}

TEST_F(LayerStackTest, TreePanelDoesNothingWithoutASelection)
{
  LayerStackModel stack;
  LayerTreePanel panel;
  panel.setModel(&stack);

  addProbe(stack, QStringLiteral("only"));
  panel.view()->setCurrentIndex(QModelIndex());

  panel.moveCurrentUp();
  panel.moveCurrentDown();
  panel.removeCurrent();

  EXPECT_EQ(stack.rowCount(), 1);
}

TEST_F(LayerStackTest, TreePanelRemovesAndAsksToFrameALayer)
{
  LayerStackModel stack;
  LayerTreePanel panel;
  panel.setModel(&stack);

  QSignalSpy zoomSpy(&panel, &LayerTreePanel::zoomToLayerRequested);

  QPointer<MapLayer> layer = addProbe(stack, QStringLiteral("probe"));
  panel.view()->setCurrentIndex(stack.index(0, 0));

  auto *zoomButton = panel.findChild<QToolButton *>(
    QStringLiteral("layerZoomButton"));
  ASSERT_NE(zoomButton, nullptr);
  ASSERT_TRUE(zoomButton->isEnabled())
    << "selecting a layer must enable the panel's actions";

  zoomButton->click();
  EXPECT_EQ(zoomSpy.count(), 1);

  panel.removeCurrent();

  EXPECT_TRUE(layer.isNull());
  EXPECT_EQ(stack.rowCount(), 0);
}

// ── Legend rows (C1c) ───────────────────────────────────────────────────────

namespace
{
  //! A styled layer with two categories, ready to produce legend rows.
  Testing::ProbeFeatureLayer *addStyledProbe(LayerStackModel &stack,
                                             const QString &name)
  {
    auto *layer = new Testing::ProbeFeatureLayer(name);

    AttributeField kind;
    kind.name = QStringLiteral("kind");
    kind.type = QMetaType::QString;
    layer->declareField(kind);

    layer->addFeature(QPointF(0.0, 0.0),
                      {{QStringLiteral("kind"), QStringLiteral("pipe")}});
    layer->addFeature(QPointF(10.0, 10.0),
                      {{QStringLiteral("kind"), QStringLiteral("weir")}});

    layer->styleRef().setMode(StyleMode::Categorized);
    layer->styleRef().setAttribute(QStringLiteral("kind"));

    stack.addLayer(layer);
    layer->restyle();

    return layer;
  }
}

TEST_F(LayerStackTest, LayersWithoutAStyleHaveNoLegendRows)
{
  LayerStackModel stack;
  addProbe(stack, QStringLiteral("plain"));

  EXPECT_EQ(stack.rowCount(stack.index(0, 0)), 0);
}

TEST_F(LayerStackTest, LegendRowsHangUnderTheirLayer)
{
  LayerStackModel stack;
  Testing::ProbeFeatureLayer *layer =
    addStyledProbe(stack, QStringLiteral("styled"));

  const QModelIndex layerIndex = stack.index(0, 0);
  ASSERT_EQ(stack.rowCount(layerIndex), 2);

  const QModelIndex first = stack.index(0, 0, layerIndex);
  ASSERT_TRUE(first.isValid());

  EXPECT_EQ(first.data(Qt::DisplayRole).toString(), QStringLiteral("pipe"));
  EXPECT_TRUE(first.data(LayerStackModel::IsLegendRole).toBool());
  EXPECT_TRUE(first.data(Qt::DecorationRole).isValid())
    << "a legend row without a swatch is just text";

  // The way back up must land on the owning layer, or a view cannot draw the
  // tree at all.
  EXPECT_EQ(stack.parent(first), layerIndex);
  EXPECT_EQ(stack.layerFor(first), layer);
  EXPECT_TRUE(LayerStackModel::isLegendIndex(first));

  // A legend row is a leaf.
  EXPECT_EQ(stack.rowCount(first), 0);
  EXPECT_FALSE(stack.index(0, 0, first).isValid());
}

TEST_F(LayerStackTest, LayerRowsRemainRootedAndDistinctFromLegendRows)
{
  LayerStackModel stack;
  addStyledProbe(stack, QStringLiteral("a"));
  addStyledProbe(stack, QStringLiteral("b"));

  ASSERT_EQ(stack.rowCount(), 2);

  for (int row = 0; row < 2; ++row)
  {
    const QModelIndex index = stack.index(row, 0);

    EXPECT_FALSE(stack.parent(index).isValid())
      << "a layer row claimed a parent";
    EXPECT_FALSE(LayerStackModel::isLegendIndex(index));
    EXPECT_EQ(stack.rowCount(index), 2);
  }
}

TEST_F(LayerStackTest, UncheckingALegendRowHidesThatClassOnTheMap)
{
  LayerStackModel stack;
  Testing::ProbeFeatureLayer *layer =
    addStyledProbe(stack, QStringLiteral("styled"));

  const QModelIndex legendRow = stack.index(0, 0, stack.index(0, 0));
  ASSERT_TRUE(legendRow.isValid());
  ASSERT_EQ(legendRow.data(Qt::CheckStateRole).toInt(), Qt::Checked);

  QSignalSpy renderSpy(&stack, &LayerStackModel::renderChanged);

  ASSERT_TRUE(stack.setData(legendRow, Qt::Unchecked, Qt::CheckStateRole));

  EXPECT_EQ(legendRow.data(Qt::CheckStateRole).toInt(), Qt::Unchecked);
  EXPECT_EQ(renderSpy.count(), 1) << "the map was not told to redraw";

  // And the style really declines to draw it — the legend edit reached the
  // thing that paints, not just the row that displays.
  EXPECT_FALSE(layer->style()->colorFor(*layer, 0).isValid());
  EXPECT_TRUE(layer->style()->colorFor(*layer, 1).isValid());
}

TEST_F(LayerStackTest, LegendRowsAreCheckableButNotDraggableOrEditable)
{
  LayerStackModel stack;
  addStyledProbe(stack, QStringLiteral("styled"));

  const QModelIndex legendRow = stack.index(0, 0, stack.index(0, 0));
  const Qt::ItemFlags flags = stack.flags(legendRow);

  EXPECT_TRUE(flags.testFlag(Qt::ItemIsUserCheckable));

  // Dragging a class or renaming it would mean nothing, and Qt would happily
  // let the user try.
  EXPECT_FALSE(flags.testFlag(Qt::ItemIsDragEnabled));
  EXPECT_FALSE(flags.testFlag(Qt::ItemIsEditable));
}

TEST_F(LayerStackTest, RestylingAnnouncesLegendRowsAppearingAndDisappearing)
{
  LayerStackModel stack;
  Testing::ProbeFeatureLayer *layer =
    addStyledProbe(stack, QStringLiteral("styled"));

  const QModelIndex layerIndex = stack.index(0, 0);
  ASSERT_EQ(stack.rowCount(layerIndex), 2);

  QSignalSpy insertSpy(&stack, &LayerStackModel::rowsInserted);
  QSignalSpy removeSpy(&stack, &LayerStackModel::rowsRemoved);

  // Three categories where there were two: a view told only "the data
  // changed" would keep addressing two rows and never show the third.
  layer->addFeature(QPointF(20.0, 20.0),
                    {{QStringLiteral("kind"), QStringLiteral("orifice")}});
  layer->restyle();

  EXPECT_EQ(stack.rowCount(layerIndex), 3);
  EXPECT_EQ(insertSpy.count(), 1);

  // And back down again: switching to a single symbol removes every class.
  layer->styleRef().setMode(StyleMode::Single);
  layer->restyle();

  EXPECT_EQ(stack.rowCount(layerIndex), 0);
  EXPECT_EQ(removeSpy.count(), 1);
}

TEST_F(LayerStackTest, TreePanelActsOnLayersNotOnLegendRows)
{
  LayerStackModel stack;
  LayerTreePanel panel;
  panel.setModel(&stack);

  addStyledProbe(stack, QStringLiteral("styled"));

  const QModelIndex legendRow = stack.index(0, 0, stack.index(0, 0));
  panel.view()->setCurrentIndex(legendRow);

  // Selecting a class must not arm the layer commands: "remove" would then
  // delete the whole layer the user had merely expanded.
  EXPECT_EQ(panel.currentLayer(), nullptr);

  panel.removeCurrent();
  EXPECT_EQ(stack.rowCount(), 1) << "removing a class deleted the layer";

  auto *removeButton = panel.findChild<QToolButton *>(
    QStringLiteral("layerRemoveButton"));
  ASSERT_NE(removeButton, nullptr);
  EXPECT_FALSE(removeButton->isEnabled());
}

// ── What the tree can say about 3D (coherence plan V4) ────────────────────

namespace
{
  //! A square split into two triangles, with elevations.
  HydroCouple::SDK::IO::MeshDefinition twoTriangleSquare()
  {
    HydroCouple::SDK::IO::MeshDefinition mesh;
    mesh.meshName = "square";
    mesh.nodeX = {0.0, 10.0, 10.0, 0.0};
    mesh.nodeY = {0.0, 0.0, 10.0, 10.0};
    mesh.nodeZ = {0.0, 0.0, 5.0, 5.0};
    mesh.faceNodeOffsets = {0, 3, 6};
    mesh.faceNodes = {0, 1, 2, 0, 2, 3};

    return mesh;
  }
}

TEST(LayerStackModel3d, TheModelTellsApartNoFormFromKeptOut)
{
  LayerStackModel model;

  QString message;
  std::unique_ptr<MeshLayer> mesh = MeshLayer::create(
    QStringLiteral("surface"), twoTriangleSquare(), MeshEntity::Face,
    message);
  ASSERT_TRUE(mesh) << message.toStdString();

  MeshLayer *layer = mesh.get();
  ASSERT_GE(model.addLayer(mesh.release()), 0);

  const QModelIndex index = model.index(0, 0);

  // A mesh has a 3D form and joins the scene by default.
  EXPECT_TRUE(model.data(index, LayerStackModel::HasSceneFormRole).toBool());
  EXPECT_TRUE(model.data(index, LayerStackModel::ShownIn3DRole).toBool());
  EXPECT_TRUE(model.data(index, Qt::ToolTipRole).toString().contains(
    QStringLiteral("Shown in 3D")));

  // Kept out: still has the form, no longer joins.
  ASSERT_TRUE(model.setData(index, false, LayerStackModel::ShownIn3DRole));
  EXPECT_FALSE(layer->isShownIn3D());
  EXPECT_TRUE(model.data(index, LayerStackModel::HasSceneFormRole).toBool());
  EXPECT_TRUE(model.data(index, Qt::ToolTipRole).toString().contains(
    QStringLiteral("Kept out of 3D")));
}

TEST(LayerStackModel3d, ALayerWithNoSceneFormSaysSoAndCannotBeToggledIntoOne)
{
  LayerStackModel model;

  auto *probe = new Testing::ProbeLayer(QStringLiteral("plain"),
                                        QRectF(0, 0, 10, 10));
  ASSERT_GE(model.addLayer(probe), 0);

  const QModelIndex index = model.index(0, 0);

  EXPECT_FALSE(model.data(index, LayerStackModel::HasSceneFormRole).toBool());
  EXPECT_TRUE(model.data(index, Qt::ToolTipRole).toString().contains(
    QStringLiteral("No 3D form")))
    << model.data(index, Qt::ToolTipRole).toString().toStdString();

  // The toggle is accepted and inert -- and the form answer does not move,
  // because whether a 3D form exists is a fact about the layer, not a
  // setting.
  ASSERT_TRUE(model.setData(index, true, LayerStackModel::ShownIn3DRole));
  EXPECT_FALSE(model.data(index, LayerStackModel::HasSceneFormRole).toBool());
}

// The offscreen render harness rebuilds per frame, so a scene test cannot
// see a missing change signal -- but the live renderer caches batches and
// rebuilds on this signal, so without it the scene draws the old stack until
// something else repaints. The signal itself is the contract.
TEST(LayerStackModel3d, TogglingShownIn3dAnnouncesItselfExactlyOnce)
{
  QString message;
  std::unique_ptr<MeshLayer> mesh = MeshLayer::create(
    QStringLiteral("surface"), twoTriangleSquare(), MeshEntity::Face,
    message);
  ASSERT_TRUE(mesh) << message.toStdString();

  QSignalSpy announced(mesh.get(), &MapLayer::appearanceChanged);

  mesh->setShownIn3D(false);
  EXPECT_EQ(announced.count(), 1)
    << "the scene draws the old stack until something else repaints";

  // Setting the value it already has says nothing, like setVisible.
  mesh->setShownIn3D(false);
  EXPECT_EQ(announced.count(), 1);

  mesh->setShownIn3D(true);
  EXPECT_EQ(announced.count(), 2);
}
