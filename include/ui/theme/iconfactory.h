/*!
 * \file   iconfactory.h
 * \author Caleb Buahin
 * \brief  IconFactory — theme-aware icons from the shared SVG chrome set.
 *
 * The icons are openswmm.gui's, borrowed so the two applications read as one
 * family, and they carry that set's convention: a monochrome glyph drawn in a
 * mid-gray (#777777 and neighbours) on a 24×24 grid. A fixed gray is legible
 * on exactly one theme, so this substitutes it with the current theme's glyph
 * colour **at render time**, per QIcon mode — normal, active, selected,
 * disabled. The SVG files stay byte-identical to their openswmm.gui originals,
 * which is what lets a fix there be picked up here by copying a file.
 *
 * Pixmaps are cached per alias, scheme, mode, size and device pixel ratio. A
 * theme flip changes the cache key rather than invalidating anything, so a
 * repaint is all that is needed to pick up recoloured glyphs.
 *
 * \note Qt's own QStyle::standardIcon is what this replaces. Those icons are
 *       full-colour platform artwork — a blue folder, a blue floppy disk —
 *       which cannot be tinted and do not sit with a monochrome set.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_THEME_ICONFACTORY_H
#define HYDROCOUPLECOMPOSER_UI_THEME_ICONFACTORY_H

#include <QIcon>
#include <QString>
#include <QStringList>

namespace HydroCouple::Composer
{

  /*!
   * \brief Builds themed icons from the `:/icons` resource set.
   */
  class IconFactory
  {
    public:
      /*!
       * \brief The themed icon for \a alias, or a null icon when unknown.
       *
       * The alias is the SVG's base name — "open", "add_mesh". One QIcon is
       * built per alias and shared: the engine renders per theme through the
       * cache key, so a single icon serves every scheme and mode.
       *
       * \param alias Base name of an SVG in the icon set.
       */
      [[nodiscard]] static QIcon icon(const QString &alias);

      /*!
       * \brief Whether \a alias resolves to an icon in the set.
       *
       * Public because a missing SVG is otherwise invisible: an icon that
       * failed to resolve renders as an empty space, which looks like a
       * layout choice rather than a build that dropped a file.
       *
       * \param alias Base name to test.
       */
      [[nodiscard]] static bool has(const QString &alias);

      /*!
       * \brief Every alias the set holds, sorted.
       */
      [[nodiscard]] static QStringList aliases();
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_THEME_ICONFACTORY_H
