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
