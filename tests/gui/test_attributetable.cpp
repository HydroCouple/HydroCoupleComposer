/*!
 * \file   test_attributetable.cpp
 * \brief  C4c — the attribute table, and the selection it shares.
 *
 * The plan asks that selecting in the map highlights the row and selecting
 * the row highlights the feature. Both directions are the same wire read from
 * opposite ends, so the case that matters most is not either direction on its
 * own — it is that setting one does not echo back and change the other.
 *
 * The panel is driven through its real widgets, because the interesting
 * behaviour is in the wiring rather than in the model: a table model that
 * reports the right rows while nothing connects them to the layer would pass
 * any test written against the model alone.
 */

#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "ui/panels/attributetablemodel.h"
#include "ui/panels/attributetablepanel.h"
#include "vectorprobe.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QTableView>
#include <QTest>

#include <memory>

using namespace HydroCouple::Composer;
using HydroCouple::Composer::Testing::VectorProbe;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  //! Two triangles, so a mesh layer has something to tabulate.
  MeshDefinition pair()
  {
    MeshDefinition mesh;
    mesh.meshName = "pair";
    mesh.nodeX = { 0.0, 1.0, 1.0, 0.0 };
    mesh.nodeY = { 0.0, 0.0, 1.0, 1.0 };
    mesh.faceNodeOffsets = { 0, 3, 6 };
    mesh.faceNodes = { 0, 1, 2, 0, 2, 3 };

    return mesh;
  }

  //! A layer with no attributes at all, to stand in for a basemap.
  class Backdrop : public MapLayer
  {
    public:
      Backdrop() : MapLayer(QStringLiteral("backdrop")) {}

      [[nodiscard]] QRectF extent() const override
      {
        return QRectF(0.0, 0.0, 1.0, 1.0);
      }

      void render(QPainter &, const MapTransform &) override {}
  };

  class AttributeTableTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!QApplication::instance())
        {
          static int argc = 1;
          static char name[] = "test_attributetable";
          static char *argv[] = { name, nullptr };
          s_app = new QApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        m_stack = std::make_unique<LayerStackModel>();

        m_conduits = new VectorProbe(QStringLiteral("conduits"));
        m_conduits->addLine({ { 0.0, 0.0 }, { 10.0, 0.0 } },
                            QStringLiteral("C1"));
        m_conduits->addLine({ { 0.0, 5.0 }, { 10.0, 5.0 } },
                            QStringLiteral("C2"));
        m_conduits->addLine({ { 0.0, 9.0 }, { 10.0, 9.0 } },
                            QStringLiteral("C3"));
        ASSERT_GE(m_stack->addLayer(m_conduits), 0);

        m_panel = std::make_unique<AttributeTablePanel>();
        m_panel->resize(600, 300);
        m_panel->show();
        m_panel->setModel(m_stack.get());

        QApplication::processEvents();

        m_chooser = m_panel->findChild<QComboBox *>(
          QStringLiteral("attributeLayerChooser"));
        ASSERT_NE(m_chooser, nullptr);
      }

      void TearDown() override
      {
        m_panel.reset();
        m_stack.reset();
      }

      static QApplication *s_app;

      std::unique_ptr<LayerStackModel> m_stack;
      std::unique_ptr<AttributeTablePanel> m_panel;
      VectorProbe *m_conduits = nullptr;
      QComboBox *m_chooser = nullptr;
  };

  QApplication *AttributeTableTest::s_app = nullptr;

  // ── the model ───────────────────────────────────────────────────────────

  TEST_F(AttributeTableTest, RowsAreFeaturesAndColumnsAreFields)
  {
    QAbstractItemModel *table = m_panel->view()->model();

    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->rowCount(), 3);
    EXPECT_EQ(table->columnCount(), 1);
    EXPECT_EQ(table->data(table->index(0, 0)).toString(),
              QStringLiteral("C1"));
    EXPECT_EQ(table->data(table->index(2, 0)).toString(),
              QStringLiteral("C3"));
  }

  TEST_F(AttributeTableTest, ARowNumberIsAFeatureIndex)
  {
    // Not a decoration: it is the same number picking returns and the
    // selection speaks in, which is what lets a row and a feature refer to
    // each other with no lookup between them.
    QAbstractItemModel *table = m_panel->view()->model();

    EXPECT_EQ(table->headerData(0, Qt::Vertical).toInt(), 0);
    EXPECT_EQ(table->headerData(2, Qt::Vertical).toInt(), 2);
  }

  TEST_F(AttributeTableTest, ACellHasNoChildren)
  {
    // A table model that answers for child indices makes a view descend into
    // every cell looking for rows under it, which is a hang rather than a
    // wrong number.
    QAbstractItemModel *table = m_panel->view()->model();

    const QModelIndex cell = table->index(0, 0);

    ASSERT_TRUE(cell.isValid());
    EXPECT_EQ(table->rowCount(cell), 0);
    EXPECT_EQ(table->columnCount(cell), 0);
  }

  TEST_F(AttributeTableTest, AColumnCarriesItsUnit)
  {
    AttributeTableModel model;

    QString message;
    const std::unique_ptr<MeshLayer> mesh = MeshLayer::create(
      QStringLiteral("mesh"), pair(), MeshEntity::Face, message);
    ASSERT_TRUE(mesh) << message.toStdString();

    ASSERT_TRUE(mesh->setValues(QStringLiteral("depth"), { 1.5, 2.5 }));

    model.setLayer(mesh.get());

    ASSERT_EQ(model.rowCount(), 2);

    bool found = false;

    for (int column = 0; column < model.columnCount(); ++column)
    {
      if (model.headerData(column, Qt::Horizontal)
            .toString()
            .contains(QStringLiteral("depth")))
      {
        found = true;
        EXPECT_DOUBLE_EQ(model.data(model.index(1, column)).toDouble(), 2.5);
      }
    }

    EXPECT_TRUE(found) << "the values attached to the mesh have no column";
  }

  TEST_F(AttributeTableTest, ALayerWithoutAttributesIsAnEmptyTable)
  {
    AttributeTableModel model;
    Backdrop backdrop;

    model.setLayer(&backdrop);

    // An answer, not a refusal: "this layer has no attributes" is a thing a
    // table can say.
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(model.columnCount(), 0);
    EXPECT_EQ(model.layer(), &backdrop);
  }

  TEST_F(AttributeTableTest, ValuesAreReadThroughRatherThanCopied)
  {
    // A results layer's values change while it is being watched, and nothing
    // signals it. Reading through means the table is never stale.
    QString message;
    const std::unique_ptr<MeshLayer> mesh = MeshLayer::create(
      QStringLiteral("mesh"), pair(), MeshEntity::Face, message);
    ASSERT_TRUE(mesh) << message.toStdString();

    AttributeTableModel model;
    ASSERT_TRUE(mesh->setValues(QStringLiteral("depth"), { 1.0, 2.0 }));
    model.setLayer(mesh.get());

    int column = -1;

    for (int i = 0; i < model.columnCount(); ++i)
    {
      if (model.headerData(i, Qt::Horizontal)
            .toString()
            .contains(QStringLiteral("depth")))
      {
        column = i;
      }
    }

    ASSERT_GE(column, 0);
    ASSERT_DOUBLE_EQ(model.data(model.index(0, column)).toDouble(), 1.0);

    ASSERT_TRUE(mesh->setValues(QStringLiteral("depth"), { 9.0, 8.0 }));

    EXPECT_DOUBLE_EQ(model.data(model.index(0, column)).toDouble(), 9.0)
      << "the table is showing values the layer no longer holds";
  }

  // ── the chooser ─────────────────────────────────────────────────────────

  TEST_F(AttributeTableTest, OnlyLayersWithAttributesAreOffered)
  {
    ASSERT_GE(m_stack->addLayer(new Backdrop), 0);
    QApplication::processEvents();

    EXPECT_EQ(m_chooser->count(), 1)
      << "a layer with no table was offered one";
    EXPECT_EQ(m_chooser->itemText(0), QStringLiteral("conduits"));
  }

  TEST_F(AttributeTableTest, AddingALayerOffersIt)
  {
    auto *nodes = new VectorProbe(QStringLiteral("nodes"));
    nodes->addPoint({ 1.0, 1.0 }, QStringLiteral("J1"));
    ASSERT_GE(m_stack->addLayer(nodes), 0);

    QApplication::processEvents();

    EXPECT_EQ(m_chooser->count(), 2);
    EXPECT_GE(m_chooser->findText(QStringLiteral("nodes")), 0);
  }

  TEST_F(AttributeTableTest, ChoosingALayerShowsIt)
  {
    auto *nodes = new VectorProbe(QStringLiteral("nodes"));
    nodes->addPoint({ 1.0, 1.0 }, QStringLiteral("J1"));
    nodes->addPoint({ 2.0, 2.0 }, QStringLiteral("J2"));
    ASSERT_GE(m_stack->addLayer(nodes), 0);

    QApplication::processEvents();

    m_panel->showLayer(nodes);

    EXPECT_EQ(m_panel->currentLayer(), nodes);
    EXPECT_EQ(m_panel->view()->model()->rowCount(), 2);
  }

  // ── the selection, both ways ────────────────────────────────────────────

  TEST_F(AttributeTableTest, SelectingAFeatureHighlightsItsRow)
  {
    m_stack->selectOnly(m_conduits, 1);
    QApplication::processEvents();

    const QModelIndexList rows =
      m_panel->view()->selectionModel()->selectedRows();

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows.first().row(), 1);
  }

  TEST_F(AttributeTableTest, SelectingARowSelectsTheFeature)
  {
    m_panel->view()->selectRow(2);
    QApplication::processEvents();

    EXPECT_EQ(m_conduits->selection(), QSet<int>({ 2 }));
  }

  TEST_F(AttributeTableTest, TheTwoDirectionsDoNotEchoEachOther)
  {
    // The case that a test of either direction alone cannot see. Both are the
    // same wire read from opposite ends, so a change at one end that is
    // copied to the other and copied back arrives at a different answer than
    // the one that was asked for.
    m_stack->selectOnly(m_conduits, 0);
    QApplication::processEvents();

    ASSERT_EQ(m_panel->view()->selectionModel()->selectedRows().size(), 1);
    ASSERT_EQ(m_conduits->selection(), QSet<int>({ 0 }));

    m_panel->view()->selectRow(2);
    QApplication::processEvents();

    EXPECT_EQ(m_conduits->selection(), QSet<int>({ 2 }));

    const QModelIndexList rows =
      m_panel->view()->selectionModel()->selectedRows();

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows.first().row(), 2)
      << "the row moved back to where the layer's selection had been";
  }

  TEST_F(AttributeTableTest, ShowingASelectionDoesNotChangeIt)
  {
    // Displaying is not editing. Copying the layer's selection onto the rows
    // makes the view announce a selection change, and answering that by
    // copying the rows back writes a *different* selection than the one being
    // shown — the table holds one row at a time and the layer need not.
    m_conduits->setSelection({ 0, 2 });
    QApplication::processEvents();

    EXPECT_EQ(m_conduits->selection(), QSet<int>({ 0, 2 }))
      << "showing the selection narrowed it to what the table can hold";
  }

  TEST_F(AttributeTableTest, ClearingTheSelectionClearsTheRows)
  {
    m_stack->selectOnly(m_conduits, 1);
    QApplication::processEvents();
    ASSERT_FALSE(m_panel->view()->selectionModel()->selectedRows().isEmpty());

    m_stack->selectOnly(nullptr, -1);
    QApplication::processEvents();

    EXPECT_TRUE(m_panel->view()->selectionModel()->selectedRows().isEmpty());
  }

  TEST_F(AttributeTableTest, TheTableFollowsTheSelectionToAnotherLayer)
  {
    auto *nodes = new VectorProbe(QStringLiteral("nodes"));
    nodes->addPoint({ 1.0, 1.0 }, QStringLiteral("J1"));
    nodes->addPoint({ 2.0, 2.0 }, QStringLiteral("J2"));
    ASSERT_GE(m_stack->addLayer(nodes), 0);

    QApplication::processEvents();

    m_panel->showLayer(m_conduits);
    ASSERT_EQ(m_panel->currentLayer(), m_conduits);

    // A click lands in the other layer. A table that went on describing the
    // conduits would be describing the wrong thing.
    m_stack->selectOnly(nodes, 1);
    QApplication::processEvents();

    EXPECT_EQ(m_panel->currentLayer(), nodes);

    const QModelIndexList rows =
      m_panel->view()->selectionModel()->selectedRows();

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows.first().row(), 1);
  }

  TEST_F(AttributeTableTest, AClickOnTheMapReachesTheTable)
  {
    // End to end, through the parts a user actually touches: the click, the
    // stack, the layer's selection, and the row.
    MapCanvas canvas;
    canvas.resize(400, 400);
    canvas.show();
    canvas.setModel(m_stack.get());
    canvas.setVisibleExtent(QRectF(0.0, 0.0, 10.0, 10.0));
    QApplication::processEvents();

    // The middle conduit, at y = 5 of a 10-unit extent drawn 400 tall.
    QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(200, 200));
    QApplication::processEvents();

    ASSERT_EQ(m_conduits->selection(), QSet<int>({ 1 }));

    const QModelIndexList rows =
      m_panel->view()->selectionModel()->selectedRows();

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows.first().row(), 1);
  }

  TEST_F(AttributeTableTest, SeveralSelectedFeaturesShowAsSeveralRows)
  {
    // C5c's band selects more than one, and the table is the other view of
    // that selection: one that could only ever show a single row would
    // describe a fraction of what the map is highlighting.
    m_stack->selectOnly(m_conduits, QSet<int>({ 0, 2 }));

    const QModelIndexList rows =
      m_panel->view()->selectionModel()->selectedRows();

    ASSERT_EQ(rows.size(), 2);

    QSet<int> shown;

    for (const QModelIndex &row : rows)
    {
      shown.insert(row.row());
    }

    EXPECT_EQ(shown, QSet<int>({ 0, 2 }));
  }

  TEST_F(AttributeTableTest, SelectingSeveralRowsSelectsSeveralFeatures)
  {
    // And the same the other way, since a table that could read a
    // multi-selection but not write one would lose it the moment the user
    // adjusted it here.
    QItemSelectionModel *selection = m_panel->view()->selectionModel();

    selection->select(m_panel->view()->model()->index(1, 0),
                      QItemSelectionModel::Select
                        | QItemSelectionModel::Rows);
    selection->select(m_panel->view()->model()->index(2, 0),
                      QItemSelectionModel::Select
                        | QItemSelectionModel::Rows);

    EXPECT_EQ(m_conduits->selection(), QSet<int>({ 1, 2 }));
  }

  TEST_F(AttributeTableTest, ADestroyedLayerIsLetGoOf)
  {
    // The model reads through the layer on every cell, so it has to let go
    // the moment the layer does. Nothing else guarantees the order: a window
    // tears its layer stack down before the docks that show it, and a header
    // view re-laid-out during that teardown asks a destroyed layer how many
    // features it has. Found by the sanitiser, not by reasoning.
    AttributeTableModel model;

    {
      VectorProbe doomed(QStringLiteral("doomed"));
      doomed.addLine({ { 0.0, 0.0 }, { 1.0, 0.0 } });

      model.setLayer(&doomed);
      ASSERT_EQ(model.rowCount(), 1);
    }

    EXPECT_EQ(model.layer(), nullptr);
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(model.columnCount(), 0);
  }

  TEST_F(AttributeTableTest, ADestroyedStackIsLetGoOf)
  {
    auto stack = std::make_unique<LayerStackModel>();

    auto *probe = new VectorProbe(QStringLiteral("conduits"));
    probe->addLine({ { 0.0, 0.0 }, { 1.0, 0.0 } });
    ASSERT_GE(stack->addLayer(probe), 0);

    AttributeTablePanel panel;
    panel.resize(400, 200);
    panel.show();
    panel.setModel(stack.get());
    QApplication::processEvents();

    ASSERT_EQ(panel.currentLayer(), probe);

    stack.reset();
    QApplication::processEvents();

    EXPECT_EQ(panel.model(), nullptr);
    EXPECT_EQ(panel.currentLayer(), nullptr);
    EXPECT_EQ(panel.view()->model()->rowCount(), 0);
  }

  TEST_F(AttributeTableTest, RemovingTheShownLayerFallsBackRatherThanCrashes)
  {
    auto *nodes = new VectorProbe(QStringLiteral("nodes"));
    nodes->addPoint({ 1.0, 1.0 }, QStringLiteral("J1"));
    ASSERT_GE(m_stack->addLayer(nodes), 0);

    QApplication::processEvents();

    m_panel->showLayer(nodes);
    ASSERT_EQ(m_panel->currentLayer(), nodes);

    ASSERT_TRUE(m_stack->removeRows(m_stack->rowOf(nodes), 1));
    QApplication::processEvents();

    EXPECT_EQ(m_panel->currentLayer(), m_conduits)
      << "the table kept pointing at a layer that no longer exists";
    EXPECT_EQ(m_panel->view()->model()->rowCount(), 3);
  }
}
