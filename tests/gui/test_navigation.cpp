/*!
 * \file   test_navigation.cpp
 * \brief  U5 — what a drag, a wheel notch and a named view do.
 *
 * All arithmetic, which is the point: every one of these decisions used
 * to live inside a QRhiWidget event handler, where no gate running under
 * the offscreen platform could reach it (D31). A sign, an inversion read
 * twice, a modifier tested backwards and a "reset" that resets to the
 * wrong tilt are all bugs a user would feel immediately and no existing
 * test could see.
 */

#include "scene/axisgizmo.h"
#include "scene/camera.h"
#include "scene/navigation.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace HydroCouple::Composer;

TEST(NavigationTest, TheStoredNamesSurviveARoundTripAndNonsenseIsTheOldBehaviour)
{
  for (ViewLink link : { ViewLink::OnTabSwitch, ViewLink::Never })
  {
    EXPECT_EQ(viewLinkFromName(viewLinkName(link)), link);
  }

  for (PanModifier pan : { PanModifier::MiddleDrag, PanModifier::ShiftDrag })
  {
    EXPECT_EQ(panModifierFromName(panModifierName(pan)), pan);
  }

  // An unreadable setting falls back to what the application did before
  // the setting existed, which is the only fallback that cannot surprise
  // someone who never opened the preferences at all.
  EXPECT_EQ(viewLinkFromName(QStringLiteral("sometimes")),
            ViewLink::OnTabSwitch);
  EXPECT_EQ(viewLinkFromName(QString()), ViewLink::OnTabSwitch);
  EXPECT_EQ(panModifierFromName(QStringLiteral("wiggle")),
            PanModifier::MiddleDrag);
}

TEST(NavigationTest, DraggingRightTurnsTheSceneRight)
{
  // The sign that makes an orbit feel like turning an object rather than
  // steering past one. Getting it backwards is the single most noticeable
  // thing navigation can get wrong.
  const OrbitStep right = orbitStep(QPoint(10, 0), 0.4);
  const OrbitStep down = orbitStep(QPoint(0, 10), 0.4);

  EXPECT_LT(right.azimuth, 0.0) << "dragging right did not turn the scene right";
  EXPECT_DOUBLE_EQ(right.elevation, 0.0);

  EXPECT_GT(down.elevation, 0.0);
  EXPECT_DOUBLE_EQ(down.azimuth, 0.0);

  // And it is proportional, so a slow drag and a fast one covering the
  // same distance arrive at the same place.
  const OrbitStep once = orbitStep(QPoint(20, 14), 0.4);
  const OrbitStep twice = orbitStep(QPoint(10, 7), 0.4);

  EXPECT_NEAR(once.azimuth, 2.0 * twice.azimuth, 1e-12);
  EXPECT_NEAR(once.elevation, 2.0 * twice.elevation, 1e-12);
}

TEST(NavigationTest, ASensitivityOfZeroIsNotAViewThatCannotTurn)
{
  // The preference is a number in a spin box, and a spin box can be typed
  // into. Zero is a camera that does not respond, which reads as a broken
  // mouse rather than as a setting; negative silently inverts both axes
  // when the user meant to slow the camera down.
  const OrbitStep none = orbitStep(QPoint(10, 0), 0.0);
  const OrbitStep negative = orbitStep(QPoint(10, 0), -0.4);
  const OrbitStep wild = orbitStep(QPoint(10, 0), 1000.0);

  EXPECT_LT(none.azimuth, 0.0) << "a sensitivity of zero froze the camera";
  EXPECT_LT(negative.azimuth, 0.0) << "a negative sensitivity inverted the drag";

  // Still turning the right way, and not by a whole revolution per pixel.
  EXPECT_LT(wild.azimuth, 0.0);
  EXPECT_LT(std::abs(wild.azimuth), 360.0);
}

TEST(NavigationTest, TheWheelZoomsInForwardsAndTheSettingReversesIt)
{
  // Forward is in: the camera comes closer, so it dollies by less than
  // one. This is the convention every map and every 3D view shares.
  EXPECT_LT(wheelDollyFactor(120, 1.15, false), 1.0);
  EXPECT_GT(wheelDollyFactor(-120, 1.15, false), 1.0);

  // Inverted is exactly the other way, and exactly reciprocal — not
  // merely "the other direction", which is how a wheel ends up moving by
  // different amounts depending on which way it is turned.
  EXPECT_NEAR(wheelDollyFactor(120, 1.15, true),
              1.0 / wheelDollyFactor(120, 1.15, false), 1e-12);
  EXPECT_NEAR(wheelDollyFactor(120, 1.15, true),
              wheelDollyFactor(-120, 1.15, false), 1e-12);

  // One notch moves by exactly the notch size, which is what makes the
  // preference mean what it says. Qt reports eighths of a degree and a
  // notch is fifteen of them, so the 120 is load-bearing: dividing by 8
  // instead would leave every property above intact and make the wheel
  // fifteen times too sensitive.
  EXPECT_NEAR(wheelDollyFactor(120, 1.15, false), 1.0 / 1.15, 1e-12);
  EXPECT_NEAR(wheelDollyFactor(-120, 1.15, false), 1.15, 1e-12);
  EXPECT_NEAR(wheelDollyFactor(120, 2.0, false), 0.5, 1e-12);
}

TEST(NavigationTest, NoNotchIsExactlyNoZoom)
{
  // A trackpad delivers a stream of zero deltas between real scrolls, and
  // the caller has to be able to tell "nothing happened" from "something
  // very small happened" — a factor of 0.9999 applied on every stray
  // event walks the camera away over a few seconds of resting a finger.
  // Exactly, not approximately. There is no special case in the code for
  // this — pow(x, 0) is exactly 1.0 — and the promise is what is gated,
  // so the branch could be added or removed without this changing.
  EXPECT_DOUBLE_EQ(wheelDollyFactor(0, 1.15, false), 1.0);
  EXPECT_DOUBLE_EQ(wheelDollyFactor(0, 1.15, true), 1.0);
  EXPECT_DOUBLE_EQ(wheelDollyFactor(0, 4.0, false), 1.0);

  // Two notches move twice as far as one, in the multiplicative sense.
  const double one = wheelDollyFactor(120, 1.15, false);
  const double two = wheelDollyFactor(240, 1.15, false);

  EXPECT_NEAR(two, one * one, 1e-12);
}

TEST(NavigationTest, AZoomPerNotchOfOneWouldBeAWheelThatDoesNothing)
{
  // Another spin box, another value nobody would choose on purpose: at
  // exactly 1.0 the wheel is dead, and below it the wheel is inverted by
  // a second route — so the invert preference would cancel it and a user
  // who set both would find their wheel working normally again.
  EXPECT_LT(wheelDollyFactor(120, 1.0, false), 1.0);
  EXPECT_LT(wheelDollyFactor(120, 0.5, false), 1.0);
  EXPECT_LT(wheelDollyFactor(120, -2.0, false), 1.0);

  // And still reversible by the setting that is supposed to reverse it.
  EXPECT_GT(wheelDollyFactor(120, 0.5, true), 1.0);
}

TEST(NavigationTest, MiddleAndRightAlwaysPanWhateverTheSettingSays)
{
  for (PanModifier setting : { PanModifier::MiddleDrag,
                               PanModifier::ShiftDrag })
  {
    EXPECT_TRUE(pressShouldPan(Qt::MiddleButton, Qt::NoModifier, setting));
    EXPECT_TRUE(pressShouldPan(Qt::RightButton, Qt::NoModifier, setting));
  }
}

TEST(NavigationTest, ShiftDragPansOnlyWhenAskedFor)
{
  // The reason the setting exists: a trackpad has no middle button, so
  // without this there is no way to pan at all on one.
  EXPECT_TRUE(pressShouldPan(Qt::LeftButton, Qt::ShiftModifier,
                             PanModifier::ShiftDrag));

  EXPECT_FALSE(pressShouldPan(Qt::LeftButton, Qt::ShiftModifier,
                              PanModifier::MiddleDrag))
    << "shift-drag panned under a setting that did not ask for it, which "
       "would take shift-add-to-selection away from the 3D view";

  // A plain left drag is never a pan: that is the orbit, or the band.
  EXPECT_FALSE(pressShouldPan(Qt::LeftButton, Qt::NoModifier,
                              PanModifier::ShiftDrag));
  EXPECT_FALSE(pressShouldPan(Qt::LeftButton, Qt::NoModifier,
                              PanModifier::MiddleDrag));
}

TEST(NavigationTest, ResetIsWhereANewViewStarts)
{
  // "Reset" that resets somewhere other than the starting point is worse
  // than no reset: the user presses it to get their bearings back and
  // gets a third unfamiliar view.
  double azimuth = 123.0;
  double elevation = 7.0;

  namedViewAngles(NamedView::Reset, azimuth, elevation);

  Camera fresh;

  EXPECT_DOUBLE_EQ(azimuth, fresh.azimuth());
  EXPECT_DOUBLE_EQ(elevation, fresh.elevation());
}

TEST(NavigationTest, TopLooksDownWithoutSpinningTheMap)
{
  double azimuth = 123.0;
  double elevation = 7.0;

  namedViewAngles(NamedView::Top, azimuth, elevation);

  EXPECT_DOUBLE_EQ(elevation, 90.0);
  EXPECT_DOUBLE_EQ(azimuth, 123.0)
    << "looking down spun the map, which nobody asked it to do";
}

TEST(NavigationTest, TopViewAndTheGizmosUpArmMeanTheSameThing)
{
  // Two controls that both say "look down" must not disagree, or the
  // ribbon button and the blue arrow would take the user to two
  // different places.
  double byRibbon = 40.0;
  double byRibbonElevation = 25.0;
  namedViewAngles(NamedView::Top, byRibbon, byRibbonElevation);

  double byGizmo = 40.0;
  double byGizmoElevation = 25.0;
  ASSERT_TRUE(axisGizmoView(GizmoAxis::Up, byGizmo, byGizmoElevation));

  EXPECT_DOUBLE_EQ(byRibbon, byGizmo);
  EXPECT_DOUBLE_EQ(byRibbonElevation, byGizmoElevation);
}

TEST(NavigationTest, TheAnglesAreOnesTheCameraWillActuallyAccept)
{
  // namedViewAngles hands back numbers; the camera clamps them. If Top
  // asked for something the camera refuses, the button would appear to
  // work and leave the view somewhere else.
  for (NamedView view : { NamedView::Reset, NamedView::Top })
  {
    double azimuth = 17.0;
    double elevation = 3.0;

    namedViewAngles(view, azimuth, elevation);

    Camera camera;
    camera.setAzimuth(azimuth);
    camera.setElevation(elevation);

    EXPECT_NEAR(camera.azimuth(), azimuth, 1e-9);

    // Elevation is the one that clamps, a thousandth of a degree short of
    // vertical, so Top is allowed to land there rather than exactly 90.
    EXPECT_NEAR(camera.elevation(), elevation, 1.0e-2)
      << "the camera would not go where the view asked";
  }
}
