/*!
 * \file   test_preferences.cpp
 * \brief  The global preferences: defaults, persistence, and announcements.
 *
 * Every manager here is built over its own .ini under the fixture
 * directory, so a test never writes where the application reads and what
 * it wrote can be opened afterwards. The suite walks the key table rather
 * than a hand-picked few, because a key that no test round-trips is a key
 * whose type coercion nobody has checked.
 */

#include "core/preferencesmanager.h"

#include <gtest/gtest.h>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>

using namespace HydroCouple::Composer;

namespace
{
  //! A value that differs from \a original and survives an ini store.
  QVariant differentFrom(const QVariant &original)
  {
    switch (original.userType())
    {
      case QMetaType::Bool:
        return !original.toBool();
      case QMetaType::Int:
        return original.toInt() + 1;
      case QMetaType::Double:
        return original.toDouble() + 1.5;
      case QMetaType::QString:
        return original.toString() + QStringLiteral("-changed");
      case QMetaType::QStringList:
        return QStringList{QStringLiteral("/one"), QStringLiteral("/two")};
      case QMetaType::QColor:
      {
        const QColor color = original.value<QColor>();

        return QColor(255 - color.red(), 255 - color.green(),
                      255 - color.blue());
      }
      default:
        ADD_FAILURE() << "no rule for " << original.typeName();

        return {};
    }
  }

  class PreferencesTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_preferences";
          static char *argv[] = {arg0, nullptr};
          s_app = new QCoreApplication(argc, argv);
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
          QStringLiteral("test_preferences.%1.ini").arg(info->name()));
        QFile::remove(path);

        settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
        prefs = std::make_unique<PreferencesManager>(settings.get());
      }

      //! A second reader over the same file, so persistence is what is tested.
      [[nodiscard]] QVariant reread(const QString &group, const QString &name)
      {
        settings->sync();
        QSettings again(path, QSettings::IniFormat);
        PreferencesManager other(&again);

        return other.value(group, name);
      }

      QString path;
      std::unique_ptr<QSettings> settings;
      std::unique_ptr<PreferencesManager> prefs;

      static inline QCoreApplication *s_app = nullptr;
  };
}

TEST_F(PreferencesTest, DefaultsApplyWhenNothingIsStored)
{
  for (const PreferencesManager::Key &key : PreferencesManager::keys())
  {
    EXPECT_EQ(prefs->value(key.group, key.name), key.fallback)
      << key.group.toStdString() << '/' << key.name.toStdString();
  }

  // The defaults are the values the code hard-coded before they were
  // preferences, so a fresh install behaves as it did.
  EXPECT_TRUE(prefs->showWelcomeOnStartUp());
  EXPECT_EQ(prefs->recentLimit(), 10);
  EXPECT_EQ(prefs->themeMode(), QStringLiteral("System"));
  EXPECT_DOUBLE_EQ(prefs->pickTolerancePixels(), 6.0);
  EXPECT_EQ(prefs->dragThresholdPixels(), 3);
  EXPECT_DOUBLE_EQ(prefs->snapTolerancePixels(), 10.0);
  EXPECT_EQ(prefs->selectionColor(), QColor(0, 200, 255));
  EXPECT_EQ(prefs->defaultMapCrs(), QStringLiteral("EPSG:3857"));
  EXPECT_EQ(prefs->sceneBackgroundColor(), QColor(0x1a, 0x1d, 0x21));
  EXPECT_EQ(prefs->defaultSceneProjection(), QStringLiteral("Perspective"));
  EXPECT_DOUBLE_EQ(prefs->defaultVerticalExaggeration(), 1.0);
  EXPECT_TRUE(prefs->componentSearchPaths().isEmpty());
}

TEST_F(PreferencesTest, EveryKeyRoundTripsThroughTheStore)
{
  for (const PreferencesManager::Key &key : PreferencesManager::keys())
  {
    const QVariant changed = differentFrom(key.fallback);
    ASSERT_TRUE(prefs->setValue(key.group, key.name, changed));

    const QVariant back = reread(key.group, key.name);

    // Value only: within one process QSettings hands back its own typed
    // cache, so the type claim is made by the ini-text test below instead.
    EXPECT_EQ(back, changed) << key.name.toStdString();
  }
}

TEST_F(PreferencesTest, EverySetterAnnouncesItsOwnKeyExactlyOnce)
{
  for (const PreferencesManager::Key &key : PreferencesManager::keys())
  {
    QSignalSpy spy(prefs.get(), &PreferencesManager::preferenceChanged);
    prefs->setValue(key.group, key.name, differentFrom(key.fallback));

    ASSERT_EQ(spy.count(), 1) << key.name.toStdString();
    EXPECT_EQ(spy.first().at(0).toString(), key.group);
    EXPECT_EQ(spy.first().at(1).toString(), key.name);
  }
}

TEST_F(PreferencesTest, AnUnchangedValueIsStoredSilently)
{
  prefs->setPickTolerancePixels(9.0);

  QSignalSpy spy(prefs.get(), &PreferencesManager::preferenceChanged);
  EXPECT_TRUE(prefs->setValue(QStringLiteral("Selection"),
                              QStringLiteral("pickTolerancePixels"), 9.0));

  // A dialog's Apply touches every key; announcing the unchanged ones
  // would make every listener redo its work for nothing.
  EXPECT_EQ(spy.count(), 0);
}

TEST_F(PreferencesTest, AnUndeclaredKeyIsRefusedAndWritesNothing)
{
  QSignalSpy spy(prefs.get(), &PreferencesManager::preferenceChanged);

  EXPECT_FALSE(prefs->setValue(QStringLiteral("Nowhere"),
                               QStringLiteral("nothing"), 1));
  EXPECT_FALSE(prefs->value(QStringLiteral("Nowhere"),
                            QStringLiteral("nothing")).isValid());
  EXPECT_EQ(spy.count(), 0);

  settings->sync();
  EXPECT_TRUE(QSettings(path, QSettings::IniFormat).allKeys().isEmpty());
}

TEST_F(PreferencesTest, ResetForgetsWhatWasSetAndAnnouncesOnlyThat)
{
  prefs->setRecentLimit(4);
  prefs->setSelectionColor(Qt::red);

  QSignalSpy spy(prefs.get(), &PreferencesManager::preferenceChanged);
  prefs->resetToDefaults();

  EXPECT_EQ(prefs->recentLimit(), 10);
  EXPECT_EQ(prefs->selectionColor(), QColor(0, 200, 255));

  // Two keys differed from their defaults, so two announcements — not one
  // per key in the table, which would redraw everything for nothing.
  EXPECT_EQ(spy.count(), 2);
  EXPECT_EQ(reread(QStringLiteral("General"),
                   QStringLiteral("recentLimit")).toInt(), 10);
}

TEST_F(PreferencesTest, TheThemeModeKeepsTheStorageNameItHadBefore)
{
  prefs->setThemeMode(QStringLiteral("Dark"));
  settings->sync();

  // The application persisted "appearance/mode" before this manager
  // existed; a choice made then is still honoured now.
  EXPECT_EQ(QSettings(path, QSettings::IniFormat)
              .value(QStringLiteral("appearance/mode")).toString(),
            QStringLiteral("Dark"));
}

TEST_F(PreferencesTest, AColourIsStoredAsTextAnyoneCanEdit)
{
  prefs->setSelectionColor(QColor(255, 128, 0));
  settings->sync();

  EXPECT_EQ(QSettings(path, QSettings::IniFormat)
              .value(QStringLiteral("preferences/selection/selectionColor"))
              .toString(),
            QStringLiteral("#ffff8000"));
  EXPECT_EQ(reread(QStringLiteral("Selection"),
                   QStringLiteral("selectionColor")).value<QColor>(),
            QColor(255, 128, 0));
}

TEST_F(PreferencesTest, ASearchPathListOfOneSurvivesTheIniStore)
{
  // QSettings stores a one-element list as a bare string and hands a
  // string back; the manager has to return a list regardless.
  prefs->setComponentSearchPaths({QStringLiteral("/only")});

  const QVariant back = reread(QStringLiteral("Components"),
                               QStringLiteral("searchPaths"));
  EXPECT_EQ(back.userType(), QMetaType::QStringList);
  EXPECT_EQ(back.toStringList(), QStringList{QStringLiteral("/only")});
}

TEST_F(PreferencesTest, ValuesWrittenAsIniTextComeBackWithTheirTypes)
{
  // Within one process QSettings serves its own typed cache, so a
  // round-trip through two managers never sees ini text. A restart does:
  // the file is written by hand here, which is what the next launch reads.
  const QString textPath = path + QStringLiteral(".text.ini");
  QFile::remove(textPath);
  {
    QFile file(textPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    // "[General]" is Qt's reserved prefix-less section, which is why the
    // manager stores under a prefix of its own; this text is what it writes.
    file.write("[preferences]\ngeneral\\showWelcomeOnStartUp=false\n"
               "general\\recentLimit=4\n"
               "selection\\pickTolerancePixels=9.5\n"
               "selection\\dragThresholdPixels=7\n"
               "components\\searchPaths=/a, /b\n"
               "selection\\selectionColor=#ffff8000\n");
  }

  QSettings text(textPath, QSettings::IniFormat);
  PreferencesManager fromText(&text);

  EXPECT_FALSE(fromText.showWelcomeOnStartUp());
  EXPECT_EQ(fromText.recentLimit(), 4);
  EXPECT_DOUBLE_EQ(fromText.pickTolerancePixels(), 9.5);
  EXPECT_EQ(fromText.dragThresholdPixels(), 7);
  EXPECT_EQ(fromText.componentSearchPaths(),
            (QStringList{QStringLiteral("/a"), QStringLiteral("/b")}));
  EXPECT_EQ(fromText.selectionColor(), QColor(255, 128, 0));

  // The type, not only the value: a "9.5" that stays a string is a
  // tolerance nobody can multiply.
  EXPECT_EQ(fromText.value(QStringLiteral("Selection"),
                           QStringLiteral("pickTolerancePixels")).userType(),
            QMetaType::Double);
}

TEST_F(PreferencesTest, AStoredValueOfTheWrongTypeFallsBackToTheDefault)
{
  settings->setValue(
    QStringLiteral("preferences/selection/pickTolerancePixels"),
    QStringLiteral("six"));
  settings->sync();

  // Garbage in the file is treated as absent rather than handed on: a
  // tolerance of NaN would make every pick miss without a message.
  EXPECT_DOUBLE_EQ(prefs->pickTolerancePixels(), 6.0);
}
