/*!
 * \file test_wcscoveragelayer.cpp
 * \brief A coverage fetched from a WCS, once it is bytes.
 *
 * The fixture is a real GetCoverage response from PDOK's Actueel
 * Hoogtebestand Nederland — 64 by 64 cells of the Dutch national elevation
 * model at half-metre resolution, in the national grid, saved verbatim. It
 * is what the service actually returns, not a GeoTIFF written to look like
 * one, which matters because the point of this layer is that GDAL can decode
 * bytes it could never have fetched.
 */

#include "layers/wcscoveragelayer.h"

#include <gtest/gtest.h>

#include <cpl_string.h>
#include <cpl_vsi.h>

#include <QByteArray>
#include <QFile>
#include <QString>

using namespace HydroCouple::Composer;

namespace
{
  QByteArray fixture(const QString &name)
  {
    QFile file(QStringLiteral(COMPOSER_OGC_FIXTURE_DIR "/") + name);

    if (!file.open(QIODevice::ReadOnly))
    {
      return {};
    }

    return file.readAll();
  }

  QByteArray coverage()
  {
    return fixture(QStringLiteral("wcs-coverage-ahn-dtm.tif"));
  }

  //! How many /vsimem files this process is holding.
  int memoryFileCount()
  {
    char **entries = VSIReadDir("/vsimem/");
    const int count = entries ? CSLCount(entries) : 0;
    CSLDestroy(entries);

    return count;
  }
} // namespace

TEST(WcsCoverageLayer, aFetchedCoverageBecomesAReadableRaster)
{
  QString message;

  std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
    coverage(), QStringLiteral("AHN DTM"), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  EXPECT_EQ(layer->rasterSize().width(), 64);
  EXPECT_EQ(layer->rasterSize().height(), 64);
  EXPECT_EQ(layer->bandCount(), 1);

  // One band of floats is data, not a picture, so it is shaded with a ramp
  // rather than shown as it stands.
  EXPECT_FALSE(layer->isColorImage());
}

TEST(WcsCoverageLayer, itLandsWhereTheServiceWasAskedAbout)
{
  QString message;

  std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
    coverage(), QStringLiteral("AHN DTM"), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  // The request was subset x(120000,120032) y(486000,486032) in EPSG:28992,
  // and the coverage has to come back on that ground: a raster that decodes
  // but lands somewhere else is the failure this catches, and it looks
  // perfectly fine on screen until it is put beside anything.
  const QRectF extent = layer->extent();

  EXPECT_NEAR(extent.left(), 120000.0, 1.0) << extent.left();
  EXPECT_NEAR(extent.right(), 120032.0, 1.0) << extent.right();
  EXPECT_NEAR(extent.top(), 486000.0, 1.0) << extent.top();
  EXPECT_NEAR(extent.bottom(), 486032.0, 1.0) << extent.bottom();
}

TEST(WcsCoverageLayer, itCarriesTheSystemTheServiceReturnedItIn)
{
  QString message;

  std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
    coverage(), QStringLiteral("AHN DTM"), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  // Without this the numbers above are read as degrees and the Netherlands
  // is placed off the coast of Africa.
  ASSERT_NE(layer->crs(), nullptr);
}

TEST(WcsCoverageLayer, theValuesAreElevationsNotColours)
{
  QString message;

  std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
    coverage(), QStringLiteral("AHN DTM"), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  double minimum = 0.0;
  double maximum = 0.0;
  layer->valueRange(minimum, maximum);

  // Metres above NAP. The service's own description bounds this coverage at
  // -8 to 322; this is 64 cells of one Dutch field, so anything outside that
  // envelope means the band was read as something it is not.
  EXPECT_LT(minimum, maximum);
  EXPECT_GE(minimum, -8.0);
  EXPECT_LE(maximum, 322.0);
}

TEST(WcsCoverageLayer, itSaysWhereItCameFromRatherThanWhereGdalReadIt)
{
  QString message;

  std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
    coverage(), QStringLiteral("AHN DTM"), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  layer->setServiceUrl(QStringLiteral("https://service.pdok.nl/rws/ahn/wcs/v1_0"));
  layer->setCoverageId(QStringLiteral("dtm_05m"));

  EXPECT_FALSE(layer->sourceDescription().contains(QStringLiteral("vsimem")));
  EXPECT_TRUE(layer->sourceDescription().contains(QStringLiteral("dtm_05m")));
}

TEST(WcsCoverageLayer, twoCoveragesDoNotShareOneBuffer)
{
  QString message;

  std::unique_ptr<WcsCoverageLayer> first = WcsCoverageLayer::fromResponse(
    coverage(), QStringLiteral("first"), message);
  std::unique_ptr<WcsCoverageLayer> second = WcsCoverageLayer::fromResponse(
    coverage(), QStringLiteral("second"), message);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  // A raster's dataset stays open for the layer's whole life -- it is read
  // again on every repaint -- so a name shared between layers would be two
  // datasets over one buffer. Both must still be readable at once.
  EXPECT_EQ(first->rasterSize(), second->rasterSize());

  const int both = memoryFileCount();

  // Destroying one must not take the other's buffer with it, which is what
  // a shared name does: the first layer's unlink removes the second's file.
  first.reset();

  EXPECT_EQ(memoryFileCount(), both - 1);
  EXPECT_GT(second->extent().width(), 0.0);

  second.reset();

  EXPECT_EQ(memoryFileCount(), both - 2);
}

TEST(WcsCoverageLayer, aClosedLayerLeavesNothingBehindInMemory)
{
  const int before = memoryFileCount();

  {
    QString message;

    std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
      coverage(), QStringLiteral("AHN DTM"), message);

    ASSERT_NE(layer, nullptr) << message.toStdString();
    EXPECT_GT(memoryFileCount(), before);
  }

  // /vsimem is process-wide and this buffer is megabytes for a real
  // coverage. A layer that forgot to unlink would hold it until the program
  // exited, and nothing on screen would ever say so.
  EXPECT_EQ(memoryFileCount(), before);
}

TEST(WcsCoverageLayer, anExceptionReportIsReportedRatherThanDrawn)
{
  QString message;

  // A WCS answers a bad request with XML as readily under a 200 as under a
  // 404, so what arrives where a coverage was expected is often a sentence
  // explaining the mistake. It belongs in front of the user.
  std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
    fixture(QStringLiteral("wcs-exception-invalid-axis.xml")),
    QStringLiteral("AHN DTM"), message);

  EXPECT_EQ(layer, nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("InvalidAxisLabel")))
    << message.toStdString();
}

TEST(WcsCoverageLayer, anEmptyAnswerSaysSo)
{
  QString message;

  std::unique_ptr<WcsCoverageLayer> layer = WcsCoverageLayer::fromResponse(
    QByteArray(), QStringLiteral("AHN DTM"), message);

  EXPECT_EQ(layer, nullptr);

  // Distinct from "that is not a coverage": a service that answered with
  // nothing has failed differently from one that answered with something
  // unreadable, and the difference is the first thing to check.
  EXPECT_TRUE(message.contains(QStringLiteral("returned nothing")))
    << message.toStdString();
}
