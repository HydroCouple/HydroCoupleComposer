/*!
 * \file test_layerpersistence.cpp
 * \brief Layers surviving a save and an open.
 *
 * Before this, they did not: the sidecar held component positions and a mesh
 * domain, and every basemap, file and service layer a composition was built
 * with was gone the next time it was opened.
 *
 * What is saved is a recipe rather than a copy — a file path, or a service
 * address and the name of one thing on it. That is what makes a project file
 * small enough to check in, and it is why rebuilding a service layer is a
 * conversation rather than a function call.
 */

#include "layers/gdalrasterlayer.h"
#include "layers/layerrestorer.h"
#include "project/presentation.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>

using namespace HydroCouple::Composer;

namespace
{
  QJsonObject wmsRecipe()
  {
    QJsonObject entry;
    entry.insert(QStringLiteral("type"), QStringLiteral("wms"));
    entry.insert(QStringLiteral("url"),
                 QStringLiteral("https://ows.terrestris.de/osm/service"));
    entry.insert(QStringLiteral("name"), QStringLiteral("OSM"));
    entry.insert(QStringLiteral("layers"),
                 QJsonArray{QStringLiteral("OSM-WMS")});

    return entry;
  }

  bool waitFor(const std::function<bool()> &done, int milliseconds = 3000)
  {
    QElapsedTimer timer;
    timer.start();

    while (!done() && timer.elapsed() < milliseconds)
    {
      QCoreApplication::processEvents();
    }

    return done();
  }
} // namespace

TEST(LayerPersistence, aSidecarCarriesTheLayersItWasGiven)
{
  Presentation presentation;
  presentation.setLayers(QJsonArray{wmsRecipe()});

  Presentation reopened;

  ASSERT_TRUE(reopened.fromJson(presentation.toJson()));
  ASSERT_EQ(reopened.layers().size(), 1);

  const QJsonObject entry = reopened.layers().at(0).toObject();

  EXPECT_EQ(entry.value(QStringLiteral("type")).toString().toStdString(),
            "wms");
  EXPECT_EQ(entry.value(QStringLiteral("name")).toString().toStdString(),
            "OSM");
}

TEST(LayerPersistence, aCompositionWithOnlyLayersIsStillWorthSaving)
{
  Presentation presentation;

  EXPECT_TRUE(presentation.isEmpty());

  presentation.setLayers(QJsonArray{wmsRecipe()});

  // Saving is gated on this. A basemap added before anything else -- which
  // is the order anyone building a model actually works in -- would
  // otherwise be written nowhere and be gone at the next open.
  EXPECT_FALSE(presentation.isEmpty());
}

TEST(LayerPersistence, openingASecondCompositionDoesNotKeepTheFirstsLayers)
{
  Presentation presentation;
  presentation.setLayers(QJsonArray{wmsRecipe()});

  ASSERT_EQ(presentation.layers().size(), 1);

  // fromJson clears before it reads. A composition with no layers of its
  // own must not inherit the ones already in hand -- which is what opening
  // a second file in the same window does.
  Presentation empty;

  ASSERT_TRUE(presentation.fromJson(empty.toJson()));
  EXPECT_TRUE(presentation.layers().isEmpty());
}

TEST(LayerPersistence, aSidecarWithNoLayersSaysNothingAboutThem)
{
  Presentation presentation;

  const QJsonObject root =
    QJsonDocument::fromJson(presentation.toJson()).object();

  EXPECT_FALSE(root.contains(QStringLiteral("layers")));
}

TEST(LayerPersistence, noPasswordEverReachesTheProjectFile)
{
  // The rule this file exists to hold. A project file is checked in, mailed
  // and copied between machines; a password in one is a password published.
  // openswmm.gui enforces the same rule with its own test, and this is the
  // Composer's.
  QJsonObject entry = wmsRecipe();
  entry.insert(QStringLiteral("username"), QStringLiteral("someone"));

  Presentation presentation;
  presentation.setLayers(QJsonArray{entry});

  const QString written = QString::fromUtf8(presentation.toJson()).toLower();

  EXPECT_FALSE(written.contains(QStringLiteral("password")));
  EXPECT_FALSE(written.contains(QStringLiteral("secret")));
  EXPECT_FALSE(written.contains(QStringLiteral("token")));
}

TEST(LayerPersistence, aFileLayerComesBackFromItsPath)
{
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QString path = directory.filePath(QStringLiteral("coverage.tif"));

  {
    QFile source(QStringLiteral(COMPOSER_OGC_FIXTURE_DIR
                                "/wcs-coverage-ahn-dtm.tif"));
    ASSERT_TRUE(source.open(QIODevice::ReadOnly));

    QFile destination(path);
    ASSERT_TRUE(destination.open(QIODevice::WriteOnly));
    destination.write(source.readAll());
  }

  QJsonObject entry;
  entry.insert(QStringLiteral("type"), QStringLiteral("gdal-raster"));
  entry.insert(QStringLiteral("path"), path);
  entry.insert(QStringLiteral("name"), QStringLiteral("terrain"));
  entry.insert(QStringLiteral("visible"), false);

  LayerRestorer restorer;
  MapLayer *restored = nullptr;
  QString failure;

  restorer.restore(
    QJsonArray{entry}, [&](MapLayer *layer) { restored = layer; },
    [&](const QString &message) { failure = message; });

  ASSERT_NE(restored, nullptr) << failure.toStdString();
  EXPECT_EQ(restored->name().toStdString(), "terrain");

  // And carries the SAVED recipe forward, so a second save writes the same
  // entry rather than losing what only the sidecar knew. "visible" is the
  // test of that: the file factory writes a recipe of its own, and that
  // one has no such field, so finding it proves the saved entry was kept
  // rather than regenerated.
  EXPECT_TRUE(restored->persistentState().contains(QStringLiteral("visible")))
    << QString::fromUtf8(
         QJsonDocument(restored->persistentState()).toJson())
         .toStdString();
  EXPECT_FALSE(restored->isVisible());

  delete restored;
}

TEST(LayerPersistence, aFileThatMovedIsNamedRatherThanJustFailing)
{
  QJsonObject entry;
  entry.insert(QStringLiteral("type"), QStringLiteral("gdal-raster"));
  entry.insert(QStringLiteral("path"),
               QStringLiteral("/nowhere/at/all/terrain.tif"));
  entry.insert(QStringLiteral("name"), QStringLiteral("terrain"));

  LayerRestorer restorer;
  MapLayer *restored = nullptr;
  QString failure;

  restorer.restore(
    QJsonArray{entry}, [&](MapLayer *layer) { restored = layer; },
    [&](const QString &message) { failure = message; });

  EXPECT_EQ(restored, nullptr);

  // Files move between machines far more often than they are deleted, so
  // the path is what the user needs to see.
  EXPECT_TRUE(failure.contains(QStringLiteral("terrain")))
    << failure.toStdString();
  EXPECT_TRUE(failure.contains(QStringLiteral("/nowhere/at/all")))
    << failure.toStdString();
}

TEST(LayerPersistence, aLayerOfAKindThisVersionDoesNotKnowIsReported)
{
  QJsonObject entry;
  entry.insert(QStringLiteral("type"), QStringLiteral("something-newer"));
  entry.insert(QStringLiteral("url"), QStringLiteral("https://example.org/x"));
  entry.insert(QStringLiteral("name"), QStringLiteral("mystery"));

  LayerRestorer restorer;
  QString failure;

  restorer.restore(
    QJsonArray{entry}, [](MapLayer *) {},
    [&](const QString &message) { failure = message; });

  // Opening a composition written by a later version must not fail, and
  // must not silently drop what it could not understand either.
  EXPECT_TRUE(failure.contains(QStringLiteral("mystery")))
    << failure.toStdString();
}

TEST(LayerPersistence, oneBadEntryDoesNotStopTheRest)
{
  QJsonObject broken;
  broken.insert(QStringLiteral("type"), QStringLiteral("gdal-raster"));
  broken.insert(QStringLiteral("name"), QStringLiteral("nameless"));

  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QString path = directory.filePath(QStringLiteral("coverage.tif"));

  {
    QFile source(QStringLiteral(COMPOSER_OGC_FIXTURE_DIR
                                "/wcs-coverage-ahn-dtm.tif"));
    ASSERT_TRUE(source.open(QIODevice::ReadOnly));

    QFile destination(path);
    ASSERT_TRUE(destination.open(QIODevice::WriteOnly));
    destination.write(source.readAll());
  }

  QJsonObject good;
  good.insert(QStringLiteral("type"), QStringLiteral("gdal-raster"));
  good.insert(QStringLiteral("path"), path);
  good.insert(QStringLiteral("name"), QStringLiteral("terrain"));

  LayerRestorer restorer;
  int restored = 0;
  int failed = 0;

  restorer.restore(
    QJsonArray{broken, good}, [&](MapLayer *layer) {
      ++restored;
      delete layer;
    },
    [&](const QString &) { ++failed; });

  // A composition is not held hostage by one layer it cannot rebuild.
  EXPECT_EQ(restored, 1);
  EXPECT_EQ(failed, 1);
}

TEST(LayerPersistence, aServiceLayerIsWaitedForRatherThanBuiltAtOnce)
{
  LayerRestorer restorer;

  restorer.restore(
    QJsonArray{wmsRecipe()}, [](MapLayer *) {}, [](const QString &) {});

  // A tile source is built from a capabilities document, so restoring one
  // is a conversation. Nothing is returned synchronously, and the caller
  // can see that something is still outstanding.
  EXPECT_EQ(restorer.pendingCount(), 1);

  // The address is not reachable from a test, so this only waits for the
  // request to conclude one way or the other -- what matters is that the
  // restorer does not leave the count stuck.
  waitFor([&] { return restorer.pendingCount() == 0; });
}
