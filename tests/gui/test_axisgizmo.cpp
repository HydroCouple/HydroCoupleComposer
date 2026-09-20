/*!
 * \file   test_axisgizmo.cpp
 * \brief  The orientation cue: which way it points, and what clicking says.
 *
 * All of it is arithmetic over the camera's angles, which is deliberate:
 * the part of a gizmo that can be wrong about where north is, is the part
 * that can be checked without a graphics device. What is *not* checked
 * here is the renderer drawing it in a corner viewport with depth off —
 * QRhiWidget cannot make a device under the offscreen platform (D31), so
 * that is the hand-off's to confirm by eye.
 */

#include "scene/axisgizmo.h"
#include "scene/camera.h"

#include <gtest/gtest.h>

#include <QVector3D>
#include <QVector4D>

#include <cmath>

using namespace HydroCouple::Composer;

namespace
{
  //! Where an arm's tip lands on screen, in the gizmo's -1…+1 square.
  QPointF tipOf(const QVector3D &axis, double azimuth, double elevation)
  {
    const QVector3D projected =
      axisGizmoMatrix(azimuth, elevation).map(axis);

    return QPointF(double(projected.x()), double(projected.y()));
  }
}

TEST(AxisGizmoTest, ItIsThreeArmsOfSolidGeometry)
{
  const SceneGeometry gizmo = buildAxisGizmo(12);

  EXPECT_EQ(gizmo.primitive, ScenePrimitive::Triangles);
  EXPECT_FALSE(gizmo.isEmpty());

  // Three arms, each a 12-sided shaft (24 vertices) and a 12-sided cone
  // with its tip (13) — 37 apiece.
  EXPECT_EQ(gizmo.vertices.size(), 3 * (24 + 13));

  // Indices divide into whole triangles, or the renderer draws rubbish.
  EXPECT_EQ(gizmo.indices.size() % 3, 0);

  // Every arm is one of the three convention colours, and all three are
  // present: a gizmo drawn in one colour says nothing.
  QSet<QRgb> colours;

  for (const SceneVertex &vertex : gizmo.vertices)
  {
    colours.insert(qRgb(int(vertex.r * 255.0f), int(vertex.g * 255.0f),
                        int(vertex.b * 255.0f)));
  }

  EXPECT_EQ(colours.size(), 3);
}

TEST(AxisGizmoTest, ASmallSegmentCountIsRaisedRatherThanDegenerate)
{
  // Two segments is not a cone, it is a flat sliver. Clamped to three
  // rather than trusted, because the caller is a preference.
  const SceneGeometry poor = buildAxisGizmo(2);
  const SceneGeometry least = buildAxisGizmo(3);

  EXPECT_EQ(poor.vertices.size(), least.vertices.size());
  EXPECT_FALSE(poor.isEmpty());
}

TEST(AxisGizmoTest, EveryArmIsASolidRatherThanACollapsedStick)
{
  // Each arm's ring is built on two vectors perpendicular to it, and the
  // obvious way to find them — cross with Z — returns a zero vector for
  // the Z arm itself. Qt normalises that to (0,0,0) rather than to NaN,
  // so a collapsed arm keeps its vertex count and its colour and every
  // other gate here stays green while one third of the gizmo is a line.
  // This is the gate that notices: each arm must have real thickness
  // across its own axis.
  const SceneGeometry gizmo = buildAxisGizmo(12);

  const QVector3D directions[3] = { { 1.0f, 0.0f, 0.0f },
                                    { 0.0f, 1.0f, 0.0f },
                                    { 0.0f, 0.0f, 1.0f } };

  const QRgb colours[3] = { qRgb(214, 69, 65), qRgb(86, 166, 75),
                            qRgb(66, 122, 214) };

  for (int arm = 0; arm < 3; ++arm)
  {
    float widest = 0.0f;

    for (const SceneVertex &vertex : gizmo.vertices)
    {
      const QRgb colour = qRgb(int(vertex.r * 255.0f), int(vertex.g * 255.0f),
                               int(vertex.b * 255.0f));

      if (colour != colours[arm])
      {
        continue;
      }

      // Distance from the arm's own axis, which is what a collapsed
      // basis drives to zero.
      const QVector3D position(vertex.x, vertex.y, vertex.z);
      const QVector3D along =
        directions[arm] * QVector3D::dotProduct(position, directions[arm]);

      widest = std::max(widest, (position - along).length());
    }

    EXPECT_GT(widest, 0.05f)
      << "arm " << arm << " has no thickness across its axis";
  }
}

TEST(AxisGizmoTest, TheGreenArmIsTheOnePointingNorth)
{
  // The geometry and the projection were tested apart, and nothing tied
  // them together: flipping the arm table's north to -Y drew a green
  // arrow pointing south while every other gate here stayed green,
  // because they all hand the matrix an axis vector of their own rather
  // than asking the builder which way it actually built. This is the
  // gate that asks.
  const SceneGeometry gizmo = buildAxisGizmo(12);

  struct Named
  {
      QRgb colour;
      QVector3D direction;
      const char *name;
  };

  const Named expected[3] = {
    { qRgb(214, 69, 65), { 1.0f, 0.0f, 0.0f }, "red is east" },
    { qRgb(86, 166, 75), { 0.0f, 1.0f, 0.0f }, "green is north" },
    { qRgb(66, 122, 214), { 0.0f, 0.0f, 1.0f }, "blue is up" },
  };

  for (const Named &arm : expected)
  {
    QVector3D farthest;
    float reach = 0.0f;

    for (const SceneVertex &vertex : gizmo.vertices)
    {
      const QRgb colour = qRgb(int(vertex.r * 255.0f), int(vertex.g * 255.0f),
                               int(vertex.b * 255.0f));

      if (colour != arm.colour)
      {
        continue;
      }

      const QVector3D position(vertex.x, vertex.y, vertex.z);

      if (position.length() > reach)
      {
        reach = position.length();
        farthest = position;
      }
    }

    // The point of the arrow, and it must lie along the axis the colour
    // claims: a dot product of one, not merely a positive one.
    ASSERT_GT(reach, 0.0f) << arm.name << ": no such arm";
    EXPECT_NEAR(QVector3D::dotProduct(farthest.normalized(), arm.direction),
                1.0, 1e-5)
      << arm.name << ", but it points elsewhere";
  }
}

TEST(AxisGizmoTest, NorthIsUpTheScreenInTheDefaultView)
{
  // Azimuth 0 looks north and the map is north-up, so the two must agree:
  // this is the whole reason the gizmo is worth drawing.
  const QPointF north = tipOf({ 0.0f, 1.0f, 0.0f }, 0.0, 30.0);
  const QPointF east = tipOf({ 1.0f, 0.0f, 0.0f }, 0.0, 30.0);

  EXPECT_GT(north.y(), 0.2) << "north does not point up the screen";
  EXPECT_NEAR(north.x(), 0.0, 1e-5) << "north is not straight ahead";

  EXPECT_GT(east.x(), 0.5) << "east does not point to the right";
  EXPECT_NEAR(east.y(), 0.0, 1e-5);
}

TEST(AxisGizmoTest, TurningTheCameraTurnsTheGizmoWithIt)
{
  // Azimuth 90 puts the eye due east looking west (Camera::eye()), so the
  // east arm is behind the viewer and projects *down* the screen. That is
  // the camera's convention, not a guess: the first version of this gate
  // asserted the opposite and the arithmetic said no.
  const QPointF eastAfter = tipOf({ 1.0f, 0.0f, 0.0f }, 90.0, 30.0);

  EXPECT_LT(eastAfter.y(), -0.2) << "east is not behind the viewer";
  EXPECT_NEAR(eastAfter.x(), 0.0, 1e-5);

  // And north, which was straight up, has swung to one side.
  const QPointF north = tipOf({ 0.0f, 1.0f, 0.0f }, 90.0, 30.0);
  EXPECT_GT(std::abs(north.x()), 0.5) << "north did not move with the camera";
}

TEST(AxisGizmoTest, TheGizmoCannotDisagreeWithTheCameraAboutNorth)
{
  // The property the whole thing rests on, and it is exact rather than
  // approximate: the gizmo borrows the camera's own eye, so an arm lands
  // where the camera would put it, scaled by the gizmo's orthographic
  // box and nothing else. Angles chosen to cover each quadrant and a
  // near-plan view.
  //
  // An earlier version of this gate compared the *signs* of the two and
  // failed at azimuth 180 and 270, where a component that is zero came
  // out as +0.0 from one and -0.0 from the other. Sign is not a property
  // of zero; the ratio is a property of every component, so it is the
  // ratio that is asserted.
  constexpr double kBoxHalfWidth = 1.4;

  const double angles[][2] = { { 0.0, 30.0 },   { 90.0, 30.0 },
                               { 180.0, 10.0 }, { 270.0, 60.0 },
                               { 35.0, 89.0 } };

  for (const auto &pair : angles)
  {
    Camera camera;
    camera.setAzimuth(pair[0]);
    camera.setElevation(pair[1]);
    camera.setTarget({ 1234.0f, -567.0f, 89.0f });
    camera.setDistance(4321.0);

    QMatrix4x4 cameraRotation = camera.viewMatrix();

    // Strip the translation: what is being compared is orientation, and
    // the gizmo deliberately has no position of its own.
    cameraRotation.setColumn(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));

    for (const QVector3D &axis : { QVector3D(1.0f, 0.0f, 0.0f),
                                   QVector3D(0.0f, 1.0f, 0.0f),
                                   QVector3D(0.0f, 0.0f, 1.0f) })
    {
      const QVector3D byCamera = cameraRotation.map(axis);
      const QVector3D byGizmo =
        axisGizmoMatrix(pair[0], pair[1]).map(axis);

      EXPECT_NEAR(double(byGizmo.x()),
                  double(byCamera.x()) / kBoxHalfWidth, 1e-6)
        << "x disagrees at azimuth " << pair[0];
      EXPECT_NEAR(double(byGizmo.y()),
                  double(byCamera.y()) / kBoxHalfWidth, 1e-6)
        << "y disagrees at azimuth " << pair[0];
    }
  }
}

TEST(AxisGizmoTest, LookingStraightDownFlattensUpToAPoint)
{
  // From directly overhead the Up arm points at the viewer, so it lands
  // on the origin rather than off to a side. Not *exactly* the origin:
  // the camera stops a thousandth of a degree short of vertical so its
  // view matrix stays non-degenerate, and the gizmo inherits that. What
  // is promised is a landing well inside a pixel of a gizmo drawn a
  // hundred pixels across, not an exact zero.
  const QPointF up = tipOf({ 0.0f, 0.0f, 1.0f }, 0.0, 90.0);

  EXPECT_NEAR(up.x(), 0.0, 1.0e-3);
  EXPECT_NEAR(up.y(), 0.0, 1.0e-3);

  // And north still runs up the screen, which is what makes a plan view
  // read as a map. This is also the gate that catches the degenerate
  // case: build the view at exactly ninety degrees, where the eye and
  // the up vector are parallel, and lookAt collapses every arm onto the
  // origin — north's tip included, so this comparison drops to zero.
  const QPointF north = tipOf({ 0.0f, 1.0f, 0.0f }, 0.0, 90.0);
  EXPECT_GT(north.y(), 0.5);
}

TEST(AxisGizmoTest, TheProjectionIgnoresEverythingButRotation)
{
  // The matrix is a function of the two angles alone. If distance,
  // target or exaggeration ever leaked in, the gizmo would slide off its
  // corner on a pan and shrink on a zoom.
  const QMatrix4x4 first = axisGizmoMatrix(35.0, 20.0);
  const QMatrix4x4 second = axisGizmoMatrix(35.0, 20.0);

  EXPECT_EQ(first, second);

  // An arm's tip stays exactly one unit from the origin in world terms,
  // so the drawn size cannot depend on the scene's scale.
  const QPointF east = tipOf({ 1.0f, 0.0f, 0.0f }, 0.0, 0.0);
  EXPECT_NEAR(std::hypot(east.x(), east.y()), 1.0 / 1.4, 1e-5);

  // And every arm lands inside the depth box at every angle, with room
  // to spare. This is the only thing the borrowed camera's distance
  // changes — it slides the arms along z and nothing else — so it is the
  // only place a distance that crept upward could be caught before the
  // renderer clipped an arm away and left the gizmo looking like it had
  // two. The margin is the point: at unit distance the arms occupy the
  // middle half of the box, and an arm merely *touching* the far plane
  // is already a bug waiting on a rounding error, so this asks for
  // clearance rather than for bare survival.
  for (double azimuth = 0.0; azimuth < 360.0; azimuth += 45.0)
  {
    for (double elevation : { -80.0, 0.0, 45.0, 89.0 })
    {
      const QMatrix4x4 matrix = axisGizmoMatrix(azimuth, elevation);

      for (const QVector3D &axis : { QVector3D(1.0f, 0.0f, 0.0f),
                                     QVector3D(0.0f, 1.0f, 0.0f),
                                     QVector3D(0.0f, 0.0f, 1.0f) })
      {
        EXPECT_LE(std::abs(double(matrix.map(axis).z())), 0.9)
          << "an arm falls outside the depth box at azimuth " << azimuth
          << " elevation " << elevation;
      }
    }
  }
}

TEST(AxisGizmoTest, ClickingAnArmPicksItAndClickingBesideItPicksNothing)
{
  const double azimuth = 0.0;
  const double elevation = 30.0;

  EXPECT_EQ(axisGizmoHit(tipOf({ 0.0f, 1.0f, 0.0f }, azimuth, elevation),
                         azimuth, elevation),
            GizmoAxis::North);
  EXPECT_EQ(axisGizmoHit(tipOf({ 1.0f, 0.0f, 0.0f }, azimuth, elevation),
                         azimuth, elevation),
            GizmoAxis::East);
  EXPECT_EQ(axisGizmoHit(tipOf({ 0.0f, 0.0f, 1.0f }, azimuth, elevation),
                         azimuth, elevation),
            GizmoAxis::Up);

  // A corner of the square is on no arm at all: a gizmo that answered
  // "north" for any click would snap the camera on every stray press.
  EXPECT_EQ(axisGizmoHit(QPointF(-0.95, -0.95), azimuth, elevation),
            GizmoAxis::None);
}

TEST(AxisGizmoTest, TheAnswersAreTheViewsTheyName)
{
  double azimuth = 123.0;
  double elevation = 45.0;

  ASSERT_TRUE(axisGizmoView(GizmoAxis::Up, azimuth, elevation));
  EXPECT_DOUBLE_EQ(elevation, 90.0);
  EXPECT_DOUBLE_EQ(azimuth, 123.0)
    << "looking down spun the map, which nobody asked it to do";

  ASSERT_TRUE(axisGizmoView(GizmoAxis::North, azimuth, elevation));
  EXPECT_DOUBLE_EQ(azimuth, 0.0);
  EXPECT_DOUBLE_EQ(elevation, 0.0);

  ASSERT_TRUE(axisGizmoView(GizmoAxis::East, azimuth, elevation));
  EXPECT_DOUBLE_EQ(azimuth, 90.0);

  // None changes nothing, so a missed click cannot move the camera.
  double keptAzimuth = azimuth;
  double keptElevation = elevation;

  EXPECT_FALSE(axisGizmoView(GizmoAxis::None, keptAzimuth, keptElevation));
  EXPECT_DOUBLE_EQ(keptAzimuth, azimuth);
  EXPECT_DOUBLE_EQ(keptElevation, elevation);
}

TEST(AxisGizmoTest, SnappingToAnArmThenClickingItAgainIsStable)
{
  // Click North, adopt that view, click North again: the same answer.
  // A gizmo whose own snapped view no longer reads as that axis would
  // drift a little further on every click.
  double azimuth = 40.0;
  double elevation = 25.0;

  ASSERT_TRUE(axisGizmoView(GizmoAxis::North, azimuth, elevation));

  const QPointF north = tipOf({ 0.0f, 1.0f, 0.0f }, azimuth, elevation);
  EXPECT_EQ(axisGizmoHit(north, azimuth, elevation), GizmoAxis::North);
}

TEST(AxisGizmoTest, TheCornerNamesSurviveARoundTripAndNonsenseIsACorner)
{
  // Preferences stores the corner by name, so the two directions have to
  // agree or a chosen corner silently reverts on the next launch.
  for (GizmoCorner corner : { GizmoCorner::BottomLeft,
                              GizmoCorner::BottomRight, GizmoCorner::TopLeft,
                              GizmoCorner::TopRight })
  {
    EXPECT_EQ(gizmoCornerFromName(gizmoCornerName(corner)), corner);
  }

  // A settings file hand-edited into nonsense puts the gizmo somewhere
  // rather than making the view undrawable.
  EXPECT_EQ(gizmoCornerFromName(QStringLiteral("Middle")),
            GizmoCorner::BottomLeft);
  EXPECT_EQ(gizmoCornerFromName(QString()), GizmoCorner::BottomLeft);

  // Case is not folded, and should not be: these are stored tokens, not
  // anything a user types.
  EXPECT_EQ(gizmoCornerName(GizmoCorner::TopRight),
            QStringLiteral("TopRight"));
}

TEST(AxisGizmoTest, EachCornerIsTheCornerItNames)
{
  const QSize view(800, 600);
  const int size = 96;

  const QRect bottomLeft =
    axisGizmoRect(view, size, GizmoCorner::BottomLeft);
  const QRect topRight = axisGizmoRect(view, size, GizmoCorner::TopRight);

  EXPECT_EQ(bottomLeft.size(), QSize(size, size));

  // Widget coordinates, so "bottom" is the larger y.
  EXPECT_LT(bottomLeft.x(), view.width() / 2);
  EXPECT_GT(bottomLeft.y(), view.height() / 2);

  EXPECT_GT(topRight.x(), view.width() / 2);
  EXPECT_LT(topRight.y(), view.height() / 2);

  // Every corner stays inside the view: a gizmo half off the edge reads
  // as a rendering fault rather than as a placement choice.
  for (GizmoCorner corner : { GizmoCorner::BottomLeft,
                              GizmoCorner::BottomRight, GizmoCorner::TopLeft,
                              GizmoCorner::TopRight })
  {
    const QRect rect = axisGizmoRect(view, size, corner);

    EXPECT_TRUE(QRect(QPoint(0, 0), view).contains(rect))
      << "a corner hangs off the view";
  }
}

TEST(AxisGizmoTest, AGizmoTooBigForItsViewShrinksRatherThanOverflowing)
{
  // The size is a preference, and a panel can be dragged smaller than
  // any sensible preference. A viewport wider than its widget is not a
  // large gizmo; on some backends it is a validation error that takes
  // the frame down with it.
  const QRect rect = axisGizmoRect(QSize(60, 40), 96, GizmoCorner::BottomLeft);

  EXPECT_TRUE(QRect(0, 0, 60, 40).contains(rect));
  EXPECT_GE(rect.width(), 0);
  EXPECT_EQ(rect.width(), rect.height()) << "the gizmo stopped being square";

  // And a view with no room at all asks for nothing to be drawn, rather
  // than for something to be drawn nowhere.
  //
  // Checked as a width of exactly zero, and NOT with isEmpty(), which is
  // the fifth time in this codebase that Qt's emptiness has hidden a
  // degenerate rectangle: QRect::isEmpty() is true for a width of -16
  // just as it is for a width of 0, so a gate written the obvious way
  // stays green while the renderer is handed a negative viewport — the
  // exact thing this is here to prevent. A rect with nothing to draw is
  // zero-sized; a rect with a negative size is a bug.
  for (const QSize &tooSmall : { QSize(8, 8), QSize(), QSize(1, 400) })
  {
    const QRect none = axisGizmoRect(tooSmall, 96, GizmoCorner::BottomLeft);

    EXPECT_EQ(none.width(), 0) << "a view of " << tooSmall.width() << "x"
                               << tooSmall.height();
    EXPECT_EQ(none.height(), 0);
  }

  EXPECT_EQ(axisGizmoRect(QSize(800, 600), 0, GizmoCorner::BottomLeft).width(),
            0);
}

TEST(AxisGizmoTest, AClickInTheCornerBecomesAPointInTheGizmosSquare)
{
  const QRect rect(20, 480, 96, 96);

  // The centre of the square is the origin of the gizmo.
  const QPointF centre =
    axisGizmoPoint(QPoint(rect.x() + 48, rect.y() + 48), rect);

  EXPECT_NEAR(centre.x(), 0.0, 1e-9);
  EXPECT_NEAR(centre.y(), 0.0, 1e-9);

  // The top of the widget rect is +1, not -1. Widget coordinates grow
  // downward and clip space grows upward, and getting this backwards is
  // how a click on the north arm answers "south" — which is worse than
  // answering nothing, because it looks like it worked.
  EXPECT_GT(axisGizmoPoint(QPoint(rect.x() + 48, rect.y()), rect).y(), 0.9);
  EXPECT_LT(
    axisGizmoPoint(QPoint(rect.x() + 48, rect.bottom()), rect).y(), -0.9);

  // X is not flipped.
  EXPECT_LT(axisGizmoPoint(QPoint(rect.x(), rect.y() + 48), rect).x(), -0.9);
  EXPECT_GT(
    axisGizmoPoint(QPoint(rect.right(), rect.y() + 48), rect).x(), 0.9);

  // An empty rect answers a definite point off the square rather than
  // dividing by zero. The distinction matters more than it looks: a NaN
  // would also read as "no arm", because every comparison against a NaN
  // is false — so the gizmo would appear to behave while the widget
  // quietly fed it nonsense on every press before the first resize.
  const QPointF nowhere = axisGizmoPoint(QPoint(5, 5), QRect());

  EXPECT_TRUE(std::isfinite(nowhere.x()));
  EXPECT_TRUE(std::isfinite(nowhere.y()));
  EXPECT_GT(std::hypot(nowhere.x(), nowhere.y()), 1.0);
  EXPECT_EQ(axisGizmoHit(nowhere, 0.0, 30.0), GizmoAxis::None);

  // A rect of negative width is the dangerous one, and the reason the
  // guard tests the width rather than trusting the caller. Dividing by
  // it does not produce an obvious infinity — it produces a perfectly
  // plausible point *inside* the square, so a press nowhere near the
  // gizmo would answer with an arm and swing the camera.
  const QPointF inverted = axisGizmoPoint(QPoint(5, 5), QRect(12, 12, -16, -16));

  EXPECT_GT(std::hypot(inverted.x(), inverted.y()), 1.0)
    << "an inverted rect made a stray click look like it landed on an arm";
  EXPECT_EQ(axisGizmoHit(inverted, 0.0, 30.0), GizmoAxis::None);
}

TEST(AxisGizmoTest, ClickingTheNorthArmOnScreenAnswersNorth)
{
  // The whole click path, end to end, in the only place it can be
  // checked without a graphics device: view size and corner to a rect,
  // a widget-coordinate press to a gizmo point, that point to an arm.
  // What is left in the widget after this is three calls and no
  // arithmetic of its own.
  const QSize view(1024, 768);
  const double azimuth = 0.0;
  const double elevation = 30.0;

  const QRect rect = axisGizmoRect(view, 96, GizmoCorner::BottomLeft);

  const QVector3D north(0.0f, 1.0f, 0.0f);
  const QVector3D projected = axisGizmoMatrix(azimuth, elevation).map(north);

  // Where that tip lands in the widget, going the other way.
  const QPoint press(
    rect.x() + int(std::lround((double(projected.x()) + 1.0) * 0.5
                               * rect.width())),
    rect.y() + int(std::lround((1.0 - double(projected.y())) * 0.5
                               * rect.height())));

  ASSERT_TRUE(rect.contains(press)) << "the north arm is off its own square";

  EXPECT_EQ(axisGizmoHit(axisGizmoPoint(press, rect), azimuth, elevation),
            GizmoAxis::North);

  // And a press in the far corner of the same square answers nothing, so
  // stray clicks near the gizmo do not move the camera.
  EXPECT_EQ(axisGizmoHit(axisGizmoPoint(QPoint(rect.x() + 2,
                                               rect.bottom() - 2),
                                        rect),
                         azimuth, elevation),
            GizmoAxis::None);
}
