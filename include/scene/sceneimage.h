/*!
 * \file   sceneimage.h
 * \author Caleb Buahin
 * \brief  renderSceneToImage — the 3D scene, drawn without a window.
 *
 * Two things need this. Exporting or printing a 3D view is one. The other is
 * that it is the only way the scene can be verified at all: QRhiWidget refuses
 * to create a graphics device under the offscreen platform plugin, which is
 * what the whole test suite runs under, whereas a standalone QRhi drawing into
 * a texture works there unchanged. The two paths were measured to agree.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_SCENEIMAGE_H
#define HYDROCOUPLECOMPOSER_SCENE_SCENEIMAGE_H

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>

namespace HydroCouple::Composer
{
  class Camera;
  class SceneRenderer;

  /*!
   * \brief Renders \a renderer through \a camera into an image.
   *
   * A device is created and destroyed per call. That is far too costly for
   * interactive drawing — which is why SceneView exists — and exactly right
   * for an export or a test, where the alternative is a device kept alive for
   * the life of the process on behalf of a caller that wanted one picture.
   *
   * \param renderer The scene to draw.
   * \param camera The view.
   * \param size Image size in pixels.
   * \param background Colour to clear to.
   * \param[out] message Diagnostic on failure.
   * \returns The image, or a null image on failure.
   */
  [[nodiscard]] QImage renderSceneToImage(SceneRenderer &renderer,
                                          const Camera &camera,
                                          const QSize &size,
                                          const QColor &background,
                                          QString &message);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENEIMAGE_H
