/*!
 * \file   test_welcome.cpp
 * \brief  The start page and the documents it remembers.
 *
 * The store is driven with its own settings file, so a test never writes
 * where the application reads. The page is driven through its own widgets,
 * because what is worth gating is what a user can reach.
 */

#include "core/composerapplication.h"
#include "core/preferencesmanager.h"
#include "settingsredirect.h"
#include "ui/composermainwindow.h"
#include "ui/recentcompositions.h"
#include "ui/theme/thememanager.h"
#include "ui/welcomepage.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QCheckBox>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>

using namespace HydroCouple::Composer;

namespace
{
  class WelcomeTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          // The page and the window read the application-wide preferences,
          // which must not be the developer's own.
          Testing::redirectSettingsTo(
            QStringLiteral(COMPOSER_PREFERENCES_FIXTURE_DIR)
            + QStringLiteral("/test_welcome"));

          static int argc = 1;
          static char arg0[] = "test_welcome";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        PreferencesManager::instance()->resetToDefaults();
        ASSERT_TRUE(directory.isValid());
        settings = std::make_unique<QSettings>(
          directory.filePath(QStringLiteral("recent.ini")),
          QSettings::IniFormat);
        recent = std::make_unique<RecentCompositions>(settings.get());
      }

      //! A real file, so "still there" and "gone" are both reachable.
      [[nodiscard]] QString writeDocument(const QString &name) const
      {
        const QString path = directory.filePath(name);
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(R"({"schema_version": "1.1", "components": []})");

        return path;
      }

      QTemporaryDir directory;
      std::unique_ptr<QSettings> settings;
      std::unique_ptr<RecentCompositions> recent;

      static inline ComposerApplication *s_app = nullptr;
  };
}

TEST_F(WelcomeTest, TheMostRecentComesFirstAndIsNotListedTwice)
{
  recent->remember(writeDocument(QStringLiteral("first.json")));
  recent->remember(writeDocument(QStringLiteral("second.json")));

  ASSERT_EQ(recent->paths().size(), 2);
  EXPECT_TRUE(recent->paths().first().endsWith(QStringLiteral("second.json")));

  // Reopening moves it up rather than listing it again — a list that
  // repeated itself would push everything else out.
  recent->remember(recent->paths().last());
  ASSERT_EQ(recent->paths().size(), 2);
  EXPECT_TRUE(recent->paths().first().endsWith(QStringLiteral("first.json")));

  recent->clear();
  EXPECT_TRUE(recent->paths().isEmpty());
}

TEST_F(WelcomeTest, OnlyTheLastTenAreKept)
{
  // Ten by default, and a preference: the second half of the test lowers
  // it and expects the list to shrink on the next document remembered.
  const int limit = PreferencesManager::instance()->recentLimit();
  ASSERT_EQ(limit, 10);

  for (int index = 0; index < limit + 5; ++index)
  {
    recent->remember(
      writeDocument(QStringLiteral("doc%1.json").arg(index)));
  }

  EXPECT_EQ(recent->paths().size(), limit);
  EXPECT_TRUE(recent->paths().first().contains(QStringLiteral("doc14")))
    << "the newest was not kept";
  EXPECT_FALSE(recent->paths().join(QChar(',')).contains(
    QStringLiteral("doc0.json")))
    << "the oldest was not forgotten";

  PreferencesManager::instance()->setRecentLimit(3);
  recent->remember(writeDocument(QStringLiteral("doc99.json")));
  EXPECT_EQ(recent->paths().size(), 3)
    << "lowering the limit did not trim the list";
  EXPECT_TRUE(recent->paths().first().contains(QStringLiteral("doc99")));
}

TEST_F(WelcomeTest, ThePageListsWhatWasOpenedAndAsksToOpenIt)
{
  const QString path = writeDocument(QStringLiteral("study.json"));
  recent->remember(path);

  WelcomePage page(recent.get());

  auto *list =
    page.findChild<QListWidget *>(QStringLiteral("welcomeRecentList"));
  ASSERT_NE(list, nullptr);
  ASSERT_EQ(list->count(), 1);
  EXPECT_TRUE(list->item(0)->text().contains(QStringLiteral("study.json")));

  // Clicking a row asks the window to open it; the page never opens
  // anything itself, so opening always goes through the one path that also
  // records it as recent.
  QSignalSpy asked(&page, &WelcomePage::openRecentRequested);
  list->setCurrentRow(0);
  Q_EMIT list->itemClicked(list->item(0));

  ASSERT_EQ(asked.count(), 1);
  EXPECT_EQ(asked.first().at(0).toString(), path);

  // The list follows the store, without the page being told directly.
  recent->clear();
  EXPECT_EQ(list->count(), 1) << "the empty placeholder should remain";
  EXPECT_TRUE(list->item(0)->text().contains(QStringLiteral("Nothing")));
}

TEST_F(WelcomeTest, TheStartUpPreferenceIsRememberedBothWays)
{
  PreferencesManager *prefs = PreferencesManager::instance();
  EXPECT_TRUE(prefs->showWelcomeOnStartUp()) << "shown until told otherwise";

  WelcomePage page(recent.get());
  auto *check =
    page.findChild<QCheckBox *>(QStringLiteral("welcomeShowOnStartUp"));
  ASSERT_NE(check, nullptr);
  EXPECT_TRUE(check->isChecked());

  check->setChecked(false);
  EXPECT_FALSE(prefs->showWelcomeOnStartUp())
    << "unchecking it did not reach the preference";

  // And the other way: the preferences dialog sets the same key, and the
  // box on the page has to follow it rather than keep a copy of its own.
  prefs->setShowWelcomeOnStartUp(true);
  EXPECT_TRUE(check->isChecked())
    << "a change made elsewhere did not reach the page";
}

// ── U1: the page closes, comes back, and its list can be managed ──────────

TEST_F(WelcomeTest, TheRecentListCanBeEmptiedAndEntriesDropped)
{
  const QString kept = writeDocument(QStringLiteral("kept.json"));
  const QString dropped = writeDocument(QStringLiteral("dropped.json"));
  recent->remember(kept);
  recent->remember(dropped);

  WelcomePage page(recent.get());
  auto *list = page.findChild<QListWidget *>(QStringLiteral("welcomeRecentList"));
  auto *clear =
    page.findChild<QPushButton *>(QStringLiteral("welcomeClearRecentButton"));
  ASSERT_NE(list, nullptr);
  ASSERT_NE(clear, nullptr);
  ASSERT_EQ(list->count(), 2);
  EXPECT_TRUE(clear->isEnabled());

  // One entry forgotten through the store, as the context menu does it: the
  // page is a view, so the list follows without being told.
  recent->forget(dropped);
  EXPECT_EQ(list->count(), 1);
  EXPECT_TRUE(list->item(0)->text().contains(QStringLiteral("kept.json")));

  clear->click();
  EXPECT_TRUE(recent->paths().isEmpty()) << "the store still holds entries";
  EXPECT_FALSE(clear->isEnabled())
    << "an empty list still offers to be cleared";

  // The placeholder is not an entry: clearing left one row saying so, and
  // it carries no path for a menu to act on.
  ASSERT_EQ(list->count(), 1);
  EXPECT_TRUE(list->item(0)->data(Qt::UserRole).toString().isEmpty());
}

TEST_F(WelcomeTest, TheRecentMenuOpensAndForgetsButOffersNothingOnThePlaceholder)
{
  const QString path = writeDocument(QStringLiteral("study.json"));
  recent->remember(path);

  WelcomePage page(recent.get());
  auto *list = page.findChild<QListWidget *>(QStringLiteral("welcomeRecentList"));
  ASSERT_NE(list, nullptr);
  ASSERT_EQ(list->count(), 1);

  std::unique_ptr<QMenu> menu(page.recentMenuFor(list->item(0)));
  ASSERT_NE(menu, nullptr);
  ASSERT_EQ(menu->actions().size(), 2);

  // Open asks the window, so that opening still goes through the one path
  // that records the document as recent.
  QSignalSpy asked(&page, &WelcomePage::openRecentRequested);
  menu->actions().at(0)->trigger();
  ASSERT_EQ(asked.count(), 1);
  EXPECT_EQ(asked.first().at(0).toString(), path);
  EXPECT_EQ(recent->paths().size(), 1) << "opening forgot the entry";

  // Remove acts on the store directly, and the list follows because it is
  // a view of the store rather than a copy of it.
  menu->actions().at(1)->trigger();
  EXPECT_TRUE(recent->paths().isEmpty());
  ASSERT_EQ(list->count(), 1);
  EXPECT_TRUE(list->item(0)->text().contains(QStringLiteral("Nothing")));

  // And the placeholder that is left offers nothing to do to it.
  EXPECT_EQ(page.recentMenuFor(list->item(0)), nullptr);
  EXPECT_EQ(page.recentMenuFor(nullptr), nullptr);
}

TEST_F(WelcomeTest, OnlyTheWelcomeTabOffersToClose)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  auto *page = window.findChild<WelcomePage *>(QStringLiteral("welcomePage"));
  ASSERT_NE(tabs, nullptr);
  ASSERT_NE(page, nullptr);

  QTabBar *bar = tabs->tabBar();
  const auto side = static_cast<QTabBar::ButtonPosition>(bar->style()->styleHint(
    QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));

  const int welcome = tabs->indexOf(page);
  ASSERT_GE(welcome, 0);
  EXPECT_NE(bar->tabButton(welcome, side), nullptr)
    << "the welcome tab has no close button";

  // setTabsClosable() would have put one on every tab, and the composition,
  // the map and the 3D view are not things a user can be without.
  for (int tab = 0; tab < tabs->count(); ++tab)
  {
    if (tab == welcome)
    {
      continue;
    }

    EXPECT_EQ(bar->tabButton(tab, side), nullptr)
      << tabs->tabText(tab).toStdString() << " offers to close";
  }
}

TEST_F(WelcomeTest, ClosingTakesTheTabAndKeepsThePage)
{
  const QString path = writeDocument(QStringLiteral("remembered.json"));
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  auto *page = window.findChild<WelcomePage *>(QStringLiteral("welcomePage"));
  ASSERT_NE(tabs, nullptr);
  ASSERT_NE(page, nullptr);

  auto *close =
    window.findChild<QToolButton *>(QStringLiteral("welcomeTabCloseButton"));
  ASSERT_NE(close, nullptr);

  const int before = tabs->count();
  close->click();

  EXPECT_EQ(tabs->indexOf(page), -1) << "the tab is still there";
  EXPECT_EQ(tabs->count(), before - 1);
  EXPECT_EQ(tabs->currentWidget(), window.canvas())
    << "closing the start page left the user nowhere";

  // The page itself survives — it is removed from the stack, not deleted,
  // so what it was showing is still what it will show.
  EXPECT_FALSE(page->isWindow()) << "the closed page became a stray window";

  auto *check =
    page->findChild<QCheckBox *>(QStringLiteral("welcomeShowOnStartUp"));
  ASSERT_NE(check, nullptr);

  // Closing the page is not a change of mind about start-up.
  EXPECT_TRUE(PreferencesManager::instance()->showWelcomeOnStartUp());
  EXPECT_TRUE(check->isChecked());

  close->click();  // harmless second time
  EXPECT_EQ(tabs->count(), before - 1);
}

TEST_F(WelcomeTest, ClosingTheStartPageFromAnotherTabLeavesYouOnIt)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  auto *page = window.findChild<WelcomePage *>(QStringLiteral("welcomePage"));
  auto *close =
    window.findChild<QToolButton *>(QStringLiteral("welcomeTabCloseButton"));
  ASSERT_NE(tabs, nullptr);
  ASSERT_NE(page, nullptr);
  ASSERT_NE(close, nullptr);

  // The close button sits on the tab bar, so it can be pressed while
  // another tab is the one being looked at.
  tabs->setCurrentWidget(window.mapCanvas());
  ASSERT_EQ(tabs->currentWidget(), window.mapCanvas());

  close->click();

  EXPECT_EQ(tabs->indexOf(page), -1);
  EXPECT_EQ(tabs->currentWidget(), window.mapCanvas())
    << "tidying away the start page pulled the user off the map";
}

TEST_F(WelcomeTest, HelpWelcomeBringsItBackAsTheSamePage)
{
  const QString path = writeDocument(QStringLiteral("study.json"));
  recent->remember(path);

  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  auto *page = window.findChild<WelcomePage *>(QStringLiteral("welcomePage"));
  auto *action = window.findChild<QAction *>(QStringLiteral("welcomeAction"));
  auto *close =
    window.findChild<QToolButton *>(QStringLiteral("welcomeTabCloseButton"));
  ASSERT_NE(tabs, nullptr);
  ASSERT_NE(page, nullptr);
  ASSERT_NE(action, nullptr);
  ASSERT_NE(close, nullptr);

  auto *list = page->findChild<QListWidget *>(QStringLiteral("welcomeRecentList"));
  ASSERT_NE(list, nullptr);
  const int remembered = list->count();
  ASSERT_GT(remembered, 0);

  close->click();
  ASSERT_EQ(tabs->indexOf(page), -1);

  action->trigger();

  EXPECT_EQ(tabs->indexOf(page), 0) << "it did not come back where it was";
  EXPECT_EQ(tabs->currentWidget(), page);
  EXPECT_FALSE(tabs->tabIcon(0).isNull()) << "the restored tab has no icon";

  // The same page, not a rebuilt one: what it was showing is still there.
  EXPECT_EQ(page->findChild<QListWidget *>(
              QStringLiteral("welcomeRecentList"))->count(), remembered);

  QTabBar *bar = tabs->tabBar();
  const auto side = static_cast<QTabBar::ButtonPosition>(bar->style()->styleHint(
    QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
  EXPECT_EQ(bar->tabButton(0, side), close)
    << "the restored tab has a different close button — the old one leaked";

  // Asking for it when it is already open selects it rather than adding a
  // second copy.
  const int count = tabs->count();
  tabs->setCurrentWidget(window.canvas());
  action->trigger();
  EXPECT_EQ(tabs->count(), count);
  EXPECT_EQ(tabs->currentWidget(), page);
}

TEST_F(WelcomeTest, TheAppearanceActionsAndThePreferenceAreOneSetting)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  PreferencesManager *prefs = PreferencesManager::instance();
  ThemeManager *theme = ThemeManager::instance();
  const ThemeManager::Mode original = theme->mode();

  auto *dark = window.findChild<QAction *>(QStringLiteral("appearanceDark"));
  auto *light = window.findChild<QAction *>(QStringLiteral("appearanceLight"));
  ASSERT_NE(dark, nullptr);
  ASSERT_NE(light, nullptr);

  // Set through the preference, as the dialog does: the theme follows and
  // the matching action lights up, without the action being touched.
  prefs->setThemeMode(QStringLiteral("Dark"));
  EXPECT_EQ(theme->mode(), ThemeManager::Mode::Dark);
  EXPECT_TRUE(dark->isChecked());

  // Set through the action, as the ribbon does: the preference records it,
  // which is what makes the choice survive a restart.
  light->trigger();
  EXPECT_EQ(prefs->themeMode(), QStringLiteral("Light"));
  EXPECT_EQ(theme->mode(), ThemeManager::Mode::Light);

  prefs->resetToDefaults();
  theme->setMode(original);
  theme->apply();
}

TEST_F(WelcomeTest, TheWindowOpensOnTheWelcomeTabAndLeavesItWhenADocumentOpens)
{
  ComposerMainWindow window;
  window.setAttribute(Qt::WA_QuitOnClose, false);

  auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
  ASSERT_NE(tabs, nullptr);

  auto *page = window.findChild<WelcomePage *>(QStringLiteral("welcomePage"));
  ASSERT_NE(page, nullptr) << "the window has no welcome page";
  EXPECT_EQ(tabs->currentWidget(), page)
    << "the window did not start on the welcome page";

  // File ▸ Open Recent exists and says so when there is nothing in it.
  auto *menu = window.findChild<QMenu *>(QStringLiteral("recentMenu"));
  ASSERT_NE(menu, nullptr);
  EXPECT_FALSE(menu->actions().isEmpty());

  // Opening a document is what the welcome page is for; once one is open,
  // the canvas is what the user asked to see.
  const QString path = writeDocument(QStringLiteral("opened.json"));
  QString message;
  ASSERT_TRUE(window.openComposition(path, message)) << message.toStdString();

  EXPECT_EQ(tabs->currentWidget(), window.canvas())
    << "opening a composition left the user looking at the welcome page";
}
