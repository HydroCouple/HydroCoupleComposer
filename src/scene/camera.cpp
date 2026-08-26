#include "scene/camera.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    constexpr double kDegreesToRadians = M_PI / 180.0;

    //! Exactly 90 degrees of elevation makes the view direction parallel to
    //! the up vector, which produces a degenerate view matrix. Held just
    //! short of it so a top-down view is available without that.
    constexpr double kMaxElevation = 89.999;

    constexpr double kMinDistance = 1.0e-6;

    double wrapDegrees(double degrees)
    {
      double wrapped = std::fmod(degrees, 360.0);

      if (wrapped < 0.0)
      {
        wrapped += 360.0;
      }

      return wrapped;
    }

  }

  Camera::Camera() = default;

  QVector3D Camera::target() const
  {
    return m_target;
  }

  void Camera::setTarget(const QVector3D &target)
  {
    m_target = target;
  }

  double Camera::distance() const
  {
    return m_distance;
  }

  void Camera::setDistance(double distance)
  {
    m_distance = std::max(kMinDistance, distance);
  }

  double Camera::azimuth() const
  {
    return m_azimuth;
  }

  void Camera::setAzimuth(double degrees)
  {
    m_azimuth = wrapDegrees(degrees);
  }

  double Camera::elevation() const
  {
    return m_elevation;
  }

  void Camera::setElevation(double degrees)
  {
    m_elevation = std::clamp(degrees, -kMaxElevation, kMaxElevation);
  }

  double Camera::fieldOfView() const
  {
    return m_fieldOfView;
  }

  void Camera::setFieldOfView(double degrees)
  {
    m_fieldOfView = std::clamp(degrees, 1.0, 179.0);
  }

  CameraProjection Camera::projection() const
  {
    return m_projection;
  }

  void Camera::setProjection(CameraProjection projection, double aspect)
  {
    if (projection == m_projection)
    {
      return;
    }

    // Carry the view across by what the user can see, not by the internal
    // parameters: the two projections share no scale term, so matching them
    // any other way makes the switch jump.
    const QRectF ground = groundExtent(aspect);

    m_projection = projection;

    if (!ground.isEmpty())
    {
      setGroundExtent(ground, aspect);
    }
  }

  double Camera::verticalExaggeration() const
  {
    return m_verticalExaggeration;
  }

  void Camera::setVerticalExaggeration(double factor)
  {
    m_verticalExaggeration = std::max(kMinDistance, factor);
  }

  QVector3D Camera::eye() const
  {
    const double azimuth = m_azimuth * kDegreesToRadians;
    const double elevation = m_elevation * kDegreesToRadians;

    const double horizontal = std::cos(elevation) * m_distance;

    // Azimuth 0 puts the eye due south of the target, so the view looks
    // north and a default 3D view agrees with a north-up map.
    return m_target + QVector3D(float(horizontal * std::sin(azimuth)),
                                float(-horizontal * std::cos(azimuth)),
                                float(std::sin(elevation) * m_distance));
  }

  QMatrix4x4 Camera::modelMatrix() const
  {
    QMatrix4x4 model;
    model.scale(1.0f, 1.0f, float(m_verticalExaggeration));

    return model;
  }

  QMatrix4x4 Camera::viewMatrix() const
  {
    QMatrix4x4 view;
    view.lookAt(eye(), m_target, QVector3D(0.0f, 0.0f, 1.0f));

    return view;
  }

  QMatrix4x4 Camera::projectionMatrix(double aspect) const
  {
    QMatrix4x4 projection;

    if (aspect <= 0.0)
    {
      return projection;
    }

    if (m_projection == CameraProjection::Perspective)
    {
      projection.perspective(float(m_fieldOfView), float(aspect),
                             float(nearPlane()), float(farPlane()));
    }
    else
    {
      const double halfHeight = m_orthoHalfHeight;
      const double halfWidth = halfHeight * aspect;

      projection.ortho(float(-halfWidth), float(halfWidth),
                       float(-halfHeight), float(halfHeight),
                       float(nearPlane()), float(farPlane()));
    }

    return projection;
  }

  QMatrix4x4 Camera::modelViewProjection(double aspect) const
  {
    return projectionMatrix(aspect) * viewMatrix() * modelMatrix();
  }

  double Camera::groundHalfHeight() const
  {
    if (m_projection == CameraProjection::Orthographic)
    {
      return m_orthoHalfHeight;
    }

    return m_distance * std::tan(m_fieldOfView * 0.5 * kDegreesToRadians);
  }

  QRectF Camera::groundExtent(double aspect) const
  {
    if (aspect <= 0.0)
    {
      return {};
    }

    const QVector3D eyePosition = eye();
    const QVector3D forward = (m_target - eyePosition).normalized();
    const QVector3D right =
      QVector3D::crossProduct(forward, QVector3D(0.0f, 0.0f, 1.0f))
        .normalized();
    const QVector3D up = QVector3D::crossProduct(right, forward).normalized();

    const double halfHeight = groundHalfHeight();
    const double halfWidth = halfHeight * aspect;

    Bounds3D bounds;

    for (int corner = 0; corner < 4; ++corner)
    {
      const double horizontal = (corner & 1) ? halfWidth : -halfWidth;
      const double vertical = (corner & 2) ? halfHeight : -halfHeight;

      QVector3D origin;
      QVector3D direction;

      if (m_projection == CameraProjection::Perspective)
      {
        // The frustum's corner rays all leave the eye; the offsets are
        // measured on the plane through the target, which is where the
        // half-extents were defined.
        origin = eyePosition;
        direction = (m_target + right * float(horizontal) +
                     up * float(vertical) - eyePosition)
                      .normalized();
      }
      else
      {
        origin = eyePosition + right * float(horizontal) +
                 up * float(vertical);
        direction = forward;
      }

      // Where the ray meets z = 0. A ray heading away from the ground never
      // will, so it is cut at the far plane instead of reported as infinite.
      const double denominator = double(direction.z());
      const double travel = std::abs(denominator) < 1.0e-12
                              ? farPlane()
                              : -double(origin.z()) / denominator;

      const double clamped =
        (travel <= 0.0 || travel > farPlane()) ? farPlane() : travel;

      bounds.expandTo(origin + direction * float(clamped));
    }

    return bounds.footprint();
  }

  void Camera::applyGroundScale(double scale)
  {
    // One length sets the size of the view. Distance is what does it for a
    // perspective camera; an orthographic one has no distance term in its
    // projection, so its half-height leads and the eye follows far enough
    // back to keep the scene inside the clip range.
    if (m_projection == CameraProjection::Orthographic)
    {
      m_orthoHalfHeight = std::max(kMinDistance, scale);
      setDistance(scale * 2.0);
    }
    else
    {
      setDistance(scale);
    }
  }

  void Camera::setGroundExtent(const QRectF &extent, double aspect)
  {
    if (extent.isNull() || aspect <= 0.0)
    {
      return;
    }

    // A degenerate extent still has to produce a usable view, exactly as
    // MapTransform::fit does for a single point.
    const double width = extent.width() > 0.0 ? extent.width() : 1.0;
    const double height = extent.height() > 0.0 ? extent.height() : 1.0;

    // Solved, not approximated. Simply looking at the rectangle's centre
    // loses its near strip at every tilt: a tilted frustum's ground footprint
    // is a trapezoid whose centre lies beyond the point being looked at, and
    // the error grows as the camera drops toward the horizon.
    //
    // With the target on the ground plane the whole configuration — eye
    // offset, frustum corners, clip range — scales about that point with the
    // one length, and translates with the target. So a single probe at unit
    // scale determines both the scale and the offset exactly.
    Camera probe = *this;
    probe.m_target = QVector3D();
    probe.applyGroundScale(1.0);

    const QRectF unit = probe.groundExtent(aspect);

    if (unit.width() <= 0.0 || unit.height() <= 0.0)
    {
      return;
    }

    // The axis that binds is the one needing the larger scale; taking the
    // maximum is what preserves aspect and guarantees containment.
    const double scale =
      std::max(width / unit.width(), height / unit.height());

    m_target =
      QVector3D(float(extent.center().x() - unit.center().x() * scale),
                float(extent.center().y() - unit.center().y() * scale), 0.0f);

    applyGroundScale(scale);
  }

  void Camera::fitTo(const Bounds3D &bounds, double aspect)
  {
    if (!bounds.isValid() || aspect <= 0.0)
    {
      return;
    }

    // The box's bounding sphere, not its ground footprint. Framing the
    // footprint and then lowering the target into the relief — the obvious
    // order — puts the eye below the rim of anything concave, so a bowl is
    // fitted from inside it. A sphere has no orientation, so this frames the
    // whole box from any azimuth and elevation the user then orbits to.
    const QVector3D span =
      (bounds.maximum() - bounds.minimum()) *
      QVector3D(1.0f, 1.0f, float(m_verticalExaggeration));

    const double radius = 0.5 * double(span.length());

    m_target = QVector3D(bounds.center().x(), bounds.center().y(),
                         float(double(bounds.center().z()) *
                               m_verticalExaggeration));

    if (radius <= 0.0)
    {
      return;
    }

    if (m_projection == CameraProjection::Orthographic)
    {
      // Whichever axis is narrower is the one that must admit the sphere.
      applyGroundScale(aspect >= 1.0 ? radius : radius / aspect);

      return;
    }

    const double halfVertical = m_fieldOfView * 0.5 * kDegreesToRadians;
    const double halfHorizontal = std::atan(std::tan(halfVertical) * aspect);

    applyGroundScale(radius / std::sin(std::min(halfVertical, halfHorizontal)));
  }

  void Camera::orbit(double deltaAzimuth, double deltaElevation)
  {
    setAzimuth(m_azimuth + deltaAzimuth);
    setElevation(m_elevation + deltaElevation);
  }

  void Camera::pan(const QPointF &pixels, double viewportHeight, double aspect)
  {
    if (viewportHeight <= 0.0 || aspect <= 0.0)
    {
      return;
    }

    // World units per pixel at the target plane. Using the target plane is
    // what makes a drag move the thing under the cursor by the drag's length
    // rather than by an amount that depends on how far away it is.
    const double perPixel = 2.0 * groundHalfHeight() / viewportHeight;

    const QVector3D eyePosition = eye();
    const QVector3D forward = (m_target - eyePosition).normalized();
    const QVector3D right =
      QVector3D::crossProduct(forward, QVector3D(0.0f, 0.0f, 1.0f))
        .normalized();
    const QVector3D up = QVector3D::crossProduct(right, forward).normalized();

    m_target -= right * float(pixels.x() * perPixel);
    m_target += up * float(pixels.y() * perPixel);
  }

  void Camera::dolly(double factor)
  {
    if (factor <= 0.0)
    {
      return;
    }

    setDistance(m_distance * factor);

    // Orthographic has no distance term in its projection, so a dolly that
    // only moved the eye would appear to do nothing at all.
    if (m_projection == CameraProjection::Orthographic)
    {
      m_orthoHalfHeight = std::max(kMinDistance, m_orthoHalfHeight * factor);
    }
  }

  bool Camera::rayThrough(const QPointF &pixel, const QSize &viewport,
                          QVector3D &origin, QVector3D &direction) const
  {
    if (viewport.width() <= 0 || viewport.height() <= 0)
    {
      return false;
    }

    const double aspect =
      double(viewport.width()) / double(viewport.height());

    bool invertible = false;
    const QMatrix4x4 inverse =
      modelViewProjection(aspect).inverted(&invertible);

    if (!invertible)
    {
      return false;
    }

    // Widget y grows downward and normalised device y grows upward, hence
    // the subtraction rather than a second scale.
    const float ndcX =
      float(2.0 * pixel.x() / double(viewport.width()) - 1.0);
    const float ndcY =
      float(1.0 - 2.0 * pixel.y() / double(viewport.height()));

    // Through QVector4D and divided by w by hand: QMatrix4x4::map() of a
    // QVector3D treats it as a point and does not divide, which is right for
    // projecting and silently wrong for undoing a projection.
    const auto unproject = [&](float ndcZ) -> QVector3D
    {
      const QVector4D clip = inverse * QVector4D(ndcX, ndcY, ndcZ, 1.0f);

      return qFuzzyIsNull(clip.w()) ? clip.toVector3D()
                                    : clip.toVector3D() / clip.w();
    };

    // Qt's projection matrices are OpenGL-style, so the near plane is at
    // z = -1 and the far plane at z = +1 in normalised device coordinates.
    const QVector3D nearPoint = unproject(-1.0f);
    const QVector3D farPoint = unproject(1.0f);
    const QVector3D along = farPoint - nearPoint;

    if (along.isNull())
    {
      return false;
    }

    origin = nearPoint;
    direction = along.normalized();

    return true;
  }

  double Camera::nearPlane() const
  {
    // Scaled to the view rather than fixed: a constant near plane clips the
    // scene when the camera is close and wrecks depth precision when it is
    // far away, and both read as renderer bugs.
    return std::max(kMinDistance, m_distance * 1.0e-3);
  }

  double Camera::farPlane() const
  {
    return std::max(nearPlane() * 10.0, m_distance * 1.0e3);
  }

} // namespace HydroCouple::Composer
