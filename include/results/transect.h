/*!
 * \file   transect.h
 * \author Caleb Buahin
 * \brief  Transect — a section line cut through a layered mesh.
 *
 * A profile answers "what is under this face"; a section answers "what is
 * along this line", which is the question a reservoir or an estuary is
 * usually looked at with. Both read the same water column; they differ in
 * how many of them are on screen and what the horizontal axis means.
 *
 * The geometry is separated from the layer that owns the mesh, because
 * "which cells does this line cross, and where along it" is arithmetic with
 * known answers, and arithmetic that can only be run through a widget is
 * arithmetic nobody checks.
 */

#ifndef HYDROCOUPLECOMPOSER_RESULTS_TRANSECT_H
#define HYDROCOUPLECOMPOSER_RESULTS_TRANSECT_H

#include <QPolygonF>
#include <QVector>

#include <utility>

namespace HydroCouple::Composer
{

  /*!
   * \brief How far along a line one ring is crossed.
   */
  struct TransectSpan
  {
      //! Index into the rings that were sliced.
      int ring = -1;

      //! Where the line enters, as a distance from its start.
      double start = 0.0;

      //! Where the line leaves, as a distance from its start.
      double end = 0.0;
  };

  /*!
   * \brief Slices \a line against \a rings, in order along the line.
   *
   * Exact for any ring shape, convex or not, because a span is decided by
   * asking which ring contains the *middle* of each interval between
   * crossings rather than by pairing crossings up. A line that leaves a
   * concave cell and re-enters it therefore reports two spans, which is what
   * it did.
   *
   * A line running along the shared edge of two rings belongs to whichever
   * of them contains the interval midpoint; ties are not split, since half a
   * cell either side would draw a section of slivers.
   *
   * \param rings Closed rings in the same coordinate system as \a line.
   * \param line The section line; two points or many.
   * \returns One span per traversal, ordered by distance along the line.
   */
  [[nodiscard]] QVector<TransectSpan> spansAlongLine(
    const QVector<QPolygonF> &rings, const QPolygonF &line);

  /*!
   * \brief One cell of a section: a quad in distance and elevation.
   */
  struct TransectCell
  {
      //! Column of the horizontal mesh this cell belongs to.
      int column = -1;

      //! Layer within the column; 0 is the surface layer.
      int layer = -1;

      //! Where the line enters the column, from its start.
      double startDistance = 0.0;

      //! Where the line leaves the column, from its start.
      double endDistance = 0.0;

      //! The interface above the cell.
      double topElevation = 0.0;

      //! The interface below the cell.
      double bottomElevation = 0.0;

      //! The layered field's value in the cell.
      double value = 0.0;
  };

  /*!
   * \brief A section: the cells a line crosses, and the line's own length.
   */
  struct TransectSection
  {
      //! Every cell the line crosses, ordered along it, surface first.
      QVector<TransectCell> cells;

      /*!
       * \brief The section line's full length.
       *
       * Kept rather than derived from the cells, because a line may start or
       * end off the mesh: drawing the axis to the last cell instead would
       * silently trim the part of the section where there was nothing, which
       * is a result in its own right.
       */
      double length = 0.0;

      //! Whether the line crossed anything.
      [[nodiscard]] bool isEmpty() const { return cells.isEmpty(); }

      /*!
       * \brief Lowest and highest elevation across the cells.
       *
       * Derived rather than accumulated as the cells are built, so an axis
       * cannot end up describing coordinates the cells do not have.
       */
      [[nodiscard]] std::pair<double, double> elevationRange() const;

      //! \copybrief elevationRange
      [[nodiscard]] std::pair<double, double> valueRange() const;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RESULTS_TRANSECT_H
