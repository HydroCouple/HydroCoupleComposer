#include "mesh/meshdomainmodel.h"

namespace HydroCouple::Composer
{
  namespace
  {
    //! Whether two domains describe the same ground, corner for corner.
    [[nodiscard]] bool sameDomain(const MeshDomain &left,
                                  const MeshDomain &right)
    {
      return left.boundary == right.boundary && left.holes == right.holes
             && left.constraintLines == right.constraintLines
             && left.points == right.points
             && qFuzzyCompare(left.maxEdgeLength + 1.0,
                              right.maxEdgeLength + 1.0);
    }
  }

  MeshDomainModel::MeshDomainModel(QObject *parent) : QObject(parent) {}

  MeshDomainModel::~MeshDomainModel() = default;

  const MeshDomain &MeshDomainModel::domain() const
  {
    return m_domain;
  }

  void MeshDomainModel::setDomain(const MeshDomain &domain)
  {
    if (sameDomain(m_domain, domain))
    {
      return;
    }

    m_domain = domain;

    Q_EMIT domainChanged();
  }

  void MeshDomainModel::setBoundary(const QPolygonF &ring)
  {
    if (m_domain.boundary == ring)
    {
      return;
    }

    m_domain.boundary = ring;

    Q_EMIT domainChanged();
  }

  void MeshDomainModel::addHole(const QPolygonF &ring)
  {
    if (ring.isEmpty())
    {
      return;
    }

    m_domain.holes.append(ring);

    Q_EMIT domainChanged();
  }

  void MeshDomainModel::addConstraintLine(const QPolygonF &line)
  {
    if (line.isEmpty())
    {
      return;
    }

    m_domain.constraintLines.append(line);

    Q_EMIT domainChanged();
  }

  void MeshDomainModel::addPoint(const QPointF &point)
  {
    m_domain.points.append(point);

    Q_EMIT domainChanged();
  }

  bool MeshDomainModel::isValid(QString &message) const
  {
    return m_domain.isValid(message);
  }

} // namespace HydroCouple::Composer
