/*!
 * \file   scenegeometry.h
 * \author Caleb Buahin
 * \brief  Bounds3D and SceneGeometry — what a layer hands the 3D scene.
 *
 * The 2D map asks a layer to *draw*; the 3D scene asks it for *geometry*.
 * That difference is why the two views cannot share one method: a painter
 * call is a thing that happens now, into a device, in a particular order,
 * whereas a triangle list is a value that is uploaded once and drawn many
 * times from many camera positions. Layers that supply the scene therefore
 * build a SceneGeometry and the renderer decides when it reaches the GPU.
 *
 * Colour is baked per vertex rather than resolved by a material. Everything
 * needed to choose it — classification, ramps, categories — already exists on
 * the CPU from C1c, and reusing it means a mesh coloured in the 3D view and
 * the same mesh coloured in the map cannot disagree. A per-vertex RGBA costs
 * four bytes, which at the phase's 500k-cell budget is under 6 MB.
 */

#ifndef HYDROCOUPLECOMPOSER_SCENE_SCENEGEOMETRY_H
#define HYDROCOUPLECOMPOSER_SCENE_SCENEGEOMETRY_H

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QVector>
#include <QVector3D>

#include <cstdint>
#include <limits>

namespace HydroCouple::Composer
{

  /*!
   * \brief An axis-aligned box in world coordinates.
   *
   * Separate from QRectF because the scene's third axis is exactly what the
   * map does not have, and because a box that has never been given a point
   * must be distinguishable from one around the origin — the same trap
   * QRectF::united() falls into with zero-area rectangles.
   */
  class Bounds3D
  {
    public:
      Bounds3D() = default;

      /*!
       * \brief Grows the box to contain \a point.
       * \param point World coordinate to include.
       */
      void expandTo(const QVector3D &point);

      /*!
       * \brief Grows the box to contain \a other, if \a other holds anything.
       * \param other Box to absorb.
       */
      void expandTo(const Bounds3D &other);

      /*!
       * \brief Whether any point has been added.
       */
      [[nodiscard]] bool isValid() const;

      [[nodiscard]] QVector3D minimum() const;
      [[nodiscard]] QVector3D maximum() const;

      /*!
       * \brief The box centre, or the origin when empty.
       */
      [[nodiscard]] QVector3D center() const;

      /*!
       * \brief Corner-to-corner length, or zero when empty.
       */
      [[nodiscard]] double diagonal() const;

      /*!
       * \brief The box's footprint on the XY plane.
       */
      [[nodiscard]] QRectF footprint() const;

    private:
      bool m_valid = false;
      QVector3D m_min;
      QVector3D m_max;
  };

  /*!
   * \brief One vertex of scene geometry.
   *
   * Laid out to match the shader's vertex input exactly, and kept trivially
   * copyable so a whole QVector uploads as one memcpy.
   */
  struct SceneVertex
  {
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f;
      float nx = 0.0f;
      float ny = 0.0f;
      float nz = 1.0f;
      float r = 1.0f;
      float g = 1.0f;
      float b = 1.0f;
      float a = 1.0f;
  };

  /*!
   * \brief How a batch of scene vertices is assembled into primitives.
   */
  enum class ScenePrimitive
  {
    Triangles,  //!< Filled surfaces.
    Lines       //!< Wireframes, network edges, extruded verticals.
  };

  /*!
   * \brief An indexed vertex batch, ready for upload.
   */
  struct SceneGeometry
  {
      QVector<SceneVertex> vertices;
      QVector<quint32> indices;
      ScenePrimitive primitive = ScenePrimitive::Triangles;
      Bounds3D bounds;

      /*!
       * \brief An image draped over the batch, or a null image.
       *
       * The one thing per-vertex colour cannot express. Classification is
       * resolved on the CPU and baked per vertex precisely so the map and the
       * scene cannot disagree about it — but a basemap or a photograph
       * carries its own resolution, and sampling it down to one colour per
       * vertex would throw away the very thing it is being shown for.
       */
      QImage texture;

      /*!
       * \brief The world rectangle \c texture covers exactly.
       *
       * There are no per-vertex texture coordinates, deliberately: a ground
       * plane's are an affine function of its position, so storing them per
       * vertex would be storing the same four numbers a million times over.
       * The shader derives them from this instead, which also means a drape
       * refined against a finer terrain does not have to be re-coordinated.
       *
       * Y is world-north-up and the image's first row is its northern edge,
       * which is the convention MapTransform draws in.
       */
      QRectF textureExtent;

      /*!
       * \brief Whether there is anything to draw.
       */
      [[nodiscard]] bool isEmpty() const
      {
        return vertices.isEmpty() || indices.isEmpty();
      }

      /*!
       * \brief Appends a vertex, growing the bounds with it.
       * \param position World coordinate.
       * \param normal Surface normal; need not be unit length.
       * \param color Vertex colour.
       * \returns The new vertex's index.
       */
      quint32 addVertex(const QVector3D &position, const QVector3D &normal,
                        const QColor &color);

      /*!
       * \brief Appends a vertex without touching the bounds.
       *
       * For builders that know a whole cell's box up front and can grow the
       * bounds once for it, rather than once per vertex. At the phase's cell
       * counts that is millions of comparisons saved; the caller takes on
       * maintaining \c bounds itself, and geometry with stale bounds frames
       * wrongly rather than failing, so this is not the default.
       *
       * \param vertex The vertex to append.
       * \returns Its index.
       */
      quint32 appendVertex(const SceneVertex &vertex)
      {
        vertices.append(vertex);

        return quint32(vertices.size() - 1);
      }
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SCENE_SCENEGEOMETRY_H
