#include "scene/scenegeometry.h"

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{

  void Bounds3D::expandTo(const QVector3D &point)
  {
    if (!m_valid)
    {
      m_min = point;
      m_max = point;
      m_valid = true;

      return;
    }

    m_min.setX(std::min(m_min.x(), point.x()));
    m_min.setY(std::min(m_min.y(), point.y()));
    m_min.setZ(std::min(m_min.z(), point.z()));

    m_max.setX(std::max(m_max.x(), point.x()));
    m_max.setY(std::max(m_max.y(), point.y()));
    m_max.setZ(std::max(m_max.z(), point.z()));
  }

  void Bounds3D::expandTo(const Bounds3D &other)
  {
    if (!other.m_valid)
    {
      return;
    }

    expandTo(other.m_min);
    expandTo(other.m_max);
  }

  bool Bounds3D::isValid() const
  {
    return m_valid;
  }

  QVector3D Bounds3D::minimum() const
  {
    return m_min;
  }

  QVector3D Bounds3D::maximum() const
  {
    return m_max;
  }

  QVector3D Bounds3D::center() const
  {
    return m_valid ? (m_min + m_max) * 0.5f : QVector3D();
  }

  double Bounds3D::diagonal() const
  {
    return m_valid ? double((m_max - m_min).length()) : 0.0;
  }

  QRectF Bounds3D::footprint() const
  {
    if (!m_valid)
    {
      return {};
    }

    return QRectF(QPointF(m_min.x(), m_min.y()),
                  QPointF(m_max.x(), m_max.y()))
      .normalized();
  }

  quint32 SceneGeometry::addVertex(const QVector3D &position,
                                   const QVector3D &normal,
                                   const QColor &color)
  {
    SceneVertex vertex;
    vertex.x = position.x();
    vertex.y = position.y();
    vertex.z = position.z();
    vertex.nx = normal.x();
    vertex.ny = normal.y();
    vertex.nz = normal.z();
    vertex.r = float(color.redF());
    vertex.g = float(color.greenF());
    vertex.b = float(color.blueF());
    vertex.a = float(color.alphaF());

    vertices.append(vertex);
    bounds.expandTo(position);

    return quint32(vertices.size() - 1);
  }

} // namespace HydroCouple::Composer
