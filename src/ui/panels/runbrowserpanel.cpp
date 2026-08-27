#include "ui/panels/runbrowserpanel.h"

#include "results/runbrowsermodel.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{
  RunBrowserPanel::RunBrowserPanel(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("runBrowserPanel"));

    m_tree = new QTreeView(this);
    m_tree->setObjectName(QStringLiteral("runTree"));
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_openButton = new QToolButton(this);
    m_openButton->setObjectName(QStringLiteral("openRunButton"));
    m_openButton->setText(tr("Open Run…"));
    m_openButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    connect(m_openButton, &QToolButton::clicked, this,
            [this] { Q_EMIT openRunRequested(); });

    m_closeButton = new QToolButton(this);
    m_closeButton->setObjectName(QStringLiteral("closeRunButton"));
    m_closeButton->setText(tr("Close Run"));
    m_closeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    connect(m_closeButton, &QToolButton::clicked, this,
            [this]
            {
              if (m_model)
              {
                m_model->removeRun(currentRunRow());
              }
            });

    m_showButton = new QToolButton(this);
    m_showButton->setObjectName(QStringLiteral("showItemButton"));
    m_showButton->setText(tr("Show on Map"));
    m_showButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    connect(m_showButton, &QToolButton::clicked, this,
            [this]
            {
              const QModelIndex index = currentItemIndex();

              if (!index.isValid())
              {
                return;
              }

              Q_EMIT showItemRequested(
                currentRunRow(),
                index.data(RunBrowserModel::ComponentIdRole).toString(),
                index.data(RunBrowserModel::ItemIdRole).toString());
            });

    m_compareButton = new QToolButton(this);
    m_compareButton->setObjectName(QStringLiteral("compareItemButton"));
    m_compareButton->setText(tr("Compare…"));
    m_compareButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_compareButton->setToolTip(
      tr("Draw this item as its difference against another open run."));

    connect(m_compareButton, &QToolButton::clicked, this,
            [this]
            {
              const QModelIndex index = currentItemIndex();

              if (!index.isValid())
              {
                return;
              }

              Q_EMIT compareItemRequested(
                currentRunRow(),
                index.data(RunBrowserModel::ComponentIdRole).toString(),
                index.data(RunBrowserModel::ItemIdRole).toString());
            });

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(m_openButton);
    buttons->addWidget(m_closeButton);
    buttons->addWidget(m_showButton);
    buttons->addWidget(m_compareButton);
    buttons->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(buttons);
    layout->addWidget(m_tree, 1);

    updateButtons();
  }

  RunBrowserPanel::~RunBrowserPanel() = default;

  void RunBrowserPanel::setModel(RunBrowserModel *model)
  {
    m_model = model;
    m_tree->setModel(model);

    if (model)
    {
      // The name column stretches and the rest size to their contents: a
      // shape or a unit is short and a run's caption is not.
      m_tree->header()->setSectionResizeMode(RunBrowserModel::NameColumn,
                                             QHeaderView::Stretch);

      connect(m_tree->selectionModel(),
              &QItemSelectionModel::currentChanged, this,
              [this](const QModelIndex &, const QModelIndex &)
              { updateButtons(); });

      // Rows arriving or leaving change what there is to close, and the
      // selection does not move by itself when a run is removed.
      connect(model, &QAbstractItemModel::rowsInserted, this,
              [this](const QModelIndex &, int, int) { updateButtons(); });
      connect(model, &QAbstractItemModel::rowsRemoved, this,
              [this](const QModelIndex &, int, int) { updateButtons(); });
    }

    updateButtons();
  }

  RunBrowserModel *RunBrowserPanel::model() const
  {
    return m_model;
  }

  int RunBrowserPanel::currentRunRow() const
  {
    if (!m_model || !m_tree->selectionModel())
    {
      return -1;
    }

    QModelIndex index = m_tree->selectionModel()->currentIndex();

    // Walked up to the run, so closing works from a selected item as well as
    // from the run's own row — which is where a user browsing results
    // actually is when they decide they are done with a run.
    while (index.isValid() && index.parent().isValid())
    {
      index = index.parent();
    }

    return index.isValid() ? index.row() : -1;
  }

  QModelIndex RunBrowserPanel::currentItemIndex() const
  {
    if (!m_model || !m_tree->selectionModel())
    {
      return {};
    }

    const QModelIndex index = m_tree->selectionModel()->currentIndex();

    // An item row, not a run or a component. Both of those carry a component
    // id as well, and only a row that names an item names something that can
    // be drawn.
    return index.data(RunBrowserModel::ItemIdRole).toString().isEmpty()
             ? QModelIndex()
             : index;
  }

  void RunBrowserPanel::updateButtons()
  {
    m_closeButton->setEnabled(m_model && currentRunRow() >= 0);
    m_showButton->setEnabled(currentItemIndex().isValid());

    // A comparison needs something to compare against, so the button is off
    // until a second run is open — rather than offered and then refused.
    m_compareButton->setEnabled(currentItemIndex().isValid() && m_model
                                && m_model->runCount() > 1);
  }

} // namespace HydroCouple::Composer
