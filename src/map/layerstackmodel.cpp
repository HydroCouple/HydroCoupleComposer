#include "map/layerstackmodel.h"

#include "layers/featurelayer.h"

#include "gis/spatialreference.h"
#include "map/maplayer.h"
#include "render/layerstyle.h"

#include <QIcon>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Legend swatch size, in logical pixels.
    constexpr int kSwatchSize = 14;

    QIcon swatch(const QColor &color)
    {
      if (!color.isValid())
      {
        return {};
      }

      QPixmap pixmap(kSwatchSize, kSwatchSize);
      pixmap.fill(Qt::transparent);

      QPainter painter(&pixmap);
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setBrush(color);

      // Outlined, so a pale swatch is still a shape against a light row
      // rather than an empty gap.
      painter.setPen(QPen(color.darker(160), 1.0));
      painter.drawRoundedRect(QRectF(0.5, 0.5, kSwatchSize - 1.0,
                                     kSwatchSize - 1.0),
                              2.0, 2.0);

      return QIcon(pixmap);
    }

    QIcon styleSwatch(const LayerStyle *style)
    {
      if (!style)
      {
        return {};
      }

      if (style->mode() == StyleMode::Single)
      {
        return swatch(style->symbol().fill);
      }

      const QVector<LegendItem> items = style->legendItems();

      if (items.isEmpty())
      {
        return swatch(style->symbol().fill);
      }

      // A striped swatch summarising the classes, so a collapsed layer still
      // shows which palette it is drawn with.
      QPixmap pixmap(kSwatchSize, kSwatchSize);
      pixmap.fill(Qt::transparent);

      QPainter painter(&pixmap);

      const double band =
        static_cast<double>(kSwatchSize) / items.size();

      for (int i = 0; i < items.size(); ++i)
      {
        painter.fillRect(QRectF(0.0, i * band, kSwatchSize, band + 0.5),
                         items.at(i).color);
      }

      painter.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(QRectF(0.5, 0.5, kSwatchSize - 1.0, kSwatchSize - 1.0));

      return QIcon(pixmap);
    }
  }

  LayerStackModel::LayerStackModel(QObject *parent)
    : QAbstractItemModel(parent)
  {
  }

  LayerStackModel::~LayerStackModel()
  {
    qDeleteAll(m_layers);
    m_layers.clear();
  }

  QModelIndex LayerStackModel::index(int row, int column,
                                     const QModelIndex &parent) const
  {
    if (!hasIndex(row, column, parent))
    {
      return {};
    }

    // A layer row carries no internal pointer; a legend row carries the layer
    // it belongs to, which is how parent() finds its way back up.
    if (!parent.isValid())
    {
      return createIndex(row, column, nullptr);
    }

    if (isLegendIndex(parent))
    {
      return {};
    }

    return createIndex(row, column, layerAt(parent.row()));
  }

  QModelIndex LayerStackModel::parent(const QModelIndex &child) const
  {
    if (!child.isValid())
    {
      return {};
    }

    auto *owner = static_cast<MapLayer *>(child.internalPointer());

    if (!owner)
    {
      return {};
    }

    const int row = rowOf(owner);

    return row >= 0 ? createIndex(row, 0, nullptr) : QModelIndex();
  }

  int LayerStackModel::rowCount(const QModelIndex &parent) const
  {
    if (!parent.isValid())
    {
      return static_cast<int>(m_layers.size());
    }

    // Legend rows are leaves; only a layer row has children.
    if (isLegendIndex(parent))
    {
      return 0;
    }

    return legendCount(layerAt(parent.row()));
  }

  int LayerStackModel::columnCount(const QModelIndex &parent) const
  {
    Q_UNUSED(parent)

    return 1;
  }

  bool LayerStackModel::isLegendIndex(const QModelIndex &index)
  {
    return index.isValid() && index.internalPointer() != nullptr;
  }

  MapLayer *LayerStackModel::layerFor(const QModelIndex &index) const
  {
    if (!index.isValid())
    {
      return nullptr;
    }

    if (auto *owner = static_cast<MapLayer *>(index.internalPointer()))
    {
      return owner;
    }

    return layerAt(index.row());
  }

  int LayerStackModel::legendCount(const MapLayer *layer) const
  {
    const LayerStyle *style = layer ? layer->style() : nullptr;

    return style ? static_cast<int>(style->legendItems().size()) : 0;
  }

  QVariant LayerStackModel::data(const QModelIndex &index, int role) const
  {
    if (!index.isValid() || index.column() != 0)
    {
      return {};
    }

    // ── Legend row ───────────────────────────────────────────────────────
    if (auto *owner = static_cast<MapLayer *>(index.internalPointer()))
    {
      const LayerStyle *style = owner->style();

      if (!style)
      {
        return {};
      }

      const QVector<LegendItem> items = style->legendItems();

      if (index.row() >= items.size())
      {
        return {};
      }

      const LegendItem &item = items.at(index.row());

      switch (role)
      {
        case Qt::DisplayRole:
          return item.label;

        case Qt::DecorationRole:
          return swatch(item.color);

        case Qt::CheckStateRole:
          return item.visible ? Qt::Checked : Qt::Unchecked;

        case IsLegendRole:
          return true;

        case LegendIndexRole:
          return item.index;

        default:
          return {};
      }
    }

    // ── Layer row ────────────────────────────────────────────────────────
    const MapLayer *layer = layerAt(index.row());

    if (!layer)
    {
      return {};
    }

    switch (role)
    {
      case Qt::DisplayRole:
      case Qt::EditRole:
        return layer->name();

      case Qt::CheckStateRole:
        return layer->isVisible() ? Qt::Checked : Qt::Unchecked;

      case Qt::DecorationRole:
        return styleSwatch(layer->style());

      case Qt::ToolTipRole:
      {
        // The 3D state is said here in words as well as shown as a badge,
        // because the badge's third state -- no badge at all -- is exactly
        // the one a tooltip has to explain.
        QString sceneLine =
          layer->sceneSource() == nullptr
            ? tr("No 3D form")
            : layer->isShownIn3D() ? tr("Shown in 3D") : tr("Kept out of 3D");

        if (layer == electedTerrainLayer())
        {
          sceneLine += tr(" — the scene's terrain");
        }

        const SpatialReference *crs = layer->crs();
        return crs ? QStringLiteral("%1\n%2\n%3")
                       .arg(layer->name(), crs->description(), sceneLine)
                   : QStringLiteral("%1\n%2").arg(layer->name(), sceneLine);
      }

      case LayerIdRole:
        return layer->id();

      case OpacityRole:
        return layer->opacity();

      case CrsDescriptionRole:
        return layer->crs() ? layer->crs()->description() : QString();

      case IsLegendRole:
        return false;

      case LegendIndexRole:
        return -1;

      case HasSceneFormRole:
        return layer->sceneSource() != nullptr;

      case ShownIn3DRole:
        return layer->isShownIn3D();

      case IsElectedTerrainRole:
        return layer == electedTerrainLayer();

      case TerrainEnabledRole:
      {
        const ISceneSource *source = layer->sceneSource();
        return source != nullptr && source->terrainEnabled();
      }

      default:
        return {};
    }
  }

  bool LayerStackModel::setData(const QModelIndex &index,
                                const QVariant &value, int role)
  {
    // ── Legend row ───────────────────────────────────────────────────────
    if (auto *owner = static_cast<MapLayer *>(index.internalPointer()))
    {
      LayerStyle *style = owner->style();

      if (!style || role != Qt::CheckStateRole)
      {
        return false;
      }

      const QVector<LegendItem> items = style->legendItems();

      if (index.row() >= items.size())
      {
        return false;
      }

      style->setLegendItemVisible(items.at(index.row()).index,
                                  value.toInt() == Qt::Checked);

      Q_EMIT dataChanged(index, index);
      Q_EMIT renderChanged();

      return true;
    }

    // ── Layer row ────────────────────────────────────────────────────────
    MapLayer *layer = layerAt(index.row());

    if (!layer)
    {
      return false;
    }

    switch (role)
    {
      case Qt::CheckStateRole:
        layer->setVisible(value.toInt() == Qt::Checked);
        return true;

      case ShownIn3DRole:
        // Meaningful only for layers with a 3D form; setting it on one
        // without is accepted and inert, the same as hiding a hidden layer.
        layer->setShownIn3D(value.toBool());
        Q_EMIT dataChanged(index, index);
        return true;

      case TerrainEnabledRole:
        if (ISceneSource *source = layer->sceneSource())
        {
          source->setTerrainEnabled(value.toBool());

          // The election may have moved to another row entirely, so the
          // whole column is announced rather than this one cell.
          Q_EMIT dataChanged(this->index(0, 0),
                             this->index(rowCount() - 1, 0));
          return true;
        }
        return false;

      case Qt::EditRole:
      {
        const QString name = value.toString().trimmed();

        if (name.isEmpty())
        {
          return false;
        }

        layer->setName(name);
        return true;
      }

      case OpacityRole:
        layer->setOpacity(value.toDouble());
        return true;

      default:
        return false;
    }
  }

  Qt::ItemFlags LayerStackModel::flags(const QModelIndex &index) const
  {
    if (!index.isValid())
    {
      // Dropping on the root is what a drag past the last row produces, and
      // it is how a layer is moved to the bottom of the stack.
      return Qt::ItemIsDropEnabled;
    }

    // A legend row is a class, not a layer: it can be switched off, but
    // dragging or renaming it would mean nothing.
    if (isLegendIndex(index))
    {
      return Qt::ItemIsEnabled | Qt::ItemIsSelectable
             | Qt::ItemIsUserCheckable | Qt::ItemNeverHasChildren;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
           | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled
           | Qt::ItemIsDropEnabled;
  }

  QVariant LayerStackModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const
  {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole
        && section == 0)
    {
      return tr("Layer");
    }

    return {};
  }

  bool LayerStackModel::moveRows(const QModelIndex &sourceParent,
                                 int sourceRow, int count,
                                 const QModelIndex &destinationParent,
                                 int destinationChild)
  {
    if (sourceParent.isValid() || destinationParent.isValid() || count <= 0)
    {
      return false;
    }

    const int rows = static_cast<int>(m_layers.size());

    if (sourceRow < 0 || sourceRow + count > rows || destinationChild < 0
        || destinationChild > rows)
    {
      return false;
    }

    // Moving a block onto itself is a no-op, and Qt rejects the corresponding
    // beginMoveRows outright, so it is filtered here rather than tripping an
    // assertion inside the view.
    if (destinationChild >= sourceRow && destinationChild <= sourceRow + count)
    {
      return false;
    }

    if (!beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1,
                       destinationParent, destinationChild))
    {
      return false;
    }

    QVector<MapLayer *> moved;
    moved.reserve(count);

    for (int i = 0; i < count; ++i)
    {
      moved.append(m_layers.at(sourceRow + i));
    }

    m_layers.remove(sourceRow, count);

    // Removing the block shifts everything after it down, so a destination
    // beyond the block has to come back by the same amount.
    const int insertAt =
      destinationChild > sourceRow ? destinationChild - count : destinationChild;

    for (int i = 0; i < count; ++i)
    {
      m_layers.insert(insertAt + i, moved.at(i));
    }

    endMoveRows();
    Q_EMIT renderChanged();

    return true;
  }

  bool LayerStackModel::removeRows(int row, int count,
                                   const QModelIndex &parent)
  {
    if (parent.isValid() || count <= 0 || row < 0
        || row + count > static_cast<int>(m_layers.size()))
    {
      return false;
    }

    beginRemoveRows(QModelIndex(), row, row + count - 1);

    for (int i = 0; i < count; ++i)
    {
      m_legendCounts.remove(m_layers.at(row + i));
      delete m_layers.at(row + i);
    }

    m_layers.remove(row, count);

    endRemoveRows();
    Q_EMIT renderChanged();

    return true;
  }

  Qt::DropActions LayerStackModel::supportedDropActions() const
  {
    // Move only: a copy would duplicate a layer that owns file handles and
    // GPU resources, which is never what dragging in a layer tree means.
    return Qt::MoveAction;
  }

  int LayerStackModel::addLayer(MapLayer *layer)
  {
    return insertLayer(0, layer);
  }

  int LayerStackModel::insertLayer(int row, MapLayer *layer)
  {
    if (!layer || m_layers.contains(layer))
    {
      return -1;
    }

    const int at = std::clamp(row, 0, static_cast<int>(m_layers.size()));

    beginInsertRows(QModelIndex(), at, at);

    layer->setParent(this);
    m_layers.insert(at, layer);
    m_legendCounts.insert(layer, legendCount(layer));
    connectLayer(layer);

    endInsertRows();
    Q_EMIT renderChanged();

    return at;
  }

  bool LayerStackModel::removeLayer(int row)
  {
    return removeRows(row, 1);
  }

  MapLayer *LayerStackModel::electedTerrainLayer() const
  {
    MapLayer *elected = nullptr;

    // renderOrder() is bottom-up -- a draw order -- so the last candidate it
    // yields is the top one. The same rule the renderer has always applied,
    // now asked here so every view reads one answer.
    for (MapLayer *layer : renderOrder())
    {
      if (!layer->isVisible() || !layer->isShownIn3D())
      {
        continue;
      }

      const ISceneSource *source = layer->sceneSource();

      if (source && source->terrain() && source->terrainEnabled())
      {
        elected = layer;
      }
    }

    return elected;
  }

  MapLayer *LayerStackModel::layerAt(int row) const
  {
    return row >= 0 && row < static_cast<int>(m_layers.size())
             ? m_layers.at(row)
             : nullptr;
  }

  int LayerStackModel::rowOf(const MapLayer *layer) const
  {
    for (int row = 0; row < static_cast<int>(m_layers.size()); ++row)
    {
      if (m_layers.at(row) == layer)
      {
        return row;
      }
    }

    return -1;
  }

  MapLayer *LayerStackModel::layerById(const QString &id) const
  {
    for (MapLayer *layer : m_layers)
    {
      if (layer->id() == id)
      {
        return layer;
      }
    }

    return nullptr;
  }

  QVector<MapLayer *> LayerStackModel::layers() const
  {
    return m_layers;
  }

  QVector<MapLayer *> LayerStackModel::renderOrder() const
  {
    QVector<MapLayer *> ordered = m_layers;
    std::reverse(ordered.begin(), ordered.end());

    return ordered;
  }

  void LayerStackModel::selectOnly(MapLayer *layer, int feature)
  {
    // In terms of the set: one feature is a set of one, and a negative index
    // is the empty set, which is how a click on empty map says "nothing".
    selectOnly(layer, feature >= 0 ? QSet<int>{feature} : QSet<int>{});
  }

  SelectionMode selectionModeFor(Qt::KeyboardModifiers modifiers)
  {
    if (modifiers.testFlag(Qt::ShiftModifier))
    {
      return SelectionMode::Add;
    }

    // ControlModifier is ⌘ on macOS, so this one test is right everywhere.
    if (modifiers.testFlag(Qt::ControlModifier))
    {
      return SelectionMode::Toggle;
    }

    return SelectionMode::Replace;
  }

  void LayerStackModel::select(MapLayer *layer, const QSet<int> &features,
                               SelectionMode mode)
  {
    auto *target = dynamic_cast<FeatureLayer *>(layer);

    if (mode == SelectionMode::Replace || !target)
    {
      selectOnly(layer, features);

      return;
    }

    // Adding across layers needs no special case, and a guard for it was
    // written here and then deleted: at most one layer holds a selection
    // at a time (selectOnly clears the rest), so when another layer holds
    // one this layer's own is empty — and a union with empty is exactly
    // the replacement the guard was going to perform. It could not change
    // an outcome, so it was code with nothing to say. The behaviour it was
    // protecting is still gated, by
    // AddingAcrossLayersReplacesRatherThanSplitting; what enforces it is
    // selectOnly below.
    QSet<int> combined = target->selection();

    for (int feature : features)
    {
      if (mode == SelectionMode::Toggle && combined.contains(feature))
      {
        combined.remove(feature);
      }
      else
      {
        combined.insert(feature);
      }
    }

    selectOnly(layer, combined);
  }

  void LayerStackModel::selectOnly(MapLayer *layer, const QSet<int> &features)
  {
    for (MapLayer *candidate : m_layers)
    {
      auto *layerFeatures = dynamic_cast<FeatureLayer *>(candidate);

      if (!layerFeatures)
      {
        continue;
      }

      if (layerFeatures == layer && !features.isEmpty())
      {
        layerFeatures->setSelection(features);
      }
      else
      {
        layerFeatures->clearSelection();
      }
    }
  }

  void LayerStackModel::clear()
  {
    if (m_layers.isEmpty())
    {
      return;
    }

    removeRows(0, static_cast<int>(m_layers.size()));
  }

  void LayerStackModel::connectLayer(MapLayer *layer)
  {
    connect(layer, &MapLayer::nameChanged, this,
            [this, layer](const QString &) { emitRowChanged(layer); });

    connect(layer, &MapLayer::appearanceChanged, this,
            [this, layer]()
            {
              emitRowChanged(layer);
              Q_EMIT renderChanged();
            });

    connect(layer, &MapLayer::extentChanged, this,
            [this]() { Q_EMIT renderChanged(); });
  }

  void LayerStackModel::emitRowChanged(MapLayer *layer)
  {
    const int row = rowOf(layer);

    if (row < 0)
    {
      return;
    }

    const int current = legendCount(layer);
    const int previous = m_legendCounts.value(layer, 0);

    // A restyle can change how many classes a layer has. Telling a view only
    // that the row's data changed would leave it addressing legend rows that
    // no longer exist, so the structural case is announced as one.
    if (current != previous)
    {
      const QModelIndex parentIndex = index(row, 0);

      if (current > previous)
      {
        beginInsertRows(parentIndex, previous, current - 1);
        m_legendCounts.insert(layer, current);
        endInsertRows();
      }
      else
      {
        beginRemoveRows(parentIndex, current, previous - 1);
        m_legendCounts.insert(layer, current);
        endRemoveRows();
      }
    }

    const QModelIndex changed = index(row, 0);
    Q_EMIT dataChanged(changed, changed);

    if (current > 0)
    {
      Q_EMIT dataChanged(index(0, 0, changed), index(current - 1, 0, changed));
    }
  }

} // namespace HydroCouple::Composer
