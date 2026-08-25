/*!
 * \file   ribbonbar.h
 * \author Caleb Buahin
 * \brief  RibbonBar — the tabbed ribbon across the top of the window.
 *
 * Each tab is a row of RibbonGroups, matching openswmm.gui's arrangement: a
 * controller owning one toolbar page per tab, so switching tabs swaps the
 * whole row rather than rearranging it.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_TOOLBARS_RIBBONBAR_H
#define HYDROCOUPLECOMPOSER_UI_TOOLBARS_RIBBONBAR_H

#include "ui/toolbars/ribbongroup.h"

#include <QHash>
#include <QString>
#include <QWidget>

class QStackedWidget;
class QTabBar;

namespace HydroCouple::Composer
{

  /*!
   * \brief A tabbed ribbon of captioned tool groups.
   */
  class RibbonBar : public QWidget
  {
      Q_OBJECT

    public:
      explicit RibbonBar(QWidget *parent = nullptr);

      /*!
       * \brief Adds a tab and returns its (empty) page.
       * \param id Stable identifier used by currentTab()/setCurrentTab().
       * \param title Tab label.
       * \returns The page widget; add groups with addGroup().
       */
      QWidget *addTab(const QString &id, const QString &title);

      /*!
       * \brief Adds a captioned group to a tab.
       * \param tabId Tab to add to; created if unknown.
       * \param caption Group caption.
       * \returns The group, ready for addAction().
       */
      RibbonGroup *addGroup(const QString &tabId, const QString &caption);

      /*!
       * \brief The identifier of the visible tab.
       */
      [[nodiscard]] QString currentTab() const;

      /*!
       * \brief Shows the tab with the given identifier.
       * \param id Tab to show; ignored when unknown.
       */
      void setCurrentTab(const QString &id);

      /*!
       * \brief Every group on a tab, in order.
       * \param tabId Tab to inspect.
       */
      [[nodiscard]] QList<RibbonGroup *> groups(const QString &tabId) const;

      /*!
       * \brief Applies a presentation mode to every group.
       * \param mode Presentation to apply.
       */
      void setMode(RibbonMode mode);

    Q_SIGNALS:
      /*!
       * \brief Emitted when the visible tab changes.
       * \param id Identifier of the newly visible tab.
       */
      void currentTabChanged(const QString &id);

    private:
      QTabBar *m_tabs = nullptr;
      QStackedWidget *m_pages = nullptr;
      QHash<QString, int> m_indexById;
      RibbonMode m_mode = RibbonMode::Full;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_TOOLBARS_RIBBONBAR_H
