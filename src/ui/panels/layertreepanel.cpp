#include "ui/panels/layertreepanel.h"

#include "map/layerstackmodel.h"
#include "map/maplayer.h"
#include "map/maplayer.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{
  namespace
  {
    QToolButton *makeButton(QWidget *parent, const QString &objectName,
                            QStyle::StandardPixmap icon, const QString &tip)
    {
      auto *button = new QToolButton(parent);
      button->setObjectName(objectName);
      button->setIcon(parent->style()->standardIcon(icon));
      button->setToolTip(tip);
      button->setAutoRaise(true);
      button->setEnabled(false);

      return button;
    }
  }

  LayerTreePanel::LayerTreePanel(QWidget *parent)
    : QWidget(parent)
  {
    setObjectName(QStringLiteral("layerTreePanel"));

    m_view = new QTreeView(this);
    m_view->setObjectName(QStringLiteral("layerTreeView"));
    m_view->setHeaderHidden(true);

    // Decorated now that the tree has a second level: the legend rows under a
    // layer need a disclosure control to reach them.
    m_view->setRootIsDecorated(true);
    m_view->setExpandsOnDoubleClick(false);
    m_view->setIndentation(14);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::SelectedClicked
                            | QAbstractItemView::EditKeyPressed);

    // Internal move: dragging in a layer tree reorders the stack, and any
    // other drop action would either duplicate a layer or accept foreign data
    // the model cannot represent.
    m_view->setDragDropMode(QAbstractItemView::InternalMove);
    m_view->setDefaultDropAction(Qt::MoveAction);
    m_view->setDragEnabled(true);
    m_view->setAcceptDrops(true);
    m_view->setDropIndicatorShown(true);

    m_upButton = makeButton(this, QStringLiteral("layerUpButton"),
                            QStyle::SP_ArrowUp, tr("Move layer up"));
    m_downButton = makeButton(this, QStringLiteral("layerDownButton"),
                              QStyle::SP_ArrowDown, tr("Move layer down"));
    m_zoomButton = makeButton(this, QStringLiteral("layerZoomButton"),
                              QStyle::SP_FileDialogContentsView,
                              tr("Zoom to layer"));
    m_styleButton = makeButton(this, QStringLiteral("layerStyleButton"),
                               QStyle::SP_DialogApplyButton,
                               tr("Style layer…"));
    m_removeButton = makeButton(this, QStringLiteral("layerRemoveButton"),
                                QStyle::SP_TrashIcon, tr("Remove layer"));

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(m_upButton);
    buttons->addWidget(m_downButton);
    buttons->addWidget(m_zoomButton);
    buttons->addWidget(m_styleButton);
    buttons->addStretch(1);
    buttons->addWidget(m_removeButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_view, 1);
    layout->addLayout(buttons);

    connect(m_upButton, &QToolButton::clicked, this,
            &LayerTreePanel::moveCurrentUp);
    connect(m_downButton, &QToolButton::clicked, this,
            &LayerTreePanel::moveCurrentDown);
    connect(m_removeButton, &QToolButton::clicked, this,
            &LayerTreePanel::removeCurrent);
    connect(m_zoomButton, &QToolButton::clicked, this,
            [this]()
            {
              if (MapLayer *layer = currentLayer())
              {
                Q_EMIT zoomToLayerRequested(layer);
              }
            });

    connect(m_styleButton, &QToolButton::clicked, this,
            &LayerTreePanel::styleCurrent);

    connect(m_view, &QTreeView::doubleClicked, this,
            [this](const QModelIndex &index)
            {
              // Double-clicking a class opens the layer's style editor, which
              // is where a class's colour is changed; double-clicking the
              // layer frames it, the more common intent.
              if (LayerStackModel::isLegendIndex(index))
              {
                if (MapLayer *owner = m_model ? m_model->layerFor(index)
                                              : nullptr)
                {
                  Q_EMIT styleLayerRequested(owner);
                }

                return;
              }

              if (MapLayer *layer = currentLayer())
              {
                Q_EMIT zoomToLayerRequested(layer);
              }
            });
  }

  LayerTreePanel::~LayerTreePanel() = default;

  void LayerTreePanel::setModel(LayerStackModel *model)
  {
    if (m_model == model)
    {
      return;
    }

    m_model = model;
    m_view->setModel(model);

    if (QItemSelectionModel *selection = m_view->selectionModel())
    {
      connect(selection, &QItemSelectionModel::currentChanged, this,
              [this](const QModelIndex &, const QModelIndex &)
              {
                updateButtons();
                Q_EMIT currentLayerChanged(currentLayer());
              });
    }

    if (m_model)
    {
      // Rows removed or reordered elsewhere change what the buttons can do,
      // and the selection signal alone does not always fire for them.
      connect(m_model, &LayerStackModel::renderChanged, this,
              &LayerTreePanel::updateButtons);
    }

    updateButtons();
  }

  LayerStackModel *LayerTreePanel::model() const
  {
    return m_model;
  }

  MapLayer *LayerTreePanel::currentLayer() const
  {
    if (!m_model)
    {
      return nullptr;
    }

    const QModelIndex current = m_view->currentIndex();

    // Only a layer row is a layer. A selected legend class is a class, and
    // acting on it as though the whole layer were selected would make
    // "remove" delete far more than the user pointed at.
    if (LayerStackModel::isLegendIndex(current))
    {
      return nullptr;
    }

    return m_model->layerAt(current.row());
  }

  QTreeView *LayerTreePanel::view() const
  {
    return m_view;
  }

  void LayerTreePanel::moveCurrentUp()
  {
    if (!m_model || !currentLayer())
    {
      return;
    }

    const int row = m_view->currentIndex().row();

    if (row <= 0)
    {
      return;
    }

    if (m_model->moveRows(QModelIndex(), row, 1, QModelIndex(), row - 1))
    {
      m_view->setCurrentIndex(m_model->index(row - 1, 0));
    }
  }

  void LayerTreePanel::moveCurrentDown()
  {
    if (!m_model || !currentLayer())
    {
      return;
    }

    const int row = m_view->currentIndex().row();

    if (row < 0 || row + 1 >= m_model->rowCount())
    {
      return;
    }

    // Qt's destination row is read before the source is lifted out, so moving
    // one row down means naming the row after the one it swaps with.
    if (m_model->moveRows(QModelIndex(), row, 1, QModelIndex(), row + 2))
    {
      m_view->setCurrentIndex(m_model->index(row + 1, 0));
    }
  }

  void LayerTreePanel::removeCurrent()
  {
    if (!m_model || !currentLayer())
    {
      return;
    }

    const int row = m_view->currentIndex().row();

    if (row >= 0)
    {
      m_model->removeLayer(row);
    }
  }

  void LayerTreePanel::styleCurrent()
  {
    if (MapLayer *layer = currentLayer())
    {
      Q_EMIT styleLayerRequested(layer);
    }
  }

  void LayerTreePanel::updateButtons()
  {
    const bool onLayer = currentLayer() != nullptr;
    const int row = onLayer ? m_view->currentIndex().row() : -1;
    const int rows = m_model ? m_model->rowCount() : 0;
    const bool hasLayer = onLayer && row >= 0 && row < rows;

    m_upButton->setEnabled(hasLayer && row > 0);
    m_downButton->setEnabled(hasLayer && row + 1 < rows);
    m_zoomButton->setEnabled(hasLayer);
    m_removeButton->setEnabled(hasLayer);

    // Styling is offered only where there is a style to edit: a basemap has
    // none, and an empty editor is worse than no menu entry.
    m_styleButton->setEnabled(hasLayer
                              && currentLayer()->style() != nullptr);
  }

} // namespace HydroCouple::Composer
