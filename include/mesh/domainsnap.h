/*!
 * \file   domainsnap.h
 * \author Caleb Buahin
 * \brief  Finding the domain vertex or edge under the pointer.
 *
 * Shared by the tool that draws a domain and the one that edits it, because
 * they ask the same question — what is within reach of where the user
 * clicked — and two answers to it would be two answers about the same map.
 *
 * The reach is a **screen** distance converted to ground, never a ground
 * distance chosen up front: ten pixels is what a hand can aim at, and a
 * tolerance fixed in metres is unusable at one zoom and grabs half the map at
 * another.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_DOMAINSNAP_H
#define HYDROCOUPLECOMPOSER_MESH_DOMAINSNAP_H

#include "mesh/meshdomain.h"

#include <QPointF>
#include <QVector>

namespace HydroCouple::Composer
{
  //! How close to a vertex or edge, in pixels, counts as being on it.
  inline constexpr double kSnapPixels = 10.0;

  /*!
   * \brief What was found near a point on the map.
   */
  struct DomainSnap
  {
      //! Whether anything was within reach at all.
      bool hit = false;

      //! Where the snap landed, in world coordinates.
      QPointF point;

      /*!
       * \brief What was hit.
       *
       * For a vertex, the vertex itself. For an edge, the address the vertex
       * inserted there would take — so the caller inserts at what it was
       * given rather than working out an off-by-one of its own.
       */
      DomainVertex at;
  };

  /*!
   * \brief \a pixels expressed in world units.
   * \param pixels A screen distance.
   * \param scale Pixels per world unit, as MapTransform reports.
   * \returns The distance in world units; 0 when \a scale is unusable.
   */
  [[nodiscard]] double worldTolerance(double pixels, double scale);

  /*!
   * \brief Every vertex in \a domain, with the address of each.
   *
   * One traversal, used both for snapping and for the handles the editor
   * draws, so the order those two speak in cannot drift apart.
   *
   * \param domain The domain to walk.
   * \param[out] addresses Where each returned point lives.
   * \returns The vertices, in the same order as \a addresses.
   */
  [[nodiscard]] QVector<QPointF> domainVertices(
    const MeshDomain &domain, QVector<DomainVertex> &addresses);

  /*!
   * \brief The domain vertex nearest \a world, if one is within \a tolerance.
   *
   * \param domain The domain to search.
   * \param world Where the pointer is, in world coordinates.
   * \param tolerance How far away still counts, in world units.
   * \param exclude A vertex to ignore — the one being dragged, which is
   *        always within reach of itself and would otherwise pin the drag to
   *        where it started.
   */
  [[nodiscard]] DomainSnap nearestVertex(const MeshDomain &domain,
                                         const QPointF &world,
                                         double tolerance,
                                         const DomainVertex &exclude = {});

  /*!
   * \brief The point on a domain edge nearest \a world, within \a tolerance.
   *
   * Rings are searched round their closing edge as well, since that edge is
   * drawn and a user who can see it can click it. Forced points have no
   * edges and are not searched.
   *
   * \param domain The domain to search.
   * \param world Where the pointer is, in world coordinates.
   * \param tolerance How far away still counts, in world units.
   */
  [[nodiscard]] DomainSnap nearestEdge(const MeshDomain &domain,
                                       const QPointF &world,
                                       double tolerance);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_DOMAINSNAP_H
