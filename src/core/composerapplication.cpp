#include "core/composerapplication.h"
#include "core/version.h"
#include "ui/theme/thememanager.h"

#include <QSettings>
#include <QStyleHints>

namespace HydroCouple::Composer
{

  ComposerApplication::ComposerApplication(int &argc, char **argv)
    : QApplication(argc, argv)
  {
    setOrganizationName(QStringLiteral("HydroCouple"));
    setOrganizationDomain(QStringLiteral("hydrocouple.org"));
    setApplicationName(QStringLiteral("HydroCoupleComposer"));
    setApplicationVersion(versionString());

    // Fusion plus the shared token palette, so Composer and openswmm.gui read
    // as one suite. Applied here rather than in main() so every entry point —
    // the window, the headless runner, and the offscreen screenshot tool —
    // gets the same appearance.
    ThemeManager *theme = ThemeManager::instance();
    theme->setMode(ThemeManager::modeFromString(
      QSettings().value(QStringLiteral("appearance/mode"),
                        QStringLiteral("System"))
        .toString()));
    theme->apply();

    // "System" follows the OS appearance live rather than only at startup.
    connect(styleHints(), &QStyleHints::colorSchemeChanged, this,
            [theme](Qt::ColorScheme)
            {
              if (theme->mode() == ThemeManager::Mode::System)
              {
                theme->apply();
              }
            });
  }

  ComposerApplication::~ComposerApplication() = default;

  QString ComposerApplication::versionString()
  {
    return QStringLiteral(COMPOSER_VERSION);
  }

} // namespace HydroCouple::Composer
