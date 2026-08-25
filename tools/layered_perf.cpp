// Perf probe: how a ~500k-cell layered mesh behaves through the C3b path.
#include "layers/layeredmesh.h"
#include "layers/meshlayer.h"
#include "map/layerstackmodel.h"
#include "scene/camera.h"
#include "scene/sceneimage.h"
#include "scene/scenerenderer.h"

#include <QApplication>
#include <QElapsedTimer>

#include <rhi/qrhi.h>

#include <cstdio>
#include <cmath>

using namespace HydroCouple::Composer;
using HydroCouple::SDK::IO::MeshDefinition;

int main(int argc, char **argv)
{
  QApplication app(argc, argv);

  const int side = argc > 1 ? atoi(argv[1]) : 224;   // columns per side
  const int layers = argc > 2 ? atoi(argv[2]) : 10;

  MeshDefinition mesh;
  mesh.meshName = "grid";

  for (int y = 0; y <= side; ++y)
    for (int x = 0; x <= side; ++x)
    {
      mesh.nodeX.push_back(double(x));
      mesh.nodeY.push_back(double(y));
      mesh.nodeZ.push_back(-10.0 - 5.0 * std::sin(x * 0.05) * std::cos(y * 0.05));
    }

  const auto node = [side](int x, int y) { return int64_t(y) * (side + 1) + x; };

  mesh.faceNodeOffsets.push_back(0);
  for (int y = 0; y < side; ++y)
    for (int x = 0; x < side; ++x)
    {
      mesh.faceNodes.push_back(node(x, y));
      mesh.faceNodes.push_back(node(x + 1, y));
      mesh.faceNodes.push_back(node(x + 1, y + 1));
      mesh.faceNodes.push_back(node(x, y + 1));
      mesh.faceNodeOffsets.push_back(int64_t(mesh.faceNodes.size()));
    }

  const int64_t columns = mesh.faceCount();

  std::vector<double> sigma(layers + 1), depth(columns), surface(columns, 0.0);
  for (int k = 0; k <= layers; ++k) sigma[k] = -double(k) / layers;
  for (int64_t c = 0; c < columns; ++c)
  {
    const int64_t from = mesh.faceNodeOffsets[c];
    double bed = 0.0;
    for (int i = 0; i < 4; ++i) bed += mesh.nodeZ[mesh.faceNodes[from + i]];
    depth[c] = -bed / 4.0;
  }

  QString message;
  const LayeredMesh layered =
    LayeredMesh::fromCfSigma(mesh, sigma, depth, surface, message);

  std::unique_ptr<MeshLayer> owned =
    MeshLayer::create(QStringLiteral("perf"), mesh, MeshEntity::Face, message);
  if (!owned) { std::printf("create failed: %s\n", qPrintable(message)); return 2; }
  if (!owned->setLayering(layered, message))
  { std::printf("layering failed: %s\n", qPrintable(message)); return 2; }

  std::printf("columns=%lld layers=%d cells=%lld\n",
              (long long)columns, layers, (long long)layered.cellCount());

  QElapsedTimer timer;
  timer.start();
  const QVector<SceneGeometry> batches = owned->sceneSource()->sceneGeometry({});
  const qint64 buildMs = timer.elapsed();

  qint64 verts = 0, tris = 0;
  for (const auto &g : batches) { verts += g.vertices.size(); tris += g.indices.size() / 3; }

  std::printf("build=%lldms  vertices=%lld  triangles=%lld  vram=%.1fMB\n",
              (long long)buildMs, (long long)verts, (long long)tris,
              double(verts * sizeof(SceneVertex) + tris * 3 * 4) / 1048576.0);

  LayerStackModel stack;
  SceneRenderer renderer;
  renderer.setModel(&stack);
  MeshLayer *raw = owned.release();
  stack.addLayer(raw);

  Camera camera;
  camera.fitTo(renderer.sceneBounds(), 4.0 / 3.0);
  camera.setElevation(30.0);

  timer.restart();
  const QImage image =
    renderSceneToImage(renderer, camera, QSize(1280, 960), QColor(0, 0, 0), message);
  std::printf("first frame (device create + upload + draw + readback) = %lldms%s\n",
              (long long)timer.elapsed(), image.isNull() ? "  [NULL]" : "");
  if (image.isNull()) std::printf("  %s\n", qPrintable(message));

  // What interaction actually costs: one device, one upload, then frames.
  // This is SceneView's path; renderSceneToImage's per-call device creation
  // and readback are an export cost, not an interaction one.
  {
    QRhiMetalInitParams params;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Metal, &params));

    if (rhi)
    {
      const QSize size(1280, 960);
      std::unique_ptr<QRhiTexture> color(rhi->newTexture(
        QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
      std::unique_ptr<QRhiRenderBuffer> depthBuffer(
        rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
      color->create();
      depthBuffer->create();

      QRhiTextureRenderTargetDescription desc((QRhiColorAttachment(color.get())));
      desc.setDepthStencilBuffer(depthBuffer.get());
      std::unique_ptr<QRhiTextureRenderTarget> target(
        rhi->newTextureRenderTarget(desc));
      std::unique_ptr<QRhiRenderPassDescriptor> pass(
        target->newCompatibleRenderPassDescriptor());
      target->setRenderPassDescriptor(pass.get());
      target->create();

      renderer.releaseResources();
      QString rendererMessage;
      renderer.initialize(rhi.get(), pass.get(), 1, rendererMessage);

      // One frame to build and upload, then time the rest.
      QRhiCommandBuffer *cb = nullptr;
      rhi->beginOffscreenFrame(&cb);
      renderer.render(cb, target.get(), camera, QColor(0, 0, 0));
      rhi->endOffscreenFrame();

      constexpr int kFrames = 60;
      timer.restart();

      for (int frame = 0; frame < kFrames; ++frame)
      {
        Camera orbited = camera;
        orbited.orbit(frame * 2.0, 0.0);

        rhi->beginOffscreenFrame(&cb);
        renderer.render(cb, target.get(), orbited, QColor(0, 0, 0));
        rhi->endOffscreenFrame();
      }

      const double perFrame = double(timer.elapsed()) / kFrames;
      std::printf("INTERACTION: %d orbit frames, cached geometry = %.2fms each"
                  "  (%.0f fps)\n", kFrames, perFrame,
                  perFrame > 0.0 ? 1000.0 / perFrame : 0.0);

      renderer.releaseResources();
    }
  }

  // Steady-state: the same scene again, on a fresh device each time, so the
  // difference from the first is only what caching removed.
  timer.restart();
  for (int frame = 0; frame < 3; ++frame)
    renderSceneToImage(renderer, camera, QSize(1280, 960), QColor(0, 0, 0), message);
  std::printf("3 more full frames = %lldms (%.0fms each)\n",
              (long long)timer.elapsed(), timer.elapsed() / 3.0);

  // What a peel costs: dragging a layer slider invalidates the cache, so
  // this is the rebuild that happens between one frame and the next.
  qint64 worst = 0;
  for (int layer = 0; layer < layers; ++layer)
  {
    raw->setVisibleLayers(layer, layer);
    timer.restart();
    const auto peeled = raw->sceneSource()->sceneGeometry({});
    worst = std::max(worst, timer.elapsed());
  }
  std::printf("peel rebuild (worst of %d) = %lldms\n", layers, (long long)worst);

  raw->setVisibleLayers(0, layers - 1);
  timer.restart();
  const auto full = raw->sceneSource()->sceneGeometry({});
  std::printf("full-stack rebuild = %lldms\n", (long long)timer.elapsed());

  return 0;
}
