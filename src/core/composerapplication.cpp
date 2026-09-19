#include "core/composerapplication.h"

#include "core/httpuriresolver.h"
#include "core/preferencesmanager.h"
#include "core/version.h"
#include "ui/theme/thememanager.h"

#include "hydrocouplesdk/io/uriresolver.h"

#include <QIcon>
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

    // The brand mark, for every platform that does not take the icon from
    // the macOS bundle or the Windows .rc (Linux window managers, portable
    // builds). Compiled in as :/branding by composer_core.
    setWindowIcon(
      QIcon(QStringLiteral(":/branding/hydrocouplecomposer.png")));

    // The SDK refuses an https argument unless a host can fetch one. This is
    // that host: the fetch tier is already in the process for the basemaps.
    m_uriResolver = std::make_unique<HttpUriResolver>();
    HydroCouple::SDK::IO::setUriResolver(m_uriResolver.get());

    // Fusion plus the shared token palette, so Composer and openswmm.gui read
    // as one suite. Applied here rather than in main() so every entry point —
    // the window, the headless runner, and the offscreen screenshot tool —
    // gets the same appearance.
    ThemeManager *theme = ThemeManager::instance();
    theme->setMode(ThemeManager::modeFromString(
      PreferencesManager::instance()->themeMode()));
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

  ComposerApplication::~ComposerApplication()
  {
    // The SDK holds a bare pointer, so it must stop holding this one first.
    HydroCouple::SDK::IO::setUriResolver(nullptr);
  }

  HttpUriResolver *ComposerApplication::uriResolver() const
  {
    return m_uriResolver.get();
  }

  QString ComposerApplication::versionString()
  {
    return QStringLiteral(COMPOSER_VERSION);
  }

} // namespace HydroCouple::Composer
