/*!
 * \file   test_crs.cpp
 * \brief  Phase C5b verification — the CRS catalogue, chooser and assignment.
 *
 * Three claims are under test, and the third is the one that matters:
 *   - the catalogue lists the systems a map can be drawn in, and finds them;
 *   - the chooser returns the system that was picked, not a neighbouring row;
 *   - assigning a CRS changes what a layer's coordinates *mean*, and the map
 *     redraws it accordingly.
 *
 * The last is asserted through where a feature can be picked, in map
 * coordinates with known answers, because that is the observable a user
 * actually has: a layer whose declared system changed but whose cached
 * projection did not goes on drawing in the old place, and every softer check
 * — a signal fired, a flag set — passes right through that.
 */

#include "core/composerapplication.h"
#include "gis/crscatalog.h"
#include "gis/spatialreference.h"
#include "ui/dialogs/crsselectiondialog.h"
#include "vectorprobe.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QLineEdit>
#include <QTreeWidget>

#include <cmath>

using namespace HydroCouple::Composer;
namespace Testing = HydroCouple::Composer::Testing;

namespace
{
  //! Web Mercator's easting/northing for 10°E, 50°N, to within a metre.
  constexpr double kTenEastInMercator = 1113194.9;
  constexpr double kFiftyNorthInMercator = 6446275.8;

  class CrsTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_crs";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      [[nodiscard]] static bool catalogHas(const QString &authCode)
      {
        for (const CrsEntry &entry : crsCatalog())
        {
          if (entry.authCode() == authCode)
          {
            return true;
          }
        }

        return false;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *CrsTest::s_app = nullptr;
}

TEST_F(CrsTest, ListsTheSystemsAMapCanBeDrawnIn)
{
  const QVector<CrsEntry> &all = crsCatalog();

  ASSERT_GT(all.size(), 1000)
    << "the PROJ database was not read; the chooser would be empty";

  EXPECT_TRUE(catalogHas(QStringLiteral("EPSG:4326")));
  EXPECT_TRUE(catalogHas(QStringLiteral("EPSG:3857")));

  // Named absentees, because "every entry has a drawable kind" is satisfied
  // by a filter that merely relabels what it should have dropped. A vertical
  // system and a geocentric one both resolve perfectly well and neither is
  // something a map is drawn in.
  EXPECT_FALSE(catalogHas(QStringLiteral("EPSG:5703")))
    << "NAVD88 height — a vertical datum — was offered as a map system";
  EXPECT_FALSE(catalogHas(QStringLiteral("EPSG:4978")))
    << "WGS 84 geocentric — X/Y/Z about the earth's centre — was offered";

  for (const CrsEntry &entry : all)
  {
    // Vertical, compound and engineering systems resolve but cannot be drawn
    // in, and a deprecated one hands over a superseded definition without
    // saying so. Neither belongs in a picker.
    EXPECT_NE(entry.kind, CrsKind::Any) << entry.authCode().toStdString();
    EXPECT_FALSE(entry.deprecated) << entry.authCode().toStdString();
  }
}

TEST_F(CrsTest, FindsASystemByAnyOfItsWords)
{
  const QVector<CrsEntry> zone =
    searchCrsCatalog(QStringLiteral("utm zone 17n"));

  ASSERT_FALSE(zone.isEmpty()) << "a multi-word search found nothing";

  bool foundWgs84Utm17N = false;

  for (const CrsEntry &entry : zone)
  {
    EXPECT_TRUE(entry.name.contains(QStringLiteral("UTM"), Qt::CaseInsensitive)
                || entry.areaName.contains(QStringLiteral("UTM"),
                                           Qt::CaseInsensitive));

    if (entry.authCode() == QStringLiteral("EPSG:32617"))
    {
      foundWgs84Utm17N = true;
    }
  }

  EXPECT_TRUE(foundWgs84Utm17N)
    << "WGS 84 / UTM zone 17N was not found by name";

  // Word order is not significant — the words are matched, not the phrase.
  EXPECT_EQ(searchCrsCatalog(QStringLiteral("zone utm 17n")).size(),
            zone.size());

  // A code is searched as well as a name, so typing the number finds it.
  const QVector<CrsEntry> byCode = searchCrsCatalog(QStringLiteral("3857"));
  ASSERT_FALSE(byCode.isEmpty());
  EXPECT_EQ(byCode.first().authCode(), QStringLiteral("EPSG:3857"));

  EXPECT_TRUE(searchCrsCatalog(QStringLiteral("zzzz no such system")).isEmpty());
}

TEST_F(CrsTest, TheKindFilterExcludesTheOtherKind)
{
  const QVector<CrsEntry> projected =
    searchCrsCatalog(QStringLiteral("3857"), CrsKind::Projected);
  const QVector<CrsEntry> geographic =
    searchCrsCatalog(QStringLiteral("3857"), CrsKind::Geographic);

  ASSERT_EQ(projected.size(), 1);
  EXPECT_EQ(projected.first().authCode(), QStringLiteral("EPSG:3857"));

  // Pseudo-Mercator is projected, so asking for geographic must not return it
  // — a filter that let everything through would pass the check above.
  EXPECT_TRUE(geographic.isEmpty());

  const QVector<CrsEntry> wgs84 =
    searchCrsCatalog(QStringLiteral("4326"), CrsKind::Geographic);
  ASSERT_EQ(wgs84.size(), 1);
  EXPECT_EQ(wgs84.first().authCode(), QStringLiteral("EPSG:4326"));
}

TEST_F(CrsTest, TheChooserOpensOnTheSystemAlreadyInUse)
{
  CrsSelectionDialog chooser;

  const std::unique_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();
  ASSERT_NE(mercator, nullptr);

  chooser.setCurrentCrs(mercator.get());

  EXPECT_EQ(chooser.selectedAuthCode(), QStringLiteral("EPSG:3857"));

  QString message;
  const std::shared_ptr<SpatialReference> chosen = chooser.selectedCrs(message);

  ASSERT_NE(chosen, nullptr) << message.toStdString();
  EXPECT_TRUE(chosen->isSameAs(*mercator))
    << "the chooser returned a different system than the one selected";
}

TEST_F(CrsTest, NarrowingTheSearchKeepsTheChosenSystem)
{
  CrsSelectionDialog chooser;

  const std::unique_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();
  chooser.setCurrentCrs(mercator.get());
  ASSERT_EQ(chooser.selectedAuthCode(), QStringLiteral("EPSG:3857"));

  auto *search = chooser.findChild<QLineEdit *>(QStringLiteral("crsSearchEdit"));
  ASSERT_NE(search, nullptr);

  // A search the chosen system still matches must keep it selected. Emptying
  // the list moves the current item to nothing on the way, so this is the
  // gate on the choice surviving that.
  search->setText(QStringLiteral("pseudo-mercator"));
  EXPECT_EQ(chooser.selectedAuthCode(), QStringLiteral("EPSG:3857"));

  auto *list = chooser.findChild<QTreeWidget *>(QStringLiteral("crsList"));
  ASSERT_NE(list, nullptr);
  EXPECT_GT(list->topLevelItemCount(), 0);

  // Filtered away entirely, it is deselected rather than left as an invisible
  // choice the OK button would still act on.
  search->setText(QStringLiteral("zzzz no such system"));
  EXPECT_EQ(list->topLevelItemCount(), 0);
  EXPECT_TRUE(chooser.selectedAuthCode().isEmpty());
}

TEST_F(CrsTest, AssigningASystemMovesWhereTheLayerIsDrawn)
{
  Testing::VectorProbe layer(QStringLiteral("probe"));
  layer.addPoint(QPointF(10.0, 50.0), QStringLiteral("mark"));

  std::shared_ptr<SpatialReference> mercator = SpatialReference::webMercator();
  ASSERT_NE(mercator, nullptr);

  layer.setMapCrs(mercator);
  layer.setCrs(SpatialReference::wgs84());

  // Degrees on a Web Mercator map: the point is drawn a million metres east
  // and six million north, and can be picked there.
  EXPECT_GE(layer.pickAt(QPointF(kTenEastInMercator, kFiftyNorthInMercator),
                         1.0),
            0)
    << "the layer was not reprojected from degrees onto the map";

  EXPECT_LT(layer.pickAt(QPointF(10.0, 50.0), 1.0), 0);

  // Now declare that those same numbers were metres all along. Nothing moves
  // in the file; what changes is what the numbers mean, so the point lands at
  // ten metres east of the origin.
  layer.setCrs(mercator);

  EXPECT_GE(layer.pickAt(QPointF(10.0, 50.0), 1.0), 0)
    << "assigning a CRS left the layer projected as it was before — the "
       "cached projection was never invalidated";

  EXPECT_LT(layer.pickAt(QPointF(kTenEastInMercator, kFiftyNorthInMercator),
                         1.0),
            0)
    << "the layer is still pickable where its old declaration put it";
}
