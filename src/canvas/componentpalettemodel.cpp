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
    m_entries = m_registry ? m_registry->entries()
                           : std::vector<HydroCouple::IComponentInfo *>{};
    endResetModel();
  }

  int ComponentPaletteModel::rowCount(const QModelIndex &parent) const
  {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
  }

  QVariant ComponentPaletteModel::data(const QModelIndex &index, int role) const
  {
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
    {
      return {};
    }

    HydroCouple::IComponentInfo *info = m_entries[static_cast<size_t>(index.row())];

    switch (role)
    {
      case Qt::DisplayRole:
      {
        const QString caption = QString::fromStdString(info->caption());
        return caption.isEmpty() ? QString::fromStdString(info->id()) : caption;
      }
      case Qt::ToolTipRole:
        return QString::fromStdString(info->description());
      case ComponentIdRole:
        return QString::fromStdString(info->id());
      case VersionRole:
        return QString::fromStdString(info->version());
      case LibraryRole:
        return QString::fromStdString(info->libraryFilePath());
      default:
        return {};
    }
  }

  Qt::ItemFlags ComponentPaletteModel::flags(const QModelIndex &index) const
  {
    Qt::ItemFlags flags = QAbstractListModel::flags(index);

    if (index.isValid())
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
    if (indexes.isEmpty())
    {
      return nullptr;
    }

    auto *mime = new QMimeData;
    mime->setData(QLatin1String(kComponentMimeType),
                  data(indexes.first(), ComponentIdRole).toString().toUtf8());

    return mime;
  }

} // namespace HydroCouple::Composer
