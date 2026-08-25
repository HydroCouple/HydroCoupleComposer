#include "render/colorramp.h"

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    double clamp01(double value)
    {
      return std::clamp(value, 0.0, 1.0);
    }

    QColor mixRgb(const QColor &from, const QColor &to, double t)
    {
      return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                              from.greenF() + (to.greenF() - from.greenF()) * t,
                              from.blueF() + (to.blueF() - from.blueF()) * t,
                              from.alphaF() + (to.alphaF() - from.alphaF()) * t);
    }

    QColor mixHsvShort(const QColor &from, const QColor &to, double t)
    {
      // A grey has no meaningful hue, and interpolating towards its reported
      // hue of -1 would swing the colour through the whole wheel.
      const double fromHue = from.hueF() < 0.0 ? to.hueF() : from.hueF();
      const double toHue = to.hueF() < 0.0 ? fromHue : to.hueF();

      double delta = toHue - fromHue;

      // Take the shorter way round: red to blue via magenta, not via green.
      if (delta > 0.5)
      {
        delta -= 1.0;
      }
      else if (delta < -0.5)
      {
        delta += 1.0;
      }

      double hue = fromHue + delta * t;

      if (hue < 0.0)
      {
        hue += 1.0;
      }
      else if (hue > 1.0)
      {
        hue -= 1.0;
      }

      return QColor::fromHsvF(
        clamp01(hue),
        clamp01(from.saturationF()
                + (to.saturationF() - from.saturationF()) * t),
        clamp01(from.valueF() + (to.valueF() - from.valueF()) * t),
        clamp01(from.alphaF() + (to.alphaF() - from.alphaF()) * t));
    }

    ColorRamp fromHexStops(const QStringList &hexes)
    {
      QVector<ColorRampStop> stops;
      stops.reserve(hexes.size());

      const int last = hexes.size() - 1;

      for (int i = 0; i <= last; ++i)
      {
        stops.append({last > 0 ? static_cast<double>(i) / last : 0.0,
                      QColor(hexes.at(i))});
      }

      return ColorRamp(stops);
    }
  }

  ColorRamp::ColorRamp(QVector<ColorRampStop> stops,
                       RampInterpolation interpolation)
    : m_stops(std::move(stops)), m_interpolation(interpolation)
  {
    // Sorted once here so every lookup can assume order; an unsorted ramp
    // would otherwise return colours from the wrong segment.
    std::sort(m_stops.begin(), m_stops.end(),
              [](const ColorRampStop &a, const ColorRampStop &b)
              { return a.position < b.position; });
  }

  QColor ColorRamp::colorAt(double position) const
  {
    if (m_stops.isEmpty())
    {
      return {};
    }

    const double t = clamp01(position);

    if (t <= m_stops.first().position)
    {
      return m_stops.first().color;
    }

    if (t >= m_stops.last().position)
    {
      return m_stops.last().color;
    }

    for (int i = 1; i < m_stops.size(); ++i)
    {
      const ColorRampStop &upper = m_stops.at(i);

      if (t > upper.position)
      {
        continue;
      }

      const ColorRampStop &lower = m_stops.at(i - 1);
      const double span = upper.position - lower.position;

      // Coincident stops are a hard colour break, which is a legitimate way
      // to author a ramp; dividing by the zero span is not.
      if (span <= 0.0)
      {
        return upper.color;
      }

      const double local = (t - lower.position) / span;

      return m_interpolation == RampInterpolation::HsvShort
               ? mixHsvShort(lower.color, upper.color, local)
               : mixRgb(lower.color, upper.color, local);
    }

    return m_stops.last().color;
  }

  QVector<QColor> ColorRamp::sample(int count) const
  {
    QVector<QColor> colors;

    if (count <= 0)
    {
      return colors;
    }

    colors.reserve(count);

    if (count == 1)
    {
      colors.append(colorAt(0.5));
      return colors;
    }

    for (int i = 0; i < count; ++i)
    {
      // Class centres: (i + 0.5) / count spreads the samples across the ramp
      // without pinning the outer classes to the extreme colours.
      colors.append(colorAt((i + 0.5) / count));
    }

    return colors;
  }

  const QVector<ColorRampStop> &ColorRamp::stops() const
  {
    return m_stops;
  }

  RampInterpolation ColorRamp::interpolation() const
  {
    return m_interpolation;
  }

  bool ColorRamp::isEmpty() const
  {
    return m_stops.isEmpty();
  }

  ColorRamp ColorRamp::reversed() const
  {
    QVector<ColorRampStop> flipped;
    flipped.reserve(m_stops.size());

    for (const ColorRampStop &stop : m_stops)
    {
      flipped.append({1.0 - stop.position, stop.color});
    }

    return ColorRamp(flipped, m_interpolation);
  }

  ColorRamp ColorRamp::builtin(const QString &name)
  {
    if (name == QLatin1String("Plasma"))
    {
      return fromHexStops({QStringLiteral("#0d0887"), QStringLiteral("#6a00a8"),
                           QStringLiteral("#b12a90"), QStringLiteral("#e16462"),
                           QStringLiteral("#fca636"),
                           QStringLiteral("#f0f921")});
    }

    if (name == QLatin1String("Blues"))
    {
      return fromHexStops({QStringLiteral("#f7fbff"), QStringLiteral("#c6dbef"),
                           QStringLiteral("#6baed6"), QStringLiteral("#2171b5"),
                           QStringLiteral("#08306b")});
    }

    if (name == QLatin1String("Reds"))
    {
      return fromHexStops({QStringLiteral("#fff5f0"), QStringLiteral("#fcbba1"),
                           QStringLiteral("#fb6a4a"), QStringLiteral("#cb181d"),
                           QStringLiteral("#67000d")});
    }

    if (name == QLatin1String("Red-Yellow-Blue"))
    {
      return fromHexStops({QStringLiteral("#d73027"), QStringLiteral("#fc8d59"),
                           QStringLiteral("#fee090"), QStringLiteral("#e0f3f8"),
                           QStringLiteral("#91bfdb"),
                           QStringLiteral("#4575b4")});
    }

    if (name == QLatin1String("Greyscale"))
    {
      return fromHexStops(
        {QStringLiteral("#000000"), QStringLiteral("#ffffff")});
    }

    // Viridis is the default because it is the safe answer: monotonic in
    // lightness, and legible in greyscale and to colour-blind readers.
    return fromHexStops({QStringLiteral("#440154"), QStringLiteral("#414487"),
                         QStringLiteral("#2a788e"), QStringLiteral("#22a884"),
                         QStringLiteral("#7ad151"), QStringLiteral("#fde725")});
  }

  QStringList ColorRamp::builtinNames()
  {
    return {QStringLiteral("Viridis"), QStringLiteral("Plasma"),
            QStringLiteral("Blues"), QStringLiteral("Reds"),
            QStringLiteral("Red-Yellow-Blue"), QStringLiteral("Greyscale")};
  }

} // namespace HydroCouple::Composer
