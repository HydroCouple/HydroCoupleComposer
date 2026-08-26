/*!
 * \file   crsselectiondialog.h
 * \author Caleb Buahin
 * \brief  CrsSelectionDialog — choosing a coordinate reference system.
 *
 * One dialog for both questions the map asks: what system is the map drawn
 * in, and what system is this layer's data already in. They are different
 * questions with the same answer type, so they share the chooser and differ
 * only in what the caller does with the result.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_DIALOGS_CRSSELECTIONDIALOG_H
#define HYDROCOUPLECOMPOSER_UI_DIALOGS_CRSSELECTIONDIALOG_H

#include "gis/crscatalog.h"

#include <QDialog>

#include <memory>

class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;

namespace HydroCouple::Composer
{
  class SpatialReference;

  /*!
   * \brief Browses the CRS database and returns one system.
   */
  class CrsSelectionDialog : public QDialog
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs the chooser.
       * \param parent Parent widget.
       */
      explicit CrsSelectionDialog(QWidget *parent = nullptr);

      ~CrsSelectionDialog() override;

      /*!
       * \brief Selects \a current when it is in the catalogue.
       *
       * So that opening the chooser shows what is already in use rather than
       * an arbitrary first row.
       *
       * \param current The system in use, or nullptr.
       */
      void setCurrentCrs(const SpatialReference *current);

      /*!
       * \brief The authority and code chosen, e.g. "EPSG:3857", or empty.
       */
      [[nodiscard]] QString selectedAuthCode() const;

      /*!
       * \brief Builds the chosen system.
       *
       * Built here rather than held, because a caller that wants it usually
       * wants to own it, and PROJ resolves the definition on construction.
       *
       * \param[out] message Diagnostic on failure.
       * \returns The system, or nullptr when nothing is selected.
       */
      [[nodiscard]] std::shared_ptr<SpatialReference> selectedCrs(
        QString &message) const;

    private:
      void buildForm();
      void refreshList();
      void updatePreview();

      //! \returns The kind the type filter is set to.
      [[nodiscard]] CrsKind selectedKind() const;

      QLineEdit *m_searchEdit = nullptr;
      QComboBox *m_kindCombo = nullptr;
      QTreeWidget *m_list = nullptr;
      QPlainTextEdit *m_preview = nullptr;
      QDialogButtonBox *m_buttons = nullptr;

      //! Reselected after a filter change, so a search does not lose it.
      QString m_selectedAuthCode;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_DIALOGS_CRSSELECTIONDIALOG_H
