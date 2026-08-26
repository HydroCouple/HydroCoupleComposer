#include "results/runsession.h"

#include "hydrocouple.h"

#include <QFileInfo>

#include <filesystem>

namespace HydroCouple::Composer
{
  RunSession::RunSession(QString manifestPath, SDK::IO::RunManifest manifest)
    : m_manifestPath(std::move(manifestPath)),
      m_manifest(std::move(manifest))
  {
  }

  RunSession::~RunSession() = default;

  std::unique_ptr<RunSession> RunSession::open(const QString &manifestPath,
                                               QString &message)
  {
    SDK::IO::RunManifest manifest;
    std::string reason;

    if (!SDK::IO::RunManifest::read(
          std::filesystem::path(manifestPath.toStdString()), manifest,
          reason))
    {
      message = QString::fromStdString(reason);

      return nullptr;
    }

    return std::unique_ptr<RunSession>(
      new RunSession(manifestPath, std::move(manifest)));
  }

  QString RunSession::manifestPath() const
  {
    return m_manifestPath;
  }

  QString RunSession::title() const
  {
    if (!m_manifest.caption.empty())
    {
      return QString::fromStdString(m_manifest.caption);
    }

    if (!m_manifest.id.empty())
    {
      return QString::fromStdString(m_manifest.id);
    }

    // Neither recorded: the file name is at least something the user chose,
    // and an untitled row in a browser of runs is a row nobody can tell from
    // the next one.
    return QFileInfo(m_manifestPath).fileName();
  }

  const SDK::IO::RunManifest &RunSession::manifest() const
  {
    return m_manifest;
  }

  QStringList RunSession::componentIds() const
  {
    QStringList ids;

    // Catalog order, deduplicated: the order entries were recorded in is the
    // order they were produced in, which is more use than alphabetical.
    for (const SDK::IO::ResultEntry &entry : m_manifest.results)
    {
      const QString id = QString::fromStdString(entry.componentId);

      if (!ids.contains(id))
      {
        ids.append(id);
      }
    }

    return ids;
  }

  QVector<const SDK::IO::ResultEntry *> RunSession::entriesFor(
    const QString &componentId) const
  {
    QVector<const SDK::IO::ResultEntry *> entries;

    const std::string wanted = componentId.toStdString();

    for (const SDK::IO::ResultEntry &entry : m_manifest.results)
    {
      if (entry.componentId == wanted)
      {
        entries.append(&entry);
      }
    }

    return entries;
  }

  SDK::ResultsModelComponent *RunSession::component(const QString &componentId,
                                                    QString &message)
  {
    auto found = m_opened.constFind(componentId);

    if (found != m_opened.constEnd())
    {
      return found.value().get();
    }

    // Constructed from the manifest already in hand rather than re-read from
    // disk: the catalog is the same one the browser is showing, and reading
    // it twice is two chances for them to disagree.
    auto component = std::make_shared<SDK::ResultsModelComponent>(
      m_manifest, componentId.toStdString());

    component->initialize();

    if (component->status()
        == HydroCouple::IModelComponent::ComponentStatus::Failed)
    {
      // The artifacts, not the catalog: this is where a moved or unreadable
      // file is found, and the manifest that named it is still perfectly
      // browsable.
      message = QObject::tr("The results for “%1” could not be opened; its "
                            "artifacts may have moved.")
                  .arg(componentId);

      return nullptr;
    }

    m_opened.insert(componentId, component);

    return component.get();
  }

  HydroCouple::IComponentDataItem *RunSession::item(const QString &componentId,
                                                    const QString &itemId,
                                                    QString &message)
  {
    SDK::ResultsModelComponent *opened = component(componentId, message);

    if (!opened)
    {
      return nullptr;
    }

    const std::string wanted = itemId.toStdString();

    for (HydroCouple::IComponentDataItem *item : opened->results())
    {
      if (item && item->id() == wanted)
      {
        return item;
      }
    }

    message = QObject::tr("“%1” recorded no item called “%2”.")
                .arg(componentId, itemId);

    return nullptr;
  }

} // namespace HydroCouple::Composer
