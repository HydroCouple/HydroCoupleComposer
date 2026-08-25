#include "plugins/componentregistry.h"

#include <QDir>
#include <QFileInfo>

namespace HydroCouple::Composer
{

  ComponentRegistry::ComponentRegistry(QObject *parent)
    : QObject(parent)
  {
  }

  ComponentRegistry::~ComponentRegistry() = default;

  HydroCouple::IComponentInfo *ComponentRegistry::loadLibrary(
    const QString &filePath, QString &message)
  {
    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();

    for (const auto &library : m_libraries)
    {
      if (library->filePath() == absolutePath)
      {
        return library->componentInfo();
      }
    }

    std::unique_ptr<ComponentLibrary> library =
      ComponentLibrary::load(absolutePath, message);

    if (!library)
    {
      return nullptr;
    }

    HydroCouple::IComponentInfo *info = library->componentInfo();
    m_libraries.push_back(std::move(library));

    Q_EMIT registryChanged();

    return info;
  }

  int ComponentRegistry::scanDirectory(const QString &directoryPath)
  {
    QDir directory(directoryPath);

    if (!directory.exists())
    {
      m_failures.push_back(
        {directoryPath, QStringLiteral("no such directory")});
      return 0;
    }

    const QStringList filters{QStringLiteral("*") +
                              ComponentLibrary::librarySuffix()};

    const QFileInfoList candidates =
      directory.entryInfoList(filters, QDir::Files | QDir::NoSymLinks,
                              QDir::Name);

    int loaded = 0;

    for (const QFileInfo &candidate : candidates)
    {
      QString message;

      if (loadLibrary(candidate.absoluteFilePath(), message))
      {
        ++loaded;
      }
      else
      {
        // Not every shared library in a plugin directory is a component;
        // record rather than raise, so a genuinely broken component is still
        // diagnosable without the noise being fatal.
        m_failures.push_back({candidate.absoluteFilePath(), message});
      }
    }

    return loaded;
  }

  QStringList ComponentRegistry::searchPaths() const
  {
    return m_searchPaths;
  }

  void ComponentRegistry::setSearchPaths(const QStringList &paths)
  {
    m_searchPaths = paths;
  }

  int ComponentRegistry::refresh()
  {
    clear();

    int loaded = 0;

    for (const QString &path : std::as_const(m_searchPaths))
    {
      loaded += scanDirectory(path);
    }

    return loaded;
  }

  std::vector<HydroCouple::IComponentInfo *> ComponentRegistry::entries() const
  {
    std::vector<HydroCouple::IComponentInfo *> result;
    result.reserve(m_libraries.size());

    for (const auto &library : m_libraries)
    {
      result.push_back(library->componentInfo());
    }

    return result;
  }

  HydroCouple::IComponentInfo *ComponentRegistry::entry(
    const QString &componentId) const
  {
    for (const auto &library : m_libraries)
    {
      HydroCouple::IComponentInfo *info = library->componentInfo();

      if (info &&
          QString::fromStdString(info->id()) == componentId)
      {
        return info;
      }
    }

    return nullptr;
  }

  std::vector<ComponentLoadFailure> ComponentRegistry::failures() const
  {
    return m_failures;
  }

  std::unique_ptr<HydroCouple::IModelComponent>
  ComponentRegistry::createInstance(const QString &componentId,
                                    QString &message)
  {
    HydroCouple::IComponentInfo *info = entry(componentId);

    if (!info)
    {
      message = QStringLiteral("no component registered with id '%1'")
                  .arg(componentId);
      return nullptr;
    }

    // Only model components can be instantiated; adapted-output factory and
    // workflow components register through the same entry point but answer a
    // different interface.
    auto *modelInfo = dynamic_cast<HydroCouple::IModelComponentInfo *>(info);

    if (!modelInfo)
    {
      message = QStringLiteral("'%1' is not a model component")
                  .arg(componentId);
      return nullptr;
    }

    std::unique_ptr<HydroCouple::IModelComponent> instance =
      modelInfo->createComponentInstance();

    if (!instance)
    {
      message = QStringLiteral("'%1' failed to create an instance")
                  .arg(componentId);
    }

    return instance;
  }

  void ComponentRegistry::clear()
  {
    const bool had = !m_libraries.empty();

    m_libraries.clear();
    m_failures.clear();

    if (had)
    {
      Q_EMIT registryChanged();
    }
  }

} // namespace HydroCouple::Composer
