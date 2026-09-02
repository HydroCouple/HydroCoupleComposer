/*!
 * \file   test_scene3d.cpp
 * \brief  C3a — the 3D scene, rendered for real.
 *
 * Every case here draws pixels on a graphics device. That is deliberate:
 * depth ordering, lighting and vertical exaggeration are not observable from
 * call counts, and a scene that silently stopped using elevations would pass
 * any test written against the geometry it *asked* for.
 *
 * The device is a standalone QRhi drawing into a texture, not a QRhiWidget:
 * QRhiWidget cannot create a device under the offscreen platform plugin this
 * suite runs under, whereas this path works there unchanged.
 *
 * Failing images are written next to the GIS fixtures so they can be looked
 * at rather than guessed at.
 */

#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "render/classification.h"
#include "render/layerstyle.h"
#include "scene/camera.h"
#include "scene/sceneimage.h"
#include "scene/scenerenderer.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QSignalSpy>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

namespace
{
  //! One flat quad at a given elevation, spanning the same footprint each
  //! time so two of them stack exactly one behind the other.
  MeshDefinition quadAt(double elevation)
  {
    MeshDefinition mesh;
    mesh.meshName = "quad";
    mesh.nodeX = { 0.0, 100.0, 100.0, 0.0 };
    mesh.nodeY = { 0.0, 0.0, 100.0, 100.0 };
    mesh.nodeZ = { elevation, elevation, elevation, elevation };
    mesh.faceNodeOffsets = { 0, 4 };
    mesh.faceNodes = { 0, 1, 2, 3 };

    return mesh;
  }

  //! A ridge: a 2x1 pair of quads whose shared middle edge is raised.
  MeshDefinition ridge(double crest)
  {
    MeshDefinition mesh;
    mesh.meshName = "ridge";
    mesh.nodeX = { 0.0, 50.0, 100.0, 0.0, 50.0, 100.0 };
    mesh.nodeY = { 0.0, 0.0, 0.0, 100.0, 100.0, 100.0 };
    mesh.nodeZ = { 0.0, crest, 0.0, 0.0, crest, 0.0 };
    mesh.faceNodeOffsets = { 0, 4, 8 };
    mesh.faceNodes = { 0, 1, 4, 3, 1, 2, 5, 4 };

    return mesh;
  }

  std::unique_ptr<MeshLayer> opaqueLayer(const QString &name,
                                         const MeshDefinition &mesh,
                                         const QColor &color)
  {
    QString message;
    std::unique_ptr<MeshLayer> layer =
      MeshLayer::create(name, mesh, MeshEntity::Face, message);

    if (!layer)
    {
      return nullptr;
    }

    // A mesh layer opens translucent, which is right for a map drawn over a
    // basemap and wrong for a test about which surface is in front.
    Symbol symbol = layer->style()->symbol();
    symbol.fill = color;
    symbol.stroke = color;
    layer->style()->setSymbol(symbol);

    return layer;
  }

  //! Creates a styled mesh layer and hands it to the stack, which owns it.
  MeshLayer *addOpaque(LayerStackModel &stack, const QString &name,
                       const MeshDefinition &mesh, const QColor &color)
  {
    std::unique_ptr<MeshLayer> layer = opaqueLayer(name, mesh, color);

    if (!layer)
    {
      return nullptr;
    }

    MeshLayer *raw = layer.release();

    return stack.addLayer(raw) >= 0 ? raw : nullptr;
  }

  int countMatching(const QImage &image, const QColor &color, int tolerance)
  {
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);

    int matches = 0;

    for (int y = 0; y < rgb.height(); ++y)
    {
      for (int x = 0; x < rgb.width(); ++x)
      {
        const QRgb pixel = rgb.pixel(x, y);

        if (std::abs(qRed(pixel) - color.red()) <= tolerance &&
            std::abs(qGreen(pixel) - color.green()) <= tolerance &&
            std::abs(qBlue(pixel) - color.blue()) <= tolerance)
        {
          ++matches;
        }
      }
    }

    return matches;
  }

  //! How many pixels two renders disagree on.
  int countDifferent(const QImage &left, const QImage &right)
  {
    const QImage a = left.convertToFormat(QImage::Format_ARGB32);
    const QImage b = right.convertToFormat(QImage::Format_ARGB32);

    if (a.size() != b.size())
    {
      return -1;
    }

    int different = 0;

    for (int y = 0; y < a.height(); ++y)
    {
      for (int x = 0; x < a.width(); ++x)
      {
        if (a.pixel(x, y) != b.pixel(x, y))
        {
          ++different;
        }
      }
    }

    return different;
  }

  //! Pixels that are not the clear colour: what the scene actually drew.
  int countDrawn(const QImage &image, const QColor &background)
  {
    const QImage rgb = image.convertToFormat(QImage::Format_ARGB32);

    int drawn = 0;

    for (int y = 0; y < rgb.height(); ++y)
    {
      for (int x = 0; x < rgb.width(); ++x)
      {
        const QRgb pixel = rgb.pixel(x, y);

        if (std::abs(qRed(pixel) - background.red()) > 6 ||
            std::abs(qGreen(pixel) - background.green()) > 6 ||
            std::abs(qBlue(pixel) - background.blue()) > 6)
        {
          ++drawn;
        }
      }
    }

    return drawn;
  }

  const QColor kBackground = QColor(0, 0, 0);
  const QSize kSize(160, 120);

  class Scene3DTest : public ::testing::Test
  {
    protected:
      void SetUp() override
      {
        if (!QApplication::instance())
        {
          static int argc = 1;
          static char name[] = "test_scene3d";
          static char *argv[] = { name, nullptr };
          m_app = std::make_unique<QApplication>(argc, argv);
        }
      }

      //! Renders and, on a failure anyone will want to look at, saves.
      QImage renderScene(SceneRenderer &renderer, const Camera &camera,
                         const QString &label)
      {
        QString message;
        const QImage image =
          renderSceneToImage(renderer, camera, kSize, kBackground, message);

        if (image.isNull())
        {
          ADD_FAILURE() << "render failed: " << message.toStdString();

          return image;
        }

        image.save(QDir(QStringLiteral(COMPOSER_GIS_FIXTURE_DIR))
                     .filePath(QStringLiteral("generated-scene-%1.png")
                                 .arg(label)));

        return image;
      }

      //! A top-down camera framing the standard 100x100 footprint.
      static Camera topDown()
      {
        Camera camera;
        camera.setElevation(90.0);
        camera.setGroundExtent(QRectF(0.0, 0.0, 100.0, 100.0),
                               double(kSize.width()) / kSize.height());

        return camera;
      }

      /*!
       * \brief An oblique camera aimed at the standard footprint's centre.
       *
       * Placed directly rather than through setGroundExtent(), because a
       * shallow camera asked to *contain* a rectangle must reach far enough
       * to cover its near strip too, which leaves the subject small and dark
       * — correct framing, and a poor picture to assert against.
       */
      static Camera oblique(double elevation, double azimuth = 0.0)
      {
        Camera camera;
        camera.setTarget(QVector3D(50.0f, 50.0f, 0.0f));
        camera.setElevation(elevation);
        camera.setAzimuth(azimuth);
        camera.setDistance(180.0);

        return camera;
      }

    private:
      std::unique_ptr<QApplication> m_app;
  };

}

// ── The device is real ──────────────────────────────────────────────────────

TEST_F(Scene3DTest, DrawsAMeshWhereTheCameraIsLooking)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QColor red(220, 40, 40);
  ASSERT_NE(addOpaque(stack, QStringLiteral("quad"), quadAt(0.0), red),
            nullptr);

  const QImage image = renderScene(renderer, topDown(), QStringLiteral("quad"));
  ASSERT_FALSE(image.isNull());

  // Not "something was drawn": a quad drawn at the wrong scale or with the
  // camera pointing elsewhere would also put pixels on screen. The 100x100
  // footprint fills the viewport's height exactly, so its area is known.
  const int drawn = countDrawn(image, kBackground);
  const int expected = kSize.height() * kSize.height();

  EXPECT_GT(drawn, expected * 8 / 10)
    << "the mesh covered " << drawn << " pixels, not the ~" << expected
    << " its framed footprint occupies";
  EXPECT_LT(drawn, expected * 12 / 10);

  EXPECT_GT(countMatching(image, red, 12), expected / 2)
    << "the mesh did not render in its own style's colour";
}

TEST_F(Scene3DTest, DrawsNothingForAnEmptyStack)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QImage image =
    renderScene(renderer, topDown(), QStringLiteral("empty"));
  ASSERT_FALSE(image.isNull());

  EXPECT_EQ(countDrawn(image, kBackground), 0);
}

// ── Depth, which is the whole point of the 3D view ─────────────────────────

TEST_F(Scene3DTest, TheNearerSurfaceOccludesTheFartherOne)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QColor high(220, 40, 40);
  const QColor low(40, 80, 220);

  ASSERT_NE(addOpaque(stack, QStringLiteral("high"), quadAt(50.0), high),
            nullptr);
  ASSERT_NE(addOpaque(stack, QStringLiteral("low"), quadAt(0.0), low),
            nullptr);

  // Row 0 is the top of the stack and is drawn last, so the low surface is
  // painted over the high one. Only a depth test can put the high one back
  // in front — draw order says the opposite, which is exactly the point.
  ASSERT_EQ(stack.layerAt(0)->name(), QStringLiteral("low"));

  const QImage image =
    renderScene(renderer, topDown(), QStringLiteral("occlusion"));
  ASSERT_FALSE(image.isNull());

  const int highPixels = countMatching(image, high, 12);
  const int lowPixels = countMatching(image, low, 12);

  EXPECT_GT(highPixels, lowPixels * 10)
    << "the farther surface won: " << lowPixels << " of its pixels survived "
    << "against " << highPixels << " of the nearer one's";
}

TEST_F(Scene3DTest, ElevationsReachTheGeometry)
{
  // A scene that read only X and Y would render a ridge and a flat sheet
  // identically from an oblique view, and no amount of "something was drawn"
  // would notice.
  LayerStackModel flatStack;
  SceneRenderer flatRenderer;
  flatRenderer.setModel(&flatStack);
  ASSERT_NE(addOpaque(flatStack, QStringLiteral("flat"), ridge(0.0),
                      QColor(200, 200, 200)),
            nullptr);

  LayerStackModel ridgeStack;
  SceneRenderer ridgeRenderer;
  ridgeRenderer.setModel(&ridgeStack);
  ASSERT_NE(addOpaque(ridgeStack, QStringLiteral("ridge"), ridge(40.0),
                      QColor(200, 200, 200)),
            nullptr);

  const Camera view = oblique(30.0);

  const QImage flat = renderScene(flatRenderer, view, QStringLiteral("flat"));
  const QImage relief =
    renderScene(ridgeRenderer, view, QStringLiteral("ridge"));

  ASSERT_FALSE(flat.isNull());
  ASSERT_FALSE(relief.isNull());

  // Both must actually be on screen. Two nearly-empty frames also differ,
  // and would let a scene that drew almost nothing pass this.
  const int flatDrawn = countDrawn(flat, kBackground);
  const int reliefDrawn = countDrawn(relief, kBackground);

  ASSERT_GT(flatDrawn, kSize.width() * kSize.height() / 10);
  ASSERT_GT(reliefDrawn, kSize.width() * kSize.height() / 10);

  // And they must differ substantially, not by a rounding edge: a 40-unit
  // crest on a 100-unit mesh moves a large part of the silhouette.
  EXPECT_GT(countDifferent(flat, relief), flatDrawn / 5)
    << "a raised ridge rendered essentially like a sheet: " << flatDrawn
    << " pixels drawn, only " << countDifferent(flat, relief) << " different";
}

TEST_F(Scene3DTest, VerticalExaggerationChangesWhatIsDrawn)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);
  ASSERT_NE(addOpaque(stack, QStringLiteral("ridge"), ridge(2.0),
                      QColor(200, 200, 200)),
            nullptr);

  const Camera plain = oblique(25.0);

  Camera exaggerated = plain;
  exaggerated.setVerticalExaggeration(20.0);

  const QImage before =
    renderScene(renderer, plain, QStringLiteral("exaggeration-off"));
  const QImage after =
    renderScene(renderer, exaggerated, QStringLiteral("exaggeration-on"));

  ASSERT_FALSE(before.isNull());
  ASSERT_FALSE(after.isNull());

  const int drawn = countDrawn(before, kBackground);
  ASSERT_GT(drawn, kSize.width() * kSize.height() / 10);

  // A 2-unit crest exaggerated twenty times becomes a 40-unit one, which is
  // a different silhouette — not a shading nudge.
  EXPECT_GT(countDifferent(before, after), drawn / 5)
    << "exaggeration never reached the model matrix";
}

TEST_F(Scene3DTest, SurfacesCloseToTheCameraAreNotClipped)
{
  // Metal, Vulkan and D3D put clip-space depth in [0, 1] where OpenGL — and
  // so QMatrix4x4::perspective — puts it in [-1, 1]. Without the backend's
  // correction the near half of that range falls outside the target's, and
  // everything within a whisker of the camera silently vanishes. That is what
  // a user meets the moment they dolly into terrain.
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QColor red(220, 40, 40);

  Camera camera = topDown();
  camera.setDistance(100.0);

  // Just past the near plane: near is a thousandth of the view distance, and
  // the uncorrected range would clip anything nearer than about twice that.
  const double eyeHeight = double(camera.eye().z());
  ASSERT_NE(addOpaque(stack, QStringLiteral("close"),
                      quadAt(eyeHeight - camera.nearPlane() * 1.5), red),
            nullptr);

  const QImage image =
    renderScene(renderer, camera, QStringLiteral("near-plane"));
  ASSERT_FALSE(image.isNull());

  EXPECT_GT(countDrawn(image, kBackground),
            kSize.width() * kSize.height() * 9 / 10)
    << "a surface just past the near plane was clipped away";
}

// ── The stack is the single owner ───────────────────────────────────────────

TEST_F(Scene3DTest, HidingALayerInTheStackRemovesItFromTheScene)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  MeshLayer *layer = addOpaque(stack, QStringLiteral("quad"), quadAt(0.0),
                               QColor(220, 40, 40));
  ASSERT_NE(layer, nullptr);

  ASSERT_GT(countDrawn(renderScene(renderer, topDown(),
                                   QStringLiteral("visible")),
                       kBackground),
            0);

  layer->setVisible(false);

  const QImage hidden =
    renderScene(renderer, topDown(), QStringLiteral("hidden"));
  ASSERT_FALSE(hidden.isNull());

  EXPECT_EQ(countDrawn(hidden, kBackground), 0)
    << "the scene kept drawing a layer the stack had hidden";
}

TEST_F(Scene3DTest, RestylingALayerReachesTheScene)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QColor first(220, 40, 40);
  const QColor second(40, 200, 60);

  MeshLayer *layer =
    addOpaque(stack, QStringLiteral("quad"), quadAt(0.0), first);
  ASSERT_NE(layer, nullptr);

  ASSERT_GT(countMatching(renderScene(renderer, topDown(),
                                      QStringLiteral("style-before")),
                          first, 12),
            0);

  Symbol symbol = layer->style()->symbol();
  symbol.fill = second;
  layer->style()->setSymbol(symbol);
  layer->notifyAppearanceChanged();

  const QImage after =
    renderScene(renderer, topDown(), QStringLiteral("style-after"));
  ASSERT_FALSE(after.isNull());

  EXPECT_GT(countMatching(after, second, 12), 0)
    << "the scene drew a colour the layer no longer has";
  EXPECT_EQ(countMatching(after, first, 12), 0);
}

TEST_F(Scene3DTest, TheRendererAnnouncesThatItsGeometryIsStale)
{
  // Restyling a layer must make the scene rebuild. The rendered-image path
  // cannot show this: it creates a device per call and therefore drops every
  // cached batch anyway, so a renderer that never noticed a change would
  // still produce a correct picture there — and a stale one on screen, where
  // the device lives on. The announcement is what the interactive view acts
  // on, so that is what this asserts.
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  MeshLayer *layer =
    addOpaque(stack, QStringLiteral("quad"), quadAt(0.0), QColor(220, 40, 40));
  ASSERT_NE(layer, nullptr);

  QSignalSpy spy(&renderer, &SceneRenderer::sceneChanged);
  ASSERT_TRUE(spy.isValid());

  Symbol symbol = layer->style()->symbol();
  symbol.fill = QColor(40, 200, 60);
  layer->style()->setSymbol(symbol);
  layer->notifyAppearanceChanged();

  EXPECT_GE(spy.count(), 1) << "the scene never learned the layer restyled";

  const int afterRestyle = spy.count();
  layer->setVisible(false);

  EXPECT_GT(spy.count(), afterRestyle)
    << "the scene never learned the layer was hidden";
}

TEST_F(Scene3DTest, LayerOpacityReachesTheScene)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QColor red(220, 40, 40);

  MeshLayer *layer =
    addOpaque(stack, QStringLiteral("quad"), quadAt(0.0), red);
  ASSERT_NE(layer, nullptr);

  const int opaque = countMatching(
    renderScene(renderer, topDown(), QStringLiteral("opacity-full")), red, 12);
  ASSERT_GT(opaque, 0);

  layer->setOpacity(0.25);

  const QImage faded =
    renderScene(renderer, topDown(), QStringLiteral("opacity-quarter"));
  ASSERT_FALSE(faded.isNull());

  // Still drawn, but no longer its own colour: blended against black.
  EXPECT_GT(countDrawn(faded, kBackground), 0);
  EXPECT_EQ(countMatching(faded, red, 12), 0)
    << "opacity never reached the material";
}

TEST_F(Scene3DTest, AClassSwitchedOffInTheLegendIsNotDrawnInTheScene)
{
  // The legend and the scene read one LayerStyle, so a class hidden from the
  // legend has to disappear from the 3D view too. A scene that drew it anyway
  // would contradict the legend sitting beside it.
  MeshDefinition twoCells = ridge(0.0);

  QString message;
  std::unique_ptr<MeshLayer> owned = MeshLayer::create(
    QStringLiteral("classified"), twoCells, MeshEntity::Face, message);
  ASSERT_NE(owned, nullptr) << message.toStdString();

  // One value per face, so the two cells land in different classes.
  ASSERT_TRUE(
    owned->setValues(QStringLiteral("depth"), QVector<double>{ 1.0, 9.0 }));

  LayerStyle *style = owned->style();
  style->setMode(StyleMode::Graduated);
  style->setAttribute(QStringLiteral("depth"));
  style->classification().setMethod(ClassificationMethod::EqualInterval);
  style->classification().setClassCount(2);
  ASSERT_TRUE(style->rebuild(*owned));
  ASSERT_EQ(style->classification().breaks().size(), 2);

  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  MeshLayer *layer = owned.release();
  ASSERT_GE(stack.addLayer(layer), 0);

  Camera view = topDown();

  const int both =
    countDrawn(renderScene(renderer, view, QStringLiteral("classes-both")),
               kBackground);
  ASSERT_GT(both, 0);

  const qsizetype allVertices =
    layer->sceneSource()->sceneGeometry({}).first().vertices.size();

  style->classification().setClassVisible(0, false);
  layer->notifyAppearanceChanged();

  const int remaining = countDrawn(
    renderScene(renderer, view, QStringLiteral("classes-one")), kBackground);

  EXPECT_LT(remaining, both * 3 / 4)
    << "hiding a class left " << remaining << " of " << both
    << " pixels drawn; the scene ignored the legend";
  EXPECT_GT(remaining, 0) << "hiding one class removed both";

  // And the hidden cell is left out of the geometry, not merely made
  // invisible. An unclassified feature's colour is invalid, whose alpha is
  // zero, so uploading it anyway would look identical here — and would still
  // put every hidden cell on the bus, which at this phase's cell counts is
  // the difference the check is for.
  EXPECT_LT(layer->sceneSource()->sceneGeometry({}).first().vertices.size(),
            allVertices)
    << "a hidden class was uploaded and then blended away";
}

// ── Geometry supply ─────────────────────────────────────────────────────────

TEST_F(Scene3DTest, ANodeMeshSuppliesNoSceneGeometry)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("nodes"), quadAt(0.0), MeshEntity::Node, message);
  ASSERT_NE(layer, nullptr);

  ASSERT_NE(layer->sceneSource(), nullptr);
  EXPECT_TRUE(layer->sceneSource()->sceneGeometry({}).isEmpty());
}

TEST_F(Scene3DTest, AnEdgeMeshSuppliesLines)
{
  MeshDefinition mesh = quadAt(5.0);
  mesh.edgeNodes = { { 0, 1 }, { 1, 2 } };

  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("edges"), mesh, MeshEntity::Edge, message);
  ASSERT_NE(layer, nullptr);

  const QVector<SceneGeometry> batches = layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);
  EXPECT_EQ(batches.first().primitive, ScenePrimitive::Lines);
  EXPECT_EQ(batches.first().indices.size(), 4);
  EXPECT_NEAR(double(batches.first().vertices.first().z), 5.0, 1.0e-6);
}

TEST_F(Scene3DTest, SceneBoundsSpanTheMeshElevations)
{
  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("ridge"), ridge(40.0), MeshEntity::Face, message);
  ASSERT_NE(layer, nullptr);

  const Bounds3D bounds = layer->sceneSource()->sceneBounds();

  ASSERT_TRUE(bounds.isValid());
  EXPECT_NEAR(double(bounds.minimum().z()), 0.0, 1.0e-6);
  EXPECT_NEAR(double(bounds.maximum().z()), 40.0, 1.0e-6);
  EXPECT_EQ(bounds.footprint(), QRectF(0.0, 0.0, 100.0, 100.0));
}

TEST_F(Scene3DTest, FaceConnectivityIsWhatSuppliesEachCornerItsElevation)
{
  // The ring the map draws carries a closing duplicate the connectivity does
  // not, so walking the two in step is off by one at the last corner unless
  // the closing vertex is skipped. A ridge is the shape that shows it: its
  // last corner sits at a different elevation from its first.
  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("ridge"), ridge(40.0), MeshEntity::Face, message);
  ASSERT_NE(layer, nullptr);

  const QVector<SceneGeometry> batches = layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);

  const SceneGeometry &geometry = batches.first();
  ASSERT_EQ(geometry.vertices.size(), 8);

  // Face 0 is nodes 0,1,4,3 with elevations 0,40,40,0 — in that order.
  const double expected[4] = { 0.0, 40.0, 40.0, 0.0 };

  for (int corner = 0; corner < 4; ++corner)
  {
    EXPECT_NEAR(double(geometry.vertices[corner].z), expected[corner], 1.0e-6)
      << "corner " << corner << " took its elevation from the wrong node";
  }
}

TEST_F(Scene3DTest, ANonPlanarFaceGetsOneNormalRatherThanAThreeCornerGuess)
{
  // A quad whose four nodes carry four different elevations is not planar; a
  // normal taken from any three of its corners depends on which three.
  MeshDefinition warped = quadAt(0.0);
  warped.nodeZ = { 0.0, 10.0, 0.0, 10.0 };

  QString message;
  const std::unique_ptr<MeshLayer> layer = MeshLayer::create(
    QStringLiteral("warped"), warped, MeshEntity::Face, message);
  ASSERT_NE(layer, nullptr);

  const QVector<SceneGeometry> batches = layer->sceneSource()->sceneGeometry({});
  ASSERT_EQ(batches.size(), 1);

  const SceneGeometry &geometry = batches.first();
  ASSERT_EQ(geometry.vertices.size(), 4);

  // Newell's normal of a saddle is vertical; a three-corner cross product is
  // not. The saddle is symmetric, so this distinguishes the two exactly.
  const QVector3D normal(geometry.vertices.first().nx,
                         geometry.vertices.first().ny,
                         geometry.vertices.first().nz);

  EXPECT_NEAR(double(normal.length()), 1.0, 1.0e-5);
  EXPECT_NEAR(double(normal.z()), 1.0, 1.0e-5)
    << "the normal came from three corners of a four-corner face";
}

TEST_F(Scene3DTest, AClockwiseFaceIsLitLikeAnyOther)
{
  // Winding is a property of whoever wrote the mesh, not of the terrain. A
  // surface that goes black when wound the other way reads as a hole.
  MeshDefinition clockwise = quadAt(0.0);
  std::reverse(clockwise.faceNodes.begin(), clockwise.faceNodes.end());

  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QColor red(220, 40, 40);
  ASSERT_NE(addOpaque(stack, QStringLiteral("cw"), clockwise, red), nullptr);

  const QImage image =
    renderScene(renderer, topDown(), QStringLiteral("clockwise"));
  ASSERT_FALSE(image.isNull());

  EXPECT_GT(countMatching(image, red, 12), kSize.height() * kSize.height() / 2)
    << "a clockwise face rendered dark or not at all";
}

TEST_F(Scene3DTest, ASurfaceSeenFromBelowIsLit)
{
  // Orbiting under a terrain is ordinary — it is how a user looks at the
  // underside of a floodplain or a pipe soffit. The light travels with the
  // eye, so from below it arrives from below, and a one-sided material would
  // return black for every face. This is what the material's two-sided term
  // is for, and it is the reason the geometry does not normalise winding.
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  const QColor red(220, 40, 40);
  ASSERT_NE(addOpaque(stack, QStringLiteral("quad"), quadAt(0.0), red),
            nullptr);

  Camera below = topDown();
  below.setElevation(-89.0);

  const QImage image = renderScene(renderer, below, QStringLiteral("below"));
  ASSERT_FALSE(image.isNull());

  // Its own colour, not merely "not the background": a one-sided material
  // still leaves the ambient term, which is dim but visible, and asking only
  // whether something was drawn would accept that.
  EXPECT_GT(countMatching(image, red, 12), kSize.height() * kSize.height() / 2)
    << "the surface was left at ambient when seen from underneath";
}

// ── Determinism ─────────────────────────────────────────────────────────────

TEST_F(Scene3DTest, TheSameSceneRendersToTheSameImage)
{
  // Hash stability is what makes a rendered image usable as a gate at all.
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);
  ASSERT_NE(addOpaque(stack, QStringLiteral("ridge"), ridge(30.0),
                      QColor(180, 190, 200)),
            nullptr);

  const Camera view = oblique(35.0, 40.0);

  const QImage first =
    renderScene(renderer, view, QStringLiteral("determinism-a"));
  const QImage second =
    renderScene(renderer, view, QStringLiteral("determinism-b"));

  ASSERT_FALSE(first.isNull());
  ASSERT_FALSE(second.isNull());

  // Two blank frames are also equal, so the gate needs a picture in them.
  ASSERT_GT(countDrawn(first, kBackground), kSize.width() * kSize.height() / 10);
  EXPECT_EQ(first, second);
}

TEST_F(Scene3DTest, StatisticsCountWhatWasDrawn)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);
  ASSERT_NE(addOpaque(stack, QStringLiteral("ridge"), ridge(10.0),
                      QColor(180, 190, 200)),
            nullptr);

  ASSERT_FALSE(
    renderScene(renderer, topDown(), QStringLiteral("statistics")).isNull());

  const SceneRenderer::Statistics statistics = renderer.statistics();

  EXPECT_EQ(statistics.batches, 1);
  EXPECT_EQ(statistics.vertices, 8);
  EXPECT_EQ(statistics.primitives, 4);
}

// ── Per-view visibility (coherence plan V4) ───────────────────────────────
//
// One checkbox used to govern both views, so there was no way to keep a
// layer on the map and out of the scene. isShownIn3D() narrows visibility
// for the scene only; the map never reads it.

TEST_F(Scene3DTest, ALayerKeptOutOf3dLeavesTheSceneButNotTheMap)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  MeshLayer *layer = addOpaque(stack, QStringLiteral("quad"), quadAt(0.0),
                               QColor(220, 40, 40));
  ASSERT_NE(layer, nullptr);

  ASSERT_GT(countDrawn(renderScene(renderer, topDown(),
                                   QStringLiteral("in3d")),
                       kBackground),
            0);

  layer->setShownIn3D(false);

  const QImage kept =
    renderScene(renderer, topDown(), QStringLiteral("keptout"));
  ASSERT_FALSE(kept.isNull());

  EXPECT_EQ(countDrawn(kept, kBackground), 0)
    << "the scene kept drawing a layer that was kept out of 3D";

  // The 2D half of visibility is untouched: the map still draws it.
  EXPECT_TRUE(layer->isVisible());
}

TEST_F(Scene3DTest, KeepingALayerOutOf3dRemovesItFromTheSceneBounds)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  MeshLayer *tall = addOpaque(stack, QStringLiteral("tall"), quadAt(80.0),
                              QColor(60, 60, 220));
  ASSERT_NE(addOpaque(stack, QStringLiteral("flat"), quadAt(0.0),
                      QColor(220, 40, 40)),
            nullptr);
  ASSERT_NE(tall, nullptr);

  ASSERT_GT(renderer.sceneBounds().maximum().z(), 40.0f);

  tall->setShownIn3D(false);

  EXPECT_LT(renderer.sceneBounds().maximum().z(), 40.0f)
    << "a layer kept out of 3D still frames the camera";
}

TEST_F(Scene3DTest, ATerrainKeptOutOf3dStopsBeingTheTerrain)
{
  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);

  MeshLayer *layer = addOpaque(stack, QStringLiteral("ridge"), ridge(40.0),
                               QColor(120, 120, 120));
  ASSERT_NE(layer, nullptr);
  ASSERT_NE(renderer.terrain(), nullptr);

  layer->setShownIn3D(false);

  EXPECT_EQ(renderer.terrain(), nullptr)
    << "everything still drapes on a surface that is not in the scene";
}
