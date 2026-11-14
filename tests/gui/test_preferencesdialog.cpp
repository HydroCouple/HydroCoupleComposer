/*!
 * \file   test_preferencesdialog.cpp
 * \brief  The preferences dialog: what it shows, what it writes, and when.
 *
 * Driven through its widgets by object name, over a manager with its own
 * .ini under the fixture directory. The round-trip gate is the one that
 * matters: every widget set, OK pressed, a second dialog opened — and
 * every widget showing what was set. A page whose widget is not wired both
 * ways passes any narrower test.
 */

#include "core/preferencesmanager.h"
#include "ui/dialogs/preferencesdialog.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>

using namespace HydroCouple::Composer;

namespace
{
  class PreferencesDialogTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_preferencesdialog";
          static char *argv[] = {arg0, nullptr};
          s_app = new QApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override
      {
        const QDir directory(QStringLiteral(COMPOSER_PREFERENCES_FIXTURE_DIR));
        ASSERT_TRUE(directory.mkpath(QStringLiteral(".")));

        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        path = directory.filePath(
          QStringLiteral("test_preferencesdialog.%1.ini").arg(info->name()));
        QFile::remove(path);

        settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
        prefs = std::make_unique<PreferencesManager>(settings.get());
        dialog = std::make_unique<PreferencesDialog>(prefs.get());
      }

      template <typename Widget>
      [[nodiscard]] Widget *widget(const QString &name,
                                   PreferencesDialog *in = nullptr) const
      {
        auto *found = (in ? in : dialog.get())->findChild<Widget *>(name);
        EXPECT_NE(found, nullptr) << name.toStdString();

        return found;
      }

      [[nodiscard]] QPushButton *button(QDialogButtonBox::StandardButton which)
      {
        return widget<QDialogButtonBox>(QStringLiteral("buttons"))
          ->button(which);
      }

      //! Sets every widget to a value that is not its default.
      void editEverything()
      {
        widget<QCheckBox>(QStringLiteral("showWelcome"))->setChecked(false);
        widget<QSpinBox>(QStringLiteral("recentLimit"))->setValue(4);
        widget<QRadioButton>(QStringLiteral("themeDark"))->setChecked(true);
        widget<QListWidget>(QStringLiteral("searchPaths"))
          ->addItems({QStringLiteral("/lib/a"), QStringLiteral("/lib/b")});
        widget<QCheckBox>(QStringLiteral("rescanOnStartUp"))->setChecked(false);
        widget<QDoubleSpinBox>(QStringLiteral("pickTolerance"))->setValue(9.5);
        widget<QSpinBox>(QStringLiteral("dragThreshold"))->setValue(7);
        widget<QDoubleSpinBox>(QStringLiteral("snapTolerance"))->setValue(14.0);
        widget<QToolButton>(QStringLiteral("selectionColor"))
          ->setProperty("color", QColor(255, 0, 128));
        widget<QLineEdit>(QStringLiteral("defaultCrs"))
          ->setText(QStringLiteral("EPSG:26912"));
        widget<QToolButton>(QStringLiteral("sceneBackground"))
          ->setProperty("color", QColor(10, 20, 30));
        widget<QComboBox>(QStringLiteral("sceneProjection"))
          ->setCurrentIndex(1);
        widget<QDoubleSpinBox>(QStringLiteral("exaggeration"))->setValue(3.5);
      }

      QString path;
      std::unique_ptr<QSettings> settings;
      std::unique_ptr<PreferencesManager> prefs;
      std::unique_ptr<PreferencesDialog> dialog;

      static inline QApplication *s_app = nullptr;
  };
}

TEST_F(PreferencesDialogTest, TheCategoriesAreTheManagersGroupsAndOpenByName)
{
  EXPECT_EQ(dialog->categories(),
            (QStringList{QStringLiteral("General"), QStringLiteral("Appearance"),
                         QStringLiteral("Components"),
                         QStringLiteral("Selection"), QStringLiteral("Map"),
                         QStringLiteral("3D View")}));

  // Every group the manager declares has a page, so no preference can be
  // set in code and unreachable in the dialog.
  for (const PreferencesManager::Key &key : PreferencesManager::keys())
  {
    EXPECT_TRUE(dialog->categories().contains(key.group))
      << key.group.toStdString();
  }

  auto *pages = widget<QStackedWidget>(QStringLiteral("pages"));
  dialog->openAtCategory(QStringLiteral("Map"));
  EXPECT_EQ(pages->currentIndex(), 4);

  dialog->openAtCategory(QStringLiteral("Nowhere"));
  EXPECT_EQ(pages->currentIndex(), 4) << "an unknown title changes nothing";
}

TEST_F(PreferencesDialogTest, EveryWidgetRoundTripsThroughOkAndAFreshDialog)
{
  editEverything();
  button(QDialogButtonBox::Ok)->click();

  EXPECT_EQ(dialog->result(), QDialog::Accepted);

  // What the manager holds...
  EXPECT_FALSE(prefs->showWelcomeOnStartUp());
  EXPECT_EQ(prefs->recentLimit(), 4);
  EXPECT_EQ(prefs->themeMode(), QStringLiteral("Dark"));
  EXPECT_EQ(prefs->componentSearchPaths(),
            (QStringList{QStringLiteral("/lib/a"), QStringLiteral("/lib/b")}));
  EXPECT_FALSE(prefs->rescanComponentsOnStartUp());
  EXPECT_DOUBLE_EQ(prefs->pickTolerancePixels(), 9.5);
  EXPECT_EQ(prefs->dragThresholdPixels(), 7);
  EXPECT_DOUBLE_EQ(prefs->snapTolerancePixels(), 14.0);
  EXPECT_EQ(prefs->selectionColor(), QColor(255, 0, 128));
  EXPECT_EQ(prefs->defaultMapCrs(), QStringLiteral("EPSG:26912"));
  EXPECT_EQ(prefs->sceneBackgroundColor(), QColor(10, 20, 30));
  EXPECT_EQ(prefs->defaultSceneProjection(), QStringLiteral("Orthographic"));
  EXPECT_DOUBLE_EQ(prefs->defaultVerticalExaggeration(), 3.5);

  // ...and what a fresh dialog shows, which is the other half of "wired
  // both ways".
  PreferencesDialog again(prefs.get());
  EXPECT_FALSE(widget<QCheckBox>(QStringLiteral("showWelcome"), &again)->isChecked());
  EXPECT_EQ(widget<QSpinBox>(QStringLiteral("recentLimit"), &again)->value(), 4);
  EXPECT_TRUE(widget<QRadioButton>(QStringLiteral("themeDark"), &again)->isChecked());
  EXPECT_EQ(widget<QListWidget>(QStringLiteral("searchPaths"), &again)->count(), 2);
  EXPECT_FALSE(widget<QCheckBox>(QStringLiteral("rescanOnStartUp"), &again)->isChecked());
  EXPECT_DOUBLE_EQ(widget<QDoubleSpinBox>(QStringLiteral("pickTolerance"), &again)->value(), 9.5);
  EXPECT_EQ(widget<QSpinBox>(QStringLiteral("dragThreshold"), &again)->value(), 7);
  EXPECT_DOUBLE_EQ(widget<QDoubleSpinBox>(QStringLiteral("snapTolerance"), &again)->value(), 14.0);
  EXPECT_EQ(widget<QToolButton>(QStringLiteral("selectionColor"), &again)
              ->property("color").value<QColor>(), QColor(255, 0, 128));
  EXPECT_EQ(widget<QLineEdit>(QStringLiteral("defaultCrs"), &again)->text(),
            QStringLiteral("EPSG:26912"));
  EXPECT_EQ(widget<QToolButton>(QStringLiteral("sceneBackground"), &again)
              ->property("color").value<QColor>(), QColor(10, 20, 30));
  EXPECT_EQ(widget<QComboBox>(QStringLiteral("sceneProjection"), &again)
              ->currentData().toString(), QStringLiteral("Orthographic"));
  EXPECT_DOUBLE_EQ(widget<QDoubleSpinBox>(QStringLiteral("exaggeration"), &again)->value(), 3.5);
}

TEST_F(PreferencesDialogTest, ApplyWritesWithoutClosing)
{
  dialog->show();
  widget<QSpinBox>(QStringLiteral("dragThreshold"))->setValue(9);

  QSignalSpy spy(prefs.get(), &PreferencesManager::preferenceChanged);
  button(QDialogButtonBox::Apply)->click();

  EXPECT_EQ(prefs->dragThresholdPixels(), 9);
  EXPECT_TRUE(dialog->isVisible());

  // One change, one announcement: Apply writes every key, and the manager
  // stays quiet about the ones that did not move.
  EXPECT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.first().at(1).toString(), QStringLiteral("dragThresholdPixels"));
}

TEST_F(PreferencesDialogTest, CancelAfterEditingEverythingChangesNothing)
{
  editEverything();
  button(QDialogButtonBox::Cancel)->click();

  EXPECT_EQ(dialog->result(), QDialog::Rejected);

  for (const PreferencesManager::Key &key : PreferencesManager::keys())
  {
    EXPECT_EQ(prefs->value(key.group, key.name), key.fallback)
      << key.name.toStdString();
  }
}

TEST_F(PreferencesDialogTest, ResetRestoresTheDefaultsAndTheWidgetsFollow)
{
  editEverything();
  button(QDialogButtonBox::Ok)->click();
  ASSERT_EQ(prefs->recentLimit(), 4);

  PreferencesDialog again(prefs.get());
  widget<QPushButton>(QStringLiteral("resetButton"), &again)->click();

  // Applied at once, not staged behind OK: a reset Cancel could undo would
  // be a reset nobody can trust.
  EXPECT_EQ(prefs->recentLimit(), 10);
  EXPECT_EQ(prefs->themeMode(), QStringLiteral("System"));
  EXPECT_EQ(widget<QSpinBox>(QStringLiteral("recentLimit"), &again)->value(), 10);
  EXPECT_TRUE(widget<QRadioButton>(QStringLiteral("themeSystem"), &again)->isChecked());
  EXPECT_EQ(widget<QListWidget>(QStringLiteral("searchPaths"), &again)->count(), 0);
}

TEST_F(PreferencesDialogTest, AnEmptyCrsFallsBackToWebMercator)
{
  widget<QLineEdit>(QStringLiteral("defaultCrs"))->setText(QStringLiteral("   "));
  dialog->apply();

  EXPECT_EQ(prefs->defaultMapCrs(), QStringLiteral("EPSG:3857"));
}

TEST_F(PreferencesDialogTest, TheCrsChooserIsOfferedOnlyWhenTheWindowSuppliesOne)
{
  auto *choose = widget<QToolButton>(QStringLiteral("chooseCrs"));
  auto *line = widget<QLineEdit>(QStringLiteral("defaultCrs"));

  // Hidden without a chooser: the dialog cannot pick a CRS by itself, and
  // a button that does nothing is worse than none.
  EXPECT_TRUE(choose->isHidden());

  QString asked;
  dialog->setCrsChooser(
    [&asked](const QString &current)
    {
      asked = current;

      return QStringLiteral("EPSG:26912");
    });
  EXPECT_FALSE(choose->isHidden());

  choose->click();
  EXPECT_EQ(asked, QStringLiteral("EPSG:3857")) << "the chooser is told the current";
  EXPECT_EQ(line->text(), QStringLiteral("EPSG:26912"));

  // A cancelled chooser answers empty and the field keeps what it had.
  dialog->setCrsChooser([](const QString &) { return QString(); });
  choose->click();
  EXPECT_EQ(line->text(), QStringLiteral("EPSG:26912"));
}

TEST_F(PreferencesDialogTest, RemoveTakesTheCurrentSearchPath)
{
  auto *list = widget<QListWidget>(QStringLiteral("searchPaths"));
  list->addItems({QStringLiteral("/lib/a"), QStringLiteral("/lib/b")});
  list->setCurrentRow(0);

  widget<QPushButton>(QStringLiteral("removeSearchPath"))->click();
  dialog->apply();

  EXPECT_EQ(prefs->componentSearchPaths(), QStringList{QStringLiteral("/lib/b")});
}
