/*!
 * \file   welcomepage.h
 * \author Caleb Buahin
 * \brief  WelcomePage — somewhere to start.
 *
 * A composition needs components before it needs anything else, and an
 * empty canvas says nothing about where they come from. This is the first
 * tab: what to do next, and what was open last.
 *
 * It asks for things rather than doing them — every entry is a signal the
 * window acts on — so opening a document still goes through the one path
 * that also records it as recent.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_WELCOMEPAGE_H
#define HYDROCOUPLECOMPOSER_UI_WELCOMEPAGE_H

#include <QWidget>

class QCheckBox;
class QListWidget;

namespace HydroCouple::Composer
{
  class RecentCompositions;

  /*!
   * \brief The start page: what to do next, and what was open last.
   */
  class WelcomePage : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds the page over \a recent.
       * \param recent The remembered documents. Not owned; must outlive this.
       * \param parent Optional Qt parent.
       */
      explicit WelcomePage(RecentCompositions *recent,
                           QWidget *parent = nullptr);

    Q_SIGNALS:
      void newRequested();
      void openRequested();
      void loadComponentsRequested();

      /*!
       * \brief A remembered document was chosen.
       * \param filePath The document to open.
       */
      void openRecentRequested(const QString &filePath);

    private:
      //! Refills the recent list from the store.
      void refresh();

      RecentCompositions *m_recent = nullptr;
      QListWidget *m_list = nullptr;
      QCheckBox *m_showOnStartUp = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_WELCOMEPAGE_H
