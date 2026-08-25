/*!
 * \file   componentlibrary.h
 * \author Caleb Buahin
 * \brief  ComponentLibrary — one loaded component shared library.
 *
 * Owns the OS handle and hands out the library's HydroCouple::IComponentInfo.
 * Loading is deliberately staged so that a library which cannot be trusted is
 * never asked to run C++ code:
 *
 *   1. open the file (dlopen / LoadLibrary)
 *   2. resolve and call the pure-C ABI stamp entry point
 *   3. compare it against this host's stamp — bail out on disagreement
 *   4. only now resolve and call the component-info entry point
 *
 * A library that is not a component library at all, or was built by a
 * different toolchain, is rejected at step 2 or 3 with a diagnostic. There is
 * no step at which a mismatched C++ symbol is invoked.
 */

#ifndef HYDROCOUPLECOMPOSER_PLUGINS_COMPONENTLIBRARY_H
#define HYDROCOUPLECOMPOSER_PLUGINS_COMPONENTLIBRARY_H

#include "plugins/componentabi.h"

#include <QString>

#include <memory>

namespace HydroCouple::Composer
{

  /*!
   * \brief A single dynamically loaded HydroCouple component library.
   *
   * Move-only: the OS handle is a unique resource. Unloading happens in the
   * destructor, so a library outlives every component instance created from
   * it only if the caller destroys those instances first (see componentabi.h).
   */
  class ComponentLibrary
  {
    public:
      ~ComponentLibrary();

      ComponentLibrary(const ComponentLibrary &) = delete;
      ComponentLibrary &operator=(const ComponentLibrary &) = delete;
      ComponentLibrary(ComponentLibrary &&other) noexcept;
      ComponentLibrary &operator=(ComponentLibrary &&other) noexcept;

      /*!
       * \brief Loads a component library, validating its ABI stamp first.
       * \param filePath Absolute path to the shared library.
       * \param[out] message Human-readable diagnostic on failure; untouched on success.
       * \returns The loaded library, or nullptr if it could not be loaded or
       *          is not a compatible HydroCouple component library.
       */
      [[nodiscard]] static std::unique_ptr<ComponentLibrary> load(
        const QString &filePath, QString &message);

      /*!
       * \brief The component metadata published by this library.
       * \returns A pointer owned by the library; never delete it, and do not
       *          use it after this ComponentLibrary is destroyed.
       */
      [[nodiscard]] HydroCouple::IComponentInfo *componentInfo() const noexcept;

      /*!
       * \brief Absolute path this library was loaded from.
       */
      [[nodiscard]] QString filePath() const;

      /*!
       * \brief The ABI stamp the library reported.
       * \returns The library's stamp, or HYDROCOUPLE_COMPONENT_UNSTAMPED for a
       *          library loaded through the legacy `CreateComponentInfo`
       *          convention, whose toolchain could not be verified.
       */
      [[nodiscard]] QString abiStamp() const;

      /*!
       * \brief Whether this library was loaded without a verifiable ABI stamp.
       *
       * True for legacy components. Such a library was loaded on trust; hosts
       * should say so rather than presenting it as validated.
       */
      [[nodiscard]] bool isUnstamped() const;

      /*!
       * \brief The stamp this host requires; a library must match it exactly.
       */
      [[nodiscard]] static QString hostAbiStamp();

      /*!
       * \brief The platform's shared-library suffix (".dylib"/".so"/".dll").
       */
      [[nodiscard]] static QString librarySuffix();

    private:
      ComponentLibrary(void *handle, QString filePath, QString abiStamp,
                       HydroCouple::IComponentInfo *info);

      //! Loads a component exporting only the legacy unstamped factory.
      [[nodiscard]] static std::unique_ptr<ComponentLibrary> loadLegacy(
        void *handle, const QString &absolutePath, QString &message);

      void unload() noexcept;

      void *m_handle = nullptr;
      QString m_filePath;
      QString m_abiStamp;
      HydroCouple::IComponentInfo *m_info = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PLUGINS_COMPONENTLIBRARY_H
