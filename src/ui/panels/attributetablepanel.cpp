#include "ui/panels/attributetablepanel.h"

#include "layers/featurelayer.h"
#include "map/layerstackmodel.h"
#include "map/maplayer.h"
#include "ui/panels/attributetablemodel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>
#include <QVariant>

namespace HydroCouple::Composer
{
  AttributeTablePanel::AttributeTablePanel(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("attributeTablePanel"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(4, 4, 4, 0);

    m_chooser = new QComboBox(this);
    m_chooser->setObjectName(QStringLiteral("attributeLayerChooser"));
    header->addWidget(m_chooser, 1);
    layout->addLayout(header);

    m_table = new AttributeTableModel(this);

    m_view = new QTableView(this);
    m_view->setObjectName(QStringLiteral("attributeTableView"));
    m_view->setModel(m_table);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);

    // One at a time, matching what a pick can express. Offering a rubber-band
    // selection the map cannot show would be offering a selection that only
    // half exists.
    // Extended, since C5c's rubber band selects several at once and the
    // table is the other view of that selection: one that could only show
    // one of them would describe a third of what the map is highlighting.
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_view, 1);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("attributeTableSummary"));
    m_summary->setContentsMargins(4, 0, 4, 4);
    layout->addWidget(m_summary);

    connect(m_chooser, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
              if (m_syncing)
              {
                return;
              }

              m_table->setLayer(
                index >= 0
                  ? m_chooser->itemData(index).value<MapLayer *>()
                  : nullptr);
              readSelection();
            });

    connect(m_view->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            [this] { writeSelection(); });
  }

  AttributeTablePanel::~AttributeTablePanel() = default;

  void AttributeTablePanel::setModel(LayerStackModel *model)
  {
    if (m_model == model)
    {
      return;
    }

    if (m_model)
    {
      disconnect(m_model, nullptr, this, nullptr);
    }

    m_model = model;

    if (m_model)
    {
      // One signal for both jobs: the stack emits it when a layer is added,
      // removed, hidden, restyled or selected in, and every one of those
      // changes either the chooser's contents or the rows highlighted.
      connect(m_model, &LayerStackModel::renderChanged, this,
              [this]
              {
                rebuildChooser();
                readSelection();
              });

      connect(m_model, &QAbstractItemModel::modelReset, this,
              [this] { rebuildChooser(); });
      connect(m_model, &QAbstractItemModel::rowsInserted, this,
              [this] { rebuildChooser(); });
      connect(m_model, &QAbstractItemModel::rowsRemoved, this,
              [this] { rebuildChooser(); });

      // And let go when the stack does. A window tears its layer stack down
      // before the docks that show it, so without this the chooser is
      // rebuilt from a destroyed model during that teardown.
      connect(m_model, &QObject::destroyed, this,
              [this]
              {
                m_model = nullptr;
                m_table->setLayer(nullptr);
                rebuildChooser();
              });
    }

    rebuildChooser();
    readSelection();
  }

  LayerStackModel *AttributeTablePanel::model() const
  {
    return m_model;
  }

  MapLayer *AttributeTablePanel::currentLayer() const
  {
    return m_table->layer();
  }

  void AttributeTablePanel::showLayer(MapLayer *layer)
  {
    const int index = m_chooser->findData(QVariant::fromValue(layer));

    if (index < 0)
    {
      return;
    }

    m_chooser->setCurrentIndex(index);
    m_table->setLayer(layer);
    readSelection();
  }

  QTableView *AttributeTablePanel::view() const
  {
    return m_view;
  }

  void AttributeTablePanel::rebuildChooser()
  {
    MapLayer *wanted = m_table->layer();

    // Rebuilt wholesale rather than diffed: a layer stack is a handful of
    // entries, and a diff is a second description of the same list that can
    // disagree with it.
    const QSignalBlocker blocker(m_chooser);

    m_chooser->clear();

    if (m_model)
    {
      for (MapLayer *layer : m_model->layers())
      {
        // Only layers with attributes: a basemap has no table, and offering
        // one that is always empty is offering a dead end.
        if (dynamic_cast<const IAttributeProvider *>(layer))
        {
          m_chooser->addItem(layer->name(), QVariant::fromValue(layer));
        }
      }
    }

    int index = m_chooser->findData(QVariant::fromValue(wanted));

    if (index < 0)
    {
      // The layer that was showing has gone, so fall to the topmost one that
      // has a table — or to nothing when none has.
      index = m_chooser->count() > 0 ? 0 : -1;
    }

    m_chooser->setCurrentIndex(index);

    m_table->setLayer(index >= 0
                        ? m_chooser->itemData(index).value<MapLayer *>()
                        : nullptr);
  }

  void AttributeTablePanel::readSelection()
  {
    if (m_syncing || !m_model)
    {
      return;
    }

    // Follow the selection to whichever layer holds it. A table that goes on
    // describing the layer the chooser happens to name, while the user has
    // just clicked something in another one, is describing the wrong thing.
    for (MapLayer *layer : m_model->layers())
    {
      auto *features = dynamic_cast<FeatureLayer *>(layer);

      if (features && !features->selection().isEmpty() &&
          features != m_table->layer())
      {
        const int index = m_chooser->findData(QVariant::fromValue(layer));

        if (index >= 0)
        {
          const QSignalBlocker blocker(m_chooser);
          m_chooser->setCurrentIndex(index);
        }

        m_table->setLayer(layer);

        break;
      }
    }

    auto *features = dynamic_cast<FeatureLayer *>(m_table->layer());

    m_syncing = true;

    QItemSelectionModel *selection = m_view->selectionModel();
    selection->clearSelection();

    if (features)
    {
      for (int feature : features->selection())
      {
        if (feature >= 0 && feature < m_table->rowCount())
        {
          selection->select(m_table->index(feature, 0),
                            QItemSelectionModel::Select |
                              QItemSelectionModel::Rows);
          m_view->scrollTo(m_table->index(feature, 0));
        }
      }
    }

    m_syncing = false;

    const int rows = m_table->rowCount();
    const int selected = features ? int(features->selection().size()) : 0;

    m_summary->setText(tr("%1 features, %2 selected").arg(rows).arg(selected));
  }

  void AttributeTablePanel::writeSelection()
  {
    if (m_syncing || !m_model)
    {
      return;
    }

    const QModelIndexList rows =
      m_view->selectionModel()->selectedRows();

    m_syncing = true;

    QSet<int> features;

    for (const QModelIndex &row : rows)
    {
      features.insert(row.row());
    }

    // Through the stack, so that selecting here clears the other layers
    // exactly as a click on the map does — one rule, in one place.
    m_model->selectOnly(m_table->layer(), features);

    m_syncing = false;

    const int rows_count = m_table->rowCount();

    m_summary->setText(tr("%1 features, %2 selected")
                         .arg(rows_count)
                         .arg(rows.size()));
  }

} // namespace HydroCouple::Composer
