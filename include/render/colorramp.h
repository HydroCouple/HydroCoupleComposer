/*!
 * \file   colorramp.h
 * \author Caleb Buahin
 * \brief  ColorRamp — a gradient from a normalized value to a color.
 *
 * One ramp type serves classification, continuous fills and legend swatches
 * alike, so a palette chosen once looks the same everywhere it appears.
 *
 * The built-ins are the perceptually-ordered palettes (Viridis and friends)
 * rather than a rainbow: a rainbow ramp reverses lightness order partway
 * along, which invents a boundary in the data that is not there and misleads
 * anyone reading the map.
 */

#ifndef HYDROCOUPLECOMPOSER_RENDER_COLORRAMP_H
#define HYDROCOUPLECOMPOSER_RENDER_COLORRAMP_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace HydroCouple::Composer
{

  /*!
   * \brief One color stop along a ramp.
   */
  struct ColorRampStop
  {
      double position = 0.0;  //!< Where the stop sits, in [0, 1].
      QColor color;           //!< The color at that position.
  };

  /*!
   * \brief Color space used between adjacent stops.
   */
  enum class RampInterpolation
  {
    Rgb,      //!< Linear in RGB; correct for published palettes.
    HsvShort  //!< Around the shorter hue arc; smoother for hand-picked stops.
  };

  /*!
   * \brief A gradient mapping [0, 1] to a color.
   */
  class ColorRamp
  {
    public:
      ColorRamp() = default;

      /*!
       * \brief Builds a ramp from ordered stops.
       * \param stops Stops, sorted on construction.
       * \param interpolation Color space to interpolate in.
       */
      explicit ColorRamp(QVector<ColorRampStop> stops,
                         RampInterpolation interpolation =
                           RampInterpolation::Rgb);

      /*!
       * \brief The color at \a position.
       * \param position Normalized position; clamped to [0, 1].
       * \returns The interpolated color, or an invalid color when empty.
       */
      [[nodiscard]] QColor colorAt(double position) const;

      /*!
       * \brief \a count colors spread evenly across the ramp.
       *
       * Samples class *centers*, not edges: sampling edges gives the first
       * and last class the extreme colors and squeezes everything between
       * them, so a five-class map looks like a three-class one.
       *
       * \param count How many colors to take.
       */
      [[nodiscard]] QVector<QColor> sample(int count) const;

      /*!
       * \brief The ramp's stops.
       */
      [[nodiscard]] const QVector<ColorRampStop> &stops() const;

      /*!
       * \brief The interpolation space.
       */
      [[nodiscard]] RampInterpolation interpolation() const;

      /*!
       * \brief Whether the ramp has any stops.
       */
      [[nodiscard]] bool isEmpty() const;

      /*!
       * \brief The ramp reversed end to end.
       */
      [[nodiscard]] ColorRamp reversed() const;

      /*!
       * \brief A named built-in ramp.
       * \param name One of builtinNames(); unknown names give Viridis.
       */
      [[nodiscard]] static ColorRamp builtin(const QString &name);

      /*!
       * \brief The names of the built-in ramps, in menu order.
       */
      [[nodiscard]] static QStringList builtinNames();

    private:
      QVector<ColorRampStop> m_stops;
      RampInterpolation m_interpolation = RampInterpolation::Rgb;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_RENDER_COLORRAMP_H
