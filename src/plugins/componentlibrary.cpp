#include "plugins/componentlibrary.h"

#include <QFileInfo>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace HydroCouple::Composer
{

  namespace
  {
    //! Opens a shared library, returning nullptr and a diagnostic on failure.
    void *openLibrary(const QString &filePath, QString &message)
    {
#if defined(_WIN32)
      HMODULE handle = ::LoadLibraryW(
        reinterpret_cast<const wchar_t *>(filePath.utf16()));

      if (!handle)
      {
        message = QStringLiteral("cannot load '%1': Windows error %2")
                    .arg(filePath)
                    .arg(::GetLastError());
      }

      return handle;
#else
      // RTLD_LOCAL keeps a component's symbols out of the global namespace, so
      // two components carrying different builds of the same static
      // dependency cannot silently bind to each other's copies.
      void *handle = ::dlopen(filePath.toUtf8().constData(),
                              RTLD_NOW | RTLD_LOCAL);

      if (!handle)
      {
        const char *error = ::dlerror();
        message = QStringLiteral("cannot load '%1': %2")
                    .arg(filePath,
                         QString::fromUtf8(error ? error : "unknown error"));
      }

      return handle;
#endif
    }

    //! Resolves a symbol, or nullptr when absent.
    void *resolveSymbol(void *handle, const char *name)
    {
#if defined(_WIN32)
      return reinterpret_cast<void *>(
        ::GetProcAddress(static_cast<HMODULE>(handle), name));
#else
      ::dlerror(); // clear any stale error before the lookup
      return ::dlsym(handle, name);
#endif
    }

    void closeLibrary(void *handle) noexcept
    {
      if (!handle)
      {
        return;
      }

#if defined(_WIN32)
      ::FreeLibrary(static_cast<HMODULE>(handle));
#else
      ::dlclose(handle);
#endif
    }
  } // namespace

  ComponentLibrary::ComponentLibrary(void *handle, QString filePath,
                                     QString abiStamp,
                                     HydroCouple::IComponentInfo *info)
    : m_handle(handle),
      m_filePath(std::move(filePath)),
      m_abiStamp(std::move(abiStamp)),
      m_info(info)
  {
  }

  ComponentLibrary::~ComponentLibrary()
  {
    unload();
  }

  ComponentLibrary::ComponentLibrary(ComponentLibrary &&other) noexcept
    : m_handle(other.m_handle),
      m_filePath(std::move(other.m_filePath)),
      m_abiStamp(std::move(other.m_abiStamp)),
      m_info(other.m_info)
  {
    other.m_handle = nullptr;
    other.m_info = nullptr;
  }

  ComponentLibrary &ComponentLibrary::operator=(
    ComponentLibrary &&other) noexcept
  {
    if (this != &other)
    {
      unload();

      m_handle = other.m_handle;
      m_filePath = std::move(other.m_filePath);
      m_abiStamp = std::move(other.m_abiStamp);
      m_info = other.m_info;

      other.m_handle = nullptr;
      other.m_info = nullptr;
    }

    return *this;
  }

  void ComponentLibrary::unload() noexcept
  {
    closeLibrary(m_handle);
    m_handle = nullptr;
    m_info = nullptr;
  }

  std::unique_ptr<ComponentLibrary> ComponentLibrary::loadLegacy(
    void *handle, const QString &absolutePath, QString &message)
  {
    auto legacyFn = reinterpret_cast<HydroCoupleLegacyComponentInfoFn>(
      resolveSymbol(handle, HYDROCOUPLE_COMPONENT_LEGACY_INFO_SYMBOL));

    if (!legacyFn)
    {
      message = QStringLiteral("'%1' has no %2")
                  .arg(absolutePath,
                       QLatin1String(HYDROCOUPLE_COMPONENT_LEGACY_INFO_SYMBOL));
      closeLibrary(handle);
      return nullptr;
    }

    // Unavoidably best-effort: an unstamped library cannot be checked before
    // it is called. This is the risk the stamped convention exists to remove,
    // and is why legacy libraries are surfaced as unstamped in the UI.
    HydroCouple::IModelComponentInfo *info = legacyFn();

    if (!info)
    {
      message = QStringLiteral("'%1' returned no component info from %2")
                  .arg(absolutePath,
                       QLatin1String(HYDROCOUPLE_COMPONENT_LEGACY_INFO_SYMBOL));
      closeLibrary(handle);
      return nullptr;
    }

    info->setLibraryFilePath(absolutePath.toStdString());

    return std::unique_ptr<ComponentLibrary>(new ComponentLibrary(
      handle, absolutePath,
      QStringLiteral(HYDROCOUPLE_COMPONENT_UNSTAMPED), info));
  }

  std::unique_ptr<ComponentLibrary> ComponentLibrary::load(
    const QString &filePath, QString &message)
  {
    const QFileInfo fileInfo(filePath);

    if (!fileInfo.isFile())
    {
      message = QStringLiteral("no such file: '%1'").arg(filePath);
      return nullptr;
    }

    const QString absolutePath = fileInfo.absoluteFilePath();

    void *handle = openLibrary(absolutePath, message);

    if (!handle)
    {
      return nullptr;
    }

    // Step 2 — the pure-C stamp. This is the only symbol that may be called
    // before compatibility is established.
    auto abiFn = reinterpret_cast<HydroCoupleComponentAbiFn>(
      resolveSymbol(handle, HYDROCOUPLE_COMPONENT_ABI_SYMBOL));

    if (!abiFn)
    {
      // No stamp: this may still be a component written against the older
      // convention that HydroCouple's Python bindings use. Falling back keeps
      // one component ecosystem instead of two.
      if (resolveSymbol(handle, HYDROCOUPLE_COMPONENT_LEGACY_INFO_SYMBOL))
      {
        return loadLegacy(handle, absolutePath, message);
      }

      message = QStringLiteral(
                  "'%1' is not a HydroCouple component library (no %2 and no %3)")
                  .arg(absolutePath,
                       QLatin1String(HYDROCOUPLE_COMPONENT_ABI_SYMBOL),
                       QLatin1String(HYDROCOUPLE_COMPONENT_LEGACY_INFO_SYMBOL));
      closeLibrary(handle);
      return nullptr;
    }

    const char *reportedStamp = abiFn();

    if (!reportedStamp)
    {
      message = QStringLiteral("'%1' reported a null ABI stamp")
                  .arg(absolutePath);
      closeLibrary(handle);
      return nullptr;
    }

    // Step 3 — refuse anything this toolchain cannot safely talk to.
    const QString stamp = QString::fromUtf8(reportedStamp);

    if (stamp != hostAbiStamp())
    {
      message = QStringLiteral(
                  "'%1' was built against an incompatible toolchain or "
                  "interface version.\n  library expects: %2\n  host provides: %3")
                  .arg(absolutePath, stamp, hostAbiStamp());
      closeLibrary(handle);
      return nullptr;
    }

    // Step 4 — now that the stamp matches, C++ across the boundary is defined.
    auto infoFn = reinterpret_cast<HydroCoupleComponentInfoFn>(
      resolveSymbol(handle, HYDROCOUPLE_COMPONENT_INFO_SYMBOL));

    if (!infoFn)
    {
      message = QStringLiteral("'%1' has a valid ABI stamp but no %2")
                  .arg(absolutePath,
                       QLatin1String(HYDROCOUPLE_COMPONENT_INFO_SYMBOL));
      closeLibrary(handle);
      return nullptr;
    }

    HydroCouple::IComponentInfo *info = infoFn();

    if (!info)
    {
      message = QStringLiteral("'%1' returned no component info")
                  .arg(absolutePath);
      closeLibrary(handle);
      return nullptr;
    }

    // Provenance: the interface carries the originating path for the composer
    // and for run manifests, and only the loader knows it.
    info->setLibraryFilePath(absolutePath.toStdString());

    return std::unique_ptr<ComponentLibrary>(
      new ComponentLibrary(handle, absolutePath, stamp, info));
  }

  HydroCouple::IComponentInfo *ComponentLibrary::componentInfo() const noexcept
  {
    return m_info;
  }

  QString ComponentLibrary::filePath() const
  {
    return m_filePath;
  }

  QString ComponentLibrary::abiStamp() const
  {
    return m_abiStamp;
  }

  bool ComponentLibrary::isUnstamped() const
  {
    return m_abiStamp == QStringLiteral(HYDROCOUPLE_COMPONENT_UNSTAMPED);
  }

  QString ComponentLibrary::hostAbiStamp()
  {
    return QStringLiteral(HYDROCOUPLE_COMPONENT_ABI_STAMP);
  }

  QString ComponentLibrary::librarySuffix()
  {
#if defined(_WIN32)
    return QStringLiteral(".dll");
#elif defined(__APPLE__)
    return QStringLiteral(".dylib");
#else
    return QStringLiteral(".so");
#endif
  }

} // namespace HydroCouple::Composer
