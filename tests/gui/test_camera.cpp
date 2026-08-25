/*!
 * \file   test_camera.cpp
 * \brief  C3a — the scene camera's arithmetic.
 *
 * Pure maths, deliberately: pan, zoom and fit are where view bugs hide, and
 * they are far easier to pin down as equations than as rendered pixels. The
 * pixels are checked separately, in test_scene3d.cpp.
 */

#include "scene/camera.h"

#include <gtest/gtest.h>

#include <QtGlobal>

#include <algorithm>
#include <cmath>

using namespace HydroCouple::Composer;

namespace
{
  constexpr double kAspect = 4.0 / 3.0;

  //! A rectangle whose aspect already matches the viewport's, so a fit binds
  //! on both axes at once and the round trip has to be exact.
  QRectF matchingRect()
  {
    return QRectF(100.0, 200.0, 40.0 * kAspect, 40.0);
  }

}

// ── Ground extent and the map correspondence ────────────────────────────────

TEST(CameraTest, TopDownGroundExtentRoundTripsExactly)
{
  for (const CameraProjection projection :
       { CameraProjection::Orthographic, CameraProjection::Perspective })
  {
    Camera camera;
    camera.setProjection(projection, kAspect);
    camera.setElevation(90.0);

    const QRectF requested = matchingRect();
    camera.setGroundExtent(requested, kAspect);

    const QRectF observed = camera.groundExtent(kAspect);

    EXPECT_NEAR(observed.left(), requested.left(), 1.0e-3);
    EXPECT_NEAR(observed.right(), requested.right(), 1.0e-3);
    EXPECT_NEAR(observed.top(), requested.top(), 1.0e-3);
    EXPECT_NEAR(observed.bottom(), requested.bottom(), 1.0e-3);
  }
}

TEST(CameraTest, GroundExtentContainsTheRequestedRectAtAnyTilt)
{
  // The 3D-to-2D-to-3D round trip the phase asks for: whatever tilt the user
  // left the scene at, switching to the map and back must not lose ground.
  for (const double elevation : { 20.0, 45.0, 70.0, 89.0 })
  {
    for (const double azimuth : { 0.0, 37.0, 135.0, 300.0 })
    {
      Camera camera;
      camera.setElevation(elevation);
      camera.setAzimuth(azimuth);

      const QRectF requested = matchingRect();
      camera.setGroundExtent(requested, kAspect);

      const QRectF observed = camera.groundExtent(kAspect);

      // Shrunk by a hair before asking: Qt's vector maths is single
      // precision, so containment is exact only to about a part in 10^5.
      // The failure this guards against — framing by the rectangle's centre,
      // which loses a tilted frustum's near strip — misses by whole world
      // units, three orders of magnitude clear of that.
      const double epsilon =
        1.0e-4 * std::max(requested.width(), requested.height());

      EXPECT_TRUE(observed.contains(
        requested.adjusted(epsilon, epsilon, -epsilon, -epsilon)))
        << "elevation " << elevation << " azimuth " << azimuth
        << " lost ground: " << observed.left() << "," << observed.top() << " "
        << observed.width() << "x" << observed.height();
    }
  }
}

TEST(CameraTest, GroundExtentStaysFiniteLookingAtTheHorizon)
{
  // A shallow camera's upper frustum rays never meet the ground. Reporting
  // that as an infinite extent would make every consumer of it — framing,
  // the map hand-off, culling — produce garbage instead of a wide view.
  Camera camera;
  camera.setElevation(0.5);
  camera.setGroundExtent(matchingRect(), kAspect);

  const QRectF observed = camera.groundExtent(kAspect);

  EXPECT_TRUE(std::isfinite(observed.left()));
  EXPECT_TRUE(std::isfinite(observed.right()));
  EXPECT_TRUE(std::isfinite(observed.top()));
  EXPECT_TRUE(std::isfinite(observed.bottom()));
  EXPECT_GT(observed.height(), 0.0);

  // Finite is not enough. A ray heading away from the ground meets it only
  // *behind* the camera, and taking that intersection puts the extent behind
  // the viewer — a perfectly finite answer describing ground nobody can see.
  // The one point the camera is certainly looking at anchors it.
  EXPECT_TRUE(observed.contains(
    QPointF(double(camera.target().x()), double(camera.target().y()))))
    << "the visible ground did not include the point being looked at";

  // And every corner of it must lie ahead of the eye, not behind it.
  const QPointF eye(double(camera.eye().x()), double(camera.eye().y()));
  const QPointF forward(double(camera.target().x()) - eye.x(),
                        double(camera.target().y()) - eye.y());

  for (const QPointF &corner :
       { observed.topLeft(), observed.topRight(), observed.bottomLeft(),
         observed.bottomRight() })
  {
    const QPointF toCorner = corner - eye;

    EXPECT_GT(toCorner.x() * forward.x() + toCorner.y() * forward.y(), 0.0)
      << "the visible ground reached behind the camera";
  }
}

TEST(CameraTest, PreservesAspectByExceedingOnTheSlackAxis)
{
  Camera camera;
  camera.setElevation(90.0);

  // Half as wide as the viewport wants, so height binds and width must grow.
  const QRectF tall(0.0, 0.0, 10.0, 40.0);
  camera.setGroundExtent(tall, kAspect);

  const QRectF observed = camera.groundExtent(kAspect);

  EXPECT_NEAR(observed.height(), tall.height(), 1.0e-3);
  EXPECT_GT(observed.width(), tall.width());
  EXPECT_NEAR(observed.width() / observed.height(), kAspect, 1.0e-3);
}

TEST(CameraTest, SwitchingProjectionKeepsTheSameGroundVisible)
{
  Camera camera;
  camera.setElevation(60.0);
  camera.setGroundExtent(matchingRect(), kAspect);

  const QRectF before = camera.groundExtent(kAspect);

  camera.setProjection(CameraProjection::Orthographic, kAspect);

  const QRectF after = camera.groundExtent(kAspect);

  // Not identical — the two projections see a different shape of ground —
  // but the view must not jump, so the centres agree and the areas are close.
  EXPECT_NEAR(after.center().x(), before.center().x(), before.width() * 0.05);
  EXPECT_NEAR(after.center().y(), before.center().y(), before.height() * 0.05);
  EXPECT_GT(after.width(), before.width() * 0.5);
  EXPECT_LT(after.width(), before.width() * 2.0);
}

// ── Orientation ─────────────────────────────────────────────────────────────

TEST(CameraTest, AzimuthZeroPutsTheEyeSouthOfTheTarget)
{
  // North-up, so a default 3D view and a north-up map agree on which way is
  // up. An eye placed north instead mirrors every scene left to right.
  Camera camera;
  camera.setTarget(QVector3D(10.0f, 20.0f, 0.0f));
  camera.setAzimuth(0.0);
  camera.setElevation(30.0);
  camera.setDistance(100.0);

  const QVector3D eye = camera.eye();

  EXPECT_NEAR(eye.x(), 10.0, 1.0e-3);
  EXPECT_LT(eye.y(), 20.0);
  EXPECT_GT(eye.z(), 0.0);
}

TEST(CameraTest, ElevationIsHeldShortOfTheZenith)
{
  // Exactly 90 degrees makes the view direction parallel to the up vector and
  // the view matrix degenerate — the classic gimbal collapse, which shows up
  // as a scene that vanishes at the top of a drag.
  Camera camera;
  camera.setElevation(90.0);

  EXPECT_LT(camera.elevation(), 90.0);
  EXPECT_GT(camera.elevation(), 89.9);

  const QMatrix4x4 view = camera.viewMatrix();

  bool finite = true;

  for (int index = 0; index < 16; ++index)
  {
    finite = finite && std::isfinite(view.constData()[index]);
  }

  EXPECT_TRUE(finite) << "the view matrix degenerated at the zenith";
}

TEST(CameraTest, OrbitTurnsAboutTheTargetWithoutMovingIt)
{
  Camera camera;
  camera.setTarget(QVector3D(5.0f, 6.0f, 7.0f));
  camera.setDistance(50.0);

  const QVector3D target = camera.target();

  camera.orbit(90.0, 15.0);

  EXPECT_EQ(camera.target(), target);
  EXPECT_NEAR(camera.distance(), 50.0, 1.0e-9);
  EXPECT_NEAR(double((camera.eye() - target).length()), 50.0, 1.0e-3);
}

TEST(CameraTest, AzimuthWrapsRatherThanGrowing)
{
  Camera camera;
  camera.setAzimuth(-30.0);

  EXPECT_NEAR(camera.azimuth(), 330.0, 1.0e-9);

  camera.setAzimuth(730.0);

  EXPECT_NEAR(camera.azimuth(), 10.0, 1.0e-9);
}

// ── Pan and dolly ───────────────────────────────────────────────────────────

TEST(CameraTest, PanMovesTheTargetByTheDragLength)
{
  // A drag has to move the ground under the cursor by the drag's own length,
  // or dragging feels geared.
  Camera camera;
  camera.setElevation(90.0);
  camera.setGroundExtent(QRectF(0.0, 0.0, 400.0 * kAspect, 400.0), kAspect);

  const double viewportHeight = 200.0;
  const double worldPerPixel = 400.0 / viewportHeight;

  const QVector3D before = camera.target();
  camera.pan(QPointF(10.0, 0.0), viewportHeight, kAspect);

  EXPECT_NEAR(double(camera.target().x() - before.x()),
              -10.0 * worldPerPixel, 1.0e-3);
}

TEST(CameraTest, DollyChangesTheOrthographicViewToo)
{
  // Orthographic projection has no distance term, so a dolly that only moved
  // the eye would appear to do nothing at all in that mode.
  Camera camera;
  camera.setProjection(CameraProjection::Orthographic, kAspect);
  camera.setElevation(90.0);
  camera.setGroundExtent(matchingRect(), kAspect);

  const double before = camera.groundExtent(kAspect).height();

  camera.dolly(0.5);

  EXPECT_NEAR(camera.groundExtent(kAspect).height(), before * 0.5, 1.0e-3);
}

TEST(CameraTest, DollyHoldsTheTarget)
{
  Camera camera;
  camera.setTarget(QVector3D(3.0f, 4.0f, 5.0f));

  const QVector3D target = camera.target();

  camera.dolly(0.25);

  EXPECT_EQ(camera.target(), target);
}

// ── Vertical exaggeration ───────────────────────────────────────────────────

TEST(CameraTest, ExaggerationScalesHeightAloneAndLeavesThePlanUntouched)
{
  Camera camera;
  camera.setVerticalExaggeration(10.0);

  const QMatrix4x4 model = camera.modelMatrix();

  const QVector3D horizontal = model.map(QVector3D(2.0f, 3.0f, 0.0f));
  const QVector3D vertical = model.map(QVector3D(0.0f, 0.0f, 4.0f));

  EXPECT_NEAR(horizontal.x(), 2.0, 1.0e-6);
  EXPECT_NEAR(horizontal.y(), 3.0, 1.0e-6);
  EXPECT_NEAR(vertical.z(), 40.0, 1.0e-6);
}

TEST(CameraTest, FitAdmitsReliefTallerThanItsFootprint)
{
  // A narrow, deep canyon: framing only the footprint would put most of the
  // scene outside the view, and exaggeration makes that routine rather than
  // exotic.
  Bounds3D bounds;
  bounds.expandTo(QVector3D(0.0f, 0.0f, 0.0f));
  bounds.expandTo(QVector3D(4.0f, 4.0f, 500.0f));

  Camera flat;
  flat.setElevation(90.0);
  flat.fitTo(bounds, kAspect);

  Camera exaggerated;
  exaggerated.setElevation(90.0);
  exaggerated.setVerticalExaggeration(5.0);
  exaggerated.fitTo(bounds, kAspect);

  EXPECT_GT(exaggerated.distance(), flat.distance())
    << "exaggerated relief was framed as though it were flat";
}

TEST(CameraTest, FitPutsTheEyeOutsideTheSceneItIsFraming)
{
  // Framing the ground footprint and then lowering the target into the
  // relief — the obvious order — leaves the eye below the rim of anything
  // concave, so a bowl or a channel is fitted from inside itself. Nothing
  // about the resulting extent looks wrong; only the picture does.
  Bounds3D bowl;
  bowl.expandTo(QVector3D(-540.0f, -540.0f, -120.0f));
  bowl.expandTo(QVector3D(540.0f, 540.0f, 0.0f));

  for (const double elevation : { 20.0, 32.0, 60.0 })
  {
    Camera camera;
    camera.setElevation(elevation);
    camera.setVerticalExaggeration(2.5);
    camera.fitTo(bowl, kAspect);

    const QVector3D centre(bowl.center().x(), bowl.center().y(),
                           float(double(bowl.center().z()) * 2.5));
    const QVector3D exaggeratedSpan =
      (bowl.maximum() - bowl.minimum()) * QVector3D(1.0f, 1.0f, 2.5f);
    const double radius = 0.5 * double(exaggeratedSpan.length());

    EXPECT_GT(double((camera.eye() - centre).length()), radius)
      << "at elevation " << elevation
      << " the camera was placed inside the scene it was framing";
  }
}

TEST(CameraTest, FitIgnoresAnEmptyBox)
{
  Camera camera;
  camera.setDistance(42.0);

  camera.fitTo(Bounds3D(), kAspect);

  EXPECT_NEAR(camera.distance(), 42.0, 1.0e-9);
}

// ── Clip range ──────────────────────────────────────────────────────────────

TEST(CameraTest, ClipRangeFollowsTheViewDistance)
{
  // A fixed near plane either clips the scene when the camera is close or
  // destroys depth precision when it is far, and both read as renderer bugs.
  Camera near;
  near.setDistance(1.0);

  Camera far;
  far.setDistance(1.0e6);

  EXPECT_LT(near.nearPlane(), far.nearPlane());
  EXPECT_LT(near.farPlane(), far.farPlane());
  EXPECT_GT(near.farPlane(), near.distance());
  EXPECT_GT(far.farPlane(), far.distance());
}

// ── Bounds3D ────────────────────────────────────────────────────────────────

TEST(Bounds3DTest, DistinguishesEmptyFromAroundTheOrigin)
{
  // The trap QRectF::united() falls into: a box that has never been given a
  // point must not read as a box at the origin, or the first point added is
  // silently paired with a phantom one.
  Bounds3D empty;

  EXPECT_FALSE(empty.isValid());

  Bounds3D atOrigin;
  atOrigin.expandTo(QVector3D(0.0f, 0.0f, 0.0f));

  EXPECT_TRUE(atOrigin.isValid());
  EXPECT_EQ(atOrigin.center(), QVector3D(0.0f, 0.0f, 0.0f));
  EXPECT_NEAR(atOrigin.diagonal(), 0.0, 1.0e-9);
}

TEST(Bounds3DTest, KeepsZeroThicknessBoxes)
{
  // A perfectly flat terrain has zero Z extent, and a footprint that dropped
  // it would frame nothing at all.
  Bounds3D bounds;
  bounds.expandTo(QVector3D(1.0f, 2.0f, 0.0f));
  bounds.expandTo(QVector3D(3.0f, 5.0f, 0.0f));

  EXPECT_TRUE(bounds.isValid());
  EXPECT_EQ(bounds.footprint(), QRectF(1.0, 2.0, 2.0, 3.0));
  EXPECT_NEAR(bounds.minimum().z(), 0.0, 1.0e-9);
  EXPECT_NEAR(bounds.maximum().z(), 0.0, 1.0e-9);
}

TEST(Bounds3DTest, AbsorbingAnEmptyBoxChangesNothing)
{
  Bounds3D bounds;
  bounds.expandTo(QVector3D(1.0f, 1.0f, 1.0f));

  const QVector3D before = bounds.maximum();
  bounds.expandTo(Bounds3D());

  EXPECT_EQ(bounds.maximum(), before);
  EXPECT_EQ(bounds.minimum(), before);
}
