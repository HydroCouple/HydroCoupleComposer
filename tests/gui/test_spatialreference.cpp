/*!
 * \file   test_spatialreference.cpp
 * \brief  Phase C1a verification — coordinate reference systems.
 *
 * The reprojection tests use known answers rather than round-trips alone: a
 * transform that did nothing at all would round-trip perfectly, so identity
 * has to be excluded by checking against values computed independently of this
 * code.
 */

#include "core/composerapplication.h"
#include "gis/spatialreference.h"

#include <gtest/gtest.h>

#include <QVector>

#include <cmath>

using namespace HydroCouple::Composer;

namespace
{
  class CrsTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_spatialreference";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *CrsTest::s_app = nullptr;
}

TEST_F(CrsTest, BuildsWellKnownSystems)
{
  const std::unique_ptr<SpatialReference> wgs84 = SpatialReference::wgs84();
  ASSERT_NE(wgs84, nullptr) << "GDAL could not build EPSG:4326";

  EXPECT_EQ(wgs84->authName(), std::string("EPSG"));
  EXPECT_EQ(wgs84->authSRID(), 4326);
  EXPECT_TRUE(wgs84->isGeographic());
  EXPECT_FALSE(wgs84->srText().empty());
  EXPECT_FALSE(wgs84->description().isEmpty());

  const std::unique_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();
  ASSERT_NE(mercator, nullptr);

  EXPECT_EQ(mercator->authSRID(), 3857);
  EXPECT_FALSE(mercator->isGeographic());
  EXPECT_FALSE(wgs84->isSameAs(*mercator));
}

// The interface's own distance enumeration must be answered honestly.
TEST_F(CrsTest, ReportsDistanceUnits)
{
  EXPECT_EQ(SpatialReference::wgs84()->distanceUnits(),
            HydroCouple::IUnit::DistanceUnits::Degrees);
  EXPECT_EQ(SpatialReference::webMercator()->distanceUnits(),
            HydroCouple::IUnit::DistanceUnits::Meters);

  // A CRS in US survey feet must not be reported as metres.
  QString message;
  const std::unique_ptr<SpatialReference> stateplane =
    SpatialReference::fromAuthority(QStringLiteral("EPSG"), 2246, message);

  if (stateplane)
  {
    EXPECT_EQ(stateplane->distanceUnits(),
              HydroCouple::IUnit::DistanceUnits::Feet);
  }
}

TEST_F(CrsTest, RejectsNonsenseDefinitions)
{
  QString message;

  EXPECT_EQ(SpatialReference::fromDefinition(QStringLiteral(""), message),
            nullptr);
  EXPECT_FALSE(message.isEmpty());

  EXPECT_EQ(SpatialReference::fromDefinition(QStringLiteral("EPSG:not-a-code"),
                                             message),
            nullptr);
  EXPECT_FALSE(message.isEmpty());
}

// A known answer: the Greenwich prime meridian at the equator is the origin of
// Web Mercator, and 1° of longitude is 111319.49 m there.
TEST_F(CrsTest, ReprojectsWithKnownAnswers)
{
  const std::unique_ptr<SpatialReference> wgs84 = SpatialReference::wgs84();
  const std::unique_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();
  ASSERT_NE(wgs84, nullptr);
  ASSERT_NE(mercator, nullptr);

  QString message;
  const std::unique_ptr<CoordinateTransform> transform =
    CoordinateTransform::between(*wgs84, *mercator, message);

  ASSERT_NE(transform, nullptr) << message.toStdString();
  EXPECT_FALSE(transform->isIdentity());

  bool ok = false;
  const QPointF origin = transform->transform(QPointF(0.0, 0.0), &ok);
  EXPECT_TRUE(ok);
  EXPECT_NEAR(origin.x(), 0.0, 1e-6);
  EXPECT_NEAR(origin.y(), 0.0, 1e-6);

  const QPointF oneDegree = transform->transform(QPointF(1.0, 0.0), &ok);
  EXPECT_TRUE(ok);
  EXPECT_NEAR(oneDegree.x(), 111319.4908, 1e-3);

  // Coordinates are (x, y) = (longitude, latitude). Without the traditional
  // axis-order mapping PROJ would hand back (latitude, longitude) and silently
  // transpose every map, so this asserts the ordering explicitly.
  const QPointF london = transform->transform(QPointF(-0.1276, 51.5072), &ok);
  ASSERT_TRUE(ok);
  EXPECT_LT(london.x(), 0.0) << "longitude and latitude are transposed";
  EXPECT_GT(london.y(), 6.0e6) << "longitude and latitude are transposed";
}

TEST_F(CrsTest, RoundTripsThroughProjectedSpace)
{
  const std::unique_ptr<SpatialReference> wgs84 = SpatialReference::wgs84();
  const std::unique_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();

  QString message;
  const std::unique_ptr<CoordinateTransform> forward =
    CoordinateTransform::between(*wgs84, *mercator, message);
  const std::unique_ptr<CoordinateTransform> backward =
    CoordinateTransform::between(*mercator, *wgs84, message);

  ASSERT_NE(forward, nullptr);
  ASSERT_NE(backward, nullptr);

  const QPointF start(-111.8910, 40.7608); // Salt Lake City
  bool ok = false;

  const QPointF projected = forward->transform(start, &ok);
  ASSERT_TRUE(ok);

  const QPointF returned = backward->transform(projected, &ok);
  ASSERT_TRUE(ok);

  EXPECT_NEAR(returned.x(), start.x(), 1e-9);
  EXPECT_NEAR(returned.y(), start.y(), 1e-9);
}

TEST_F(CrsTest, IdentityTransformIsFreeAndExact)
{
  const std::unique_ptr<SpatialReference> a = SpatialReference::wgs84();
  const std::unique_ptr<SpatialReference> b = SpatialReference::wgs84();

  QString message;
  const std::unique_ptr<CoordinateTransform> transform =
    CoordinateTransform::between(*a, *b, message);

  ASSERT_NE(transform, nullptr) << message.toStdString();
  EXPECT_TRUE(transform->isIdentity());

  bool ok = false;
  const QPointF point(12.5, -7.25);
  EXPECT_EQ(transform->transform(point, &ok), point);
  EXPECT_TRUE(ok);
}

// Bulk transformation is what pan and zoom actually use.
TEST_F(CrsTest, TransformsManyPointsInOneCall)
{
  const std::unique_ptr<SpatialReference> wgs84 = SpatialReference::wgs84();
  const std::unique_ptr<SpatialReference> mercator =
    SpatialReference::webMercator();

  QString message;
  const std::unique_ptr<CoordinateTransform> transform =
    CoordinateTransform::between(*wgs84, *mercator, message);
  ASSERT_NE(transform, nullptr);

  QVector<QPointF> points;
  for (int degree = -170; degree <= 170; degree += 10)
  {
    points.append(QPointF(degree, 0.0));
  }

  const QVector<QPointF> before = points;
  const int failures = transform->transformInPlace(points);

  EXPECT_EQ(failures, 0);
  ASSERT_EQ(points.size(), before.size());

  for (int index = 0; index < points.size(); ++index)
  {
    // Longitude 0 is the Web Mercator origin, so that one point legitimately
    // does not move; every other must, and x must rise with longitude.
    if (!qFuzzyIsNull(before[index].x()))
    {
      EXPECT_NE(points[index], before[index])
        << "longitude " << before[index].x() << " was not transformed";
    }

    if (index > 0)
    {
      EXPECT_GT(points[index].x(), points[index - 1].x());
    }
  }

  // ...and the scale is right, not merely monotonic.
  const int zeroIndex = static_cast<int>(before.indexOf(QPointF(0.0, 0.0)));
  ASSERT_GE(zeroIndex, 0);
  ASSERT_LT(zeroIndex + 1, points.size());
  EXPECT_NEAR(points[zeroIndex + 1].x(), 10.0 * 111319.4908, 1.0);
}
