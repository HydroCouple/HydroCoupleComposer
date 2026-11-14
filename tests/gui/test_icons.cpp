/*!
 * \file   test_icons.cpp
 * \brief  The shared SVG chrome set and its theming, plus the application's
 *         own brand mark.
 *
 * An icon that fails to resolve renders as empty space, which looks like a
 * layout choice rather than a build that dropped a file — so the gates here
 * are mostly about *absence* being caught. The rest is that the glyph really
 * is recoloured per theme: a set that stayed mid-gray would be invisible on
 * one of the two themes and nothing about the widget tree would say so.
 */

#include "core/composerapplication.h"
#include "ui/composermainwindow.h"
#include "ui/panels/layertreepanel.h"
#include "ui/theme/iconfactory.h"
#include "ui/theme/thememanager.h"
#include "ui/toolbars/ribbonbar.h"
#include "ui/toolbars/ribbongroup.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QTabWidget>
#include <cmath>
#include <QToolButton>

using namespace HydroCouple::Composer;

namespace
{
  //! Proportion of an icon's pixels that are neither transparent nor at the
  //! extremes — i.e. actual glyph.
  int inkedPixels(const QIcon &icon, QIcon::Mode mode = QIcon::Normal)
  {
    const QImage image =
      icon.pixmap(QSize(24, 24), mode, QIcon::Off).toImage().convertToFormat(
        QImage::Format_ARGB32);

    int inked = 0;

    for (int y = 0; y < image.height(); ++y)
    {
      for (int x = 0; x < image.width(); ++x)
      {
        if (qAlpha(image.pixel(x, y)) > 32)
        {
          ++inked;
        }
      }
    }

    return inked;
  }

  //! The mean colour of an icon's opaque pixels.
  QColor glyphColor(const QIcon &icon, QIcon::Mode mode = QIcon::Normal)
  {
    const QImage image =
      icon.pixmap(QSize(24, 24), mode, QIcon::Off).toImage().convertToFormat(
        QImage::Format_ARGB32);

    qint64 red = 0;
    qint64 green = 0;
    qint64 blue = 0;
    qint64 count = 0;

    for (int y = 0; y < image.height(); ++y)
    {
      for (int x = 0; x < image.width(); ++x)
      {
        const QRgb pixel = image.pixel(x, y);

        if (qAlpha(pixel) > 200)
        {
          red += qRed(pixel);
          green += qGreen(pixel);
          blue += qBlue(pixel);
          ++count;
        }
      }
    }

    return count > 0 ? QColor(int(red / count), int(green / count),
                              int(blue / count))
                     : QColor();
  }

  class IconTest : public ::testing::Test
  {
    protected:
      void SetUp() override
      {
        if (!QApplication::instance())
        {
          static int argc = 1;
          static char name[] = "test_icons";
          static char *argv[] = { name, nullptr };
          // The real application class, so what it does at startup — the
          // window icon among it — is part of what these gates see.
          m_app = std::make_unique<ComposerApplication>(argc, argv);
        }
      }

      void TearDown() override
      {
        ThemeManager::instance()->setMode(ThemeManager::Mode::System);
      }

    private:
      std::unique_ptr<ComposerApplication> m_app;
  };

}

// ── The application's own face ──────────────────────────────────────────────

TEST_F(IconTest, TheApplicationShipsItsWindowIcon)
{
  // The v1→v2 CMake port shipped the app icon-less: CFBundleIconFile empty,
  // no setWindowIcon anywhere. The window icon is the one face an offscreen
  // gate can check on every platform; the bundle and .rc wiring ride the
  // same tools/make_icons.sh artifacts.
  const QIcon icon = QApplication::windowIcon();

  ASSERT_FALSE(icon.isNull()) << "no window icon; :/branding did not "
                                 "initialise or setWindowIcon is gone";
  EXPECT_GT(inkedPixels(icon), 8) << "the mark rendered blank";

  // And it is the full-colour brand mark, not one of the recolourable
  // monochrome chrome glyphs: the blue C-and-node must show up saturated.
  const QImage image = icon.pixmap(QSize(24, 24))
                         .toImage()
                         .convertToFormat(QImage::Format_ARGB32);

  int saturated = 0;

  for (int y = 0; y < image.height(); ++y)
  {
    for (int x = 0; x < image.width(); ++x)
    {
      const QRgb pixel = image.pixel(x, y);

      if (qAlpha(pixel) > 200 && QColor(pixel).saturation() > 40)
      {
        ++saturated;
      }
    }
  }

  EXPECT_GT(saturated, 40)
    << "the window icon does not look like the brand mark";
}

// ── The set is present ──────────────────────────────────────────────────────

TEST_F(IconTest, TheIconSetIsCompiledIn)
{
  // A .qrc listing a file that is not there, or a resource that failed to
  // initialise out of the static library, both end here.
  const QStringList aliases = IconFactory::aliases();

  ASSERT_FALSE(aliases.isEmpty())
    << "the icon resource did not initialise; :/icons is empty";
  EXPECT_GE(aliases.size(), 20);
}

TEST_F(IconTest, EveryAliasInTheSetRendersInk)
{
  // Not merely "resolves": an SVG that is malformed, or whose viewBox is
  // wrong, resolves fine and draws nothing.
  for (const QString &alias : IconFactory::aliases())
  {
    const QIcon icon = IconFactory::icon(alias);

    ASSERT_FALSE(icon.isNull()) << alias.toStdString() << " did not resolve";
    EXPECT_GT(inkedPixels(icon), 8)
      << alias.toStdString() << " rendered blank";
  }
}

TEST_F(IconTest, AnUnknownAliasIsNullRatherThanBlank)
{
  EXPECT_FALSE(IconFactory::has(QStringLiteral("no_such_icon")));
  EXPECT_TRUE(IconFactory::icon(QStringLiteral("no_such_icon")).isNull());
  EXPECT_TRUE(IconFactory::icon(QString()).isNull());
}

// ── Theming ─────────────────────────────────────────────────────────────────

TEST_F(IconTest, GlyphsFollowTheTheme)
{
  // The whole reason the set is recoloured at render time. Left at the mid
  // gray the SVGs carry, every icon would be near-invisible on one theme.
  ThemeManager::instance()->setMode(ThemeManager::Mode::Light);
  const QColor onLight = glyphColor(IconFactory::icon(QStringLiteral("open")));

  ThemeManager::instance()->setMode(ThemeManager::Mode::Dark);
  const QColor onDark = glyphColor(IconFactory::icon(QStringLiteral("open")));

  ASSERT_TRUE(onLight.isValid());
  ASSERT_TRUE(onDark.isValid());

  EXPECT_NE(onLight, onDark) << "the glyph did not change with the theme";

  // And in the right direction: dark glyphs on a light theme, light on dark.
  EXPECT_LT(onLight.lightness(), onDark.lightness())
    << "light theme glyph " << onLight.name().toStdString()
    << " vs dark theme glyph " << onDark.name().toStdString();
}

TEST_F(IconTest, EveryIconIsDrawnEntirelyInTheThemeGlyphColour)
{
  // Per icon and per pixel. The recolouring works by substituting a known
  // family of mid-grays, so a path drawn in a gray outside that family is
  // left alone — and an averaged check misses it, because the paths that
  // *were* substituted drag the mean back into place. That is the realistic
  // failure: one path of one icon, copied from somewhere else.
  //
  // This is also the check to satisfy when an icon is brought over from
  // openswmm.gui: if its gray is not in the family, extend the family.
  ThemeManager::instance()->setMode(ThemeManager::Mode::Light);

  const QColor expected = ThemeManager::instance()->colors().hintText;

  for (const QString &alias : IconFactory::aliases())
  {
    const QImage image = IconFactory::icon(alias)
                           .pixmap(QSize(24, 24))
                           .toImage()
                           .convertToFormat(QImage::Format_ARGB32);

    int strays = 0;
    QColor firstStray;

    for (int y = 0; y < image.height(); ++y)
    {
      for (int x = 0; x < image.width(); ++x)
      {
        const QRgb pixel = image.pixel(x, y);

        // Only fully opaque pixels: an antialiased edge is a blend toward
        // transparency and is legitimately not the glyph colour.
        if (qAlpha(pixel) < 250)
        {
          continue;
        }

        const int difference = std::abs(qRed(pixel) - expected.red()) +
                               std::abs(qGreen(pixel) - expected.green()) +
                               std::abs(qBlue(pixel) - expected.blue());

        if (difference > 24)
        {
          if (strays == 0)
          {
            firstStray = QColor(pixel);
          }

          ++strays;
        }
      }
    }

    EXPECT_EQ(strays, 0)
      << alias.toStdString() << " has " << strays
      << " pixels the recolouring did not reach — the first is "
      << firstStray.name().toStdString() << ", wanted "
      << expected.name().toStdString();
  }
}

TEST_F(IconTest, DisabledGlyphsAreFainterThanNormalOnes)
{
  ThemeManager::instance()->setMode(ThemeManager::Mode::Light);

  const QIcon icon = IconFactory::icon(QStringLiteral("save"));

  const QColor normal = glyphColor(icon, QIcon::Normal);
  const QColor disabled = glyphColor(icon, QIcon::Disabled);

  ASSERT_TRUE(normal.isValid());
  ASSERT_TRUE(disabled.isValid());

  // Fainter means closer to the surface it sits on, which on a light theme
  // is lighter. A disabled face that looked enabled is the bug here.
  EXPECT_GT(disabled.lightness(), normal.lightness())
    << "disabled " << disabled.name().toStdString() << " vs normal "
    << normal.name().toStdString();
}

TEST_F(IconTest, TheSetIsMonochromeSoItCanBeTinted)
{
  // A full-colour icon cannot be recoloured, so one that crept into the set
  // would render as itself on both themes and break the family. Checked by
  // rendering: an SVG whose colours are hidden in a gradient or a filter
  // would pass a grep of the file.
  ThemeManager::instance()->setMode(ThemeManager::Mode::Light);

  for (const QString &alias : IconFactory::aliases())
  {
    const QImage image = IconFactory::icon(alias)
                           .pixmap(QSize(24, 24))
                           .toImage()
                           .convertToFormat(QImage::Format_ARGB32);

    int coloured = 0;

    for (int y = 0; y < image.height(); ++y)
    {
      for (int x = 0; x < image.width(); ++x)
      {
        const QRgb pixel = image.pixel(x, y);

        if (qAlpha(pixel) < 200)
        {
          continue;
        }

        // Saturation is what separates a tinted gray from a brand colour;
        // antialiasing never introduces any.
        if (QColor(pixel).saturation() > 40)
        {
          ++coloured;
        }
      }
    }

    EXPECT_EQ(coloured, 0)
      << alias.toStdString() << " has " << coloured
      << " saturated pixels; it cannot be tinted and will not follow the theme";
  }
}

// ── Every face the UI shows has one ─────────────────────────────────────────

TEST_F(IconTest, EveryRibbonFaceCarriesAnIcon)
{
  // The ribbon draws icon-over-label faces, so a missing icon is a hole in
  // the toolbar. Asked of the buttons the ribbon actually built rather than
  // of every QAction in the window: menu-only entries — the basemap radio
  // items, say — take a check mark instead of an icon, and demanding one of
  // them would be demanding the wrong thing.
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.show();

  auto *ribbon = window.findChild<RibbonBar *>(QStringLiteral("ribbonBar"));
  ASSERT_NE(ribbon, nullptr);

  int checked = 0;

  for (const QString &tab :
       { QStringLiteral("home"), QStringLiteral("map"),
         QStringLiteral("view") })
  {
    const QList<RibbonGroup *> groups = ribbon->groups(tab);
    ASSERT_FALSE(groups.isEmpty()) << "ribbon tab " << tab.toStdString()
                                   << " has no groups";

    for (RibbonGroup *group : groups)
    {
      const QList<QToolButton *> buttons =
        group->findChildren<QToolButton *>();
      ASSERT_FALSE(buttons.isEmpty())
        << group->caption().toStdString() << " has no faces";

      for (QToolButton *button : buttons)
      {
        EXPECT_GT(inkedPixels(button->icon()), 8)
          << "the " << group->caption().toStdString() << " face for \""
          << button->text().remove(QLatin1Char('\n')).toStdString()
          << "\" is blank";
        ++checked;
      }
    }
  }

  EXPECT_GE(checked, 18) << "only " << checked << " ribbon faces were checked";
}

TEST_F(IconTest, EveryWorkspaceTabAndLayerButtonCarriesAnIcon)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);
  window.show();

  auto *workspace =
    window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  ASSERT_NE(workspace, nullptr);
  ASSERT_GE(workspace->count(), 3);

  for (int tab = 0; tab < workspace->count(); ++tab)
  {
    EXPECT_GT(inkedPixels(workspace->tabIcon(tab)), 8)
      << "workspace tab " << workspace->tabText(tab).toStdString()
      << " has no icon";
  }

  const QList<QToolButton *> buttons =
    window.layerTree()->findChildren<QToolButton *>();
  ASSERT_GE(buttons.size(), 5);

  for (QToolButton *button : buttons)
  {
    EXPECT_GT(inkedPixels(button->icon()), 8)
      << button->objectName().toStdString() << " has a blank icon";
  }

  // The transport carries no text, so an SVG that failed to reach the
  // resource set leaves a button that looks like a gap in the layout.
  auto *transport =
    window.findChild<QWidget *>(QStringLiteral("timeControlPanel"));
  ASSERT_NE(transport, nullptr);

  const QList<QToolButton *> transportButtons =
    transport->findChildren<QToolButton *>();
  ASSERT_GE(transportButtons.size(), 6);

  for (QToolButton *button : transportButtons)
  {
    EXPECT_GT(inkedPixels(button->icon()), 8)
      << button->objectName().toStdString() << " has a blank icon";
  }
}
