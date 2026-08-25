/*!
 * \file   test_labelling.cpp
 * \brief  Phase C1c verification — label placement, scale gating and halos.
 *
 * The collision map is the part worth testing hardest: text drawn over text
 * is worse than no text at all, and an overlap check that is subtly wrong
 * still produces a map full of labels that looks fine until it is read.
 */

#include "core/composerapplication.h"
#include "render/labelconfig.h"

#include <gtest/gtest.h>

#include <QFontMetricsF>
#include <QImage>
#include <QPainter>

using namespace HydroCouple::Composer;

namespace
{
  class LabelTest : public ::testing::Test
  {
    protected:
      static void SetUpTestSuite()
      {
        if (!qApp)
        {
          static int argc = 1;
          static char arg0[] = "test_labelling";
          static char *argv[] = {arg0, nullptr};
          s_app = new ComposerApplication(argc, argv);
        }
      }

      static void TearDownTestSuite()
      {
        delete s_app;
        s_app = nullptr;
      }

      static ComposerApplication *s_app;
  };

  ComposerApplication *LabelTest::s_app = nullptr;

  const QSizeF kText(40.0, 12.0);
}

TEST_F(LabelTest, PlacesTextRelativeToTheFeature)
{
  LabelConfig config;
  config.offset = 4.0;

  const QPointF anchor(100.0, 100.0);

  config.placement = LabelPlacement::AboveRight;
  const QRectF aboveRight = LabelPainter::labelRect(config, anchor, kText);
  EXPECT_GT(aboveRight.left(), anchor.x());
  EXPECT_LT(aboveRight.bottom(), anchor.y());

  config.placement = LabelPlacement::Below;
  const QRectF below = LabelPainter::labelRect(config, anchor, kText);
  EXPECT_GT(below.top(), anchor.y());
  EXPECT_NEAR(below.center().x(), anchor.x(), 1e-9);

  config.placement = LabelPlacement::Left;
  const QRectF left = LabelPainter::labelRect(config, anchor, kText);
  EXPECT_LT(left.right(), anchor.x());
  EXPECT_NEAR(left.center().y(), anchor.y(), 1e-9);

  config.placement = LabelPlacement::Centered;
  const QRectF centered = LabelPainter::labelRect(config, anchor, kText);
  EXPECT_NEAR(centered.center().x(), anchor.x(), 1e-9);
  EXPECT_NEAR(centered.center().y(), anchor.y(), 1e-9);

  // Every placement keeps the text's own size; only its position moves.
  EXPECT_NEAR(centered.width(), kText.width(), 1e-9);
  EXPECT_NEAR(centered.height(), kText.height(), 1e-9);
}

TEST_F(LabelTest, HidesLabelsOutsideTheirScaleRange)
{
  LabelConfig config;
  config.minScale = 10000.0;  // hide when zoomed further out than 1:10 000
  config.maxScale = 500.0;    // hide when zoomed further in than 1:500

  EXPECT_TRUE(LabelPainter::scaleVisible(config, 5000.0));
  EXPECT_FALSE(LabelPainter::scaleVisible(config, 25000.0))
    << "labels survived being zoomed out past their minimum scale";
  EXPECT_FALSE(LabelPainter::scaleVisible(config, 100.0))
    << "labels survived being zoomed in past their maximum scale";

  // Bounds of zero mean unbounded, and an unknown scale disables the check
  // rather than hiding everything.
  EXPECT_TRUE(LabelPainter::scaleVisible(LabelConfig(), 1.0e9));
  EXPECT_TRUE(LabelPainter::scaleVisible(config, 0.0));
}

TEST_F(LabelTest, CollisionMapRefusesOverlappingLabels)
{
  LabelCollisionMap map(2.0);

  EXPECT_TRUE(map.tryPlace(QRectF(0.0, 0.0, 50.0, 12.0)));

  // Directly on top of the first.
  EXPECT_FALSE(map.tryPlace(QRectF(10.0, 2.0, 50.0, 12.0)));

  // Clear of it.
  EXPECT_TRUE(map.tryPlace(QRectF(0.0, 40.0, 50.0, 12.0)));

  EXPECT_EQ(map.placedCount(), 2);
}

TEST_F(LabelTest, CollisionMapKeepsLabelsFromTouchingEdgeToEdge)
{
  LabelCollisionMap map(3.0);

  ASSERT_TRUE(map.tryPlace(QRectF(0.0, 0.0, 50.0, 12.0)));

  // Two pixels of clear air between them: legal without padding, unreadable
  // in practice, which is what the padding exists to prevent.
  EXPECT_FALSE(map.tryPlace(QRectF(52.0, 0.0, 50.0, 12.0)));

  // Ten pixels away is fine.
  EXPECT_TRUE(map.tryPlace(QRectF(60.0, 0.0, 50.0, 12.0)));
}

TEST_F(LabelTest, CollisionMapRejectsEmptyRectsAndResets)
{
  LabelCollisionMap map;

  EXPECT_FALSE(map.tryPlace(QRectF()));
  EXPECT_EQ(map.placedCount(), 0);

  ASSERT_TRUE(map.tryPlace(QRectF(0.0, 0.0, 10.0, 10.0)));
  map.clear();

  // A new frame starts empty, or the second frame would place nothing.
  EXPECT_EQ(map.placedCount(), 0);
  EXPECT_TRUE(map.tryPlace(QRectF(0.0, 0.0, 10.0, 10.0)));
}

TEST_F(LabelTest, DrawsTextWithAHaloThatSeparatesItFromTheBackground)
{
  QImage image(120, 40, QImage::Format_ARGB32);

  // Black text on a black background is invisible without a halo, so the
  // halo is the only thing that can make this readable.
  image.fill(Qt::black);

  LabelConfig config;
  config.color = Qt::black;
  config.haloColor = Qt::white;
  config.haloWidth = 2.0;

  QPainter painter(&image);
  const QRectF box =
    LabelPainter::labelRect(config, QPointF(10.0, 20.0),
                            QSizeF(QFontMetricsF(config.font)
                                     .horizontalAdvance(QStringLiteral("Node")),
                                   QFontMetricsF(config.font).height()));

  LabelPainter::draw(painter, config, box, QStringLiteral("Node"));
  painter.end();

  int white = 0;

  for (int y = 0; y < image.height(); ++y)
  {
    for (int x = 0; x < image.width(); ++x)
    {
      if (image.pixelColor(x, y).lightness() > 200)
      {
        ++white;
      }
    }
  }

  EXPECT_GT(white, 20) << "no halo was drawn, so the text is invisible";
}

TEST_F(LabelTest, DrawsNoHaloWhenItIsSwitchedOff)
{
  QImage image(120, 40, QImage::Format_ARGB32);
  image.fill(Qt::black);

  LabelConfig config;
  config.color = Qt::black;
  config.haloWidth = 0.0;

  QPainter painter(&image);
  LabelPainter::draw(painter, config, QRectF(4.0, 4.0, 100.0, 20.0),
                     QStringLiteral("Node"));
  painter.end();

  for (int y = 0; y < image.height(); ++y)
  {
    for (int x = 0; x < image.width(); ++x)
    {
      ASSERT_LE(image.pixelColor(x, y).lightness(), 200)
        << "a halo was drawn when none was configured";
    }
  }
}
