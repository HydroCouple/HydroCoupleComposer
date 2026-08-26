#include "results/julianday.h"

#include "hydrocouplesdk/temporal/timedata.h"

#include <QTimeZone>

#include <cmath>

namespace HydroCouple::Composer
{
  QDateTime dateTimeFromJulianDay(double julianDay)
  {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    double second = 0.0;

    HydroCouple::SDK::Temporal::TimeData::julianDayToGregorian(
      julianDay, year, month, day, hour, minute, second);

    const QDate date(year, month, day);

    if (!date.isValid())
    {
      return {};
    }

    // Rounded rather than truncated: a step recorded on the hour arrives as
    // 59.9999 seconds of the previous minute often enough to matter, and a
    // plot axis labelled one minute early is wrong in a way that looks like
    // a modelling result.
    return QDateTime(date,
                     QTime(0, 0).addMSecs(static_cast<int>(
                       std::lround((hour * 3600.0 + minute * 60.0 + second)
                                   * 1000.0))),
                     QTimeZone::utc());
  }

} // namespace HydroCouple::Composer
