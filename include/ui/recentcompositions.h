/*!
 * \file   recentcompositions.h
 * \author Caleb Buahin
 * \brief  RecentCompositions — the documents this user opened last.
 *
 * Kept in QSettings rather than beside any one composition, because it is a
 * property of the person using the program and not of the work.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_RECENTCOMPOSITIONS_H
#define HYDROCOUPLECOMPOSER_UI_RECENTCOMPOSITIONS_H

#include <QObject>
#include <QString>
#include <QStringList>

class QSettings;

namespace HydroCouple::Composer
{

  /*!
   * \brief The recently opened compositions, most recent first.
   */
  class RecentCompositions : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Opens the list.
       * \param settings Where to keep it; the application's own when null.
       *        Not owned.
       * \param parent Optional Qt parent.
       */
      explicit RecentCompositions(QSettings *settings = nullptr,
                                  QObject *parent = nullptr);

      /*!
       * \brief The remembered paths, most recent first.
       *
       * Paths that have since been deleted or moved are kept, not filtered:
       * "it was here and is gone" is worth telling the user, and a list
       * that silently shortened would leave them wondering.
       */
      [[nodiscard]] QStringList paths() const;

      /*!
       * \brief Records \a filePath as the most recently opened.
       *
       * Absolute, and de-duplicated — reopening a document moves it to the
       * top rather than listing it twice.
       */
      void remember(const QString &filePath);

      //! Forgets one path.
      void forget(const QString &filePath);

      //! Forgets all of them.
      void clear();

    Q_SIGNALS:
      //! Emitted whenever the remembered list changes.
      void changed();

    private:
      void write(const QStringList &paths);

      QSettings *m_settings = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_RECENTCOMPOSITIONS_H
