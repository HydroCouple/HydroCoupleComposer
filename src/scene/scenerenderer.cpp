#include "scene/scenerenderer.h"

#include "map/layerstackmodel.h"
#include "map/maplayer.h"
#include "scene/axisgizmo.h"
#include "scene/scenesource.h"

#include <QFile>
#include <QMatrix4x4>

#include <rhi/qrhi.h>

#include <algorithm>

namespace HydroCouple::Composer
{
  namespace
  {
    //! mat4 mvp, mat4 normalMatrix, vec4 lightDirection, vec4 params,
    //! vec4 texMap.
    constexpr int kUniformBlockSize = 64 + 64 + 16 + 16 + 16;

    /*!
     * \brief One step toward the eye, in clip space.
     *
     * Much of this scene is coplanar with itself by construction: a mesh's
     * edges lie on its own faces, a draped network lies on the terrain it was
     * sampled from, and a draped basemap lies on that same terrain. Their
     * depths then differ only by how each interpolator rounded, and which one
     * wins changes with the camera angle — measured on one flat sheet, a
     * draped line is drawn at 89 degrees of elevation and gone at 90, which
     * is exactly the view the map hands over.
     *
     * Neither of the two obvious remedies works for the lines. Depth bias is
     * applied by Metal, D3D11 and Vulkan to filled primitives only, so a
     * biased line pipeline is inert — measured, pixel-identical with and
     * without. And a LessOrEqual compare only covers exact ties, which this
     * is not.
     *
     * Nudging in the shader keeps the geometry honest, which is the point:
     * the vertices stay on the surface they claim to be on, so measuring one
     * gives back the elevation it was sampled at, and only the depth written
     * for it moves. Small enough that anything genuinely behind a hill still
     * loses — a hundredth of a percent of the depth range.
     */
    constexpr float kCoplanarNudge = 1.0e-4f;

    //! How many steps out of the surface a batch is drawn.
    float nudgeSteps(bool textured, bool lines)
    {
      // A ground plane takes one step, off the terrain it was draped on. A
      // line takes two, so that a network drawn over a draped basemap is in
      // front of it rather than tied with it — a single shared step would
      // simply move the conflict up one layer.
      if (lines)
      {
        return 2.0f;
      }

      return textured ? 1.0f : 0.0f;
    }

    QShader loadShader(const QString &path)
    {
      QFile file(path);

      if (!file.open(QIODevice::ReadOnly))
      {
        return {};
      }

      return QShader::fromSerialized(file.readAll());
    }

  }

  SceneRenderer::SceneRenderer(QObject *parent) : QObject(parent)
  {
  }

  SceneRenderer::~SceneRenderer()
  {
    releaseResources();
  }

  void SceneRenderer::setModel(LayerStackModel *model)
  {
    if (m_model == model)
    {
      return;
    }

    if (m_model)
    {
      disconnect(m_model, nullptr, this, nullptr);
    }

    m_model = model;

    if (m_model)
    {
      // The stack is the single owner of what is drawn, so the scene learns
      // about a hidden layer or a restyle the same way the map does rather
      // than through the widget that made the change.
      connect(m_model, &LayerStackModel::renderChanged, this,
              &SceneRenderer::invalidate);
    }

    invalidate();
  }

  LayerStackModel *SceneRenderer::model() const
  {
    return m_model;
  }

  void SceneRenderer::invalidate()
  {
    m_batchesValid = false;

    Q_EMIT sceneChanged();
  }

  double SceneRenderer::ambient() const
  {
    return m_ambient;
  }

  void SceneRenderer::setAmbient(double ambient)
  {
    m_ambient = std::clamp(ambient, 0.0, 1.0);
  }

  Bounds3D SceneRenderer::sceneBounds() const
  {
    Bounds3D bounds;

    if (!m_model)
    {
      return bounds;
    }

    for (const MapLayer *layer : m_model->renderOrder())
    {
      if (!layer->isVisible() || !layer->isShownIn3D())
      {
        continue;
      }

      // sceneSource() is the layer's opt-in to being DRAWN; bounds answer
      // the different question of where the data IS. A point layer opts out
      // of drawing -- it has no 3D form yet -- but its footprint still
      // belongs in the framing, or "Zoom to Full Extent" frames a different
      // world in each view. Backdrops stay excluded in both, by their own
      // answer: a basemap's sceneBounds() is empty on purpose.
      const auto *source = layer->sceneSource();

      if (!source)
      {
        source = dynamic_cast<const ISceneSource *>(layer);
      }

      if (source)
      {
        bounds.expandTo(source->sceneBounds());
      }
    }

    return bounds;
  }

  const ITerrainSource *SceneRenderer::terrain() const
  {
    if (!m_model)
    {
      return nullptr;
    }

    // The model holds the election, so the tree's badge, the dialog's
    // checkbox and this renderer cannot disagree about who won.
    const MapLayer *elected = m_model->electedTerrainLayer();

    if (!elected)
    {
      return nullptr;
    }

    const ISceneSource *source = elected->sceneSource();

    return source ? source->terrain() : nullptr;
  }

  bool SceneRenderer::initialize(QRhi *rhi, QRhiRenderPassDescriptor *descriptor,
                                 int sampleCount, QString &message)
  {
    if (!rhi || !descriptor)
    {
      message = tr("No graphics device.");

      return false;
    }

    // A pipeline belongs to the device and the render pass it was built for,
    // so a change to either invalidates every one of them.
    if (m_rhi == rhi && m_descriptor == descriptor &&
        m_sampleCount == sampleCount && m_trianglePipeline)
    {
      return true;
    }

    releaseResources();

    m_rhi = rhi;
    m_descriptor = descriptor;
    m_sampleCount = sampleCount;

    const QShader vertex =
      loadShader(QStringLiteral(":/shaders/scene.vert.qsb"));
    const QShader fragment =
      loadShader(QStringLiteral(":/shaders/scene.frag.qsb"));
    const QShader groundVertex =
      loadShader(QStringLiteral(":/shaders/ground.vert.qsb"));
    const QShader groundFragment =
      loadShader(QStringLiteral(":/shaders/ground.frag.qsb"));

    if (!vertex.isValid() || !fragment.isValid() || !groundVertex.isValid() ||
        !groundFragment.isValid())
    {
      message = tr("The scene shaders are missing from the build.");
      m_rhi = nullptr;

      return false;
    }

    QRhiVertexInputLayout layout;
    layout.setBindings({ { sizeof(SceneVertex) } });
    layout.setAttributes(
      { { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float) },
        { 0, 2, QRhiVertexInputAttribute::Float4, 6 * sizeof(float) } });

    // The bindings are only a layout at pipeline-creation time, so one
    // throwaway set describes every batch's; each batch then supplies its own
    // buffer through a compatible set of its own.
    std::unique_ptr<QRhiBuffer> layoutBuffer(rhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBlockSize));

    if (!layoutBuffer->create())
    {
      message = tr("The graphics device rejected a uniform buffer.");
      m_rhi = nullptr;

      return false;
    }

    std::unique_ptr<QRhiShaderResourceBindings> layoutBindings(
      rhi->newShaderResourceBindings());
    layoutBindings->setBindings({ QRhiShaderResourceBinding::uniformBuffer(
      0,
      QRhiShaderResourceBinding::VertexStage |
        QRhiShaderResourceBinding::FragmentStage,
      layoutBuffer.get()) });

    if (!layoutBindings->create())
    {
      message = tr("The graphics device rejected a resource binding.");
      m_rhi = nullptr;

      return false;
    }

    // The textured pipeline's bindings carry a sampler as well, and a
    // pipeline is built against a binding *layout* — which is why the ground
    // needs a pipeline of its own rather than a branch in the shared shader.
    std::unique_ptr<QRhiTexture> layoutTexture(
      rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1)));
    std::unique_ptr<QRhiSampler> layoutSampler(rhi->newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));

    if (!layoutTexture->create() || !layoutSampler->create())
    {
      message = tr("The graphics device rejected a texture.");
      m_rhi = nullptr;

      return false;
    }

    std::unique_ptr<QRhiShaderResourceBindings> groundLayoutBindings(
      rhi->newShaderResourceBindings());
    groundLayoutBindings->setBindings(
      { QRhiShaderResourceBinding::uniformBuffer(
          0,
          QRhiShaderResourceBinding::VertexStage |
            QRhiShaderResourceBinding::FragmentStage,
          layoutBuffer.get()),
        QRhiShaderResourceBinding::sampledTexture(
          1, QRhiShaderResourceBinding::FragmentStage, layoutTexture.get(),
          layoutSampler.get()) });

    if (!groundLayoutBindings->create())
    {
      message = tr("The graphics device rejected a resource binding.");
      m_rhi = nullptr;

      return false;
    }

    const auto makePipeline =
      [&](QRhiGraphicsPipeline::Topology topology, bool textured)
      -> std::unique_ptr<QRhiGraphicsPipeline>
    {
      std::unique_ptr<QRhiGraphicsPipeline> pipeline(
        rhi->newGraphicsPipeline());

      pipeline->setTopology(topology);
      pipeline->setShaderStages(
        { { QRhiShaderStage::Vertex, textured ? groundVertex : vertex },
          { QRhiShaderStage::Fragment,
            textured ? groundFragment : fragment } });
      pipeline->setVertexInputLayout(layout);
      pipeline->setShaderResourceBindings(
        textured ? groundLayoutBindings.get() : layoutBindings.get());
      pipeline->setRenderPassDescriptor(descriptor);
      pipeline->setSampleCount(sampleCount);

      // Depth, not painter's order: the whole point of the 3D view is that a
      // hill in front hides what is behind it, which draw order cannot express.
      pipeline->setDepthTest(true);
      pipeline->setDepthWrite(true);

      // No culling. A mesh read from a file may be wound either way, and a
      // terrain that vanishes when orbited past a certain angle is a far
      // worse failure than drawing a few back faces.
      pipeline->setCullMode(QRhiGraphicsPipeline::None);

      QRhiGraphicsPipeline::TargetBlend blend;
      blend.enable = true;
      blend.srcColor = QRhiGraphicsPipeline::One;
      blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
      blend.srcAlpha = QRhiGraphicsPipeline::One;
      blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
      pipeline->setTargetBlends({ blend });

      return pipeline->create() ? std::move(pipeline) : nullptr;
    };

    m_trianglePipeline = makePipeline(QRhiGraphicsPipeline::Triangles, false);
    m_linePipeline = makePipeline(QRhiGraphicsPipeline::Lines, false);
    m_groundPipeline = makePipeline(QRhiGraphicsPipeline::Triangles, true);

    if (!m_trianglePipeline || !m_linePipeline || !m_groundPipeline)
    {
      message = tr("The graphics device rejected the scene pipeline.");
      releaseResources();

      return false;
    }

    invalidate();

    return true;
  }

  void SceneRenderer::releaseResources()
  {
    m_batches.clear();
    m_batchesValid = false;
    m_gizmo = {};
    m_trianglePipeline.reset();
    m_linePipeline.reset();
    m_groundPipeline.reset();
    m_rhi = nullptr;
    m_descriptor = nullptr;
  }

  void SceneRenderer::rebuildBatches()
  {
    m_batches.clear();
    m_batchesValid = true;

    if (!m_rhi || !m_model)
    {
      return;
    }

    // Resolved once for the whole stack, before anything is built: draping is
    // a property of the composition, and a layer that went looking for its own
    // terrain would be a layer that knows what else is in the stack.
    SceneContext context;
    context.terrain = terrain();

    // What the scene is about. sceneBounds() already answers this correctly
    // without a rule of its own: a backdrop reports no bounds, on the same
    // reasoning that keeps the map from framing the planet.
    context.focus = sceneBounds().footprint();

    for (const MapLayer *layer : m_model->renderOrder())
    {
      if (!layer->isVisible() || !layer->isShownIn3D())
      {
        continue;
      }

      const ISceneSource *source = layer->sceneSource();

      if (!source)
      {
        continue;
      }

      for (const SceneGeometry &geometry : source->sceneGeometry(context))
      {
        if (geometry.isEmpty())
        {
          continue;
        }

        Batch batch;
        batch.primitive = geometry.primitive;
        batch.opacity = float(layer->opacity());
        batch.indexCount = quint32(geometry.indices.size());
        batch.vertexCount = quint32(geometry.vertices.size());

        const quint32 vertexBytes =
          quint32(geometry.vertices.size() * sizeof(SceneVertex));
        const quint32 indexBytes =
          quint32(geometry.indices.size() * sizeof(quint32));

        batch.vertexBuffer.reset(m_rhi->newBuffer(
          QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, vertexBytes));
        batch.indexBuffer.reset(m_rhi->newBuffer(
          QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, indexBytes));
        batch.uniformBuffer.reset(m_rhi->newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBlockSize));

        if (!batch.vertexBuffer->create() || !batch.indexBuffer->create() ||
            !batch.uniformBuffer->create())
        {
          continue;
        }

        // Held until the first frame, because a resource update batch can
        // only be submitted inside a frame and this may run outside one.
        QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();

        if (!geometry.texture.isNull() && !geometry.textureExtent.isEmpty())
        {
          // RGBA8888 premultiplied, because that is the one layout QRhi
          // uploads without reinterpreting, and the target's blend state
          // expects premultiplied colour anyway. The painter that drew this
          // image already worked in it, so on most paths this is a swizzle.
          const QImage image = geometry.texture.convertToFormat(
            QImage::Format_RGBA8888_Premultiplied);

          batch.texture.reset(
            m_rhi->newTexture(QRhiTexture::RGBA8, image.size()));
          batch.sampler.reset(m_rhi->newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));

          if (!batch.texture->create() || !batch.sampler->create())
          {
            continue;
          }

          updates->uploadTexture(batch.texture.get(), image);

          const QRectF box = geometry.textureExtent.normalized();

          // Origin at the western, *northern* corner — bottom() in Qt's
          // y-down rectangle is the largest y, which in a north-up world is
          // the north edge. Inverse sizes, so the shader multiplies.
          batch.textureMap[0] = float(box.left());
          batch.textureMap[1] = float(box.bottom());
          batch.textureMap[2] = float(1.0 / box.width());
          batch.textureMap[3] = float(1.0 / box.height());
        }

        batch.bindings.reset(m_rhi->newShaderResourceBindings());

        if (batch.texture)
        {
          batch.bindings->setBindings(
            { QRhiShaderResourceBinding::uniformBuffer(
                0,
                QRhiShaderResourceBinding::VertexStage |
                  QRhiShaderResourceBinding::FragmentStage,
                batch.uniformBuffer.get()),
              QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage,
                batch.texture.get(), batch.sampler.get()) });
        }
        else
        {
          batch.bindings->setBindings(
            { QRhiShaderResourceBinding::uniformBuffer(
              0,
              QRhiShaderResourceBinding::VertexStage |
                QRhiShaderResourceBinding::FragmentStage,
              batch.uniformBuffer.get()) });
        }

        if (!batch.bindings->create())
        {
          continue;
        }

        updates->uploadStaticBuffer(batch.vertexBuffer.get(),
                                    geometry.vertices.constData());
        updates->uploadStaticBuffer(batch.indexBuffer.get(),
                                    geometry.indices.constData());
        m_pendingUpdates.push_back(updates);

        m_batches.push_back(std::move(batch));
      }
    }
  }

  void SceneRenderer::setAxisGizmoViewport(const QRect &deviceRect)
  {
    m_gizmoRect = deviceRect;
  }

  bool SceneRenderer::ensureAxisGizmo(QRhiResourceUpdateBatch *updates)
  {
    if (m_gizmo.indexCount > 0)
    {
      return true;
    }

    // A device that refused the cue once is not asked again every frame.
    if (m_gizmo.refused || !m_rhi)
    {
      return false;
    }

    const SceneGeometry geometry = buildAxisGizmo();

    if (geometry.isEmpty())
    {
      m_gizmo.refused = true;

      return false;
    }

    const quint32 vertexBytes =
      quint32(geometry.vertices.size() * int(sizeof(SceneVertex)));
    const quint32 indexBytes =
      quint32(geometry.indices.size() * int(sizeof(quint32)));

    m_gizmo.vertexBuffer.reset(m_rhi->newBuffer(
      QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, vertexBytes));
    m_gizmo.indexBuffer.reset(m_rhi->newBuffer(
      QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, indexBytes));
    m_gizmo.uniformBuffer.reset(m_rhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBlockSize));

    if (!m_gizmo.vertexBuffer->create() || !m_gizmo.indexBuffer->create() ||
        !m_gizmo.uniformBuffer->create())
    {
      m_gizmo = {};
      m_gizmo.refused = true;

      return false;
    }

    m_gizmo.bindings.reset(m_rhi->newShaderResourceBindings());
    m_gizmo.bindings->setBindings({ QRhiShaderResourceBinding::uniformBuffer(
      0,
      QRhiShaderResourceBinding::VertexStage |
        QRhiShaderResourceBinding::FragmentStage,
      m_gizmo.uniformBuffer.get()) });

    if (!m_gizmo.bindings->create())
    {
      m_gizmo = {};
      m_gizmo.refused = true;

      return false;
    }

    // Uploaded into the frame's own batch rather than queued in
    // m_pendingUpdates, which render() has already drained by the time it
    // asks for the cue: queueing here would hold the geometry back a
    // frame and draw the first one from buffers nothing had written.
    updates->uploadStaticBuffer(m_gizmo.vertexBuffer.get(),
                                geometry.vertices.constData());
    updates->uploadStaticBuffer(m_gizmo.indexBuffer.get(),
                                geometry.indices.constData());

    m_gizmo.indexCount = quint32(geometry.indices.size());

    return true;
  }

  void SceneRenderer::drawAxisGizmo(QRhiCommandBuffer *cb,
                                    const QSize &pixelSize)
  {
    if (m_gizmoRect.width() <= 0 || m_gizmoRect.height() <= 0
        || m_gizmo.indexCount == 0 || !m_trianglePipeline)
    {
      return;
    }

    cb->setGraphicsPipeline(m_trianglePipeline.get());

    // QRhi takes viewports in OpenGL's bottom-left origin, and the rect
    // arrives in the widget's top-left one. Without this line the cue
    // appears in the corner diagonally opposite the one the user chose,
    // which is the sort of bug that looks like a wrong preference rather
    // than a wrong renderer.
    const int bottomUpY =
      pixelSize.height() - m_gizmoRect.y() - m_gizmoRect.height();

    // The cue has to sit in front of the scene, and the depth buffer
    // already holds the scene's depths — this pass cleared it once, at
    // the top, and QRhi gives no way to clear a region part-way through.
    //
    // So rather than turning depth off, which would make the cue's own
    // arms draw in submission order and put Up in front of East whichever
    // way the camera faced, the viewport's depth range is squeezed into
    // the nearest hundredth of the buffer. Every fragment of the cue then
    // beats every fragment of the scene, while the arms keep their depths
    // *relative to each other* and occlude one another correctly. It is
    // the one thing a viewport can do that a pipeline cannot.
    constexpr float kGizmoDepth = 0.01f;

    cb->setViewport({ float(m_gizmoRect.x()), float(bottomUpY),
                      float(m_gizmoRect.width()),
                      float(m_gizmoRect.height()), 0.0f, kGizmoDepth });

    cb->setShaderResources(m_gizmo.bindings.get());

    const QRhiCommandBuffer::VertexInput input(m_gizmo.vertexBuffer.get(), 0);
    cb->setVertexInput(0, 1, &input, m_gizmo.indexBuffer.get(), 0,
                       QRhiCommandBuffer::IndexUInt32);
    cb->drawIndexed(m_gizmo.indexCount);

    // Deliberately not counted in the statistics. Those are there to tell
    // the user how heavy their scene is, and a fixed three-arm cue that
    // is always the same size would only be noise in that number.
  }

  void SceneRenderer::render(QRhiCommandBuffer *cb, QRhiRenderTarget *target,
                             const Camera &camera, const QColor &background)
  {
    m_statistics = {};

    if (!cb || !target || !m_rhi || !m_trianglePipeline)
    {
      return;
    }

    if (!m_batchesValid)
    {
      rebuildBatches();
    }

    QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();

    for (QRhiResourceUpdateBatch *pending : m_pendingUpdates)
    {
      updates->merge(pending);
    }

    m_pendingUpdates.clear();

    const QSize pixelSize = target->pixelSize();
    const double aspect =
      pixelSize.height() > 0
        ? double(pixelSize.width()) / double(pixelSize.height())
        : 1.0;

    // The backend's clip-space convention, applied here rather than in the
    // camera: it differs between Metal, Vulkan and OpenGL, and a camera that
    // knew about it would produce different numbers on different machines.
    const QMatrix4x4 mvp =
      m_rhi->clipSpaceCorrMatrix() * camera.modelViewProjection(aspect);

    const QMatrix4x4 normalMatrix =
      camera.modelMatrix().inverted().transposed();

    // A headlight: the light travels with the eye, so nothing the user turns
    // toward is in shadow. A fixed sun leaves half of every orbit unreadable.
    const QVector3D toLight = (camera.eye() - camera.target()).normalized();

    for (Batch &batch : m_batches)
    {
      float block[kUniformBlockSize / sizeof(float)] = {};

      std::copy_n(mvp.constData(), 16, block);
      std::copy_n(normalMatrix.constData(), 16, block + 16);

      block[32] = toLight.x();
      block[33] = toLight.y();
      block[34] = toLight.z();
      block[35] = 0.0f;
      block[36] = batch.opacity;
      block[37] = float(m_ambient);
      block[38] =
        kCoplanarNudge * nudgeSteps(batch.texture != nullptr,
                                    batch.primitive == ScenePrimitive::Lines);

      std::copy_n(batch.textureMap, 4, block + 40);

      updates->updateDynamicBuffer(batch.uniformBuffer.get(), 0,
                                   kUniformBlockSize, block);
    }

    // The cue's own uniform, written here with the rest because a
    // resource update batch is submitted by beginPass and there is no
    // second chance at it once the pass is open.
    if (!m_gizmoRect.isEmpty() && ensureAxisGizmo(updates))
    {
      float block[kUniformBlockSize / sizeof(float)] = {};

      // The camera's rotation and nothing else. No model matrix, so the
      // cue is untouched by vertical exaggeration — a north arrow that
      // stretched when the user exaggerated the terrain would be telling
      // them something false about the terrain.
      const QMatrix4x4 gizmoMvp =
        m_rhi->clipSpaceCorrMatrix()
        * axisGizmoMatrix(camera.azimuth(), camera.elevation());

      const QMatrix4x4 identity;

      std::copy_n(gizmoMvp.constData(), 16, block);
      std::copy_n(identity.constData(), 16, block + 16);

      // The same headlight the scene uses, so the arm facing the user is
      // the lit one at every angle.
      block[32] = toLight.x();
      block[33] = toLight.y();
      block[34] = toLight.z();
      block[35] = 0.0f;
      block[36] = 1.0f;
      block[37] = float(m_ambient);

      // No coplanar nudge: nothing in the cue is coplanar with anything,
      // and it is drawn into a depth range of its own in any case.
      block[38] = 0.0f;

      updates->updateDynamicBuffer(m_gizmo.uniformBuffer.get(), 0,
                                   kUniformBlockSize, block);
    }

    cb->beginPass(target, background, { 1.0f, 0 }, updates);

    for (const Batch &batch : m_batches)
    {
      QRhiGraphicsPipeline *pipeline = m_trianglePipeline.get();

      if (batch.primitive == ScenePrimitive::Lines)
      {
        pipeline = m_linePipeline.get();
      }
      else if (batch.texture)
      {
        pipeline = m_groundPipeline.get();
      }

      cb->setGraphicsPipeline(pipeline);
      cb->setViewport({ 0.0f, 0.0f, float(pixelSize.width()),
                        float(pixelSize.height()) });
      cb->setShaderResources(batch.bindings.get());

      const QRhiCommandBuffer::VertexInput input(batch.vertexBuffer.get(), 0);
      cb->setVertexInput(0, 1, &input, batch.indexBuffer.get(), 0,
                         QRhiCommandBuffer::IndexUInt32);
      cb->drawIndexed(batch.indexCount);

      ++m_statistics.batches;
      m_statistics.vertices += batch.vertexCount;
      m_statistics.primitives +=
        batch.primitive == ScenePrimitive::Lines ? batch.indexCount / 2
                                                 : batch.indexCount / 3;
    }

    // Last, so that it is drawn over a scene that has finished writing
    // its depths rather than into the middle of them.
    drawAxisGizmo(cb, pixelSize);

    cb->endPass();
  }

  SceneRenderer::Statistics SceneRenderer::statistics() const
  {
    return m_statistics;
  }

} // namespace HydroCouple::Composer
