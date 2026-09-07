/*!
 * \file   test_serviceconnections.cpp
 * \brief  ARGGEN P6/O2 — web services remembered by name, with credentials
 *         bound to the machine.
 *
 * The store is driven with its own settings file and its own machine id,
 * because the property worth gating — a settings file that decrypts on the
 * machine that wrote it and nowhere else — is only observable when the test
 * can be two machines.
 */

#include "core/composerapplication.h"
#include "layers/serviceconnections.h"
#include "ui/dialogs/ogcservicedialog.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>

using namespace HydroCouple::Composer;

namespace
{
  class ServiceConnectionsTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_serviceconnections";
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
        // A real file in a reviewable place, not the machine's own
        // settings: a test must never write where the application reads.
        ASSERT_TRUE(directory.isValid());
        settings = std::make_unique<QSettings>(
          directory.filePath(QStringLiteral("connections.ini")),
          QSettings::IniFormat);
      }

      [[nodiscard]] QString settingsText() const
      {
        QFile file(directory.filePath(QStringLiteral("connections.ini")));
        EXPECT_TRUE(file.open(QIODevice::ReadOnly));

        return QString::fromUtf8(file.readAll());
      }

      QTemporaryDir directory;
      std::unique_ptr<QSettings> settings;

      static inline ComposerApplication *s_app = nullptr;
  };

  ServiceConnection sample()
  {
    ServiceConnection connection;
    connection.name = QStringLiteral("Kadaster");
    connection.url = QStringLiteral("https://service.example.org/ows");
    connection.username = QStringLiteral("hydro");
    connection.password = QStringLiteral("correct horse battery staple");

    return connection;
  }
}

TEST_F(ServiceConnectionsTest, ASavedConnectionComesBackWholeIncludingItsPassword)
{
  ServiceConnections store(settings.get(), QByteArrayLiteral("machine-one"));

  ASSERT_TRUE(store.save(sample()));
  EXPECT_EQ(store.names(), QStringList{QStringLiteral("Kadaster")});

  const ServiceConnection read = store.connection(QStringLiteral("Kadaster"));
  ASSERT_TRUE(read.isValid());
  EXPECT_EQ(read.url, sample().url);
  EXPECT_EQ(read.username, sample().username);
  EXPECT_EQ(read.password, sample().password);

  // An unknown name is not a half-filled connection.
  EXPECT_FALSE(store.connection(QStringLiteral("nothing")).isValid());

  EXPECT_TRUE(store.remove(QStringLiteral("Kadaster")));
  EXPECT_TRUE(store.names().isEmpty());
  EXPECT_FALSE(store.remove(QStringLiteral("Kadaster")));
}

TEST_F(ServiceConnectionsTest, ThePasswordIsNeverOnDiskInTheClear)
{
  ServiceConnections store(settings.get(), QByteArrayLiteral("machine-one"));
  ASSERT_TRUE(store.save(sample()));
  settings->sync();

  const QString text = settingsText();

  // The address is not a secret and is readable; the password is.
  EXPECT_TRUE(text.contains(QStringLiteral("service.example.org")))
    << text.toStdString();
  EXPECT_FALSE(text.contains(QStringLiteral("correct horse battery staple")))
    << "the password was written out in the clear";

  // Nor is it merely encoded: the same password saved twice must not
  // produce the same bytes, or a store is a rainbow table of its users.
  ServiceConnection second = sample();
  second.name = QStringLiteral("Second");
  ASSERT_TRUE(store.save(second));
  settings->sync();

  const QString first =
    settings->value(QStringLiteral("serviceConnections/Kadaster/password"))
      .toByteArray()
      .toBase64();
  const QString repeat =
    settings->value(QStringLiteral("serviceConnections/Second/password"))
      .toByteArray()
      .toBase64();

  EXPECT_FALSE(first.isEmpty());
  EXPECT_NE(first, repeat) << "the same secret encrypted to the same bytes";
}

TEST_F(ServiceConnectionsTest, AnotherMachineGetsTheAddressButNotThePassword)
{
  ServiceConnections mine(settings.get(), QByteArrayLiteral("machine-one"));
  ASSERT_TRUE(mine.save(sample()));
  settings->sync();

  // The same file, opened where it was not written — copied, synced, or
  // restored from a backup.
  ServiceConnections theirs(settings.get(),
                            QByteArrayLiteral("machine-two"));

  const ServiceConnection read =
    theirs.connection(QStringLiteral("Kadaster"));

  ASSERT_TRUE(read.isValid()) << "the address was lost with the password";
  EXPECT_EQ(read.url, sample().url);
  EXPECT_EQ(read.username, sample().username);
  EXPECT_TRUE(read.password.isEmpty())
    << "a password from another machine was handed back: "
    << read.password.toStdString();
}

TEST_F(ServiceConnectionsTest, TheDialogSavesAndReloadsWhatWasTypedIn)
{
  ServiceConnections store(settings.get(), QByteArrayLiteral("machine-one"));

  OgcServiceDialog dialog;
  dialog.setConnections(&store);

  auto *url = dialog.findChild<QLineEdit *>(QStringLiteral("serviceUrlEdit"));
  auto *user =
    dialog.findChild<QLineEdit *>(QStringLiteral("serviceUsernameEdit"));
  auto *password =
    dialog.findChild<QLineEdit *>(QStringLiteral("servicePasswordEdit"));
  auto *saved =
    dialog.findChild<QComboBox *>(QStringLiteral("serviceSavedCombo"));
  ASSERT_NE(url, nullptr);
  ASSERT_NE(saved, nullptr);

  url->setText(sample().url);
  user->setText(sample().username);
  password->setText(sample().password);

  ASSERT_TRUE(dialog.saveConnectionAs(QStringLiteral("Kadaster")));

  // The list now offers it, and the row that means "not one of these" is
  // still first.
  EXPECT_EQ(saved->count(), 2);
  EXPECT_EQ(saved->itemText(1), QStringLiteral("Kadaster"));

  // A fresh dialog over the same store fills its fields from the name.
  OgcServiceDialog reopened;
  reopened.setConnections(&store);
  reopened.loadConnection(QStringLiteral("Kadaster"));

  EXPECT_EQ(reopened.findChild<QLineEdit *>(QStringLiteral("serviceUrlEdit"))
              ->text(),
            sample().url);
  EXPECT_EQ(
    reopened.findChild<QLineEdit *>(QStringLiteral("servicePasswordEdit"))
      ->text(),
    sample().password);

  // An address is required; a name alone saves nothing.
  OgcServiceDialog empty;
  empty.setConnections(&store);
  EXPECT_FALSE(empty.saveConnectionAs(QStringLiteral("Nothing")));
  EXPECT_FALSE(store.names().contains(QStringLiteral("Nothing")));
}
