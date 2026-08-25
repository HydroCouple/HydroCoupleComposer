#include "ui/theme/thememanager.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace HydroCouple::Composer
{

  namespace
  {
    //! Token values copied from openswmm.gui's ui/theme/themetokens.h.
    const ThemeColors &lightColors()
    {
      static const ThemeColors c = {
        /* surfaceWindow */ QColor(0xF5, 0xF5, 0xF7),
        /* surfaceRaised */ QColor(0xFF, 0xFF, 0xFF),
        /* surfaceSunken */ QColor(0xE9, 0xE9, 0xED),
        /* text          */ QColor(0x1D, 0x1D, 0x1F),
        /* hintText      */ QColor(0x5A, 0x5A, 0x60),
        /* border        */ QColor(0xC9, 0xC9, 0xCE),
        /* accent        */ QColor(0x0A, 0x66, 0xC2),
        /* accentText    */ QColor(0xFF, 0xFF, 0xFF),
        /* focusRing     */ QColor(0x0A, 0x66, 0xC2),
        /* selectionFill */ QColor(0x0A, 0x66, 0xC2),
        /* selectionText */ QColor(0xFF, 0xFF, 0xFF),
      };

      return c;
    }

    const ThemeColors &darkColors()
    {
      static const ThemeColors c = {
        /* surfaceWindow */ QColor(0x1E, 0x1E, 0x22),
        /* surfaceRaised */ QColor(0x2A, 0x2A, 0x30),
        /* surfaceSunken */ QColor(0x17, 0x17, 0x1A),
        /* text          */ QColor(0xE8, 0xE8, 0xEA),
        /* hintText      */ QColor(0xA5, 0xA5, 0xAD),
        /* border        */ QColor(0x3F, 0x3F, 0x46),
        /* accent        */ QColor(0x4C, 0x9A, 0xFF),
        /* accentText    */ QColor(0x0B, 0x1E, 0x36),
        /* focusRing     */ QColor(0x4C, 0x9A, 0xFF),
        /* selectionFill */ QColor(0x4C, 0x9A, 0xFF),
        /* selectionText */ QColor(0x0B, 0x1E, 0x36),
      };

      return c;
    }

    QColor mixed(const QColor &a, const QColor &b, double weightOfA)
    {
      const double wb = 1.0 - weightOfA;

      return QColor::fromRgbF(a.redF() * weightOfA + b.redF() * wb,
                              a.greenF() * weightOfA + b.greenF() * wb,
                              a.blueF() * weightOfA + b.blueF() * wb);
    }
  } // namespace

  ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
  {
  }

  ThemeManager *ThemeManager::instance()
  {
    static ThemeManager theme;
    return &theme;
  }

  ThemeManager::Mode ThemeManager::mode() const
  {
    return m_mode;
  }

  void ThemeManager::setMode(Mode mode)
  {
    m_mode = mode;
  }

  Qt::ColorScheme ThemeManager::effectiveScheme() const
  {
    switch (m_mode)
    {
      case Mode::Light:
        return Qt::ColorScheme::Light;
      case Mode::Dark:
        return Qt::ColorScheme::Dark;
      case Mode::System:
        break;
    }

    const QStyleHints *hints = QGuiApplication::styleHints();
    const Qt::ColorScheme system =
      hints ? hints->colorScheme() : Qt::ColorScheme::Unknown;

    // Offscreen/minimal platforms report Unknown — light is the safe floor.
    return system == Qt::ColorScheme::Dark ? Qt::ColorScheme::Dark
                                           : Qt::ColorScheme::Light;
  }

  const ThemeColors &ThemeManager::colors() const
  {
    return effectiveScheme() == Qt::ColorScheme::Dark ? darkColors()
                                                      : lightColors();
  }

  void ThemeManager::apply()
  {
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());

    if (!app)
    {
      return;
    }

    // Fusion renders identically on every platform, so a palette verified once
    // holds everywhere and the chrome does not drift between hosts.
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const Qt::ColorScheme scheme = effectiveScheme();
    const ThemeColors &c = colors();

    QPalette pal;
    pal.setColor(QPalette::Window, c.surfaceWindow);
    pal.setColor(QPalette::WindowText, c.text);
    pal.setColor(QPalette::Base, c.surfaceRaised);
    pal.setColor(QPalette::AlternateBase, c.surfaceSunken);
    pal.setColor(QPalette::Text, c.text);
    pal.setColor(QPalette::Button, c.surfaceWindow);
    pal.setColor(QPalette::ButtonText, c.text);
    pal.setColor(QPalette::ToolTipBase, c.surfaceRaised);
    pal.setColor(QPalette::ToolTipText, c.text);
    pal.setColor(QPalette::PlaceholderText, c.hintText);
    pal.setColor(QPalette::Highlight, c.selectionFill);
    pal.setColor(QPalette::HighlightedText, c.selectionText);
    pal.setColor(QPalette::Link, c.accent);
    pal.setColor(QPalette::BrightText, c.surfaceRaised);
    pal.setColor(QPalette::Mid, c.hintText);
    pal.setColor(QPalette::Dark, c.border);
    pal.setColor(QPalette::Midlight, mixed(c.border, c.surfaceWindow, 0.5));
    pal.setColor(QPalette::Light, c.surfaceRaised);
    pal.setColor(QPalette::Shadow, mixed(c.text, c.surfaceSunken, 0.5));

    const QColor disabledText = mixed(c.text, c.surfaceWindow, 0.45);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText, disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Highlight, c.border);
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);

    app->setPalette(pal);

    // Minimal overlay — NOT a re-skin. Only what the palette cannot express:
    // a visible keyboard-focus ring under Fusion (whose default focus rect is
    // a faint dotted line) and toolbar breathing room.
    const QString focus = c.focusRing.name(QColor::HexRgb);

    app->setStyleSheet(
      QStringLiteral(
        "QToolButton:focus, QPushButton:focus, QComboBox:focus, "
        "QTabBar::tab:focus { outline: 2px solid %1; outline-offset: -2px; }\n"
        "QToolBar { spacing: 3px; }\n")
        .arg(focus));

    if (scheme != m_appliedScheme)
    {
      m_appliedScheme = scheme;
      Q_EMIT themeChanged();
    }
  }

  QString ThemeManager::modeToString(Mode mode)
  {
    switch (mode)
    {
      case Mode::Light:
        return QStringLiteral("Light");
      case Mode::Dark:
        return QStringLiteral("Dark");
      case Mode::System:
        break;
    }

    return QStringLiteral("System");
  }

  ThemeManager::Mode ThemeManager::modeFromString(const QString &text)
  {
    if (text.compare(QStringLiteral("Light"), Qt::CaseInsensitive) == 0)
    {
      return Mode::Light;
    }

    if (text.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0)
    {
      return Mode::Dark;
    }

    return Mode::System;
  }

} // namespace HydroCouple::Composer
