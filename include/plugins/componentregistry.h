/*!
 * \file   componentregistry.h
 * \author Caleb Buahin
 * \brief  ComponentRegistry — the set of component libraries known to a session.
 *
 * Scans directories for component libraries, keeps the successfully loaded
 * ones, and reports on the rejected ones. Two consumers matter:
 *
 *  - the composition canvas and component palette, which browse `entries()`;
 *  - the composition document, which calls `createInstance()` to realise the
 *    components a document names. The document — not this registry — owns
 *    those instances, and so is what supplies `ModelInitializer` with its
 *    `ComponentResolver` (A4); the registry only knows how to make them.
 *
 * \threadsafety Not thread-safe; drive from the GUI thread. Instances handed
 * out may afterwards be driven on worker threads by the simulation manager.
 */

#ifndef HYDROCOUPLECOMPOSER_PLUGINS_COMPONENTREGISTRY_H
#define HYDROCOUPLECOMPOSER_PLUGINS_COMPONENTREGISTRY_H

#include "plugins/componentlibrary.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

namespace HydroCouple::Composer
{

  /*!
   * \brief One component library that failed to load, and why.
   */
  struct ComponentLoadFailure
  {
      QString filePath;
      QString message;
  };

  /*!
   * \brief The component libraries available to this session.
   */
  class ComponentRegistry : public QObject
  {
      Q_OBJECT

    public:
      explicit ComponentRegistry(QObject *parent = nullptr);

      ~ComponentRegistry() override;

      /*!
       * \brief Loads one component library and keeps it on success.
       * \param filePath Path to the shared library to load.
       * \param[out] message Diagnostic when the library is rejected.
       * \returns The library's metadata, or nullptr on failure.
       *
       * Loading the same path twice is a no-op that returns the existing
       * entry: component info objects are library-owned singletons, so a
       * second load would hand out a duplicate identity for one library.
       */
      HydroCouple::IComponentInfo *loadLibrary(const QString &filePath,
                                               QString &message);

      /*!
       * \brief Loads every shared library directly inside \a directoryPath.
       * \returns The number of libraries newly loaded.
       *
       * Files that are not component libraries are not errors — a plugin
       * directory legitimately contains dependencies alongside components —
       * but each rejection is recorded in failures() so a component that was
       * *meant* to load can still be diagnosed.
       */
      int scanDirectory(const QString &directoryPath);

      /*!
       * \brief The directories scanned by refresh().
       */
      [[nodiscard]] QStringList searchPaths() const;

      void setSearchPaths(const QStringList &paths);

      /*!
       * \brief Clears everything loaded and rescans searchPaths().
       * \returns The number of libraries loaded.
       */
      int refresh();

      /*!
       * \brief What a loaded library's info answers to.
       */
      enum class ComponentKind
      {
        Model,          //!< IModelComponentInfo — placeable on the canvas.
        AdapterFactory, //!< IAdaptedOutputFactoryComponentInfo — attaches
                        //!< to connections, never placed as a component.
        Other           //!< Loads, but answers neither interface (e.g. a
                        //!< future workflow component).
      };

      /*!
       * \brief Classifies an info by the interface it answers.
       *
       * A cross-image dynamic_cast — safe under the interface headers'
       * default-visibility push, the same guarantee createInstance()
       * already leans on.
       */
      [[nodiscard]] static ComponentKind kindOf(
        HydroCouple::IComponentInfo *info);

      /*!
       * \brief Metadata for every loaded component library.
       */
      [[nodiscard]] std::vector<HydroCouple::IComponentInfo *> entries() const;

      /*!
       * \brief entries() filtered to one kind.
       */
      [[nodiscard]] std::vector<HydroCouple::IComponentInfo *> entries(
        ComponentKind kind) const;

      /*!
       * \brief Metadata for the loaded library whose component id matches,
       *        or nullptr.
       */
      [[nodiscard]] HydroCouple::IComponentInfo *entry(
        const QString &componentId) const;

      /*!
       * \brief Libraries rejected during the most recent scan.
       */
      [[nodiscard]] std::vector<ComponentLoadFailure> failures() const;

      /*!
       * \brief Creates a component instance from a loaded library.
       * \param componentId Identifier of a registered IModelComponentInfo.
       * \param[out] message Diagnostic on failure.
       * \returns The new instance, or nullptr. The caller owns it and must
       *          destroy it before this registry is destroyed.
       */
      [[nodiscard]] std::unique_ptr<HydroCouple::IModelComponent>
      createInstance(const QString &componentId, QString &message);

      /*!
       * \brief Creates an adapted-output factory instance from a loaded
       *        library.
       * \param componentId Identifier of a registered
       *        IAdaptedOutputFactoryComponentInfo.
       * \param[out] message Diagnostic on failure.
       * \returns The new factory, or nullptr. The caller owns it and must
       *          destroy it before this registry is destroyed.
       */
      [[nodiscard]] std::unique_ptr<HydroCouple::IAdaptedOutputFactoryComponent>
      createAdaptedOutputFactory(const QString &componentId, QString &message);

      /*!
       * \brief Drops every loaded library.
       *
       * Undefined behaviour follows if component instances created from these
       * libraries outlive this call, so callers must release instances first.
       */
      void clear();

    Q_SIGNALS:
      //! Emitted after any change to the set of loaded libraries.
      void registryChanged();

    private:
      QStringList m_searchPaths;
      std::vector<std::unique_ptr<ComponentLibrary>> m_libraries;
      std::vector<ComponentLoadFailure> m_failures;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PLUGINS_COMPONENTREGISTRY_H
