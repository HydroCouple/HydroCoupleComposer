#include "canvas/componentpalettemodel.h"
#include "canvas/compositioncanvas.h"

#include <QMimeData>

namespace HydroCouple::Composer
{

  ComponentPaletteModel::ComponentPaletteModel(ComponentRegistry *registry,
                                               QObject *parent)
    : QAbstractListModel(parent),
      m_registry(registry)
  {
    if (m_registry)
    {
      connect(m_registry, &ComponentRegistry::registryChanged, this,
              &ComponentPaletteModel::reload);
    }

    reload();
  }

  void ComponentPaletteModel::reload()
  {
    beginResetModel();
    m_rows.clear();

    if (m_registry)
    {
      const std::vector<HydroCouple::IComponentInfo *> models =
        m_registry->entries(ComponentRegistry::ComponentKind::Model);
      const std::vector<HydroCouple::IComponentInfo *> factories =
        m_registry->entries(ComponentRegistry::ComponentKind::AdapterFactory);

      // Section headings only when there is more than one section — a
      // single-kind palette stays the flat list it always was.
      const bool sectioned = !models.empty() && !factories.empty();

      if (sectioned)
      {
        m_rows.push_back({tr("Model components"), nullptr});
      }
      for (HydroCouple::IComponentInfo *info : models)
      {
        m_rows.push_back({QString(), info});
      }
      if (sectioned)
      {
        m_rows.push_back({tr("Adapter factories"), nullptr});
      }
      for (HydroCouple::IComponentInfo *info : factories)
      {
        m_rows.push_back({QString(), info});
      }
    }

    endResetModel();
  }

  int ComponentPaletteModel::rowCount(const QModelIndex &parent) const
  {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
  }

  QVariant ComponentPaletteModel::data(const QModelIndex &index, int role) const
  {
    if (!index.isValid() || index.row() >= static_cast<int>(m_rows.size()))
    {
      return {};
    }

    const Row &row = m_rows[static_cast<size_t>(index.row())];

    if (!row.info)
    {
      switch (role)
      {
        case Qt::DisplayRole:
          return row.header;
        case IsHeaderRole:
          return true;
        case KindRole:
          return -1;
        default:
          return {};
      }
    }

    switch (role)
    {
      case Qt::DisplayRole:
      {
        const QString caption = QString::fromStdString(row.info->caption());
        return caption.isEmpty() ? QString::fromStdString(row.info->id())
                                 : caption;
      }
      case Qt::ToolTipRole:
        return QString::fromStdString(row.info->description());
      case ComponentIdRole:
        return QString::fromStdString(row.info->id());
      case VersionRole:
        return QString::fromStdString(row.info->version());
      case LibraryRole:
        return QString::fromStdString(row.info->libraryFilePath());
      case KindRole:
        return static_cast<int>(ComponentRegistry::kindOf(row.info));
      case IsHeaderRole:
        return false;
      default:
        return {};
    }
  }

  Qt::ItemFlags ComponentPaletteModel::flags(const QModelIndex &index) const
  {
    if (!index.isValid() || index.row() >= static_cast<int>(m_rows.size()))
    {
      return QAbstractListModel::flags(index);
    }

    const Row &row = m_rows[static_cast<size_t>(index.row())];

    // Headings are inert; adapter factories are browsable but must not
    // start a drag — the canvas has nowhere sensible to put one.
    if (!row.info)
    {
      return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = QAbstractListModel::flags(index);

    if (ComponentRegistry::kindOf(row.info) ==
        ComponentRegistry::ComponentKind::Model)
    {
      flags |= Qt::ItemIsDragEnabled;
    }

    return flags;
  }

  QStringList ComponentPaletteModel::mimeTypes() const
  {
    return {QLatin1String(kComponentMimeType)};
  }

  QMimeData *ComponentPaletteModel::mimeData(const QModelIndexList &indexes) const
  {
    if (indexes.isEmpty() ||
        !(flags(indexes.first()) & Qt::ItemIsDragEnabled))
    {
      return nullptr;
    }

    auto *mime = new QMimeData;
    mime->setData(QLatin1String(kComponentMimeType),
                  data(indexes.first(), ComponentIdRole).toString().toUtf8());

    return mime;
  }

} // namespace HydroCouple::Composer
