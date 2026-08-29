/*!
 * \file   meshdomain.h
 * \author Caleb Buahin
 * \brief  MeshDomain — the ground a mesh will be generated over.
 *
 * What the user draws before there is a mesh: an outer boundary, holes cut
 * out of it, breaklines the triangulation must follow, and points that force
 * a vertex where one is wanted. It is authoring input, not a result — the
 * mesh is what comes out of it, and the domain is what you edit when the
 * mesh comes out wrong.
 *
 * The shape is the SDK's `TriangulationInput` rather than one of Composer's
 * own, because that is what E2 will hand to the triangulator and a private
 * model would only have to be translated. Two things are deliberately
 * different:
 *
 *   - **Constraints are polylines here, segments there.** A breakline is
 *     drawn and edited as one line with many vertices; the triangulator
 *     consumes pairs. Storing pairs would mean the editor could not tell a
 *     single line from several that happen to touch.
 *   - **Rings are `QPolygonF`**, which is what the map draws and what
 *     `FeatureLayer` already carries, so the editor in E1b needs no
 *     conversion to show what it is editing.
 *
 * Self-intersection is *not* checked. A ring that crosses itself is a real
 * error, but detecting it costs a sweep and the triangulator already refuses
 * one with a diagnostic — so this validates what would otherwise pass
 * silently, and leaves what already fails loudly to fail loudly.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_MESHDOMAIN_H
#define HYDROCOUPLECOMPOSER_MESH_MESHDOMAIN_H

#include "hydrocouplesdk/tools/triangulator.h"

#include <QJsonObject>
#include <QPolygonF>
#include <QString>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief A boundary, its holes, its breaklines and its forced points.
   */
  struct MeshDomain
  {
      /*!
       * \brief Outer boundary ring; the first point is not repeated.
       *
       * Stored in whatever orientation it was drawn. The triangulator wants
       * counter-clockwise, and toTriangulationInput() is where that is
       * imposed — a boundary drawn clockwise is not an error, it is a
       * boundary drawn clockwise.
       */
      QPolygonF boundary;

      //! Rings cut out of the boundary; orientation is immaterial.
      QVector<QPolygonF> holes;

      //! Breaklines the triangulation must conform to, as polylines.
      QVector<QPolygonF> constraintLines;

      //! Points that force a mesh vertex where one is wanted.
      QVector<QPointF> points;

      /*!
       * \brief Longest edge to leave unsplit; 0 leaves edges alone.
       *
       * The practical refinement control: boundary, hole and constraint
       * edges longer than this are densified before triangulation.
       */
      double maxEdgeLength = 0.0;

      //! Whether anything has been drawn at all.
      [[nodiscard]] bool isEmpty() const;

      /*!
       * \brief Whether this domain can be meshed, and why not when it cannot.
       *
       * Checked here rather than at generation time because the answer is
       * something the editor can show while the user is still drawing, and
       * because the failures that matter are the quiet ones: a ring of three
       * collinear points has a perfectly good bounding box, a positive
       * vertex count and no area at all.
       *
       * \param[out] message What is wrong, naming the offending ring.
       */
      [[nodiscard]] bool isValid(QString &message) const;

      /*!
       * \brief This domain as the SDK triangulator's input.
       *
       * The boundary is oriented counter-clockwise and constraint polylines
       * are exploded into the segment pairs the triangulator takes. Invalid
       * domains convert too — validity is the caller's question to ask, and
       * a converter that silently returned an empty domain would turn a
       * refusal into an empty mesh.
       */
      [[nodiscard]] HydroCouple::SDK::Tools::TriangulationInput
      toTriangulationInput() const;

      //! Serialises to the sidecar's mesh-domain object.
      [[nodiscard]] QJsonObject toJson() const;

      /*!
       * \brief Parses a sidecar mesh-domain object.
       * \param json The object written by toJson().
       * \param[out] domain Receives the parsed domain.
       * \param[out] message Diagnostic on failure.
       * \returns False when the object is not a mesh domain; \a domain is
       *          then left empty rather than half-filled.
       */
      [[nodiscard]] static bool fromJson(const QJsonObject &json,
                                         MeshDomain &domain, QString &message);
  };

  /*!
   * \brief Twice the signed area of \a ring; positive is counter-clockwise.
   *
   * Exposed because orientation and degeneracy are the same computation, and
   * because a test that checks "the boundary came out counter-clockwise"
   * should be able to say so in the same terms the code does.
   *
   * \param ring A ring whose first point is not repeated.
   */
  [[nodiscard]] double signedDoubleArea(const QPolygonF &ring);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_MESHDOMAIN_H
