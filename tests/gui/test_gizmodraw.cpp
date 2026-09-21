/*!
 * \file   test_gizmodraw.cpp
 * \brief  U4 — the orientation cue, drawn on a real device.
 *
 * test_axisgizmo.cpp covers the arithmetic: where the arms point, what a
 * click answers, which corner a rectangle names. None of that needs a
 * graphics device and none of it proves a single pixel was drawn.
 *
 * This does. It draws through the same SceneRenderer::render() the widget
 * uses, into a texture rather than a window — QRhiWidget cannot make a
 * device under the offscreen platform this suite runs under, and this
 * path works there unchanged (D31). What it buys is the three things the
 * arithmetic cannot say: that the cue appears at all, that it appears in
 * the corner it was asked for, and that it survives being drawn over a
 * scene that has already written its depths.
 *
 * That last one is the reason this file exists. The cue is squeezed into
 * the nearest hundredth of the depth buffer so it beats the scene without
 * losing the arms' depths relative to one another. It is the kind of
 * trick that either works or leaves the cue invisible behind a hill, and
 * no amount of matrix arithmetic will tell you which.
 */

#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "render/layerstyle.h"
#include "scene/axisgizmo.h"
#include "scene/camera.h"
#include "scene/sceneimage.h"
#include "scene/scenerenderer.h"

#include <gtest/gtest.h>

#include <QColor>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  constexpr int kSide = 256;
  const QColor kBackground(0, 0, 0);

  //! A flat sheet big enough to fill the view and hide anything behind it.
  MeshDefinition sheet()
  {
    MeshDefinition mesh;
    mesh.meshName = "sheet";
    mesh.nodeX = { -500.0, 500.0, 500.0, -500.0 };
    mesh.nodeY = { -500.0, -500.0, 500.0, 500.0 };
    mesh.nodeZ = { 0.0, 0.0, 0.0, 0.0 };
    mesh.faceNodeOffsets = { 0, 4 };
    mesh.faceNodes = { 0, 1, 2, 3 };

    return mesh;
  }

  //! How strongly \a region leans toward one channel over the other two.
  int strongest(const QImage &image, const QRect &region, int channel)
  {
    int best = 0;

    for (int y = region.top(); y <= region.bottom(); ++y)
    {
      for (int x = region.left(); x <= region.right(); ++x)
      {
        const QColor pixel = image.pixelColor(x, y);
        const int rgb[3] = { pixel.red(), pixel.green(), pixel.blue() };
        const int other = std::max(rgb[(channel + 1) % 3],
                                   rgb[(channel + 2) % 3]);

        best = std::max(best, rgb[channel] - other);
      }
    }

    return best;
  }

  /*!
   * \brief Pixels inside \a region where the two images differ.
   *
   * Against a reference *render*, never against a nominal colour. The
   * first version of this compared each pixel to the sheet's fill of
   * (90,90,90) and every pixel differed — the sheet is lit, so it is
   * never drawn in the colour it was given. That gate failed in all four
   * corners at once and in the empty-viewport case too, and accused the
   * viewport of being upside down while the renderer was correct.
   *
   * Comparing two renders of the same scene removes the assumption
   * entirely: whatever lighting does, it does to both.
   */
  int differing(const QImage &left, const QImage &right, const QRect &region)
  {
    int count = 0;

    for (int y = region.top(); y <= region.bottom(); ++y)
    {
      for (int x = region.left(); x <= region.right(); ++x)
      {
        if (left.pixelColor(x, y) != right.pixelColor(x, y))
        {
          ++count;
        }
      }
    }

    return count;
  }

  //! Everything outside \a region, as the rectangles above and below it
  //! and the two beside it.
  QVector<QRect> outside(const QRect &region, int side)
  {
    return {
      QRect(0, 0, side, region.top()),
      QRect(0, region.bottom() + 1, side, side - region.bottom() - 1),
      QRect(0, region.top(), region.left(), region.height()),
      QRect(region.right() + 1, region.top(), side - region.right() - 1,
            region.height()),
    };
  }

  class GizmoDrawTest : public ::testing::Test
  {
    protected:
      //! The scene every case draws: one opaque sheet filling the view, so
      //! that a cue which lost its depth fight would simply not be there.
      void SetUp() override
      {
        m_stack = std::make_unique<LayerStackModel>();

        QString message;
        std::unique_ptr<MeshLayer> terrain = MeshLayer::create(
          QStringLiteral("sheet"), sheet(), MeshEntity::Face, message);

        ASSERT_TRUE(terrain) << message.toStdString();

        Symbol symbol = terrain->style()->symbol();
        symbol.fill = QColor(90, 90, 90);
        symbol.stroke = QColor(90, 90, 90);
        terrain->style()->setSymbol(symbol);

        ASSERT_GE(m_stack->addLayer(terrain.release()), 0);

        m_renderer.setModel(m_stack.get());
        m_camera.fitTo(m_renderer.sceneBounds(), 1.0);
        m_camera.setAzimuth(0.0);
        m_camera.setElevation(30.0);
      }

      QImage draw(const QRect &gizmo)
      {
        m_renderer.setAxisGizmoViewport(gizmo);

        QString message;
        QImage image = renderSceneToImage(m_renderer, m_camera,
                                          QSize(kSide, kSide), kBackground,
                                          message);

        EXPECT_FALSE(image.isNull()) << message.toStdString();

        return image;
      }

      std::unique_ptr<LayerStackModel> m_stack;
      SceneRenderer m_renderer;
      Camera m_camera;
  };
}

TEST_F(GizmoDrawTest, TheCueIsDrawnOverASceneThatFillsTheView)
{
  // The depth squeeze, checked the only way it can be: the sheet covers
  // every pixel, so any red, green and blue in the corner arrived after
  // it and in front of it.
  const QRect rect =
    axisGizmoRect(QSize(kSide, kSide), 96, GizmoCorner::BottomLeft);

  ASSERT_FALSE(rect.isEmpty());

  const QImage image = draw(rect);
  ASSERT_FALSE(image.isNull());

  EXPECT_GT(strongest(image, rect, 0), 40) << "no red arm";
  EXPECT_GT(strongest(image, rect, 1), 40) << "no green arm";
  EXPECT_GT(strongest(image, rect, 2), 40) << "no blue arm";
}

TEST_F(GizmoDrawTest, AnEmptyViewportDrawsNoCueAtAll)
{
  // Switching the cue off has to leave the corner alone, not draw it
  // somewhere harmless. Measured as a difference between two renders of
  // the same scene, so nothing here depends on knowing what colour a lit
  // sheet comes out.
  const QRect rect =
    axisGizmoRect(QSize(kSide, kSide), 96, GizmoCorner::BottomLeft);

  ASSERT_FALSE(rect.isEmpty());

  const QImage without = draw(QRect());
  const QImage with = draw(rect);

  ASSERT_FALSE(without.isNull());
  ASSERT_FALSE(with.isNull());

  EXPECT_GT(differing(with, without, rect), 100)
    << "the cue never appeared";

  // And the same scene drawn twice with no cue is the same picture — the
  // control for the comparison above, without which a renderer that
  // varied frame to frame would make this gate meaningless.
  EXPECT_EQ(differing(without, draw(QRect()), rect), 0)
    << "two renders of one scene disagree, so no pixel gate here means "
       "anything";
}

TEST_F(GizmoDrawTest, TheCueIsInTheCornerItWasAskedFor)
{
  // The one that catches a bottom-left origin mixed up with a top-left
  // one: QRhi takes viewports the OpenGL way round, and getting it
  // backwards puts the cue in the corner diagonally opposite the one the
  // user chose, which reads as a broken preference rather than a broken
  // renderer.
  //
  // Stated as "inside the asked rectangle something changed, and outside
  // it nothing did". That is stronger than checking the opposite corner
  // — it catches the cue landing anywhere it was not asked to, not only
  // in the one place a flip would put it.
  const QImage without = draw(QRect());
  ASSERT_FALSE(without.isNull());

  struct Case
  {
      GizmoCorner corner;
      const char *name;
  };

  const Case cases[4] = { { GizmoCorner::BottomLeft, "bottom left" },
                          { GizmoCorner::BottomRight, "bottom right" },
                          { GizmoCorner::TopLeft, "top left" },
                          { GizmoCorner::TopRight, "top right" } };

  for (const Case &one : cases)
  {
    const QRect asked = axisGizmoRect(QSize(kSide, kSide), 96, one.corner);

    ASSERT_FALSE(asked.isEmpty()) << one.name;

    const QImage image = draw(asked);
    ASSERT_FALSE(image.isNull()) << one.name;

    EXPECT_GT(differing(image, without, asked), 100)
      << "nothing drawn in the " << one.name << " corner";

    for (const QRect &elsewhere : outside(asked, kSide))
    {
      if (elsewhere.isEmpty())
      {
        continue;
      }

      EXPECT_EQ(differing(image, without, elsewhere), 0)
        << "the cue drew outside the " << one.name
        << " corner it was asked for";
    }
  }
}

TEST_F(GizmoDrawTest, TurningTheCameraTurnsTheDrawnCue)
{
  // Not the matrix this time: the pixels. A cue wired to a stale camera
  // would keep pointing the way it did on the first frame, and every
  // arithmetic gate in the suite would still pass.
  const QRect rect =
    axisGizmoRect(QSize(kSide, kSide), 96, GizmoCorner::BottomLeft);

  const QImage north = draw(rect);

  m_camera.setAzimuth(90.0);

  const QImage east = draw(rect);

  ASSERT_FALSE(north.isNull());
  ASSERT_FALSE(east.isNull());

  EXPECT_GT(differing(north, east, rect), 100)
    << "the cue drew the same picture from two different angles";
}
