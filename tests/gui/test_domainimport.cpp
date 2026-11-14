/*!
 * \file   test_domainimport.cpp
 * \brief  E1c verification — features already on the map become a domain.
 *
 * Two kinds of gate, because there are two ways this fails. What a ring
 * *means* is decided by geometry — which ring is the outer one, which is a
 * hole, which encloses no ground at all — and those are checked against
 * layers built by hand, where every coordinate is known. That the geometry
 * arrives at all, through the reader the map uses and in the CRS the map is
 * drawn in, is checked against real GeoJSON files, because the reader
 * flattens some geometry in ways only a real file shows.
 *
 * The failures worth catching here are all quiet: a ring imported with its
 * repeated closing point looks right and hands the mesher a zero-length
 * edge; a MultiPolygon read as one polygon with holes looks right and cuts
 * away the ground it should have meshed; and geometry taken unprojected
 * lands a boundary a hundred kilometres from the map it was drawn on.
 */

#include "core/composerapplication.h"
#include "gis/spatialreference.h"
#include "layers/featurelayer.h"
#include "layers/gdalvectorlayer.h"
#include "map/mapcanvas.h"
#include "map/layerstackmodel.h"
#include "mesh/domainimport.h"
#include "mesh/meshdomainmodel.h"

#include <gtest/gtest.h>

#include <QSignalSpy>

#include <memory>

using namespace HydroCouple::Composer;

namespace
{
  //! A layer whose features are placed by hand, coordinate for coordinate.
  class BuiltLayer : public FeatureLayer
  {
    public:
      explicit BuiltLayer(const QString &name) : FeatureLayer(name) {}

      //! Adds one feature of \a kind made of \a parts.
      void add(GeometryKind kind, QVector<QPolygonF> parts)
      {
        VectorFeature feature;
        feature.kind = kind;
        feature.parts = std::move(parts);

        addFeature(std::move(feature));
      }
  };

  //! A closed ring, as OGR hands one over: the first point repeated.
  QPolygonF closedRing(double left, double bottom, double right, double top)
  {
    return QPolygonF({QPointF(left, bottom), QPointF(right, bottom),
                      QPointF(right, top), QPointF(left, top),
                      QPointF(left, bottom)});
  }

  class DomainImportTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_domainimport";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      void SetUp() override { m_model = std::make_unique<MeshDomainModel>(); }

      void TearDown() override { m_model.reset(); }

      //! \returns The path of fixture \a name.
      static QString fixture(const QString &name)
      {
        return QStringLiteral(COMPOSER_MESH_FIXTURE_DIR "/") + name;
      }

      //! Selects every feature of \a layer.
      static void selectAll(FeatureLayer &layer)
      {
        QSet<int> all;

        for (int index = 0; index < layer.features().size(); ++index)
        {
          all.insert(index);
        }

        layer.setSelection(all);
      }

      std::unique_ptr<MeshDomainModel> m_model;

      static ComposerApplication *s_app;
  };

  ComposerApplication *DomainImportTest::s_app = nullptr;
}

// ── what a ring means ───────────────────────────────────────────────────────

TEST_F(DomainImportTest, AnImportedRingLosesTheClosingPointOgrRepeats)
{
  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 10.0, 10.0)});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();

  const QPolygonF &ring = m_model->domain().boundary;
  ASSERT_EQ(ring.size(), 4)
    << "the repeated closing point was kept, which is a corner sitting on "
       "another and a zero-length edge for the mesher";
  EXPECT_NE(ring.first(), ring.last());
  EXPECT_EQ(ring.first(), QPointF(0.0, 0.0));
}

TEST_F(DomainImportTest, APolygonsHolesBecomeHolesOfTheDomain)
{
  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 10.0, 10.0),
                                    closedRing(2.0, 2.0, 4.0, 4.0)});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.boundaries, 1);
  ASSERT_EQ(result.holes, 1);
  EXPECT_EQ(m_model->domain().boundary.first(), QPointF(0.0, 0.0));
  EXPECT_EQ(m_model->domain().holes.first().first(), QPointF(2.0, 2.0))
    << "the hole and the ring it was cut from were swapped";
}

TEST_F(DomainImportTest, TheSeparatePolygonsOfAMultiPolygonAreNotEachOthersHoles)
{
  // The reader flattens a MultiPolygon's rings into one list, so a rule that
  // read "the first ring is the outer one and the rest are its holes" would
  // cut the second square out of the first. Neither ring is inside the
  // other, and that is what says so.
  // The larger square is the feature's SECOND ring, deliberately: with the
  // small one first, "ring 0 is the outer one and the rest are its holes"
  // and "neither ring is inside the other" give the same answer, and the
  // gate would pass on the rule it exists to rule out.
  BuiltLayer layer(QStringLiteral("pair"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 4.0, 4.0),
                                    closedRing(20.0, 0.0, 30.0, 10.0)});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.boundaries, 1);
  EXPECT_EQ(result.holes, 1);
  EXPECT_EQ(result.islands, 0);

  // The larger of the two is the ground; the smaller is a hole in it, not a
  // hole cut in the shape of the larger.
  EXPECT_EQ(m_model->domain().boundary.first(), QPointF(20.0, 0.0))
    << "the second polygon of the pair was read as a hole in the first";
  EXPECT_EQ(m_model->domain().holes.first().first(), QPointF(0.0, 0.0));
}

TEST_F(DomainImportTest, TheLargestOuterRingIsTheBoundaryAndTheRestAreHoles)
{
  // The winner is wound clockwise, which is how a shapefile winds an outer
  // ring: compared by signed area rather than by size it is the *smallest*
  // of the three, and the wrong ring becomes the ground.
  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon, {closedRing(1.0, 1.0, 3.0, 3.0)});
  layer.add(GeometryKind::Polygon,
            {QPolygonF({QPointF(0.0, 0.0), QPointF(0.0, 10.0),
                        QPointF(10.0, 10.0), QPointF(10.0, 0.0),
                        QPointF(0.0, 0.0)})});
  layer.add(GeometryKind::Polygon, {closedRing(5.0, 5.0, 6.0, 6.0)});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();

  // The winner is the middle feature, so neither "the first" nor "the last"
  // passes by accident.
  EXPECT_EQ(m_model->domain().boundary.first(), QPointF(0.0, 0.0));
  ASSERT_EQ(m_model->domain().holes.size(), 2);
  EXPECT_EQ(m_model->domain().holes.at(0).first(), QPointF(1.0, 1.0))
    << "the holes did not arrive in the order their features are in";
  EXPECT_EQ(m_model->domain().holes.at(1).first(), QPointF(5.0, 5.0));
}

TEST_F(DomainImportTest, ARingThatEnclosesNoGroundIsLeftOutAndSaidSo)
{
  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 10.0, 10.0)});

  // Three collinear points on a diagonal: ten by ten of bounding box, three
  // vertices, and no ground. Laid along an axis instead, its box would be
  // empty too and "no area" and "no box" could not be told apart.
  layer.add(GeometryKind::Polygon,
            {QPolygonF({QPointF(0.0, 0.0), QPointF(5.0, 5.0),
                        QPointF(10.0, 10.0), QPointF(0.0, 0.0)})});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.skipped, 1);
  EXPECT_EQ(result.holes, 0) << "a ring with no ground in it became a hole";

  // Said out loud: geometry that goes missing quietly is indistinguishable
  // from a file that never held it.
  EXPECT_TRUE(result.message.contains(QStringLiteral("no ground")))
    << result.message.toStdString();
}

TEST_F(DomainImportTest, GroundStandingInsideAHoleCannotBeImported)
{
  // Listed innermost first, so the order the rings arrive in is not their
  // nesting: a rule that counted pieces would call the middle ring the
  // ground and the outermost one an island.
  BuiltLayer layer(QStringLiteral("nested"));
  layer.add(GeometryKind::Polygon, {closedRing(4.0, 4.0, 6.0, 6.0),
                                    closedRing(2.0, 2.0, 8.0, 8.0),
                                    closedRing(0.0, 0.0, 10.0, 10.0)});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.holes, 1);
  EXPECT_EQ(m_model->domain().boundary.first(), QPointF(0.0, 0.0))
    << "the outermost ring is the ground, whichever order it arrived in";
  EXPECT_EQ(m_model->domain().holes.first().first(), QPointF(2.0, 2.0));
  EXPECT_EQ(result.islands, 1)
    << "the island inside the hole was cut out as another hole, taking away "
       "the ground it stands on";
  EXPECT_EQ(m_model->domain().holes.size(), 1);
}

// ── which part, and what it does to what is already there ───────────────────

TEST_F(DomainImportTest, ImportingABoundaryReplacesTheOneThatWasThere)
{
  m_model->setBoundary(QPolygonF({QPointF(0.0, 0.0), QPointF(1.0, 0.0),
                                  QPointF(1.0, 1.0)}));

  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 10.0, 10.0)});
  selectAll(layer);

  ASSERT_TRUE(
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model).ok);

  // There is exactly one boundary, so importing one is how you correct it.
  EXPECT_EQ(m_model->domain().boundary.size(), 4);
  EXPECT_EQ(m_model->domain().boundary.at(2), QPointF(10.0, 10.0));
}

TEST_F(DomainImportTest, ImportingHolesAddsToWhatIsAlreadyThere)
{
  // Open, because that is how the model holds a ring — closedRing() is the
  // shape a *file* hands over, not the shape a domain stores.
  m_model->setBoundary(QPolygonF({QPointF(0.0, 0.0), QPointF(100.0, 0.0),
                                  QPointF(100.0, 100.0),
                                  QPointF(0.0, 100.0)}));
  m_model->addHole(QPolygonF({QPointF(50.0, 50.0), QPointF(60.0, 50.0),
                              QPointF(60.0, 60.0)}));

  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon, {closedRing(1.0, 1.0, 3.0, 3.0)});
  layer.add(GeometryKind::Polygon, {closedRing(5.0, 5.0, 7.0, 7.0)});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Holes, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.holes, 2);
  EXPECT_EQ(result.boundaries, 0);

  // Three: the one that was drawn and the two that arrived.
  ASSERT_EQ(m_model->domain().holes.size(), 3);
  EXPECT_EQ(m_model->domain().holes.first().first(), QPointF(50.0, 50.0))
    << "the hole that was already there was replaced rather than kept";

  // And the boundary is untouched, because holes are not boundaries.
  EXPECT_EQ(m_model->domain().boundary.size(), 4);
}

TEST_F(DomainImportTest, AHolesOwnHoleIsReportedRatherThanCutOutTwice)
{
  BuiltLayer layer(QStringLiteral("nested"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 10.0, 10.0),
                                    closedRing(2.0, 2.0, 4.0, 4.0)});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Holes, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.holes, 1);
  EXPECT_EQ(result.islands, 1);
  EXPECT_TRUE(result.message.contains(QStringLiteral("island")))
    << result.message.toStdString();
}

TEST_F(DomainImportTest, BreaklinesArriveAsPolylinesAndAreNotOpened)
{
  BuiltLayer layer(QStringLiteral("lines"));

  // A line that closes on itself is a legitimate constraint, and opening it
  // the way a ring is opened would take its last vertex off.
  layer.add(GeometryKind::Line,
            {QPolygonF({QPointF(0.0, 0.0), QPointF(5.0, 5.0),
                        QPointF(10.0, 0.0), QPointF(0.0, 0.0)})});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Breaklines, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  ASSERT_EQ(m_model->domain().constraintLines.size(), 1);
  EXPECT_EQ(m_model->domain().constraintLines.first().size(), 4);
}

TEST_F(DomainImportTest, ALineOfOnePointDescribesNothing)
{
  BuiltLayer layer(QStringLiteral("lines"));
  layer.add(GeometryKind::Line, {QPolygonF({QPointF(1.0, 1.0)})});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Breaklines, *m_model);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.skipped, 1);
  EXPECT_TRUE(m_model->domain().constraintLines.isEmpty());
}

TEST_F(DomainImportTest, EveryPointOfAMultipointIsForced)
{
  BuiltLayer layer(QStringLiteral("points"));
  layer.add(GeometryKind::Point, {QPolygonF({QPointF(1.0, 1.0)}),
                                  QPolygonF({QPointF(2.0, 2.0)})});
  selectAll(layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::ForcedPoints, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.points, 2) << "only the first point of the feature landed";
  EXPECT_EQ(m_model->domain().points.at(1), QPointF(2.0, 2.0));
}

TEST_F(DomainImportTest, TheWholeImportIsOneChangeToTheModel)
{
  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 10.0, 10.0)});
  layer.add(GeometryKind::Polygon, {closedRing(1.0, 1.0, 2.0, 2.0)});
  layer.add(GeometryKind::Polygon, {closedRing(3.0, 3.0, 4.0, 4.0)});
  selectAll(layer);

  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  ASSERT_TRUE(
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model).ok);

  // One thing the user did, so one repaint — not one per ring, with the map
  // showing two half-imported domains on the way.
  EXPECT_EQ(spy.count(), 1);
}

// ── refusals ────────────────────────────────────────────────────────────────

TEST_F(DomainImportTest, AnEmptySelectionIsRefusedAndNamesTheLayer)
{
  BuiltLayer layer(QStringLiteral("catchments"));
  layer.add(GeometryKind::Polygon, {closedRing(0.0, 0.0, 10.0, 10.0)});

  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(spy.count(), 0);
  EXPECT_TRUE(m_model->domain().isEmpty())
    << "nothing was selected and the whole layer was taken";
  EXPECT_TRUE(result.message.contains(QStringLiteral("catchments")))
    << result.message.toStdString();
}

TEST_F(DomainImportTest, LinesCannotBeABoundaryAndTheRefusalSaysWhy)
{
  BuiltLayer layer(QStringLiteral("streams"));
  layer.add(GeometryKind::Line,
            {QPolygonF({QPointF(0.0, 0.0), QPointF(1.0, 1.0)})});
  selectAll(layer);

  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(spy.count(), 0);

  // Both halves named, because "cannot import" leaves the user guessing
  // which of the two things they chose was the wrong one.
  EXPECT_TRUE(result.message.contains(QStringLiteral("lines")))
    << result.message.toStdString();
  EXPECT_TRUE(result.message.contains(QStringLiteral("polygons")))
    << result.message.toStdString();
}

TEST_F(DomainImportTest, ASelectionWithNothingImportableInItChangesNothing)
{
  BuiltLayer layer(QStringLiteral("rings"));
  layer.add(GeometryKind::Polygon,
            {QPolygonF({QPointF(0.0, 0.0), QPointF(5.0, 5.0),
                        QPointF(10.0, 10.0), QPointF(0.0, 0.0)})});
  selectAll(layer);

  QSignalSpy spy(m_model.get(), &MeshDomainModel::domainChanged);

  const DomainImportResult result =
    importSelectionAsDomainPart(layer, DomainPart::Boundary, *m_model);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(spy.count(), 0);
  EXPECT_TRUE(m_model->domain().isEmpty());
}

// ── through the reader the map uses ─────────────────────────────────────────

TEST_F(DomainImportTest, APolygonFileArrivesThroughTheReaderTheMapUses)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("domain_polygons.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  selectAll(*layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(*layer, DomainPart::Boundary, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.boundaries, 1);

  // "small", the two halves of the MultiPolygon, and "big"'s own hole.
  EXPECT_EQ(result.holes, 4);
  EXPECT_EQ(result.skipped, 1) << "the collinear ring was taken as ground";
  EXPECT_EQ(result.islands, 0);

  // Every hole lies inside the boundary, so what was imported is a domain
  // the mesher would accept — not merely a plausible count of rings.
  QString why;
  EXPECT_TRUE(m_model->domain().isValid(why)) << why.toStdString();
}

TEST_F(DomainImportTest, TheImportedDomainIsInTheMapsCrsNotTheFilesDegrees)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("domain_polygons.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  GdalVectorLayer *raw = layer.get();

  LayerStackModel stack;
  MapCanvas canvas;
  canvas.setModel(&stack);
  canvas.setCrs(SpatialReference::webMercator());
  canvas.resize(400, 400);
  stack.addLayer(layer.release());

  selectAll(*raw);

  ASSERT_TRUE(
    importSelectionAsDomainPart(*raw, DomainPart::Boundary, *m_model).ok);

  // The file says the boundary reaches longitude 1, which in the map's CRS
  // is 111 km from the origin. Taken unprojected it would be one metre out,
  // and a domain drawn a hundred kilometres from the map it belongs to
  // looks empty rather than wrong.
  EXPECT_NEAR(m_model->domain().boundary.boundingRect().right(), 111319.49,
              1.0);
}

TEST_F(DomainImportTest, EveryLineOfAMultiLineFileBecomesItsOwnBreakline)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("domain_lines.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  selectAll(*layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(*layer, DomainPart::Breaklines, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();

  // One line plus the two the second feature carries — a breakline pool
  // that cannot tell one line from two would report a different number.
  EXPECT_EQ(result.breaklines, 3);
  EXPECT_EQ(m_model->domain().constraintLines.first().size(), 3);
}

TEST_F(DomainImportTest, AMultiPointFileForcesEveryPointItHolds)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("domain_points.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  selectAll(*layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(*layer, DomainPart::ForcedPoints, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();
  EXPECT_EQ(result.points, 4) << "the multipoint contributed only one point";
}

TEST_F(DomainImportTest, ImportingTheSameFileAsHolesReportsWhatItCannotTake)
{
  QString message;
  std::unique_ptr<GdalVectorLayer> layer = GdalVectorLayer::open(
    fixture(QStringLiteral("domain_polygons.geojson")), message);
  ASSERT_NE(layer, nullptr) << message.toStdString();

  selectAll(*layer);

  const DomainImportResult result =
    importSelectionAsDomainPart(*layer, DomainPart::Holes, *m_model);

  ASSERT_TRUE(result.ok) << result.message.toStdString();

  // Every outer ring is a hole here — "big" included, since nothing in a
  // hole import is a boundary.
  EXPECT_EQ(result.holes, 4);
  EXPECT_EQ(result.islands, 1) << "big's own hole was cut out of the domain";
  EXPECT_EQ(result.skipped, 1);
  EXPECT_TRUE(m_model->domain().boundary.isEmpty());
}
