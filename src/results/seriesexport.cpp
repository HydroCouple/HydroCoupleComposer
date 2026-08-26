#include "results/seriesexport.h"

#include "results/julianday.h"

#include <QObject>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace HydroCouple::Composer
{
  namespace
  {
    //! RFC 4180 quoting, and only when the field needs it.
    QString csvQuoted(const QString &field)
    {
      if (!field.contains(QLatin1Char(','))
          && !field.contains(QLatin1Char('"'))
          && !field.contains(QLatin1Char('\n')))
      {
        return field;
      }

      QString escaped = field;
      escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));

      return QLatin1Char('"') + escaped + QLatin1Char('"');
    }

    /*!
     * \brief A value at full precision.
     *
     * Twelve significant digits, because an export is what someone reads a
     * run back out with: rounding it to look tidy would make the file
     * disagree with the run it came from.
     */
    QString formatValue(double value)
    {
      return QString::number(value, 'g', 12);
    }

    /*!
     * \brief Writes \a text to \a path.
     *
     * The stream is flushed and its status checked before the file is
     * called written: a disk that filled halfway through otherwise leaves a
     * truncated export that opens perfectly well and is missing its tail.
     */
    bool writeTextFile(const QString &path, const QString &text,
                       QString &message)
    {
      QFile file(path);

      if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
        message = QObject::tr("Could not open %1 for writing: %2")
                    .arg(path, file.errorString());
        return false;
      }

      QTextStream out(&file);
      out << text;
      out.flush();

      if (out.status() != QTextStream::Ok || !file.flush())
      {
        message = QObject::tr("Could not write %1: %2")
                    .arg(path, file.errorString());
        return false;
      }

      message.clear();
      return true;
    }
  } // namespace

  QString seriesToCsv(const QVector<ExportSeries> &series)
  {
    // Keyed on the epoch millisecond rather than on the Julian day: two
    // series naming the same instant almost never name it with the same
    // double, and a map keyed on the raw value would give them a row each.
    std::map<qint64, QVector<double>> rows;

    const double missing = std::numeric_limits<double>::quiet_NaN();

    for (int index = 0; index < series.size(); ++index)
    {
      const ExportSeries &one = series.at(index);
      const int count = std::min(one.julianDays.size(), one.values.size());

      for (int level = 0; level < count; ++level)
      {
        const QDateTime at = dateTimeFromJulianDay(one.julianDays.at(level));

        if (!at.isValid())
        {
          continue;
        }

        auto row = rows.find(at.toMSecsSinceEpoch());

        if (row == rows.end())
        {
          row = rows.emplace(at.toMSecsSinceEpoch(),
                             QVector<double>(series.size(), missing))
                  .first;
        }

        row->second[index] = one.values.at(level);
      }
    }

    QString text;
    QTextStream out(&text);

    out << "Date/Time";

    for (const ExportSeries &one : series)
    {
      out << ',' << csvQuoted(one.name);
    }

    out << '\n';

    for (const auto &[msecs, values] : rows)
    {
      out << QDateTime::fromMSecsSinceEpoch(msecs, QTimeZone::utc())
               .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

      for (double value : values)
      {
        out << ',';

        // Left empty when the series has nothing at this instant. A zero
        // would read as a measurement, and this is the absence of one.
        if (std::isfinite(value))
        {
          out << formatValue(value);
        }
      }

      out << '\n';
    }

    return text;
  }

  QString seriesToDat(const ExportSeries &series)
  {
    QString text;
    QTextStream out(&text);

    out << ';' << series.name << '\n';

    const int count = std::min(series.julianDays.size(), series.values.size());

    for (int level = 0; level < count; ++level)
    {
      if (!std::isfinite(series.values.at(level)))
      {
        continue;
      }

      const QDateTime at = dateTimeFromJulianDay(series.julianDays.at(level));

      if (!at.isValid())
      {
        continue;
      }

      out << at.toString(QStringLiteral("MM/dd/yyyy HH:mm:ss")) << ' '
          << formatValue(series.values.at(level)) << '\n';
    }

    return text;
  }

  bool writeSeriesCsv(const QString &path, const QVector<ExportSeries> &series,
                      QString &message)
  {
    if (series.isEmpty())
    {
      message = QObject::tr("There is no series to export.");
      return false;
    }

    return writeTextFile(path, seriesToCsv(series), message);
  }

  QStringList writeSeriesDat(const QString &path,
                             const QVector<ExportSeries> &series,
                             QString &message)
  {
    if (series.isEmpty())
    {
      message = QObject::tr("There is no series to export.");
      return {};
    }

    if (series.size() == 1)
    {
      return writeTextFile(path, seriesToDat(series.first()), message)
               ? QStringList{path}
               : QStringList{};
    }

    const QFileInfo target(path);
    QStringList written;

    for (const ExportSeries &one : series)
    {
      const QString each = QStringLiteral("%1/%2_%3.dat")
                             .arg(target.absolutePath(),
                                  target.completeBaseName(),
                                  sanitizedFileToken(one.name));

      if (!writeTextFile(each, seriesToDat(one), message))
      {
        return {};
      }

      written.append(each);
    }

    message.clear();
    return written;
  }

  QString sanitizedFileToken(const QString &name)
  {
    static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9_-]+"));

    QString token = name;
    token.replace(unsafe, QStringLiteral("_"));

    // A name of nothing but punctuation would otherwise collapse to an empty
    // token and two series would fan out onto one file, the second silently
    // overwriting the first.
    return token.isEmpty() ? QStringLiteral("series") : token;
  }

} // namespace HydroCouple::Composer
