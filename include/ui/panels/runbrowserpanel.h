/*!
 * \file   runbrowserpanel.h
 * \author Caleb Buahin
 * \brief  RunBrowserPanel — the view onto finished runs.
 *
 * A second view of RunBrowserModel, which owns the open runs: the panel
 * holds no catalog of its own, so what it shows and what a plot reads later
 * cannot come apart.
 *
 * Opening a file is the window's business, not the panel's — the panel asks,
 * and whoever owns the model answers. That keeps the file dialog, and the
 * decision about where runs come from, in one place.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_PANELS_RUNBROWSERPANEL_H
#define HYDROCOUPLECOMPOSER_UI_PANELS_RUNBROWSERPANEL_H

#include <QModelIndex>
#include <QWidget>

class QToolButton;
class QTreeView;

namespace HydroCouple::Composer
{
  class RunBrowserModel;

  /*!
   * \brief Browses the runs a RunBrowserModel holds.
   */
  class RunBrowserPanel : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the panel.
       * \param parent Parent widget.
       */
      explicit RunBrowserPanel(QWidget *parent = nullptr);

      ~RunBrowserPanel() override;

      /*!
       * \brief Shows \a model, or nothing when null.
       * \param model The runs to browse; not owned.
       */
      void setModel(RunBrowserModel *model);

      //! \returns The model being browsed, or nullptr.
      [[nodiscard]] RunBrowserModel *model() const;

      /*!
       * \brief The run the selection is in, as a row, or -1.
       *
       * A row rather than a session, because closing one is done by row and
       * a panel that handed out pointers would invite holding a stale one.
       */
      [[nodiscard]] int currentRunRow() const;

    Q_SIGNALS:
      //! Emitted when the user asks to open a run.
      void openRunRequested();

      /*!
       * \brief Emitted when the user asks to see a recorded item on the map.
       * \param runRow Which open run it belongs to.
       * \param componentId The component that recorded it.
       * \param itemId The item's identifier.
       *
       * Identifiers rather than the item itself: the panel browses a catalog
       * and does not open artifacts, and whoever owns the runs is the one
       * that can say whether this one will open at all.
       */
      void showItemRequested(int runRow, const QString &componentId,
                             const QString &itemId);

    private:
      /*!
       * \brief The selected row when it names a recorded item, else invalid.
       */
      [[nodiscard]] QModelIndex currentItemIndex() const;

      void updateButtons();

      QTreeView *m_tree = nullptr;
      QToolButton *m_openButton = nullptr;
      QToolButton *m_closeButton = nullptr;
      QToolButton *m_showButton = nullptr;

      RunBrowserModel *m_model = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_PANELS_RUNBROWSERPANEL_H
