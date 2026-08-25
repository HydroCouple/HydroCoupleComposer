#include "render/classification.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Jenks is O(k·n²); above this the values are strided down to it.
    constexpr int kNaturalBreaksSampleCap = 512;

    QVector<double> sortedFinite(const QVector<double> &values)
    {
      QVector<double> clean;
      clean.reserve(values.size());

      for (const double value : values)
      {
        // A NaN sorts unpredictably and would poison every comparison that
        // follows, so it is dropped here rather than defended against later.
        if (std::isfinite(value))
        {
          clean.append(value);
        }
      }

      std::sort(clean.begin(), clean.end());

      return clean;
    }

    QVector<double> equalIntervalEdges(const QVector<double> &sorted,
                                       int classCount)
    {
      QVector<double> edges;

      const double minimum = sorted.first();
      const double maximum = sorted.last();
      const double step = (maximum - minimum) / classCount;

      edges.reserve(classCount + 1);

      for (int i = 0; i <= classCount; ++i)
      {
        edges.append(minimum + step * i);
      }

      return edges;
    }

    QVector<double> quantileEdges(const QVector<double> &sorted,
                                  int classCount)
    {
      QVector<double> edges;
      edges.reserve(classCount + 1);
      edges.append(sorted.first());

      for (int i = 1; i < classCount; ++i)
      {
        // Linear interpolation between the two straddling samples, so the
        // edge does not jump as one feature is added or removed.
        const double position =
          static_cast<double>(i) * (sorted.size() - 1) / classCount;
        const int lower = static_cast<int>(std::floor(position));
        const int upper =
          std::min(lower + 1, static_cast<int>(sorted.size()) - 1);
        const double fraction = position - lower;

        edges.append(sorted.at(lower)
                     + (sorted.at(upper) - sorted.at(lower)) * fraction);
      }

      edges.append(sorted.last());

      return edges;
    }

    /*!
     * \brief Fisher-Jenks natural breaks over a sorted sample.
     *
     * Minimises the sum of squared deviations within classes, which is what
     * "the gaps the data already has" means numerically.
     */
    QVector<double> naturalBreaksEdges(const QVector<double> &sorted,
                                       int classCount)
    {
      QVector<double> sample;

      if (sorted.size() > kNaturalBreaksSampleCap)
      {
        // A deterministic stride rather than a random sample: the same layer
        // must classify the same way every time it is opened.
        sample.reserve(kNaturalBreaksSampleCap);

        for (int i = 0; i < kNaturalBreaksSampleCap; ++i)
        {
          const int index = static_cast<int>(
            static_cast<double>(i) * (sorted.size() - 1)
            / (kNaturalBreaksSampleCap - 1));
          sample.append(sorted.at(index));
        }
      }
      else
      {
        sample = sorted;
      }

      const int n = sample.size();
      const int k = classCount;

      // (n+1) x (k+1), one-based to match the classic formulation.
      QVector<int> limits((n + 1) * (k + 1), 1);
      QVector<double> variances((n + 1) * (k + 1),
                                std::numeric_limits<double>::max());

      const auto at = [k](int row, int column) { return row * (k + 1) + column; };

      for (int column = 1; column <= k; ++column)
      {
        variances[at(1, column)] = 0.0;
      }

      for (int row = 2; row <= n; ++row)
      {
        double sum = 0.0;
        double sumSquares = 0.0;
        double count = 0.0;
        double variance = 0.0;

        for (int m = 1; m <= row; ++m)
        {
          const int lower = row - m + 1;
          const double value = sample.at(lower - 1);

          count += 1.0;
          sum += value;
          sumSquares += value * value;
          variance = sumSquares - (sum * sum) / count;

          const int previous = lower - 1;

          if (previous == 0)
          {
            continue;
          }

          for (int column = 2; column <= k; ++column)
          {
            const double candidate =
              variance + variances.at(at(previous, column - 1));

            if (variances.at(at(row, column)) >= candidate)
            {
              limits[at(row, column)] = lower;
              variances[at(row, column)] = candidate;
            }
          }
        }

        limits[at(row, 1)] = 1;
        variances[at(row, 1)] = variance;
      }

      QVector<double> edges(k + 1, sample.last());
      edges[0] = sample.first();

      int row = n;

      for (int column = k; column >= 2; --column)
      {
        const int index = limits.at(at(row, column)) - 1;
        edges[column - 1] = sample.at(index);
        row = limits.at(at(row, column)) - 1;
      }

      return edges;
    }
  }

  Classification::Classification()
    : m_ramp(ColorRamp::builtin(QStringLiteral("Viridis")))
  {
  }

  ClassificationMethod Classification::method() const
  {
    return m_method;
  }

  void Classification::setMethod(ClassificationMethod method)
  {
    m_method = method;
  }

  int Classification::classCount() const
  {
    return m_classCount;
  }

  void Classification::setClassCount(int count)
  {
    // Above two dozen classes no reader can tell one colour from its
    // neighbour, and the legend is longer than the map.
    m_classCount = std::clamp(count, 1, 24);
  }

  const ColorRamp &Classification::ramp() const
  {
    return m_ramp;
  }

  void Classification::setRamp(const ColorRamp &ramp)
  {
    m_ramp = ramp;
    applyRampColors();
  }

  int Classification::labelPrecision() const
  {
    return m_labelPrecision;
  }

  void Classification::setLabelPrecision(int precision)
  {
    m_labelPrecision = std::clamp(precision, 0, 12);
  }

  bool Classification::classify(const QVector<double> &values)
  {
    // Manual boundaries are the user's, and recomputing them from data would
    // silently discard a deliberate choice.
    if (m_method == ClassificationMethod::Manual)
    {
      return !m_breaks.isEmpty();
    }

    const QVector<double> sorted = sortedFinite(values);

    if (sorted.isEmpty())
    {
      m_breaks.clear();
      return false;
    }

    // Every value identical: one class, which is the honest answer. Splitting
    // it would produce classes no feature can fall into.
    if (qFuzzyCompare(sorted.first() + 1.0, sorted.last() + 1.0))
    {
      buildFrom({sorted.first(), sorted.last()});
      return true;
    }

    // Fewer distinct values than classes means empty classes, which are
    // legend entries that match nothing.
    QVector<double> distinct = sorted;
    distinct.erase(std::unique(distinct.begin(), distinct.end()),
                   distinct.end());

    const int count =
      std::max(1, std::min(m_classCount, static_cast<int>(distinct.size())));

    switch (m_method)
    {
      case ClassificationMethod::Quantile:
        buildFrom(quantileEdges(sorted, count));
        break;

      case ClassificationMethod::NaturalBreaks:
        buildFrom(naturalBreaksEdges(sorted, count));
        break;

      case ClassificationMethod::EqualInterval:
      case ClassificationMethod::Manual:
      default:
        buildFrom(equalIntervalEdges(sorted, count));
        break;
    }

    return !m_breaks.isEmpty();
  }

  bool Classification::setManualBreaks(const QVector<double> &boundaries)
  {
    QVector<double> edges = sortedFinite(boundaries);
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

    if (edges.size() < 2)
    {
      return false;
    }

    m_method = ClassificationMethod::Manual;
    buildFrom(edges);

    return true;
  }

  const QVector<ClassBreak> &Classification::breaks() const
  {
    return m_breaks;
  }

  int Classification::indexFor(double value) const
  {
    if (m_breaks.isEmpty() || !std::isfinite(value))
    {
      return -1;
    }

    for (int i = 0; i < m_breaks.size(); ++i)
    {
      const ClassBreak &candidate = m_breaks.at(i);

      // The last class owns its upper bound, so the largest feature in the
      // layer is drawn rather than dropped.
      const bool inside = i == m_breaks.size() - 1
                            ? value >= candidate.lower && value <= candidate.upper
                            : value >= candidate.lower && value < candidate.upper;

      if (inside)
      {
        return i;
      }
    }

    return -1;
  }

  void Classification::setClassVisible(int index, bool visible)
  {
    if (index >= 0 && index < m_breaks.size())
    {
      m_breaks[index].visible = visible;
    }
  }

  void Classification::setClassColor(int index, const QColor &color)
  {
    if (index >= 0 && index < m_breaks.size() && color.isValid())
    {
      m_breaks[index].color = color;
    }
  }

  bool Classification::isEmpty() const
  {
    return m_breaks.isEmpty();
  }

  QString Classification::methodName(ClassificationMethod method)
  {
    switch (method)
    {
      case ClassificationMethod::Quantile:
        return QStringLiteral("Quantile");
      case ClassificationMethod::NaturalBreaks:
        return QStringLiteral("Natural Breaks");
      case ClassificationMethod::Manual:
        return QStringLiteral("Manual");
      case ClassificationMethod::EqualInterval:
        break;
    }

    return QStringLiteral("Equal Interval");
  }

  void Classification::buildFrom(const QVector<double> &boundaries)
  {
    m_breaks.clear();

    if (boundaries.size() < 2)
    {
      return;
    }

    for (int i = 0; i + 1 < boundaries.size(); ++i)
    {
      ClassBreak entry;
      entry.lower = boundaries.at(i);
      entry.upper = boundaries.at(i + 1);
      entry.label = QStringLiteral("%1 – %2")
                      .arg(entry.lower, 0, 'f', m_labelPrecision)
                      .arg(entry.upper, 0, 'f', m_labelPrecision);

      m_breaks.append(entry);
    }

    applyRampColors();
  }

  void Classification::applyRampColors()
  {
    const QVector<QColor> colors = m_ramp.sample(m_breaks.size());

    for (int i = 0; i < m_breaks.size() && i < colors.size(); ++i)
    {
      m_breaks[i].color = colors.at(i);
    }
  }

} // namespace HydroCouple::Composer
