/*!
 * \file   settingsredirect.h
 * \brief  Points the application's default QSettings at a fixture file.
 *
 * PreferencesManager::instance() reads the default QSettings, which for a
 * test process is the developer's own configuration — so a test that sets
 * a preference through the application's own widgets would write there.
 * Called before the QApplication is constructed (the application reads
 * its theme preference in its constructor), this sends every default
 * QSettings to an .ini under the fixture directory instead, where what a
 * test stored can be opened and read after the run.
 */

#ifndef HYDROCOUPLECOMPOSER_TESTS_SETTINGSREDIRECT_H
#define HYDROCOUPLECOMPOSER_TESTS_SETTINGSREDIRECT_H

#include <QDir>
#include <QSettings>
#include <QString>

namespace HydroCouple::Composer::Testing
{
  /*!
   * \brief Sends default-constructed QSettings to \a directory.
   *
   * The file lands at <directory>/<organization>/<application>.ini once the
   * application has named itself.
   */
  inline void redirectSettingsTo(const QString &directory)
  {
    QDir().mkpath(directory);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory);
  }
} // namespace HydroCouple::Composer::Testing

#endif // HYDROCOUPLECOMPOSER_TESTS_SETTINGSREDIRECT_H
