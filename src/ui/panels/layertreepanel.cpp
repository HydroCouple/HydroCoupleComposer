#include "ui/panels/layertreepanel.h"

#include "map/layerstackmodel.h"
#include "map/maplayer.h"
#include "map/maplayer.h"
#include "ui/theme/iconfactory.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{
  namespace
  {
    QToolButton *makeButton(QWidget *parent, const QString &objectName,
                            const QString &alias, const QString &tip)
    {
      auto *button = new QToolButton(parent);
      button->setObjectName(objectName);
      button->setIcon(IconFactory::icon(alias));
      button->setToolTip(tip);
      button->setAutoRaise(true);
      button->setEnabled(false);

      return button;
    }

    /*!
     * \brief Paints the 3D badge at the right edge of a layer row.
     *
     * Three states, because two would lie: a solid cube for a layer in the
     * scene, a faded one for a layer kept out of it, and none at all for a
     * layer with no 3D form -- which is a fact about the layer, not a
     * setting, and must not be drawn as a checkbox somebody forgot to tick.
     */
    class SceneBadgeDelegate : public QStyledItemDelegate
    {
      public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
        {
          QStyledItemDelegate::paint(painter, option, index);

          if (LayerStackModel::isLegendIndex(index))
          {
            return;
          }

          if (!index.data(LayerStackModel::HasSceneFormRole).toBool())
          {
            return;
          }

          const int side = option.rect.height() - 6;

          if (side <= 0)
          {
            return;
          }

          const QRect badge(option.rect.right() - side - 4,
                            option.rect.top() + 3, side, side);

          painter->save();
          painter->setOpacity(
            index.data(LayerStackModel::ShownIn3DRole).toBool() ? 0.9 : 0.3);
          IconFactory::icon(QStringLiteral("scene_3d"))
            .paint(painter, badge);
          painter->restore();
        }
    };
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
    m_view->setItemDelegate(new SceneBadgeDelegate(m_view));

    // The panel's verbs, on the rows themselves. The toolbar buttons stay --
    // they are discoverable -- but a context menu is where per-layer state
    // that is not worth a button lives, starting with the 3D toggle.
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QTreeView::customContextMenuRequested, this,
            [this](const QPoint &position)
            {
              const QModelIndex index = m_view->indexAt(position);

              if (!index.isValid() || LayerStackModel::isLegendIndex(index))
              {
                return;
              }

              m_view->setCurrentIndex(index);

              MapLayer *layer = currentLayer();

              if (!layer)
              {
                return;
              }

              QMenu menu(this);

              QAction *shownIn3d =
                menu.addAction(tr("Show in 3D"));
              shownIn3d->setObjectName(QStringLiteral("layerShownIn3dAction"));
              shownIn3d->setCheckable(true);

              const bool hasSceneForm =
                index.data(LayerStackModel::HasSceneFormRole).toBool();
              shownIn3d->setEnabled(hasSceneForm);
              shownIn3d->setChecked(
                hasSceneForm &&
                index.data(LayerStackModel::ShownIn3DRole).toBool());

              if (!hasSceneForm)
              {
                shownIn3d->setToolTip(
                  tr("This layer has no 3D form."));
              }

              connect(shownIn3d, &QAction::toggled, this,
                      [this, index](bool checked)
                      {
                        m_model->setData(index, checked,
                                         LayerStackModel::ShownIn3DRole);
                      });

              menu.addSeparator();

              QAction *zoom = menu.addAction(tr("Zoom to Layer"));
              connect(zoom, &QAction::triggered, this,
                      [this, layer] { Q_EMIT zoomToLayerRequested(layer); });

              QAction *properties = menu.addAction(tr("Properties…"));
              connect(properties, &QAction::triggered, this,
                      [this, layer]
                      { Q_EMIT layerPropertiesRequested(layer); });

              menu.addSeparator();

              QAction *remove = menu.addAction(tr("Remove Layer"));
              connect(remove, &QAction::triggered, this,
                      &LayerTreePanel::removeCurrent);

              menu.exec(m_view->viewport()->mapToGlobal(position));
            });

    m_upButton = makeButton(this, QStringLiteral("layerUpButton"),
                            QStringLiteral("move_up"), tr("Move layer up"));
    m_downButton = makeButton(this, QStringLiteral("layerDownButton"),
                              QStringLiteral("move_down"), tr("Move layer down"));
    m_zoomButton = makeButton(this, QStringLiteral("layerZoomButton"),
                              QStringLiteral("extent"),
                              tr("Zoom to layer"));
    m_propertiesButton = makeButton(this, QStringLiteral("layerPropertiesButton"),
                               QStringLiteral("layer_styling"),
                               tr("Layer properties…"));
    m_removeButton = makeButton(this, QStringLiteral("layerRemoveButton"),
                                QStringLiteral("delete"), tr("Remove layer"));

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(m_upButton);
    buttons->addWidget(m_downButton);
    buttons->addWidget(m_zoomButton);
    buttons->addWidget(m_propertiesButton);
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

    connect(m_propertiesButton, &QToolButton::clicked, this,
            &LayerTreePanel::openProperties);

    connect(m_view, &QTreeView::doubleClicked, this,
            [this](const QModelIndex &index)
            {
              // Double-clicking a class opens the layer's properties, which
              // is where a class's colour is changed; double-clicking the
              // layer frames it, the more common intent.
              if (LayerStackModel::isLegendIndex(index))
              {
                if (MapLayer *owner = m_model ? m_model->layerFor(index)
                                              : nullptr)
                {
                  Q_EMIT layerPropertiesRequested(owner);
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

  void LayerTreePanel::openProperties()
  {
    if (MapLayer *layer = currentLayer())
    {
      Q_EMIT layerPropertiesRequested(layer);
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

    // Any layer: the ribbon's properties action has always answered for
    // basemaps too (the dialog itself omits the tabs that do not apply), and
    // the same layer being editable from one button and not the other was
    // one of the incoherences this program exists to remove.
    m_propertiesButton->setEnabled(hasLayer);
  }

} // namespace HydroCouple::Composer
