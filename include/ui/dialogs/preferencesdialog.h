/*!
 * \file   preferencesdialog.h
 * \author Caleb Buahin
 * \brief  PreferencesDialog — the editor over PreferencesManager.
 *
 * Categories down the left, one scrolled page per category on the right,
 * and the openswmm.gui button row: Reset to defaults | Apply / Cancel / OK.
 * The dialog owns no preference: it reads the manager into its widgets on
 * open and writes them back on Apply and OK, so Cancel after any amount of
 * editing changes nothing. Behaviour follows openswmm.gui's dialog; the
 * code does not (that program is GPL-3.0).
 */

#ifndef HYDROCOUPLECOMPOSER_UI_DIALOGS_PREFERENCESDIALOG_H
#define HYDROCOUPLECOMPOSER_UI_DIALOGS_PREFERENCESDIALOG_H

#include <QColor>
#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;

namespace HydroCouple::Composer
{
  class PreferencesManager;

  /*!
   * \brief The global preferences, as a dialog.
   */
  class PreferencesDialog : public QDialog
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds the dialog over \a preferences.
       * \param preferences The manager edited. Not owned; must outlive this.
       * \param parent Optional Qt parent.
       */
      explicit PreferencesDialog(PreferencesManager *preferences,
                                 QWidget *parent = nullptr);

      /*!
       * \brief Shows the page whose category is titled \a title.
       *
       * Unknown titles leave the current page alone.
       */
      void openAtCategory(const QString &title);

      /*!
       * \brief The categories, in the order the list shows them.
       */
      [[nodiscard]] QStringList categories() const;

      /*!
       * \brief Installs how "Choose…" beside the default CRS picks one.
       *
       * Given the current "AUTHORITY:CODE", returns the chosen one, or an
       * empty string when the user cancelled. The dialog does not know how
       * a CRS is chosen — that needs the GIS catalogue — so the window
       * supplies it; without a chooser the button is hidden.
       */
      void setCrsChooser(std::function<QString(const QString &)> chooser);

      /*!
       * \brief Copies every widget into the manager.
       *
       * Public so a test can drive Apply without finding the button.
       */
      void apply();

      /*!
       * \brief Refills every widget from the manager.
       */
      void revert();

    private:
      QWidget *buildGeneralPage();
      QWidget *buildAppearancePage();
      QWidget *buildComponentsPage();
      QWidget *buildSelectionPage();
      QWidget *buildMapPage();
      QWidget *buildScenePage();
      void addCategory(const QString &title, QWidget *page);
      void resetToDefaults();

      PreferencesManager *m_preferences = nullptr;
      std::function<QString(const QString &)> m_crsChooser;

      QListWidget *m_categories = nullptr;
      QStackedWidget *m_pages = nullptr;

      // General
      QCheckBox *m_showWelcome = nullptr;
      QSpinBox *m_recentLimit = nullptr;
      // Appearance
      QRadioButton *m_themeSystem = nullptr;
      QRadioButton *m_themeLight = nullptr;
      QRadioButton *m_themeDark = nullptr;
      // Components
      QListWidget *m_searchPaths = nullptr;
      QCheckBox *m_rescanOnStartUp = nullptr;
      // Selection & picking
      QDoubleSpinBox *m_pickTolerance = nullptr;
      QSpinBox *m_dragThreshold = nullptr;
      QDoubleSpinBox *m_snapTolerance = nullptr;
      QToolButton *m_selectionColor = nullptr;
      // Map
      QLineEdit *m_defaultCrs = nullptr;
      QToolButton *m_chooseCrs = nullptr;
      // 3D view
      QToolButton *m_sceneBackground = nullptr;
      QComboBox *m_sceneProjection = nullptr;
      QDoubleSpinBox *m_exaggeration = nullptr;
      QCheckBox *m_showGizmo = nullptr;
      QSpinBox *m_gizmoSize = nullptr;
      QComboBox *m_gizmoCorner = nullptr;
      QComboBox *m_linkViews = nullptr;
      QDoubleSpinBox *m_orbitSensitivity = nullptr;
      QCheckBox *m_invertWheel = nullptr;
      QComboBox *m_panModifier = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_DIALOGS_PREFERENCESDIALOG_H
