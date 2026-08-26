#include "pick/terrainray.h"

#include "scene/scenesource.h"

#include <algorithm>
#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Steps a march may take before giving up.
    constexpr int kMaxSteps = 4096;

    /*!
     * \brief Samples a march takes along the ray, at least.
     *
     * The step has to be fine against *both* lengths in play. The terrain's
     * own spacing is the length over which its surface can turn, but a coarse
     * terrain gives a coarse step, and a ray crossing a whole scene in three
     * of them can enter the terrain's footprint already below the surface —
     * with nothing above it to bracket the crossing against.
     */
    constexpr int kMinSteps = 256;

    //! Halvings used to close in on the crossing once it is bracketed.
    constexpr int kBisections = 24;

    /*!
     * \brief How far the ray is above the terrain at \a travel along it.
     * \returns False where the terrain does not answer — off its edge.
     */
    bool clearanceAt(const QVector3D &origin, const QVector3D &direction,
                     const ITerrainSource &terrain, double travel,
                     double &above, QPointF &ground)
    {
      const QVector3D at = origin + direction * float(travel);

      ground = QPointF(at.x(), at.y());

      double surface = 0.0;

      if (!terrain.elevationAt(ground, surface))
      {
        return false;
      }

      above = double(at.z()) - surface;

      return true;
    }

  }

  bool rayGroundPoint(const QVector3D &origin, const QVector3D &direction,
                      const ITerrainSource *terrain, double reach,
                      QPointF &ground)
  {
    if (direction.isNull() || reach <= 0.0)
    {
      return false;
    }

    const QVector3D along = direction.normalized();

    if (!terrain)
    {
      // The ground plane, which is where the map's geometry lives. A ray
      // parallel to it never meets it, which is a miss like any other.
      if (qFuzzyIsNull(along.z()))
      {
        return false;
      }

      const double travel = -double(origin.z()) / double(along.z());

      if (travel < 0.0 || travel > reach)
      {
        return false;
      }

      const QVector3D at = origin + along * float(travel);
      ground = QPointF(at.x(), at.y());

      return true;
    }

    // Half the terrain's own sample spacing, and never coarser than the ray
    // itself needs — see kMinSteps.
    const double step =
      std::clamp(terrain->terrainResolution() * 0.5,
                 reach / double(kMaxSteps), reach / double(kMinSteps));

    double aboveTravel = 0.0;
    bool haveAbove = false;
    bool haveSample = false;

    for (int i = 0; i <= kMaxSteps; ++i)
    {
      const double travel = std::min(double(i) * step, reach);

      double clearance = 0.0;
      QPointF at;

      if (clearanceAt(origin, along, *terrain, travel, clearance, at))
      {
        if (clearance > 0.0)
        {
          aboveTravel = travel;
          haveAbove = true;
          haveSample = true;
        }
        else if (haveAbove)
        {
          // Bracketed. Bisect rather than accept the step's own resolution:
          // a pick tolerance is a few pixels, and a terrain cell is not.
          double low = aboveTravel;
          double high = travel;

          for (int halving = 0; halving < kBisections; ++halving)
          {
            const double middle = 0.5 * (low + high);

            double middleClearance = 0.0;
            QPointF middleGround;

            if (!clearanceAt(origin, along, *terrain, middle,
                             middleClearance, middleGround))
            {
              break;
            }

            if (middleClearance > 0.0)
            {
              low = middle;
            }
            else
            {
              high = middle;
              at = middleGround;
            }
          }

          ground = at;

          return true;
        }
        else if (haveSample || i == 0)
        {
          // Below the surface with nothing above it to bracket against, and
          // not because the ray has just arrived: the eye is underground, and
          // what is in front of it is not the ground.
          return false;
        }
        else
        {
          // The first sample inside the terrain's footprint is already below
          // it, so the ray crossed the surface at the terrain's own edge —
          // between this sample and one the terrain would not answer for.
          // That is where it landed, to within a step.
          ground = at;

          return true;
        }
      }

      if (travel >= reach)
      {
        break;
      }
    }

    return false;
  }

} // namespace HydroCouple::Composer
