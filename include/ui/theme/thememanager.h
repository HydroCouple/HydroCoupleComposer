/*!
 * \file   thememanager.h
 * \author Caleb Buahin
 * \brief  ThemeManager — the Fusion-based chrome shared with openswmm.gui.
 *
 * Composer uses the same appearance as openswmm.gui so the two applications
 * read as one suite: the Fusion style, the same token palette, and the same
 * minimal overlay. The token values are copied from openswmm.gui's
 * `ui/theme/themetokens.h` rather than re-invented, because "looks similar" and
 * "is the same" are different claims — matching values keeps them the same
 * through future adjustments to either side.
 *
 * Fusion is chosen deliberately over the native styles: it renders identically
 * on macOS, Windows and Linux, so a palette verified once holds everywhere, and
 * dock/graphics chrome does not drift between platforms.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_THEME_THEMEMANAGER_H
#define HYDROCOUPLECOMPOSER_UI_THEME_THEMEMANAGER_H

#include <QColor>
#include <QObject>

namespace HydroCouple::Composer
{

  /*!
   * \brief The colour tokens one appearance mode is built from.
   */
  struct ThemeColors
  {
      QColor surfaceWindow;  //!< Window and toolbar background.
      QColor surfaceRaised;  //!< Views, editors, cards (QPalette::Base).
      QColor surfaceSunken;  //!< Wells and alternate rows.
      QColor text;           //!< Primary text on any surface.
      QColor hintText;       //!< Secondary and placeholder text.
      QColor border;         //!< Separators and frames.
      QColor accent;         //!< Links and checked states.
      QColor accentText;     //!< Text painted on accent fills.
      QColor focusRing;      //!< Keyboard-focus outline.
      QColor selectionFill;  //!< Item-view selection background.
      QColor selectionText;  //!< Text on selectionFill.
  };

  /*!
   * \brief Applies the application's appearance.
   */
  class ThemeManager : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Which appearance to use.
       */
      enum class Mode
      {
        System,  //!< Follow the operating system, live.
        Light,
        Dark
      };

      /*!
       * \brief The process-wide instance.
       */
      static ThemeManager *instance();

      /*!
       * \brief The mode currently requested.
       */
      [[nodiscard]] Mode mode() const;

      /*!
       * \brief Sets the requested mode; call apply() to make it visible.
       * \param mode The appearance to request.
       */
      void setMode(Mode mode);

      /*!
       * \brief The scheme actually in force, resolving System.
       *
       * Offscreen and minimal platforms report an unknown OS scheme, so light
       * is the floor — otherwise headless renders and CI screenshots would
       * depend on the host's appearance.
       */
      [[nodiscard]] Qt::ColorScheme effectiveScheme() const;

      /*!
       * \brief The tokens for the scheme in force.
       */
      [[nodiscard]] const ThemeColors &colors() const;

      /*!
       * \brief Installs Fusion, the palette and the overlay on the application.
       */
      void apply();

      /*!
       * \brief Parses a persisted mode name.
       * \param text One of "System", "Light" or "Dark"; anything else is System.
       */
      [[nodiscard]] static Mode modeFromString(const QString &text);

      /*!
       * \brief The persistable name of \a mode.
       * \param mode Mode to name.
       */
      [[nodiscard]] static QString modeToString(Mode mode);

    Q_SIGNALS:
      /*!
       * \brief Emitted when the effective light/dark scheme changes.
       */
      void themeChanged();

    private:
      explicit ThemeManager(QObject *parent = nullptr);

      Mode m_mode = Mode::System;
      Qt::ColorScheme m_appliedScheme = Qt::ColorScheme::Unknown;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_THEME_THEMEMANAGER_H
