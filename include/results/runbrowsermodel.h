/*!
 * \file   runbrowsermodel.h
 * \author Caleb Buahin
 * \brief  RunBrowserModel — opened runs, their components and what they hold.
 *
 * Three levels, which is what the catalog is: a run, the components that
 * recorded something in it, and the items each recorded. Everything shown
 * comes from the manifest — kind, shape, units, mesh, time axis — so a row
 * says what the run says and not what the viewer guessed.
 *
 * The model owns the sessions. Two runs are open at once routinely, since
 * comparing one against another is the reason a viewer exists.
 */

#ifndef HYDROCOUPLECOMPOSER_RESULTS_RUNBROWSERMODEL_H
#define HYDROCOUPLECOMPOSER_RESULTS_RUNBROWSERMODEL_H

#include "results/runsession.h"

#include <QAbstractItemModel>

#include <memory>
#include <vector>

namespace HydroCouple::Composer
{
  /*!
   * \brief A tree of open runs, from their manifests.
   */
  class RunBrowserModel : public QAbstractItemModel
  {
      Q_OBJECT

    public:
      /*!
       * \brief What each column reports.
       *
       * Named rather than numbered, because a view that hides a column and
       * a test that reads one both have to agree about which is which.
       */
      enum Column
      {
        NameColumn = 0,
        KindColumn,
        ShapeColumn,
        UnitsColumn,
        MeshColumn,
        TimeColumn,
        ColumnCount
      };

      /*!
       * \brief The run a row belongs to, for a caller holding an index.
       *
       * A user role rather than a method taking an index, so a view can sort
       * or filter on it without knowing how the tree is built.
       */
      static constexpr int ComponentIdRole = Qt::UserRole + 1;
      static constexpr int ItemIdRole = Qt::UserRole + 2;

      explicit RunBrowserModel(QObject *parent = nullptr);

      ~RunBrowserModel() override;

      /*!
       * \brief Opens the run at \a manifestPath and adds it to the tree.
       * \param manifestPath The run manifest to read.
       * \param[out] message Diagnostic on failure.
       * \returns The session, or nullptr; the model keeps ownership.
       */
      RunSession *addRun(const QString &manifestPath, QString &message);

      /*!
       * \brief Forgets the run at \a row.
       * \param row Index into the open runs.
       */
      void removeRun(int row);

      //! \returns How many runs are open.
      [[nodiscard]] int runCount() const;

      /*!
       * \brief The run at \a row, or nullptr.
       * \param row Index into the open runs.
       */
      [[nodiscard]] RunSession *run(int row) const;

      /*!
       * \brief The run a row belongs to, at any depth.
       * \param index Any index in the tree.
       */
      [[nodiscard]] RunSession *runFor(const QModelIndex &index) const;

      // ── QAbstractItemModel ───────────────────────────────────────────────

      [[nodiscard]] QModelIndex index(
        int row, int column,
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;

      [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] int columnCount(
        const QModelIndex &parent = QModelIndex()) const override;

      [[nodiscard]] QVariant data(const QModelIndex &index,
                                  int role = Qt::DisplayRole) const override;

      [[nodiscard]] QVariant headerData(
        int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    private:
      /*!
       * \brief What one row stands for.
       *
       * A tree of three fixed levels does not need a node per row: the level
       * plus two integers locates any row, and an internal id encodes them
       * in the space QModelIndex already provides.
       */
      enum class Level
      {
        Run,
        Component,
        Item,
      };

      struct Location
      {
          Level level = Level::Run;
          int run = 0;
          int component = 0;
          int item = 0;
      };

      [[nodiscard]] Location locate(const QModelIndex &index) const;
      [[nodiscard]] static quintptr encode(const Location &location);
      [[nodiscard]] static Location decode(quintptr id);

      std::vector<std::unique_ptr<RunSession>> m_runs;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RESULTS_RUNBROWSERMODEL_H
