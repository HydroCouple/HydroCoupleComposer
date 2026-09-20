#include "scene/axisgizmo.h"

#include "scene/camera.h"

#include <QColor>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace HydroCouple::Composer
{

  namespace
  {
    //! Where the shaft stops and the arrowhead begins.
    constexpr float kShaftLength = 0.72f;

    //! Half-thickness of a shaft. Thin, but a solid rather than a line:
    //! one batch, one pipeline, and it reads at any angle.
    constexpr float kShaftRadius = 0.035f;

    //! Base radius of an arrowhead.
    constexpr float kConeRadius = 0.11f;

    /*!
     * \brief The three arms: direction and colour.
     *
     * Red east, green north, blue up — the convention every 3D tool uses,
     * so it needs no legend. North is +Y because the camera's azimuth is
     * measured from it (camera.h): a gizmo that disagreed with the camera
     * about north would be worse than none.
     */
    struct Arm
    {
        QVector3D direction;
        QColor color;
    };

    const std::array<Arm, 3> &arms()
    {
      static const std::array<Arm, 3> table = {
        Arm{ { 1.0f, 0.0f, 0.0f }, QColor(214, 69, 65) },
        Arm{ { 0.0f, 1.0f, 0.0f }, QColor(86, 166, 75) },
        Arm{ { 0.0f, 0.0f, 1.0f }, QColor(66, 122, 214) },
      };

      return table;
    }

    //! Two unit vectors perpendicular to \a axis, and to each other.
    void basisFor(const QVector3D &axis, QVector3D &u, QVector3D &v)
    {
      // Crossing with Z fails for the Z arm itself, so that one crosses
      // with X instead. Picking the "obvious" up vector unconditionally is
      // how a builder ends up with a degenerate ring on exactly one arm.
      const QVector3D helper = std::abs(axis.z()) > 0.9f
                                 ? QVector3D(1.0f, 0.0f, 0.0f)
                                 : QVector3D(0.0f, 0.0f, 1.0f);

      u = QVector3D::crossProduct(axis, helper).normalized();
      v = QVector3D::crossProduct(axis, u).normalized();
    }
  } // namespace

  SceneGeometry buildAxisGizmo(int coneSegments)
  {
    SceneGeometry gizmo;
    gizmo.primitive = ScenePrimitive::Triangles;

    const int segments = std::max(3, coneSegments);

    for (const Arm &arm : arms())
    {
      QVector3D u;
      QVector3D v;
      basisFor(arm.direction, u, v);

      const QVector3D shaftEnd = arm.direction * kShaftLength;
      const QVector3D tip = arm.direction;

      // ── the shaft, as a ring extruded from origin to shaftEnd ──────────
      const quint32 shaftBase = quint32(gizmo.vertices.size());

      for (int s = 0; s < segments; ++s)
      {
        const float angle = float(2.0 * M_PI * s / segments);
        const QVector3D offset =
          (u * std::cos(angle) + v * std::sin(angle)) * kShaftRadius;
        const QVector3D normal = offset.normalized();

        gizmo.addVertex(offset, normal, arm.color);
        gizmo.addVertex(shaftEnd + offset, normal, arm.color);
      }

      for (int s = 0; s < segments; ++s)
      {
        const quint32 a = shaftBase + quint32(s) * 2;
        const quint32 b = a + 1;
        const quint32 c = shaftBase + quint32((s + 1) % segments) * 2;
        const quint32 d = c + 1;

        gizmo.indices.append(a);
        gizmo.indices.append(c);
        gizmo.indices.append(b);
        gizmo.indices.append(b);
        gizmo.indices.append(c);
        gizmo.indices.append(d);
      }

      // ── the arrowhead, as a fan from the tip ───────────────────────────
      const quint32 coneBase = quint32(gizmo.vertices.size());
      gizmo.addVertex(tip, arm.direction, arm.color);

      for (int s = 0; s < segments; ++s)
      {
        const float angle = float(2.0 * M_PI * s / segments);
        const QVector3D offset =
          (u * std::cos(angle) + v * std::sin(angle)) * kConeRadius;

        gizmo.addVertex(shaftEnd + offset, offset.normalized(), arm.color);
      }

      for (int s = 0; s < segments; ++s)
      {
        gizmo.indices.append(coneBase);
        gizmo.indices.append(coneBase + 1 + quint32(s));
        gizmo.indices.append(coneBase + 1 + quint32((s + 1) % segments));
      }
    }

    return gizmo;
  }

  GizmoCorner gizmoCornerFromName(const QString &name)
  {
    if (name == QLatin1String("BottomRight"))
    {
      return GizmoCorner::BottomRight;
    }

    if (name == QLatin1String("TopLeft"))
    {
      return GizmoCorner::TopLeft;
    }

    if (name == QLatin1String("TopRight"))
    {
      return GizmoCorner::TopRight;
    }

    return GizmoCorner::BottomLeft;
  }

  QString gizmoCornerName(GizmoCorner corner)
  {
    switch (corner)
    {
      case GizmoCorner::BottomRight:
        return QStringLiteral("BottomRight");

      case GizmoCorner::TopLeft:
        return QStringLiteral("TopLeft");

      case GizmoCorner::TopRight:
        return QStringLiteral("TopRight");

      case GizmoCorner::BottomLeft:
        break;
    }

    return QStringLiteral("BottomLeft");
  }

  QRect axisGizmoRect(const QSize &viewport, int size, GizmoCorner corner)
  {
    //! Breathing room between the gizmo and the edges of the view.
    constexpr int kInset = 12;

    if (viewport.isEmpty() || size <= 0)
    {
      return {};
    }

    // Shrunk to fit rather than clamped at the edge. A gizmo asked to be
    // larger than the view — a preference typed into a small docked
    // panel, say — would otherwise be handed a viewport running off the
    // widget, and a negative viewport width is not a small gizmo, it is
    // a validation error on some backends.
    const int side =
      std::min({ size, viewport.width() - 2 * kInset,
                 viewport.height() - 2 * kInset });

    if (side <= 0)
    {
      return {};
    }

    const int left = kInset;
    const int right = viewport.width() - side - kInset;
    const int top = kInset;
    const int bottom = viewport.height() - side - kInset;

    switch (corner)
    {
      case GizmoCorner::BottomRight:
        return { right, bottom, side, side };

      case GizmoCorner::TopLeft:
        return { left, top, side, side };

      case GizmoCorner::TopRight:
        return { right, top, side, side };

      case GizmoCorner::BottomLeft:
        break;
    }

    return { left, bottom, side, side };
  }

  QPointF axisGizmoPoint(const QPoint &point, const QRect &rect)
  {
    if (rect.width() <= 0 || rect.height() <= 0)
    {
      return { 2.0, 2.0 };
    }

    const double x =
      2.0 * double(point.x() - rect.x()) / double(rect.width()) - 1.0;

    // Widget coordinates grow downward and clip space grows upward, so
    // this one subtraction is the difference between a click on the
    // north arm answering "north" and answering "south".
    const double y =
      1.0 - 2.0 * double(point.y() - rect.y()) / double(rect.height());

    return { x, y };
  }

  QMatrix4x4 axisGizmoMatrix(double azimuthDegrees, double elevationDegrees)
  {
    // A box a little wider than the arms: an arrowhead pointing straight at
    // the viewer would otherwise be sliced off by the near plane, which
    // looks like the arm is missing rather than foreshortened.
    QMatrix4x4 projection;
    projection.ortho(-1.4f, 1.4f, -1.4f, 1.4f, -4.0f, 4.0f);

    // Rotation only — the camera's own view matrix carries its distance
    // and its target, and using it would slide the gizmo off its corner
    // the moment the user panned and shrink it as they zoomed out.
    //
    // The eye comes from a real Camera rather than from a second copy of
    // the same trigonometry. Two reasons, both learned the hard way. A
    // sign convention written out twice is a sign convention that will
    // eventually be written out two different ways, and a gizmo that
    // disagreed with the camera about which way north is would be worse
    // than none. And the camera holds its elevation a hair short of
    // vertical on purpose: at exactly ninety degrees the view direction
    // is parallel to the up vector and lookAt collapses to a matrix that
    // maps every arm onto the origin. Borrowing the camera borrows that
    // guard, so a plan view still shows three arms rather than none.
    //
    // Unit distance, because that is the only thing here the distance
    // changes: it slides the arms along z, and a camera parked further
    // out would push one past the far plane set above.
    Camera camera;
    camera.setTarget(QVector3D(0.0f, 0.0f, 0.0f));
    camera.setDistance(1.0);
    camera.setAzimuth(azimuthDegrees);
    camera.setElevation(elevationDegrees);

    QMatrix4x4 view;
    view.lookAt(camera.eye(), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 0.0f, 1.0f));

    return projection * view;
  }

  GizmoAxis axisGizmoHit(const QPointF &point, double azimuthDegrees,
                         double elevationDegrees, double tolerance)
  {
    const QMatrix4x4 matrix =
      axisGizmoMatrix(azimuthDegrees, elevationDegrees);

    GizmoAxis best = GizmoAxis::None;

    // Seeded past any candidate rather than at the tolerance, so that the
    // tolerance is stated exactly once — in the test below. Seeding it
    // here too made that test redundant, and a rule written twice is a
    // rule half of which can be deleted without anything noticing.
    double bestDistance = std::numeric_limits<double>::infinity();
    float bestDepth = 2.0f;

    const GizmoAxis names[3] = { GizmoAxis::East, GizmoAxis::North,
                                 GizmoAxis::Up };

    for (int i = 0; i < 3; ++i)
    {
      const QVector3D projected = matrix.map(arms().at(size_t(i)).direction);
      const double dx = double(projected.x()) - point.x();
      const double dy = double(projected.y()) - point.y();
      const double distance = std::hypot(dx, dy);

      if (distance > tolerance)
      {
        continue;
      }

      // Nearest to the click wins. Depth breaks a tie and nothing more:
      // an arm pointing at the viewer is very near the origin, so letting
      // depth override distance would make it swallow clicks meant for
      // whichever arm the user was actually aiming at.
      constexpr double kSameSpot = 1.0e-3;

      if (distance < bestDistance - kSameSpot
          || (distance < bestDistance + kSameSpot
              && projected.z() < bestDepth))
      {
        best = names[i];
        bestDistance = distance;
        bestDepth = projected.z();
      }
    }

    return best;
  }

  bool axisGizmoView(GizmoAxis axis, double &azimuthDegrees,
                     double &elevationDegrees)
  {
    switch (axis)
    {
      case GizmoAxis::Up:
        // Straight down. The azimuth is left alone: a plan view has no
        // meaningful one, and spinning the map because the user asked to
        // look down would be answering a question they did not ask.
        elevationDegrees = 90.0;

        return true;

      case GizmoAxis::North:
        // Looking north means standing to the south, which is azimuth 0
        // by the camera's own definition.
        azimuthDegrees = 0.0;
        elevationDegrees = 0.0;

        return true;

      case GizmoAxis::East:
        azimuthDegrees = 90.0;
        elevationDegrees = 0.0;

        return true;

      case GizmoAxis::None:
        break;
    }

    return false;
  }

} // namespace HydroCouple::Composer
