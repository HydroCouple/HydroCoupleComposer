#include "results/transect.h"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief Where \a segment crosses \a ring, as parameters along it.
     *
     * Parameters rather than points: the whole slice is decided by ordering
     * along the line, and converting back and forth loses exactly the
     * ordering that matters where two cells meet.
     */
    void ringCrossings(const QLineF &segment, const QPolygonF &ring,
                       QVector<double> &parameters)
    {
      const int corners = ring.size();

      if (corners < 3)
      {
        return;
      }

      const double length = segment.length();

      if (length <= 0.0)
      {
        return;
      }

      for (int corner = 0; corner < corners; ++corner)
      {
        const QPointF &from = ring.at(corner);
        const QPointF &to = ring.at((corner + 1) % corners);

        if (from == to)
        {
          continue;
        }

        QPointF crossing;

        if (QLineF(from, to).intersects(segment, &crossing)
            != QLineF::BoundedIntersection)
        {
          continue;
        }

        // Projected back onto the segment rather than solved for again: the
        // intersection is already the answer, and the parameter is only
        // needed to sort by.
        const QPointF offset = crossing - segment.p1();
        const QPointF direction =
          (segment.p2() - segment.p1()) / length;

        parameters.append(
          (offset.x() * direction.x() + offset.y() * direction.y()) / length);
      }
    }
  }

  std::pair<double, double> TransectSection::elevationRange() const
  {
    double lowest = std::numeric_limits<double>::max();
    double highest = std::numeric_limits<double>::lowest();

    for (const TransectCell &cell : cells)
    {
      lowest = std::min({lowest, cell.bottomElevation, cell.topElevation});
      highest = std::max({highest, cell.bottomElevation, cell.topElevation});
    }

    return cells.isEmpty() ? std::pair<double, double>{0.0, 0.0}
                           : std::pair<double, double>{lowest, highest};
  }

  std::pair<double, double> TransectSection::valueRange() const
  {
    double lowest = std::numeric_limits<double>::max();
    double highest = std::numeric_limits<double>::lowest();

    for (const TransectCell &cell : cells)
    {
      if (!std::isfinite(cell.value))
      {
        continue;
      }

      lowest = std::min(lowest, cell.value);
      highest = std::max(highest, cell.value);
    }

    return lowest > highest ? std::pair<double, double>{0.0, 0.0}
                            : std::pair<double, double>{lowest, highest};
  }

  QVector<TransectSpan> spansAlongLine(const QVector<QPolygonF> &rings,
                                       const QPolygonF &line)
  {
    QVector<TransectSpan> spans;

    if (line.size() < 2 || rings.isEmpty())
    {
      return spans;
    }

    double travelled = 0.0;

    for (int vertex = 0; vertex + 1 < line.size(); ++vertex)
    {
      const QLineF segment(line.at(vertex), line.at(vertex + 1));
      const double length = segment.length();

      if (length <= 0.0)
      {
        continue;
      }

      // Every crossing on this segment, and which rings made them. The rings
      // that made one are also the only rings the segment can be inside
      // between two of them, which is what keeps this from testing every
      // cell of the mesh against every interval.
      QVector<double> boundaries{0.0, 1.0};
      QVector<int> candidates;

      for (int ring = 0; ring < rings.size(); ++ring)
      {
        const int before = boundaries.size();
        ringCrossings(segment, rings.at(ring), boundaries);

        if (boundaries.size() != before)
        {
          candidates.append(ring);
        }
      }

      std::sort(boundaries.begin(), boundaries.end());

      // Relative to the segment, so a section drawn across a metre-scale
      // mesh and one across a continent are decided the same way.
      constexpr double kParameterEpsilon = 1e-9;

      for (int step = 0; step + 1 < boundaries.size(); ++step)
      {
        const double from = std::clamp(boundaries.at(step), 0.0, 1.0);
        const double to = std::clamp(boundaries.at(step + 1), 0.0, 1.0);

        if (to - from <= kParameterEpsilon)
        {
          continue;
        }

        const QPointF middle = segment.pointAt((from + to) / 2.0);

        int found = -1;

        for (int ring : candidates)
        {
          if (rings.at(ring).containsPoint(middle, Qt::OddEvenFill))
          {
            found = ring;
            break;
          }
        }

        if (found < 0)
        {
          // A segment lying wholly inside one ring makes no crossings, so
          // the ring that holds it is not a candidate. Searched for rather
          // than skipped: that is a line drawn inside a single large cell,
          // which is a perfectly ordinary thing to draw.
          for (int ring = 0; ring < rings.size(); ++ring)
          {
            if (rings.at(ring).containsPoint(middle, Qt::OddEvenFill))
            {
              found = ring;
              break;
            }
          }
        }

        if (found < 0)
        {
          continue;
        }

        const double start = travelled + from * length;
        const double end = travelled + to * length;

        // Merged when the line stayed in the same ring across a crossing it
        // only touched, or bent inside one: two abutting spans of one cell
        // would draw a seam through the middle of it.
        if (!spans.isEmpty() && spans.back().ring == found
            && std::abs(spans.back().end - start)
                 <= kParameterEpsilon * length)
        {
          spans.back().end = end;
          continue;
        }

        spans.append(TransectSpan{found, start, end});
      }

      travelled += length;
    }

    return spans;
  }

} // namespace HydroCouple::Composer
