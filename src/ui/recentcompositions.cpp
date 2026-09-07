#include "ui/recentcompositions.h"

#include <QFileInfo>
#include <QSettings>

namespace HydroCouple::Composer
{

  namespace
  {
    constexpr const char *kPathsKey = "recentCompositions/paths";
    constexpr const char *kWelcomeKey = "recentCompositions/showWelcome";
  } // namespace

  RecentCompositions::RecentCompositions(QSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings)
  {
  }

  QStringList RecentCompositions::paths() const
  {
    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    return settings.value(QLatin1String(kPathsKey)).toStringList();
  }

  void RecentCompositions::write(const QStringList &paths)
  {
    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    settings.setValue(QLatin1String(kPathsKey), paths);
    settings.sync();

    Q_EMIT changed();
  }

  void RecentCompositions::remember(const QString &filePath)
  {
    if (filePath.isEmpty())
    {
      return;
    }

    // Absolute, so the same document opened from two working directories is
    // one entry rather than two.
    const QString absolute = QFileInfo(filePath).absoluteFilePath();

    QStringList current = paths();
    current.removeAll(absolute);
    current.prepend(absolute);

    while (current.size() > kMaximum)
    {
      current.removeLast();
    }

    write(current);
  }

  void RecentCompositions::forget(const QString &filePath)
  {
    QStringList current = paths();

    if (current.removeAll(QFileInfo(filePath).absoluteFilePath()) > 0
        || current.removeAll(filePath) > 0)
    {
      write(current);
    }
  }

  void RecentCompositions::clear()
  {
    write({});
  }

  bool RecentCompositions::showsWelcomeOnStartUp() const
  {
    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    // Shown until the user says otherwise: the first thing a new user meets
    // should be somewhere to start, not an empty canvas.
    return settings.value(QLatin1String(kWelcomeKey), true).toBool();
  }

  void RecentCompositions::setShowsWelcomeOnStartUp(bool shows)
  {
    QSettings own;
    QSettings &settings = m_settings ? *m_settings : own;

    settings.setValue(QLatin1String(kWelcomeKey), shows);
    settings.sync();
  }

} // namespace HydroCouple::Composer
