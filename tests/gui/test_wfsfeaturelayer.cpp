/*!
 * \file   test_wfsfeaturelayer.cpp
 * \brief  Features fetched from a service, and made into a mesh domain.
 *
 * The point of a feature service, as against the map services beside it:
 * what comes back is the catchment rather than a picture of it, so it can
 * be selected and imported. The last gate here is the whole reason the
 * layer exists — a fetched boundary reaching the domain through the same
 * importer a file goes through, with nothing added for its being remote.
 */

#include "core/composerapplication.h"
#include "layers/wfsfeaturelayer.h"
#include "mesh/domainimport.h"
#include "mesh/meshdomainmodel.h"

#include <gtest/gtest.h>

#include <QByteArray>

#include <cpl_conv.h>
#include <cpl_string.h>
#include <cpl_vsi.h>

using namespace HydroCouple::Composer;

namespace
{
  //! A GeoJSON answer of the shape a WFS returns.
  QByteArray geoJson()
  {
    return R"({
      "type": "FeatureCollection",
      "crs": {"type": "name",
              "properties": {"name": "urn:ogc:def:crs:EPSG::4326"}},
      "features": [
        {"type": "Feature",
         "properties": {"name": "Upper", "area_km2": 12.5},
         "geometry": {"type": "Polygon",
                      "coordinates": [[[0,0],[10,0],[10,10],[0,10],[0,0]],
                                      [[2,2],[4,2],[4,4],[2,4],[2,2]]]}},
        {"type": "Feature",
         "properties": {"name": "Lower", "area_km2": 3.25},
         "geometry": {"type": "Polygon",
                      "coordinates": [[[20,0],[26,0],[26,6],[20,6],[20,0]]]}}
      ]})";
  }

  //! Whether this program has left anything in GDAL's in-memory namespace.
  bool vsimemIsClean()
  {
    char **entries = VSIReadDirRecursive("/vsimem/");

    if (!entries)
    {
      return true;
    }

    bool clean = true;

    for (int i = 0; entries[i]; ++i)
    {
      if (QString::fromUtf8(entries[i]).contains(
            QStringLiteral("hydrocouple-wfs")))
      {
        clean = false;
      }
    }

    CSLDestroy(entries);

    return clean;
  }

  class WfsFeatureLayerTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_wfsfeaturelayer";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static inline ComposerApplication *s_app = nullptr;
  };
}

TEST_F(WfsFeatureLayerTest, WhatCameBackIsFeaturesWithTheirFieldsAndSystem)
{
  QString message;
  const std::unique_ptr<WfsFeatureLayer> layer = WfsFeatureLayer::fromResponse(
    geoJson(), QStringLiteral("Catchments"), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();
  EXPECT_EQ(layer->featureCount(), 2);

  // The attributes come with them, which is what makes a fetched layer
  // classifiable and queryable rather than merely visible.
  ASSERT_EQ(layer->attributeFields().size(), 2);
  EXPECT_EQ(layer->attributeFields().at(0).name, QStringLiteral("name"));

  // GDAL decoded these bytes. It could not have fetched them: this build
  // has no curl, so its WFS driver is not registered at all.
  EXPECT_NE(layer->crs(), nullptr)
    << "the system the service answered in was not read";
}

TEST_F(WfsFeatureLayerTest, AnAnswerThatIsNotFeaturesSaysWhatTheServiceSaid)
{
  QString message;

  // How a WFS refuses: a document, under an HTTP 200 as often as not.
  const std::unique_ptr<WfsFeatureLayer> refused =
    WfsFeatureLayer::fromResponse(
      "<?xml version=\"1.0\"?><ows:ExceptionReport "
      "xmlns:ows=\"http://www.opengis.net/ows/1.1\"><ows:Exception>"
      "<ows:ExceptionText>Unknown typeName bag:nonesuch</ows:ExceptionText>"
      "</ows:Exception></ows:ExceptionReport>",
      QStringLiteral("Catchments"), message);

  EXPECT_EQ(refused, nullptr);
  EXPECT_TRUE(message.contains(QStringLiteral("bag:nonesuch")))
    << "the service named the problem and it was thrown away: "
    << message.toStdString();

  QString emptyMessage;
  EXPECT_EQ(WfsFeatureLayer::fromResponse(QByteArray(), QStringLiteral("X"),
                                          emptyMessage),
            nullptr);
  EXPECT_FALSE(emptyMessage.isEmpty());

  // A collection that simply holds nothing over that ground is not a
  // failure of this program, and says so in its own terms.
  QString noneMessage;
  EXPECT_EQ(WfsFeatureLayer::fromResponse(
              R"({"type":"FeatureCollection","features":[]})",
              QStringLiteral("X"), noneMessage),
            nullptr);
  EXPECT_TRUE(noneMessage.contains(QStringLiteral("no features")))
    << noneMessage.toStdString();
}

TEST_F(WfsFeatureLayerTest, OneAnswerIsNotDecodedFromTheLastOnesBytes)
{
  // The in-memory file GDAL is handed lives in a process-wide namespace
  // under a fixed name, so a response left behind is the one the next
  // fetch would decode.
  QString firstMessage;
  QString secondMessage;

  const std::unique_ptr<WfsFeatureLayer> first =
    WfsFeatureLayer::fromResponse(geoJson(), QStringLiteral("First"),
                                  firstMessage);

  const std::unique_ptr<WfsFeatureLayer> second =
    WfsFeatureLayer::fromResponse(
      R"({"type":"FeatureCollection","features":[
          {"type":"Feature","properties":{},
           "geometry":{"type":"Point","coordinates":[1,2]}}]})",
      QStringLiteral("Second"), secondMessage);

  ASSERT_NE(first, nullptr) << firstMessage.toStdString();
  ASSERT_NE(second, nullptr) << secondMessage.toStdString();

  EXPECT_EQ(first->featureCount(), 2);
  EXPECT_EQ(second->featureCount(), 1)
    << "the second answer was decoded from the first one's bytes";

  // And nothing of either is left in memory afterwards. A response that
  // outlives its decode is both a leak and the thing the next one would
  // read.
  EXPECT_TRUE(vsimemIsClean()) << "a response was left behind in /vsimem";

  // Including when the answer could not be read at all.
  QString refusedMessage;
  WfsFeatureLayer::fromResponse("<html>Sign in</html>", QStringLiteral("X"),
                                refusedMessage);

  EXPECT_TRUE(vsimemIsClean())
    << "a response that failed to open was left behind in /vsimem";
}

TEST_F(WfsFeatureLayerTest, AFetchedBoundaryBecomesAMeshDomainUnchanged)
{
  QString message;
  const std::unique_ptr<WfsFeatureLayer> layer = WfsFeatureLayer::fromResponse(
    geoJson(), QStringLiteral("Catchments"), message);

  ASSERT_NE(layer, nullptr) << message.toStdString();

  // The first catchment: an outer ring with one hole in it.
  layer->setSelection({0});

  MeshDomainModel model;
  const DomainImportResult result =
    importSelectionAsDomainPart(*layer, DomainPart::Boundary, model);

  // Nothing about this path knows the features came off a network. A WFS
  // layer is a FeatureLayer with a selection, which is exactly what the
  // importer already took.
  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.boundaries, 1);
  EXPECT_EQ(result.holes, 1)
    << "the ring cut out of the catchment did not come with it";
  EXPECT_FALSE(model.domain().boundary.isEmpty());
  EXPECT_EQ(model.domain().holes.size(), 1);
}
