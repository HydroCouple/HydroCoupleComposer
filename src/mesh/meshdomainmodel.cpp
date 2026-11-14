#include "mesh/meshdomainmodel.h"

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief The ring an address names, or nullptr.
     *
     * Forced points are not rings and are handled by each caller: a point is
     * a shape of one vertex, and folding it in here would mean pretending
     * the point list is a ring, which is exactly the kind of "nearly the
     * same" that puts a hole's vertex into the boundary.
     */
    [[nodiscard]] QPolygonF *ringFor(MeshDomain &domain,
                                     const DomainVertex &at)
    {
      switch (at.part)
      {
        case DomainPart::Boundary:
          return at.shape == 0 ? &domain.boundary : nullptr;

        case DomainPart::Holes:
          return at.shape >= 0 && at.shape < domain.holes.size()
                   ? &domain.holes[at.shape]
                   : nullptr;

        case DomainPart::Breaklines:
          return at.shape >= 0 && at.shape < domain.constraintLines.size()
                   ? &domain.constraintLines[at.shape]
                   : nullptr;

        case DomainPart::ForcedPoints:
          return nullptr;
      }

      return nullptr;
    }

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

  bool MeshDomainModel::moveVertex(const DomainVertex &at, const QPointF &to)
  {
    if (!at.isValid())
    {
      return false;
    }

    if (at.part == DomainPart::ForcedPoints)
    {
      if (at.vertex != 0 || at.shape >= m_domain.points.size()
          || m_domain.points.at(at.shape) == to)
      {
        return false;
      }

      m_domain.points[at.shape] = to;

      Q_EMIT domainChanged();

      return true;
    }

    QPolygonF *ring = ringFor(m_domain, at);

    if (!ring || at.vertex >= ring->size() || ring->at(at.vertex) == to)
    {
      return false;
    }

    (*ring)[at.vertex] = to;

    Q_EMIT domainChanged();

    return true;
  }

  bool MeshDomainModel::insertVertex(const DomainVertex &at,
                                     const QPointF &point)
  {
    if (!at.isValid())
    {
      return false;
    }

    if (at.part == DomainPart::ForcedPoints)
    {
      if (at.vertex != 0 || at.shape > m_domain.points.size())
      {
        return false;
      }

      m_domain.points.insert(at.shape, point);

      Q_EMIT domainChanged();

      return true;
    }

    QPolygonF *ring = ringFor(m_domain, at);

    // One past the last is an append, which is what the closing edge of a
    // ring asks for.
    if (!ring || at.vertex > ring->size())
    {
      return false;
    }

    ring->insert(at.vertex, point);

    Q_EMIT domainChanged();

    return true;
  }

  bool MeshDomainModel::removeVertex(const DomainVertex &at)
  {
    if (!at.isValid())
    {
      return false;
    }

    if (at.part == DomainPart::ForcedPoints)
    {
      if (at.vertex != 0 || at.shape >= m_domain.points.size())
      {
        return false;
      }

      m_domain.points.removeAt(at.shape);

      Q_EMIT domainChanged();

      return true;
    }

    QPolygonF *ring = ringFor(m_domain, at);

    if (!ring || at.vertex >= ring->size())
    {
      return false;
    }

    if (ring->size() - 1 < minimumVertices(at.part))
    {
      // What is left would not be the thing it claims to be, so the shape
      // goes rather than a two-cornered hole staying in the domain.
      switch (at.part)
      {
        case DomainPart::Boundary:
          m_domain.boundary.clear();
          break;

        case DomainPart::Holes:
          m_domain.holes.removeAt(at.shape);
          break;

        case DomainPart::Breaklines:
          m_domain.constraintLines.removeAt(at.shape);
          break;

        case DomainPart::ForcedPoints:
          break;
      }
    }
    else
    {
      ring->removeAt(at.vertex);
    }

    Q_EMIT domainChanged();

    return true;
  }

  bool MeshDomainModel::isValid(QString &message) const
  {
    return m_domain.isValid(message);
  }

} // namespace HydroCouple::Composer
