/*!
 * \file   scenerenderer.h
 * \author Caleb Buahin
 * \brief  SceneRenderer — draws the layer stack in 3D, without a widget.
 *
 * Deliberately not a widget, and not derived from one. QRhiWidget cannot
 * create a graphics device under the offscreen platform plugin — "QRhi is not
 * supported on this platform" — which is exactly the platform every test in
 * this suite runs under. A renderer that lived inside the widget would
 * therefore be unverifiable, and a 3D view whose output nothing checks is a
 * 3D view that quietly stops matching its data.
 *
 * Taking the device and the render target as arguments makes the same code
 * serve both: SceneView hands it the widget's, and renderSceneToImage() hands
 * it a texture target created on a standalone QRhi, which does work headless.
 * The two paths were measured to produce identical output.
 *
 * \note The renderer holds GPU resources tied to one QRhi. Handing it a
 *       different device re-creates them; that is what a widget does when its
 *       window moves to another screen.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_SCENERENDERER_H
#define HYDROCOUPLECOMPOSER_SCENE_SCENERENDERER_H

#include "scene/camera.h"
#include "scene/scenegeometry.h"

#include <QColor>
#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiRenderTarget;
class QRhiResourceUpdateBatch;
class QRhiShaderResourceBindings;

namespace HydroCouple::Composer
{
  class LayerStackModel;

  /*!
   * \brief Renders a LayerStackModel's 3D geometry.
   */
  class SceneRenderer : public QObject
  {
      Q_OBJECT

    public:
      /*!
       * \brief What one frame drew; for the phase's performance budget.
       */
      struct Statistics
      {
          int batches = 0;
          qint64 vertices = 0;
          qint64 primitives = 0;
      };

      explicit SceneRenderer(QObject *parent = nullptr);

      ~SceneRenderer() override;

      /*!
       * \brief Draws the layers of \a model.
       *
       * The same model the map draws, so a layer hidden in the tree
       * disappears from both views without either being told about the other.
       *
       * \param model The stack to draw; may be nullptr.
       */
      void setModel(LayerStackModel *model);

      /*!
       * \brief The stack being drawn, or nullptr.
       */
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief Discards cached geometry, rebuilding it on the next frame.
       */
      void invalidate();

      /*!
       * \brief The box every visible layer's geometry occupies.
       *
       * Costs no tessellation: layers answer it from their own extents.
       */
      [[nodiscard]] Bounds3D sceneBounds() const;

      /*!
       * \brief The ambient fraction of the lighting, from 0 to 1.
       */
      [[nodiscard]] double ambient() const;

      /*!
       * \brief Sets the ambient fraction.
       *
       * Fully directional lighting leaves faces turned away from the light
       * black, which on a terrain reads as missing data.
       *
       * \param ambient Clamped to [0, 1].
       */
      void setAmbient(double ambient);

      /*!
       * \brief Prepares GPU resources for \a rhi and \a descriptor.
       *
       * Safe to call every frame: it returns immediately once prepared for
       * the device it was given, and re-prepares when handed a different one.
       *
       * \param rhi The graphics device.
       * \param descriptor The render pass the frames will be drawn in.
       * \param sampleCount Multisample count of the target.
       * \param[out] message Diagnostic on failure.
       * \returns True when the renderer is ready to draw.
       */
      bool initialize(QRhi *rhi, QRhiRenderPassDescriptor *descriptor,
                      int sampleCount, QString &message);

      /*!
       * \brief Drops every GPU resource.
       */
      void releaseResources();

      /*!
       * \brief Draws one frame into \a target.
       *
       * Begins and ends the render pass itself, because the depth buffer must
       * be cleared with the colour and a caller that forgot would get a scene
       * whose first frame is correct and whose second is not.
       *
       * \param cb Command buffer of the frame in progress.
       * \param target Where to draw.
       * \param camera The view.
       * \param background Colour to clear to.
       */
      void render(QRhiCommandBuffer *cb, QRhiRenderTarget *target,
                  const Camera &camera, const QColor &background);

      /*!
       * \brief What the last render() call drew.
       */
      [[nodiscard]] Statistics statistics() const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when the cached geometry has been dropped.
       *
       * A host repaints on this rather than watching the layer stack itself.
       * Two listeners on the same stack would be two chances to disagree
       * about whether a frame is stale, and the renderer is the one that
       * knows: it is the thing holding the cache.
       */
      void sceneChanged();

    private:
      //! One layer's geometry, uploaded.
      struct Batch
      {
          std::unique_ptr<QRhiBuffer> vertexBuffer;
          std::unique_ptr<QRhiBuffer> indexBuffer;
          std::unique_ptr<QRhiBuffer> uniformBuffer;
          std::unique_ptr<QRhiShaderResourceBindings> bindings;
          quint32 indexCount = 0;
          quint32 vertexCount = 0;
          ScenePrimitive primitive = ScenePrimitive::Triangles;
          float opacity = 1.0f;
      };

      void rebuildBatches();

      QRhi *m_rhi = nullptr;
      QRhiRenderPassDescriptor *m_descriptor = nullptr;
      int m_sampleCount = 1;

      std::unique_ptr<QRhiGraphicsPipeline> m_trianglePipeline;
      std::unique_ptr<QRhiGraphicsPipeline> m_linePipeline;

      std::vector<Batch> m_batches;
      bool m_batchesValid = false;

      //! Uploads waiting for a frame. Geometry is rebuilt whenever the stack
      //! changes, which is not necessarily inside a frame, and a resource
      //! update batch can only be submitted within one.
      std::vector<QRhiResourceUpdateBatch *> m_pendingUpdates;

      LayerStackModel *m_model = nullptr;
      double m_ambient = 0.35;
      Statistics m_statistics;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENERENDERER_H
