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

  bool MeshDomainModel::isValid(QString &message) const
  {
    return m_domain.isValid(message);
  }

} // namespace HydroCouple::Composer
