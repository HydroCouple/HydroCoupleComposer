/*!
 * \file   test_maptransform.cpp
 * \brief  Phase C1b verification — the world-to-pixel mapping.
 *
 * The transform is tested on its own, without a widget, because pan, zoom and
 * fit are arithmetic and a rendered pixel is a poor instrument for checking
 * arithmetic: an off-by-half-a-pixel error is invisible in an image and
 * obvious in an equation.
 *
 * Round-trips alone are avoided as the only evidence — the identity transform
 * round-trips perfectly — so each property is also checked against a value
 * derived independently of the implementation.
 */

#include "map/maptransform.h"

#include <gtest/gtest.h>

#include <QRectF>
#include <QSizeF>

#include <cmath>

using namespace HydroCouple::Composer;

namespace
{
  //! A viewport wider than it is tall, so the two axes cannot be confused.
  const QSizeF kViewport(800.0, 400.0);
}

TEST(MapTransformTest, IsInvalidWithoutAViewport)
{
  MapTransform transform;
  EXPECT_FALSE(transform.isValid());

  // A zero-sized viewport is what a widget reports before it is laid out, and
  // fitting into it must not produce an infinite scale.
  transform.fit(QRectF(0.0, 0.0, 10.0, 10.0), QSizeF(0.0, 0.0));
  EXPECT_FALSE(transform.isValid());
}

TEST(MapTransformTest, MapsCentreOfExtentToCentreOfViewport)
{
  MapTransform transform;
  transform.fit(QRectF(100.0, 200.0, 40.0, 20.0), kViewport, 0.0);

  const QPointF screen = transform.toScreen(QPointF(120.0, 210.0));

  EXPECT_NEAR(screen.x(), 400.0, 1e-9);
  EXPECT_NEAR(screen.y(), 200.0, 1e-9);
}

TEST(MapTransformTest, RoundTripsWorldAndScreen)
{
  MapTransform transform;
  transform.fit(QRectF(-500.0, 1000.0, 250.0, 125.0), kViewport, 0.0);

  const QPointF world(-433.25, 1077.5);
  const QPointF back = transform.toWorld(transform.toScreen(world));

  EXPECT_NEAR(back.x(), world.x(), 1e-9);
  EXPECT_NEAR(back.y(), world.y(), 1e-9);
}

TEST(MapTransformTest, NorthIsUp)
{
  MapTransform transform;
  transform.fit(QRectF(0.0, 0.0, 100.0, 100.0), kViewport, 0.0);

  const QPointF south = transform.toScreen(QPointF(50.0, 10.0));
  const QPointF north = transform.toScreen(QPointF(50.0, 90.0));

  // Widget Y grows downward, so the northern point must have the smaller Y.
  EXPECT_LT(north.y(), south.y());

  // And X must be untouched by the flip.
  EXPECT_NEAR(north.x(), south.x(), 1e-9);
}

TEST(MapTransformTest, FitShowsTheWholeExtentWithoutDistortion)
{
  MapTransform transform;

  // A square extent in a 2:1 viewport: the height is the binding constraint,
  // so the visible extent must be twice as wide as requested and exactly as
  // tall.
  const QRectF extent(0.0, 0.0, 100.0, 100.0);
  transform.fit(extent, kViewport, 0.0);

  const QRectF visible = transform.visibleExtent();

  // Inflated by a rounding tolerance: the visible extent is derived through
  // a division, so its edge lands within an ULP of the requested one rather
  // than exactly on it.
  EXPECT_TRUE(visible.adjusted(-1e-6, -1e-6, 1e-6, 1e-6).contains(extent))
    << "requested extent is not fully visible";
  EXPECT_NEAR(visible.height(), 100.0, 1e-6);
  EXPECT_NEAR(visible.width(), 200.0, 1e-6);

  // Uniform scale: a square in the world must be a square on screen.
  const QPointF origin = transform.toScreen(QPointF(0.0, 0.0));
  const QPointF right = transform.toScreen(QPointF(10.0, 0.0));
  const QPointF up = transform.toScreen(QPointF(0.0, 10.0));

  EXPECT_NEAR(right.x() - origin.x(), origin.y() - up.y(), 1e-9);
}

TEST(MapTransformTest, MarginLeavesSpaceAroundTheExtent)
{
  const QRectF extent(0.0, 0.0, 100.0, 100.0);

  MapTransform tight;
  tight.fit(extent, kViewport, 0.0);

  MapTransform margined;
  margined.fit(extent, kViewport, 0.10);

  // A 10% margin on each side means the fitted extent occupies 1/1.2 of the
  // viewport, so the scale drops by exactly that factor.
  EXPECT_NEAR(margined.scale(), tight.scale() / 1.2, 1e-9);
  EXPECT_TRUE(margined.visibleExtent().contains(extent));
}

TEST(MapTransformTest, PansByExactlyThePixelsRequested)
{
  MapTransform transform;
  transform.fit(QRectF(0.0, 0.0, 100.0, 100.0), kViewport, 0.0);

  const QPointF probe(25.0, 75.0);
  const QPointF before = transform.toScreen(probe);

  transform.panByPixels(QPointF(30.0, -12.0));

  const QPointF after = transform.toScreen(probe);

  // Dragging right moves the map right by the same number of pixels: the
  // world point follows the hand rather than the view.
  EXPECT_NEAR(after.x() - before.x(), 30.0, 1e-9);
  EXPECT_NEAR(after.y() - before.y(), -12.0, 1e-9);
}

TEST(MapTransformTest, ZoomHoldsTheAnchorWorldPointStill)
{
  MapTransform transform;
  transform.fit(QRectF(0.0, 0.0, 100.0, 100.0), kViewport, 0.0);

  const QPointF anchor(120.0, 90.0);
  const QPointF worldUnderAnchor = transform.toWorld(anchor);

  transform.zoomAt(2.5, anchor);

  const QPointF stillThere = transform.toWorld(anchor);

  EXPECT_NEAR(stillThere.x(), worldUnderAnchor.x(), 1e-9);
  EXPECT_NEAR(stillThere.y(), worldUnderAnchor.y(), 1e-9);

  // And the zoom actually happened — an implementation that ignored the
  // factor would pass the assertions above.
  EXPECT_NEAR(transform.scale(), 2.5 * 4.0, 1e-9);
}

TEST(MapTransformTest, ZoomAboutTheCentreLeavesTheCentreAlone)
{
  MapTransform transform;
  transform.fit(QRectF(0.0, 0.0, 100.0, 100.0), kViewport, 0.0);

  const QRectF before = transform.visibleExtent();

  transform.zoomAt(2.0, QPointF(400.0, 200.0));

  const QRectF after = transform.visibleExtent();

  EXPECT_NEAR(after.center().x(), before.center().x(), 1e-9);
  EXPECT_NEAR(after.center().y(), before.center().y(), 1e-9);
  EXPECT_NEAR(after.width(), before.width() / 2.0, 1e-6);
}

TEST(MapTransformTest, ResizeRevealsMoreRatherThanRescaling)
{
  MapTransform transform;
  transform.fit(QRectF(0.0, 0.0, 100.0, 100.0), kViewport, 0.0);

  const double scale = transform.scale();
  const QPointF centre = transform.visibleExtent().center();

  transform.setViewport(QSizeF(1600.0, 800.0));

  EXPECT_NEAR(transform.scale(), scale, 1e-12)
    << "a resize must not change the map's scale";

  const QRectF visible = transform.visibleExtent();

  EXPECT_NEAR(visible.center().x(), centre.x(), 1e-9);
  EXPECT_NEAR(visible.center().y(), centre.y(), 1e-9);
  EXPECT_NEAR(visible.width(), 400.0, 1e-6);
}

TEST(MapTransformTest, DegenerateExtentStillProducesAUsableView)
{
  // A single point — one node, or a layer whose features all share a
  // coordinate — has zero width and height, and a naive fit divides by it.
  MapTransform transform;
  transform.fit(QRectF(QPointF(500.0, 500.0), QSizeF(0.0, 0.0)), kViewport);

  ASSERT_TRUE(transform.isValid());
  EXPECT_GT(transform.scale(), 0.0);
  EXPECT_TRUE(std::isfinite(transform.scale()));

  const QPointF screen = transform.toScreen(QPointF(500.0, 500.0));

  EXPECT_NEAR(screen.x(), 400.0, 1e-9);
  EXPECT_NEAR(screen.y(), 200.0, 1e-9);
}

TEST(MapTransformTest, IgnoresNonsensicalZoomFactors)
{
  MapTransform transform;
  transform.fit(QRectF(0.0, 0.0, 100.0, 100.0), kViewport, 0.0);

  const double scale = transform.scale();

  transform.zoomAt(0.0, QPointF(10.0, 10.0));
  transform.zoomAt(-2.0, QPointF(10.0, 10.0));

  EXPECT_NEAR(transform.scale(), scale, 1e-12);
}
