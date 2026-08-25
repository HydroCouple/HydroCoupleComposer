#include "scene/groundplane.h"

#include "map/maplayer.h"
#include "map/maptransform.h"
#include "scene/scenesource.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <vector>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief Rows or columns the ground plane may be cut into.
     *
     * A bound, not a budget. A basemap covering a continent against a
     * metre-scale terrain would otherwise ask for a grid of billions, and it
     * would be spent on relief nobody can see from that distance anyway.
     */
    constexpr int kMaxDivisions = 128;

    //! Divisions along one axis, from the terrain's own sample spacing.
    int divisionsFor(double length, double step)
    {
      if (step <= 0.0 || length <= 0.0)
      {
        return 1;
      }

      return std::clamp(int(std::ceil(length / step)), 1, kMaxDivisions);
    }

  }

  GroundImage renderLayerToImage(MapLayer &layer, const QRectF &extent,
                                 int maximumPixels)
  {
    GroundImage ground;

    const QRectF box = extent.normalized();

    if (box.isEmpty() || maximumPixels <= 0)
    {
      return ground;
    }

    // Sized to the extent's own shape, so that fitting it to the viewport
    // adds no margin — a margin would be texture that covers world the layer
    // was never asked about, and it would be sampled as though it did.
    const double longest = std::max(box.width(), box.height());
    const double scale = double(maximumPixels) / longest;
    const QSize size(std::max(1, int(std::lround(box.width() * scale))),
                     std::max(1, int(std::lround(box.height() * scale))));

    const MapTransform transform(box, QSizeF(size));

    if (!transform.isValid())
    {
      return ground;
    }

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    {
      QPainter painter(&image);
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

      layer.render(painter, transform);
    }

    ground.image = std::move(image);

    // What the transform actually shows, not what it was handed: rounding the
    // pixel size to whole pixels moves the edges, and a texture mapped by the
    // requested rectangle would then be off by a fraction of a pixel
    // everywhere — visible as a seam where two ground planes meet.
    ground.extent = transform.visibleExtent();

    return ground;
  }

  SceneGeometry buildGroundPlane(const GroundImage &ground,
                                 const ITerrainSource *terrain)
  {
    SceneGeometry geometry;

    if (!ground.isValid())
    {
      return geometry;
    }

    const QRectF box = ground.extent.normalized();

    geometry.primitive = ScenePrimitive::Triangles;
    geometry.texture = ground.image;
    geometry.textureExtent = box;

    const double step = terrain ? terrain->terrainResolution() : 0.0;
    const int columns = terrain ? divisionsFor(box.width(), step) : 1;
    const int rows = terrain ? divisionsFor(box.height(), step) : 1;

    const int across = columns + 1;
    const int up = rows + 1;
    const double spacingX = box.width() / double(columns);
    const double spacingY = box.height() / double(rows);

    // Sampled first, in full, because a vertex's normal is decided by its
    // neighbours' heights and not by its own.
    std::vector<double> heights(size_t(across) * size_t(up), 0.0);

    if (terrain)
    {
      for (int row = 0; row < up; ++row)
      {
        for (int column = 0; column < across; ++column)
        {
          const QPointF sample(box.left() + spacingX * double(column),
                               box.top() + spacingY * double(row));

          // A point the terrain does not answer for stays at zero rather than
          // borrowing a neighbour's height: a basemap runs well past every
          // terrain it is shown with, and dragging its far corners up to the
          // elevation of the last cell would tilt the whole world.
          (void)terrain->elevationAt(
            sample, heights[size_t(row) * size_t(across) + size_t(column)]);
        }
      }
    }

    // Colour is white and opaque: the texture is the colour here, and a tint
    // multiplied into it would be a second place the ground's appearance is
    // decided.
    const QColor white(Qt::white);

    geometry.vertices.reserve(across * up);

    const auto heightAt = [&](int column, int row) -> double
    {
      return heights[size_t(std::clamp(row, 0, up - 1)) * size_t(across) +
                     size_t(std::clamp(column, 0, across - 1))];
    };

    for (int row = 0; row < up; ++row)
    {
      for (int column = 0; column < across; ++column)
      {
        // Central differences, so a draped raster is lit by the relief it is
        // lying on. Straight-up normals would leave a hillside shaded exactly
        // like the valley floor beside it, which is the whole difference
        // between a 3D view and the map.
        const double slopeX =
          (heightAt(column + 1, row) - heightAt(column - 1, row)) /
          (spacingX * (column > 0 && column + 1 < across ? 2.0 : 1.0));
        const double slopeY =
          (heightAt(column, row + 1) - heightAt(column, row - 1)) /
          (spacingY * (row > 0 && row + 1 < up ? 2.0 : 1.0));

        geometry.addVertex(
          QVector3D(float(box.left() + spacingX * double(column)),
                    float(box.top() + spacingY * double(row)),
                    float(heights[size_t(row) * size_t(across) +
                                  size_t(column)])),
          QVector3D(float(-slopeX), float(-slopeY), 1.0f), white);
      }
    }

    geometry.indices.reserve(columns * rows * 6);

    for (int row = 0; row < rows; ++row)
    {
      for (int column = 0; column < columns; ++column)
      {
        const quint32 corner = quint32(row * (columns + 1) + column);
        const quint32 next = corner + quint32(columns + 1);

        geometry.indices.append(corner);
        geometry.indices.append(corner + 1);
        geometry.indices.append(next + 1);
        geometry.indices.append(corner);
        geometry.indices.append(next + 1);
        geometry.indices.append(next);
      }
    }

    return geometry;
  }

} // namespace HydroCouple::Composer
