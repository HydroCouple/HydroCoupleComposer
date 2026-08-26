/*!
 * \file   julianday.h
 * \author Caleb Buahin
 * \brief  One conversion from a recorded instant to a calendar one.
 *
 * The times a run records are Julian days; everything that shows them to a
 * person needs a calendar date. Two conversions written separately disagree
 * by a rounding, and a transport readout that names one instant while the
 * plot axis beside it labels another is a difference no one can act on —
 * so there is one, and it is this.
 *
 * It goes through the SDK's own conversion rather than Qt's, because the
 * times came from the SDK. `QDate::fromJulianDay` takes a whole day and
 * would drop the time of day entirely.
 */

#ifndef HYDROCOUPLECOMPOSER_RESULTS_JULIANDAY_H
#define HYDROCOUPLECOMPOSER_RESULTS_JULIANDAY_H

#include <QDateTime>

namespace HydroCouple::Composer
{
  /*!
   * \brief \a julianDay as a UTC calendar instant.
   *
   * UTC, not local: a run records instants, and re-labelling them in
   * whatever zone the viewer happens to sit in would move a model's output
   * by hours depending on who opened it.
   *
   * \param julianDay The instant, as a Julian day.
   * \returns The instant, to the second.
   */
  [[nodiscard]] QDateTime dateTimeFromJulianDay(double julianDay);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RESULTS_JULIANDAY_H
