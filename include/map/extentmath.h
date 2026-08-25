/*!
 * \file   extentmath.h
 * \author Caleb Buahin
 * \brief  Extent arithmetic that survives degenerate rectangles.
 *
 * `QRectF::united()` **discards** a rectangle it considers null, and a zero-
 * area rectangle is null — so uniting the bounds of point features one by one
 * leaves the extent of the last point rather than the extent of all of them.
 * The failure is silent and looks like a projection bug: the map frames one
 * feature and everything else is off-screen.
 *
 * Every union of feature or layer bounds goes through here for that reason.
 */

#ifndef HYDROCOUPLECOMPOSER_MAP_EXTENTMATH_H
#define HYDROCOUPLECOMPOSER_MAP_EXTENTMATH_H

#include <QPointF>
#include <QRectF>

#include <algorithm>

namespace HydroCouple::Composer
{

  /*!
   * \brief Grows \a bounds to include \a point.
   * \param bounds Rectangle to grow; empty and unset when \a valid is false.
   * \param valid Whether \a bounds holds anything yet; set to true here.
   * \param point The point to include.
   */
  inline void expandTo(QRectF &bounds, bool &valid, const QPointF &point)
  {
    if (!valid)
    {
      bounds = QRectF(point, QSizeF(0.0, 0.0));
      valid = true;

      return;
    }

    bounds.setLeft(std::min(bounds.left(), point.x()));
    bounds.setRight(std::max(bounds.right(), point.x()));
    bounds.setTop(std::min(bounds.top(), point.y()));
    bounds.setBottom(std::max(bounds.bottom(), point.y()));
  }

  /*!
   * \brief Grows \a bounds to include \a other, zero-area or not.
   * \param bounds Rectangle to grow.
   * \param valid Whether \a bounds holds anything yet; set to true here.
   * \param other The rectangle to include.
   */
  inline void expandTo(QRectF &bounds, bool &valid, const QRectF &other)
  {
    expandTo(bounds, valid, other.topLeft());
    expandTo(bounds, valid, other.bottomRight());
  }

  /*!
   * \brief Whether \a a and \a b overlap, zero-area rectangles included.
   *
   * `QRectF::intersects()` answers false whenever either rectangle has no
   * area, so a point feature's bounds never "intersects" the view and the
   * feature is silently never drawn.
   *
   * \param a First rectangle.
   * \param b Second rectangle.
   */
  inline bool overlaps(const QRectF &a, const QRectF &b)
  {
    return a.left() <= b.right() && a.right() >= b.left()
           && a.top() <= b.bottom() && a.bottom() >= b.top();
  }

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MAP_EXTENTMATH_H
