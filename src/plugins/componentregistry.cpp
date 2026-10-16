#include "plugins/componentregistry.h"
#include "hydrocouplesdk/component/abstractmodelcomponent.h"

#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

namespace HydroCouple::Composer
{

  ComponentRegistry::ComponentRegistry(QObject *parent)
    : QObject(parent)
  {
  }

  ComponentRegistry::~ComponentRegistry() = default;

  namespace
  {
    QString canonicalPath(const QString &path)
    {
      const QFileInfo info(path);
      const QString canonical = info.canonicalFilePath();
      return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
    }

    QStringList uniquePaths(const QStringList &paths)
    {
      QStringList result;
      for (const QString &path : paths)
      {
        if (path.isEmpty()) continue;
        const QString canonical = canonicalPath(path);
        if (!result.contains(canonical)) result.append(canonical);
      }
      return result;
    }
  }

  QStringList ComponentRegistry::installedSearchPaths(const QString &applicationDirectory)
  {
    const QDir base(applicationDirectory.isEmpty()?QCoreApplication::applicationDirPath():applicationDirectory);
    QStringList candidates{base.filePath("components"),base.filePath("../lib/hydrocouple/components")};
#ifdef Q_OS_MACOS
    candidates.prepend(base.filePath("../PlugIns/HydroCouple"));
    candidates.append(base.filePath("../../../lib/hydrocouple/components"));
#endif
    QStringList paths;
    for(const auto &candidate:candidates){QDir directory(candidate);if(directory.exists()){const auto canonical=directory.canonicalPath();if(!paths.contains(canonical))paths.append(canonical);}}
    return paths;
  }

  QStringList ComponentRegistry::defaultSearchPaths(
    const QStringList &additionalPaths, const QString &applicationDirectory)
  {
    const QStringList environmentPaths = qEnvironmentVariable("HYDROCOUPLE_COMPONENT_PATH")
                                          .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    return uniquePaths(installedSearchPaths(applicationDirectory) +
                       environmentPaths + additionalPaths);
  }

  HydroCouple::IComponentInfo *ComponentRegistry::loadLibrary(
    const QString &filePath, QString &message)
  {
    message.clear();
    const QString absolutePath = canonicalPath(filePath);

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
    for (const auto &existing : m_libraries)
    {
      if (existing->componentInfo()->id() == info->id())
      {
        message = QStringLiteral("refused: duplicate component id '%1' in '%2'; already loaded from '%3'")
                    .arg(QString::fromStdString(info->id()), absolutePath, existing->filePath());
        // Some loaders resolve aliases to the same metadata singleton. Keep
        // its original owning path even if the attempted load updated it.
        existing->componentInfo()->setLibraryFilePath(existing->filePath().toStdString());
        return nullptr;
      }
    }
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
      Q_EMIT registryChanged();
      return 0;
    }

    const QFileInfoList candidates =
      directory.entryInfoList(ComponentLibrary::libraryFilters(),
                              QDir::Files, QDir::Name);

    int loaded = 0;

    for (const QFileInfo &candidate : candidates)
    {
      QString message;
      const auto before = m_libraries.size();

      if (loadLibrary(candidate.absoluteFilePath(), message))
      {
        loaded += static_cast<int>(m_libraries.size() - before);
      }
      else
      {
        // Not every shared library in a plugin directory is a component;
        // record rather than raise, so a genuinely broken component is still
        // diagnosable without the noise being fatal.
        m_failures.push_back({candidate.absoluteFilePath(), message});
      }
    }

    // Failure-only scans must update the browser too. Neither scanning nor
    // refreshing unloads a library that may still own live instances.
    Q_EMIT registryChanged();
    return loaded;
  }

  QStringList ComponentRegistry::searchPaths() const
  {
    return m_searchPaths;
  }

  void ComponentRegistry::setSearchPaths(const QStringList &paths)
  {
    m_searchPaths = uniquePaths(paths);
  }

  int ComponentRegistry::refresh()
  {
    m_failures.clear();

    int loaded = 0;

    for (const QString &path : std::as_const(m_searchPaths))
    {
      loaded += scanDirectory(path);
    }

    Q_EMIT registryChanged();
    return loaded;
  }

  ComponentRegistry::ComponentKind ComponentRegistry::kindOf(
    HydroCouple::IComponentInfo *info)
  {
    if (dynamic_cast<HydroCouple::IModelComponentInfo *>(info))
    {
      return ComponentKind::Model;
    }

    if (dynamic_cast<HydroCouple::IAdaptedOutputFactoryComponentInfo *>(info))
    {
      return ComponentKind::AdapterFactory;
    }

    return ComponentKind::Other;
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

  std::vector<HydroCouple::IComponentInfo *> ComponentRegistry::entries(
    ComponentKind kind) const
  {
    std::vector<HydroCouple::IComponentInfo *> result;

    for (const auto &library : m_libraries)
    {
      if (kindOf(library->componentInfo()) == kind)
      {
        result.push_back(library->componentInfo());
      }
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

    // Only model components can be instantiated here; adapted-output factory
    // and workflow components register through the same entry point but
    // answer a different interface.
    auto *modelInfo = dynamic_cast<HydroCouple::IModelComponentInfo *>(info);

    if (!modelInfo)
    {
      message =
        kindOf(info) == ComponentKind::AdapterFactory
          ? QStringLiteral("'%1' is an adapted-output factory; adapters "
                           "attach to connections, they are not placed as "
                           "components")
              .arg(componentId)
          : QStringLiteral("'%1' is not a model component").arg(componentId);
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

  bool ComponentRegistry::assignInstanceIdentity(
    HydroCouple::IModelComponent &component, const QString &instanceId,
    QString &message)
  {
    message.clear();
    if (instanceId.isEmpty() || component.status() !=
          HydroCouple::IModelComponent::ComponentStatus::Created)
    {
      message = QStringLiteral("Assign a nonempty component instance ID before initialization");
      return false;
    }
    try
    {
      const std::string id = instanceId.toStdString();
      for (HydroCouple::IArgument *argument : component.arguments())
      {
        if (!argument || argument->id() != "hydrocouple.instance_id") continue;
        std::string diagnostic;
        if (argument->dataKind() != HydroCouple::DataKind::String ||
            argument->shape() != std::vector<int64_t>{1} ||
            argument->role() != HydroCouple::IArgument::ArgumentRole::Configuration ||
            !argument->initialize(id, HydroCouple::IArgument::ArgumentInputType::String,
                                  diagnostic) || component.id() != id)
        {
          message = QStringLiteral("Cannot assign instance '%1': %2").arg(
            instanceId, diagnostic.empty()
              ? QStringLiteral("hydrocouple.instance_id must accept a scalar String and update id()")
              : QString::fromStdString(diagnostic));
          return false;
        }
        return true;
      }
      if (auto *sdk = dynamic_cast<HydroCouple::SDK::AbstractModelComponent *>(&component))
      {
        std::string diagnostic;
        if (sdk->setInstanceId(id, diagnostic)) return true;
        message = QString::fromStdString(diagnostic);
        return false;
      }
      if (component.id() == id) return true;
      message = QStringLiteral("Component '%1' cannot accept instance '%2'; expose "
                               "the hydrocouple.instance_id configuration argument")
                  .arg(QString::fromStdString(component.id()), instanceId);
    }
    catch (const std::exception &error)
    {
      message = QString::fromUtf8(error.what());
    }
    return false;
  }

  std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent>
  ComponentRegistry::createAdaptedOutputFactory(const QString &componentId,
                                                QString &message)
  {
    HydroCouple::IComponentInfo *info = entry(componentId);

    if (!info)
    {
      message = QStringLiteral("no component registered with id '%1'")
                  .arg(componentId);
      return nullptr;
    }

    auto *factoryInfo =
      dynamic_cast<HydroCouple::IAdaptedOutputFactoryComponentInfo *>(info);

    if (!factoryInfo)
    {
      message = QStringLiteral("'%1' is not an adapted-output factory")
                  .arg(componentId);
      return nullptr;
    }

    std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent> factory =
      factoryInfo->createComponentInstance();

    if (!factory)
    {
      message = QStringLiteral("'%1' failed to create a factory instance")
                  .arg(componentId);
    }

    return factory;
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
