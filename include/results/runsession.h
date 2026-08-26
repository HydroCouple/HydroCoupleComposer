/*!
 * \file   runsession.h
 * \author Caleb Buahin
 * \brief  RunSession — one finished run, reopened for looking at.
 *
 * A run manifest is a catalog: what was recorded, by which component, in
 * which file, of what shape and units, over what time axis. Reopening one
 * needs none of the model libraries that produced it — that is the whole
 * point of the manifest, and the reason a results viewer can be
 * model-agnostic.
 *
 * The catalog is read at once, because it is small and the browser shows all
 * of it. The artifacts are opened lazily, one component at a time, because
 * they are not: a run with a hundred recorded variables would otherwise open
 * a hundred files to draw a tree.
 */

#ifndef HYDROCOUPLECOMPOSER_RESULTS_RUNSESSION_H
#define HYDROCOUPLECOMPOSER_RESULTS_RUNSESSION_H

#include "hydrocouplesdk/component/resultsmodelcomponent.h"
#include "hydrocouplesdk/io/runmanifest.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

namespace HydroCouple
{
  class IComponentDataItem;
}

namespace HydroCouple::Composer
{
  /*!
   * \brief A finished run, browsable without its model libraries.
   */
  class RunSession
  {
    public:
      ~RunSession();

      RunSession(const RunSession &) = delete;
      RunSession &operator=(const RunSession &) = delete;

      /*!
       * \brief Reads the manifest at \a manifestPath.
       *
       * Only the manifest: no artifact is touched here, so a catalog whose
       * files have been moved still opens and says what it expected to find.
       *
       * \param manifestPath The run manifest to read.
       * \param[out] message Diagnostic on failure.
       * \returns The session, or nullptr.
       */
      [[nodiscard]] static std::unique_ptr<RunSession> open(
        const QString &manifestPath, QString &message);

      //! \returns The manifest's path on disk.
      [[nodiscard]] QString manifestPath() const;

      //! \returns The run's caption, or its id when it has none.
      [[nodiscard]] QString title() const;

      //! \returns The manifest itself, catalog and all.
      [[nodiscard]] const SDK::IO::RunManifest &manifest() const;

      /*!
       * \brief The components that recorded something, in catalog order.
       *
       * From the catalog rather than from the outcome list: a component that
       * ran but recorded nothing has nothing to browse, and one that
       * recorded something is worth showing even if the run failed after it.
       */
      [[nodiscard]] QStringList componentIds() const;

      /*!
       * \brief The catalog entries \a componentId recorded, in order.
       * \param componentId Component whose entries are wanted.
       */
      [[nodiscard]] QVector<const SDK::IO::ResultEntry *> entriesFor(
        const QString &componentId) const;

      /*!
       * \brief Opens \a componentId's artifacts, or returns what was opened.
       *
       * The expensive half, kept apart from the catalog: this is where the
       * files are read, and where a moved or corrupt artifact is discovered.
       *
       * \param componentId Component to open.
       * \param[out] message Diagnostic on failure.
       * \returns The reopened component, or nullptr.
       */
      SDK::ResultsModelComponent *component(const QString &componentId,
                                            QString &message);

      /*!
       * \brief One recorded item, ready to read values from.
       * \param componentId The component that recorded it.
       * \param itemId The item's identifier.
       * \param[out] message Diagnostic on failure.
       * \returns The item, or nullptr.
       */
      HydroCouple::IComponentDataItem *item(const QString &componentId,
                                            const QString &itemId,
                                            QString &message);

    private:
      RunSession(QString manifestPath, SDK::IO::RunManifest manifest);

      QString m_manifestPath;
      SDK::IO::RunManifest m_manifest;

      //! Opened components, by id. Kept because reopening one re-reads its
      //! artifacts, and a browser asks for the same one repeatedly.
      QHash<QString, std::shared_ptr<SDK::ResultsModelComponent>> m_opened;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RESULTS_RUNSESSION_H
