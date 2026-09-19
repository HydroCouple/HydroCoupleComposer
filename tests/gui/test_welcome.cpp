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
#include <QTabWidget>
#include <QTemporaryDir>

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
