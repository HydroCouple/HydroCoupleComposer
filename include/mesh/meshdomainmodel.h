/*!
 * \file   meshdomainmodel.h
 * \author Caleb Buahin
 * \brief  MeshDomainModel — the one live copy of the ground being meshed.
 *
 * A domain is drawn on the map, and it will also be listed, imported into,
 * and eventually generated from. That is more than one view of one thing, so
 * the thing itself is an object with a signal rather than a struct each view
 * keeps a copy of: the alternative is four copies that disagree the moment
 * one of them is edited.
 *
 * The model owns no widgets and knows about no views. Views connect to
 * domainChanged() and re-read; nothing pushes to them, and nothing they do
 * reaches another view except through here.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_MESHDOMAINMODEL_H
#define HYDROCOUPLECOMPOSER_MESH_MESHDOMAINMODEL_H

#include "mesh/meshdomain.h"

#include <QObject>

namespace HydroCouple::Composer
{

  /*!
   * \brief The mesh domain being edited, and who to tell when it changes.
   */
  class MeshDomainModel : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs a model over an empty domain.
       * \param parent Parent object.
       */
      explicit MeshDomainModel(QObject *parent = nullptr);

      ~MeshDomainModel() override;

      //! \returns The domain as it stands.
      [[nodiscard]] const MeshDomain &domain() const;

      /*!
       * \brief Replaces the whole domain.
       *
       * How a domain arrives from a file, an import, or an undo. Announced
       * even when the new domain equals the old one only if it differs —
       * a view that redrew on every save of an unchanged domain would
       * repaint the map for nothing.
       *
       * \param domain The domain to hold.
       */
      void setDomain(const MeshDomain &domain);

      /*!
       * \brief Replaces the outer boundary.
       *
       * Replaces rather than appends, because there is exactly one: drawing
       * a second boundary is how you correct the first.
       *
       * \param ring The new boundary; the first point is not repeated.
       */
      void setBoundary(const QPolygonF &ring);

      /*!
       * \brief Adds a hole.
       *
       * Accepted even when it lies outside the boundary. The model holds
       * what was drawn and isValid() is the question you ask about it —
       * a model that silently dropped a badly-placed hole would leave the
       * user watching their gesture do nothing.
       *
       * \param ring The hole ring.
       */
      void addHole(const QPolygonF &ring);

      //! \brief Adds a breakline. \param line The polyline.
      void addConstraintLine(const QPolygonF &line);

      //! \brief Adds a forced interior point. \param point Where.
      void addPoint(const QPointF &point);

      /*!
       * \brief Moves one vertex.
       *
       * The whole of dragging: the editor writes each position through here
       * as the pointer moves, so what is on the map during a drag is the
       * domain itself rather than a preview of it that could disagree.
       *
       * \param at Which vertex.
       * \param to Where it goes, in world coordinates.
       * \returns False when \a at addresses nothing, or when the vertex is
       *          already there — an unchanged domain is not announced.
       */
      bool moveVertex(const DomainVertex &at, const QPointF &to);

      /*!
       * \brief Puts a new vertex at an address.
       *
       * \param at Where the new vertex goes; its index may be one past the
       *        last, which appends.
       * \param point The vertex, in world coordinates.
       * \returns False when \a at addresses no shape, or an index outside
       *          it.
       */
      bool insertVertex(const DomainVertex &at, const QPointF &point);

      /*!
       * \brief Takes a vertex out.
       *
       * A shape left below the count its part needs is **removed entirely**
       * rather than kept: two corners is not a hole, and leaving one behind
       * would put a shape in the domain that nothing downstream can use and
       * the user cannot see is broken. Refusing instead would leave the
       * gesture doing nothing with no way to say why.
       *
       * \param at Which vertex.
       * \returns False when \a at addresses nothing.
       */
      bool removeVertex(const DomainVertex &at);

      /*!
       * \brief Whether the domain can be meshed, and why not when it cannot.
       * \param[out] message What is wrong.
       */
      [[nodiscard]] bool isValid(QString &message) const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when the domain changes in any way.
       *
       * One signal, not one per part: a view of a domain redraws the whole
       * of what it shows, and a finer-grained set would be four ways to
       * forget to emit one.
       */
      void domainChanged();

    private:
      MeshDomain m_domain;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_MESHDOMAINMODEL_H
