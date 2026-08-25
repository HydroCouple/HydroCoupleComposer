#include "scene/scenerenderer.h"

#include "map/layerstackmodel.h"
#include "map/maplayer.h"
#include "scene/scenesource.h"

#include <QFile>
#include <QMatrix4x4>

#include <rhi/qrhi.h>

#include <algorithm>

namespace HydroCouple::Composer
{
  namespace
  {
    //! mat4 mvp, mat4 normalMatrix, vec4 lightDirection, vec4 params.
    constexpr int kUniformBlockSize = 64 + 64 + 16 + 16;

    /*!
     * \brief How far a line is pulled toward the eye, in clip space.
     *
     * Lines in this scene are coplanar with surfaces by construction: a mesh's
     * edges lie on its own faces, and a draped network lies on the terrain it
     * was sampled from. Their depths then differ only by how each interpolator
     * rounded, and which one wins changes with the camera angle — measured on
     * one flat sheet, the line is drawn at 89 degrees of elevation and gone at
     * 90, which is exactly the view the map hands over.
     *
     * Neither of the two obvious remedies works. Depth bias is applied by
     * Metal, D3D11 and Vulkan to filled primitives only, so a biased line
     * pipeline is inert — measured, pixel-identical with and without. And a
     * LessOrEqual compare only covers exact ties, which this is not.
     *
     * Nudging in the shader keeps the drape honest, which is the point:
     * the vertices stay on the surface they claim to be on, so measuring one
     * gives back the elevation it was sampled at, and only the depth written
     * for it moves. Small enough that a line genuinely behind a hill still
     * loses — a hundredth of a percent of the depth range.
     */
    constexpr float kCoplanarLineNudge = 1.0e-4f;

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
      if (!layer->isVisible())
      {
        continue;
      }

      if (const ISceneSource *source = layer->sceneSource())
      {
        bounds.expandTo(source->sceneBounds());
      }
    }

    return bounds;
  }

  const ITerrainSource *SceneRenderer::resolveTerrain() const
  {
    if (!m_model)
    {
      return nullptr;
    }

    // The uppermost terrain in the tree wins. renderOrder() is bottom-up —
    // it is a draw order — so the last one it yields is the top one, and
    // reversing that would silently drape on whatever happened to be lowest.
    const ITerrainSource *terrain = nullptr;

    for (const MapLayer *layer : m_model->renderOrder())
    {
      if (!layer->isVisible())
      {
        continue;
      }

      if (const ISceneSource *source = layer->sceneSource())
      {
        if (const ITerrainSource *candidate = source->terrain())
        {
          terrain = candidate;
        }
      }
    }

    return terrain;
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

    if (!vertex.isValid() || !fragment.isValid())
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

    const auto makePipeline =
      [&](QRhiGraphicsPipeline::Topology topology)
      -> std::unique_ptr<QRhiGraphicsPipeline>
    {
      std::unique_ptr<QRhiGraphicsPipeline> pipeline(
        rhi->newGraphicsPipeline());

      pipeline->setTopology(topology);
      pipeline->setShaderStages({ { QRhiShaderStage::Vertex, vertex },
                                  { QRhiShaderStage::Fragment, fragment } });
      pipeline->setVertexInputLayout(layout);
      pipeline->setShaderResourceBindings(layoutBindings.get());
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

    m_trianglePipeline = makePipeline(QRhiGraphicsPipeline::Triangles);
    m_linePipeline = makePipeline(QRhiGraphicsPipeline::Lines);

    if (!m_trianglePipeline || !m_linePipeline)
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
    m_trianglePipeline.reset();
    m_linePipeline.reset();
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
    const SceneContext context = { resolveTerrain() };

    for (const MapLayer *layer : m_model->renderOrder())
    {
      if (!layer->isVisible())
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

        batch.bindings.reset(m_rhi->newShaderResourceBindings());
        batch.bindings->setBindings(
          { QRhiShaderResourceBinding::uniformBuffer(
            0,
            QRhiShaderResourceBinding::VertexStage |
              QRhiShaderResourceBinding::FragmentStage,
            batch.uniformBuffer.get()) });

        if (!batch.bindings->create())
        {
          continue;
        }

        // Held until the first frame, because a resource update batch can
        // only be submitted inside a frame and this may run outside one.
        QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();
        updates->uploadStaticBuffer(batch.vertexBuffer.get(),
                                    geometry.vertices.constData());
        updates->uploadStaticBuffer(batch.indexBuffer.get(),
                                    geometry.indices.constData());
        m_pendingUpdates.push_back(updates);

        m_batches.push_back(std::move(batch));
      }
    }
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
      block[38] = batch.primitive == ScenePrimitive::Lines
                    ? kCoplanarLineNudge
                    : 0.0f;

      updates->updateDynamicBuffer(batch.uniformBuffer.get(), 0,
                                   kUniformBlockSize, block);
    }

    cb->beginPass(target, background, { 1.0f, 0 }, updates);

    for (const Batch &batch : m_batches)
    {
      QRhiGraphicsPipeline *pipeline =
        batch.primitive == ScenePrimitive::Lines ? m_linePipeline.get()
                                                 : m_trianglePipeline.get();

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

    cb->endPass();
  }

  SceneRenderer::Statistics SceneRenderer::statistics() const
  {
    return m_statistics;
  }

} // namespace HydroCouple::Composer
