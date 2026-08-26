/*!
 * \file   seriesexport.h
 * \author Caleb Buahin
 * \brief  Writing plotted series out as CSV or SWMM .dat.
 *
 * Free functions over plain data rather than methods on a panel, so what is
 * written can be checked without a chart, a selection or a window — the
 * formats are the deliverable here, and a test of them should not have to
 * assemble a GUI to read one.
 *
 * The two formats answer different questions. A CSV holds every series side
 * by side against one column of instants, which is what a spreadsheet wants;
 * a .dat holds one series and is what SWMM reads back, so several series
 * become several files rather than one file that SWMM would reject.
 */

#ifndef HYDROCOUPLECOMPOSER_RESULTS_SERIESEXPORT_H
#define HYDROCOUPLECOMPOSER_RESULTS_SERIESEXPORT_H

#include <QString>
#include <QStringList>
#include <QVector>

namespace HydroCouple::Composer
{
  /*!
   * \brief One named series of values against instants.
   */
  struct ExportSeries
  {
      QString name;                 //!< Legend name; becomes a column header.
      QVector<double> julianDays;   //!< The instants, oldest first.
      QVector<double> values;       //!< One value per instant.
  };

  /*!
   * \brief \a series as CSV text: one column of instants, one per series.
   *
   * The rows are the *union* of every series' instants, so two variables
   * recorded on different axes line up against one time column instead of
   * one being resampled onto the other's. A series with nothing at an
   * instant leaves its field empty rather than writing a zero — a gap in a
   * record is not a measurement of nothing.
   *
   * \param series The series to write.
   */
  [[nodiscard]] QString seriesToCsv(const QVector<ExportSeries> &series);

  /*!
   * \brief \a series as SWMM .dat text.
   *
   * The name goes in a leading comment, because the format has nowhere else
   * to put it and a file of bare numbers cannot say what it holds.
   *
   * \param series The one series to write.
   */
  [[nodiscard]] QString seriesToDat(const ExportSeries &series);

  /*!
   * \brief Writes \a series to \a path as CSV.
   * \param path Destination file.
   * \param series The series to write.
   * \param[out] message Diagnostic on failure.
   * \returns True when the file was written and closed cleanly.
   */
  [[nodiscard]] bool writeSeriesCsv(const QString &path,
                                    const QVector<ExportSeries> &series,
                                    QString &message);

  /*!
   * \brief Writes \a series as SWMM .dat files, one per series.
   *
   * One series goes to \a path itself. Several fan out beside it as
   * `<base>_<name>.dat`, because a .dat holds one series and writing them
   * into one file would produce something SWMM cannot read — silently, since
   * the extra columns simply look like the next reading.
   *
   * \param path Destination for the first (or only) file.
   * \param series The series to write.
   * \param[out] message Diagnostic on failure.
   * \returns The files written, or empty on failure.
   */
  [[nodiscard]] QStringList writeSeriesDat(const QString &path,
                                           const QVector<ExportSeries> &series,
                                           QString &message);

  /*!
   * \brief \a name reduced to characters a file name can carry.
   *
   * Exposed because the fan-out above names files after series, and a test
   * of where those files land needs the same answer the writer used.
   *
   * \param name The series name.
   */
  [[nodiscard]] QString sanitizedFileToken(const QString &name);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RESULTS_SERIESEXPORT_H
