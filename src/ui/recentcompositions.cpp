#include "ui/recentcompositions.h"

#include "core/preferencesmanager.h"

#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace HydroCouple::Composer
{

  namespace
  {
    constexpr const char *kPathsKey = "recentCompositions/paths";
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

    // How many are kept is a preference, read on every remember rather
    // than once, so lowering it trims the list on the next document opened.
    const int limit = std::max(1, PreferencesManager::instance()->recentLimit());

    while (current.size() > limit)
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

} // namespace HydroCouple::Composer
