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

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(m_openButton);
    buttons->addWidget(m_closeButton);
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

  void RunBrowserPanel::updateButtons()
  {
    m_closeButton->setEnabled(m_model && currentRunRow() >= 0);
  }

} // namespace HydroCouple::Composer
