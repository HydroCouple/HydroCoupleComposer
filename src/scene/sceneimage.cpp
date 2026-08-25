#include "scene/sceneimage.h"

#include "scene/camera.h"
#include "scene/scenerenderer.h"

#include <QOffscreenSurface>

#include <rhi/qrhi.h>

#include <memory>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief Creates the platform's graphics device, with no window.
     *
     * The backend is chosen the way QRhiWidget chooses it, so an exported or
     * tested image comes off the same driver as the on-screen view rather
     * than off a fallback that might shade differently.
     */
    std::unique_ptr<QRhi> createDevice(
      std::unique_ptr<QOffscreenSurface> &surface)
    {
      Q_UNUSED(surface)

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
      QRhiMetalInitParams metal;

      return std::unique_ptr<QRhi>(QRhi::create(QRhi::Metal, &metal));
#elif defined(Q_OS_WIN)
      QRhiD3D11InitParams d3d;

      return std::unique_ptr<QRhi>(QRhi::create(QRhi::D3D11, &d3d));
#else
      // OpenGL needs something to be current on, and a QOffscreenSurface is
      // that something; it must outlive the QRhi, hence the caller's handle.
      surface.reset(QRhiGles2InitParams::newFallbackSurface());

      QRhiGles2InitParams gles;
      gles.fallbackSurface = surface.get();

      return std::unique_ptr<QRhi>(QRhi::create(QRhi::OpenGLES2, &gles));
#endif
    }

  }

  QImage renderSceneToImage(SceneRenderer &renderer, const Camera &camera,
                            const QSize &size, const QColor &background,
                            QString &message)
  {
    if (size.isEmpty())
    {
      message = QObject::tr("The requested image size is empty.");

      return {};
    }

    std::unique_ptr<QOffscreenSurface> surface;
    const std::unique_ptr<QRhi> rhi = createDevice(surface);

    if (!rhi)
    {
      message = QObject::tr("No graphics device is available.");

      return {};
    }

    const std::unique_ptr<QRhiTexture> color(rhi->newTexture(
      QRhiTexture::RGBA8, size, 1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));

    const std::unique_ptr<QRhiRenderBuffer> depth(
      rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));

    if (!color->create() || !depth->create())
    {
      message = QObject::tr("The graphics device rejected the image buffers.");

      return {};
    }

    QRhiTextureRenderTargetDescription description(
      QRhiColorAttachment(color.get()));
    description.setDepthStencilBuffer(depth.get());

    const std::unique_ptr<QRhiTextureRenderTarget> target(
      rhi->newTextureRenderTarget(description));

    const std::unique_ptr<QRhiRenderPassDescriptor> pass(
      target->newCompatibleRenderPassDescriptor());

    target->setRenderPassDescriptor(pass.get());

    if (!target->create())
    {
      message = QObject::tr("The graphics device rejected the render target.");

      return {};
    }

    // Resources belong to this device, which did not exist a moment ago, so
    // anything the renderer holds from a previous call must go.
    renderer.releaseResources();

    if (!renderer.initialize(rhi.get(), pass.get(), 1, message))
    {
      return {};
    }

    QRhiCommandBuffer *commands = nullptr;

    if (rhi->beginOffscreenFrame(&commands) != QRhi::FrameOpSuccess)
    {
      message = QObject::tr("The graphics device would not begin a frame.");
      renderer.releaseResources();

      return {};
    }

    renderer.render(commands, target.get(), camera, background);

    QRhiReadbackResult readback;
    QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();
    updates->readBackTexture({ color.get() }, &readback);
    commands->resourceUpdate(updates);

    rhi->endOffscreenFrame();

    // Released before the device goes away: the resources it holds are the
    // device's, and outliving it is a crash rather than a leak.
    renderer.releaseResources();

    if (readback.data.isEmpty())
    {
      message = QObject::tr("The graphics device returned no pixels.");

      return {};
    }

    const QImage borrowed(
      reinterpret_cast<const uchar *>(readback.data.constData()),
      readback.pixelSize.width(), readback.pixelSize.height(),
      QImage::Format_RGBA8888_Premultiplied);

    // Copied because the image above only borrows the readback's bytes, and
    // those go out of scope with this function.
    return borrowed.copy();
  }

} // namespace HydroCouple::Composer
