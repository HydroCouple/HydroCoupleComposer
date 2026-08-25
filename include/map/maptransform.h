/*!
 * \file   maptransform.h
 * \author Caleb Buahin
 * \brief  MapTransform — the mapping between world coordinates and pixels.
 *
 * Every layer draws in world coordinates and every mouse event arrives in
 * pixels, so exactly one object owns the conversion. Keeping it separate from
 * the canvas makes the arithmetic testable on its own — pan, zoom and fit are
 * where map bugs hide, and they are far easier to pin down as equations than
 * as rendered pixels.
 *
 * The transform is uniform (one scale for both axes) and Y-flipped: world Y
 * increases northward while widget Y increases downward. A non-uniform scale
 * would stretch geometry and make a circle an ellipse, so it is not offered.
 */

#ifndef HYDROCOUPLECOMPOSER_MAP_MAPTRANSFORM_H
#define HYDROCOUPLECOMPOSER_MAP_MAPTRANSFORM_H

#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace HydroCouple::Composer
{

  /*!
   * \brief Converts between world coordinates and device pixels.
   */
  class MapTransform
  {
    public:
      MapTransform() = default;

      /*!
       * \brief Builds a transform showing \a extent inside \a viewport.
       * \param extent World rectangle to show.
       * \param viewport Widget size in pixels.
       */
      MapTransform(const QRectF &extent, const QSizeF &viewport);

      /*!
       * \brief Fits \a extent into \a viewport, preserving aspect ratio.
       *
       * The visible extent therefore usually exceeds the requested one on the
       * axis with slack; that is the point — the requested extent is fully
       * visible and nothing is distorted.
       *
       * \param extent World rectangle that must be visible.
       * \param viewport Widget size in pixels.
       * \param marginFraction Extra space around the extent, as a fraction.
       */
      void fit(const QRectF &extent, const QSizeF &viewport,
               double marginFraction = 0.05);

      /*!
       * \brief World coordinate to pixel.
       * \param world Point in world coordinates.
       */
      [[nodiscard]] QPointF toScreen(const QPointF &world) const;

      /*!
       * \brief Pixel to world coordinate.
       * \param screen Point in widget pixels.
       */
      [[nodiscard]] QPointF toWorld(const QPointF &screen) const;

      /*!
       * \brief The world rectangle currently visible.
       */
      [[nodiscard]] QRectF visibleExtent() const;

      /*!
       * \brief Pixels per world unit; identical on both axes.
       */
      [[nodiscard]] double scale() const;

      /*!
       * \brief The viewport size this transform was built for.
       */
      [[nodiscard]] QSizeF viewport() const;

      /*!
       * \brief Keeps the same centre and scale for a resized viewport.
       * \param viewport New widget size in pixels.
       */
      void setViewport(const QSizeF &viewport);

      /*!
       * \brief Moves the view by a pixel offset.
       * \param pixels Offset in widget pixels.
       */
      void panByPixels(const QPointF &pixels);

      /*!
       * \brief Zooms about a fixed pixel, which stays put.
       *
       * Anchoring at the cursor is what makes wheel-zoom feel right: zooming
       * about the centre slides whatever the user is pointing at off-screen.
       *
       * \param factor Values above 1 zoom in.
       * \param anchor Pixel that must not move.
       */
      void zoomAt(double factor, const QPointF &anchor);

      /*!
       * \brief Whether the transform describes a usable view.
       */
      [[nodiscard]] bool isValid() const;

    private:
      QPointF m_center;      //!< World coordinate at the viewport's centre.
      double m_scale = 1.0;  //!< Pixels per world unit.
      QSizeF m_viewport;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MAP_MAPTRANSFORM_H
