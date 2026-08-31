#include "layers/gdalrasterlayer.h"

#include "gis/spatialreference.h"
#include "map/maptransform.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QPainter>

#include <limits>
#include <gdal_priv.h>
#include <gdalwarper.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace HydroCouple::Composer
{
  namespace
  {
    void ensureDriversRegistered()
    {
      static const bool registered = []
      {
        GDALAllRegister();
        return true;
      }();

      Q_UNUSED(registered)
    }

    QRectF extentOf(GDALDataset *dataset)
    {
      double geotransform[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

      if (dataset->GetGeoTransform(geotransform) != CE_None)
      {
        // No georeferencing: the dataset is placed on pixel coordinates,
        // which at least puts it somewhere findable rather than nowhere.
        return QRectF(0.0, 0.0, dataset->GetRasterXSize(),
                      dataset->GetRasterYSize());
      }

      const double left = geotransform[0];
      const double top = geotransform[3];
      const double right = left + geotransform[1] * dataset->GetRasterXSize();
      const double bottom = top + geotransform[5] * dataset->GetRasterYSize();

      return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
    }
  }

  GdalRasterLayer::GdalRasterLayer(const QString &name, const QString &filePath)
    : MapLayer(name), m_filePath(filePath),
      m_ramp(ColorRamp::builtin(QStringLiteral("Viridis")))
  {
    setSourceDescription(filePath);
  }

  GdalRasterLayer::~GdalRasterLayer()
  {
    if (m_warped)
    {
      GDALClose(m_warped);
    }

    if (m_dataset)
    {
      GDALClose(m_dataset);
    }
  }

  std::unique_ptr<GdalRasterLayer> GdalRasterLayer::open(
    const QString &filePath, QString &message)
  {
    ensureDriversRegistered();

    auto *dataset = static_cast<GDALDataset *>(
      GDALOpen(filePath.toUtf8().constData(), GA_ReadOnly));

    if (!dataset)
    {
      message = QObject::tr("%1 could not be opened as a raster.")
                  .arg(QFileInfo(filePath).fileName());

      return nullptr;
    }

    std::unique_ptr<GdalRasterLayer> layer(
      new GdalRasterLayer(QFileInfo(filePath).completeBaseName(), filePath));

    if (!layer->adoptDataset(dataset, message))
    {
      GDALClose(dataset);

      return nullptr;
    }

    QJsonObject state;
    state.insert(QStringLiteral("type"), QStringLiteral("gdal-raster"));
    state.insert(QStringLiteral("path"), filePath);
    state.insert(QStringLiteral("name"), layer->name());
    layer->setPersistentState(state);

    return layer;
  }

  bool GdalRasterLayer::adoptDataset(GDALDataset *dataset, QString &message)
  {
    if (!dataset)
    {
      message = QObject::tr("There is no raster to read.");

      return false;
    }

    if (dataset->GetRasterCount() < 1)
    {
      message = QObject::tr("%1 holds no raster bands.").arg(name());

      return false;
    }

    m_dataset = dataset;
    m_bandCount = dataset->GetRasterCount();
    m_size = QSize(dataset->GetRasterXSize(), dataset->GetRasterYSize());
    m_extent = extentOf(dataset);

    // Three bands or more of bytes is a picture; anything else is data, and
    // shading data with a ramp says far more than showing its first band as
    // grey would.
    m_colorImage = m_bandCount >= 3
                   && dataset->GetRasterBand(1)->GetRasterDataType() == GDT_Byte;

    if (const char *wkt = dataset->GetProjectionRef())
    {
      if (wkt[0] != '\0')
      {
        QString crsMessage;
        setCrs(SpatialReference::fromDefinition(QString::fromUtf8(wkt),
                                                crsMessage));
      }
    }

    if (!m_colorImage)
    {
      computeRange();
    }

    return true;
  }

  void GdalRasterLayer::computeRange()
  {
    // The array is written into, so it is not optional — GDAL dereferences
    // it unconditionally. Approximate statistics: exact ones read every
    // pixel, which for a terrain model is seconds of work to choose two
    // numbers with.
    double values[2] = {0.0, 1.0};

    if (m_dataset->GetRasterBand(1)->ComputeRasterMinMax(TRUE, values)
        != CE_None)
    {
      values[0] = 0.0;
      values[1] = 1.0;
    }

    m_minimum = values[0];
    m_maximum = values[1] > values[0] ? values[1] : values[0] + 1.0;
  }

  QString GdalRasterLayer::filePath() const
  {
    return m_filePath;
  }

  QSize GdalRasterLayer::rasterSize() const
  {
    return m_size;
  }

  int GdalRasterLayer::bandCount() const
  {
    return m_bandCount;
  }

  bool GdalRasterLayer::isColorImage() const
  {
    return m_colorImage;
  }

  const ColorRamp &GdalRasterLayer::ramp() const
  {
    return m_ramp;
  }

  void GdalRasterLayer::setRamp(const ColorRamp &ramp)
  {
    m_ramp = ramp;
    m_groundValid = false;
    notifyAppearanceChanged();
  }

  void GdalRasterLayer::valueRange(double &minimum, double &maximum) const
  {
    minimum = m_minimum;
    maximum = m_maximum;
  }

  const QImage &GdalRasterLayer::lastImage() const
  {
    return m_lastImage;
  }

  bool GdalRasterLayer::sample(const QVector<QPointF> &points,
                               QVector<double> &values,
                               QString &message) const
  {
    values.assign(points.size(), std::numeric_limits<double>::quiet_NaN());

    if (points.isEmpty())
    {
      return true;
    }

    // The native dataset, not the warped one. The caller's points are in
    // this raster's own system by contract, and the warp exists to put the
    // picture on a map rather than to move the measurements.
    if (!m_dataset)
    {
      message = tr("%1 has nothing to read.").arg(name());

      return false;
    }

    double geotransform[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

    if (m_dataset->GetGeoTransform(geotransform) != CE_None
        || qFuzzyIsNull(geotransform[1]) || qFuzzyIsNull(geotransform[5]))
    {
      message = tr("%1 does not say where its cells are.").arg(name());

      return false;
    }

    // Cell coordinates, where a whole number is a cell's edge and the
    // half-offset below moves to its centre -- which is where its value
    // actually is, and getting that wrong shifts a whole terrain by half a
    // cell.
    const auto columnOf = [&](const QPointF &point) {
      return (point.x() - geotransform[0]) / geotransform[1] - 0.5;
    };
    const auto rowOf = [&](const QPointF &point) {
      return (point.y() - geotransform[3]) / geotransform[5] - 0.5;
    };

    double minimumColumn = std::numeric_limits<double>::max();
    double maximumColumn = std::numeric_limits<double>::lowest();
    double minimumRow = std::numeric_limits<double>::max();
    double maximumRow = std::numeric_limits<double>::lowest();

    for (const QPointF &point : points)
    {
      minimumColumn = std::min(minimumColumn, columnOf(point));
      maximumColumn = std::max(maximumColumn, columnOf(point));
      minimumRow = std::min(minimumRow, rowOf(point));
      maximumRow = std::max(maximumRow, rowOf(point));
    }

    // One cell of margin, because bilinear reads the neighbour.
    int left = static_cast<int>(std::floor(minimumColumn)) - 1;
    int top = static_cast<int>(std::floor(minimumRow)) - 1;
    int right = static_cast<int>(std::ceil(maximumColumn)) + 1;
    int bottom = static_cast<int>(std::ceil(maximumRow)) + 1;

    left = std::max(left, 0);
    top = std::max(top, 0);
    right = std::min(right, m_size.width() - 1);
    bottom = std::min(bottom, m_size.height() - 1);

    if (left > right || top > bottom)
    {
      // Every point is off the raster. Not a failure: a mesh may simply
      // extend past the ground that was fetched, and the caller is told by
      // the NaNs.
      return true;
    }

    const int width = right - left + 1;
    const int height = bottom - top + 1;

    std::vector<float> block(static_cast<size_t>(width)
                             * static_cast<size_t>(height));

    GDALRasterBand *band = m_dataset->GetRasterBand(1);

    if (!band
        || band->RasterIO(GF_Read, left, top, width, height, block.data(),
                          width, height, GDT_Float32, 0, 0)
             != CE_None)
    {
      message = tr("%1 could not be read.").arg(name());

      return false;
    }

    int hasNoData = 0;
    const double noData = band->GetNoDataValue(&hasNoData);

    const auto at = [&](int column, int row) -> double {
      if (column < left || column > right || row < top || row > bottom)
      {
        return std::numeric_limits<double>::quiet_NaN();
      }

      const double value =
        block[static_cast<size_t>(row - top) * static_cast<size_t>(width)
              + static_cast<size_t>(column - left)];

      if (hasNoData && qFuzzyCompare(value, noData))
      {
        return std::numeric_limits<double>::quiet_NaN();
      }

      return value;
    };

    for (int i = 0; i < points.size(); ++i)
    {
      const double column = columnOf(points.at(i));
      const double row = rowOf(points.at(i));

      const int column0 = static_cast<int>(std::floor(column));
      const int row0 = static_cast<int>(std::floor(row));

      const double fx = column - column0;
      const double fy = row - row0;

      const double v00 = at(column0, row0);
      const double v10 = at(column0 + 1, row0);
      const double v01 = at(column0, row0 + 1);
      const double v11 = at(column0 + 1, row0 + 1);

      // A no-data neighbour carries through on its own: at() returns NaN
      // for one, and NaN survives the arithmetic below whatever weight it
      // is given, so a vertex that needs a cell the survey does not have
      // gets nothing rather than a number. An explicit check here would
      // read as though it were doing that work, and would not be.
      const double top_ = v00 + (v10 - v00) * fx;
      const double bottom_ = v01 + (v11 - v01) * fx;

      values[i] = top_ + (bottom_ - top_) * fy;
    }

    return true;
  }

  QRectF GdalRasterLayer::extent() const
  {
    return m_extent;
  }

  void GdalRasterLayer::setDrape(SceneDrape drape)
  {
    if (this->drape() == drape)
    {
      return;
    }

    ISceneSource::setDrape(drape);

    notifyAppearanceChanged();
  }

  void GdalRasterLayer::onProjectionChanged()
  {
    if (m_warped)
    {
      GDALClose(m_warped);
      m_warped = nullptr;
    }

    m_groundValid = false;

    notifyAppearanceChanged();
  }

  GDALDataset *GdalRasterLayer::readable()
  {
    if (!m_dataset)
    {
      return nullptr;
    }

    if (!crs() || !mapCrs() || crs()->isSameAs(*mapCrs()))
    {
      return m_dataset;
    }

    if (m_warped)
    {
      return m_warped;
    }

    char *wkt = nullptr;

    if (mapCrs()->handle()->exportToWkt(&wkt) != OGRERR_NONE || !wkt)
    {
      return m_dataset;
    }

    // A warped VRT reprojects lazily, per read, so opening a large raster in
    // a different CRS costs nothing until something asks for pixels.
    m_warped = static_cast<GDALDataset *>(GDALAutoCreateWarpedVRT(
      m_dataset, nullptr, wkt, GRA_Bilinear, 0.0, nullptr));

    CPLFree(wkt);

    return m_warped ? m_warped : m_dataset;
  }

  void GdalRasterLayer::render(QPainter &painter, const MapTransform &transform)
  {
    GDALDataset *dataset = readable();

    if (!dataset || !transform.isValid())
    {
      return;
    }

    double geotransform[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

    if (dataset->GetGeoTransform(geotransform) != CE_None
        || qFuzzyIsNull(geotransform[1]) || qFuzzyIsNull(geotransform[5]))
    {
      return;
    }

    const QRectF raster = extentOf(dataset);
    const QRectF wanted = raster.intersected(transform.visibleExtent());

    if (wanted.isEmpty())
    {
      return;
    }

    // Source window, in pixels.
    const auto columnOf = [&](double x)
    { return (x - geotransform[0]) / geotransform[1]; };
    const auto rowOf = [&](double y)
    { return (y - geotransform[3]) / geotransform[5]; };

    int left = static_cast<int>(std::floor(columnOf(wanted.left())));
    int right = static_cast<int>(std::ceil(columnOf(wanted.right())));
    int top = static_cast<int>(std::floor(rowOf(wanted.bottom())));
    int bottom = static_cast<int>(std::ceil(rowOf(wanted.top())));

    if (left > right)
    {
      std::swap(left, right);
    }

    if (top > bottom)
    {
      std::swap(top, bottom);
    }

    left = std::clamp(left, 0, dataset->GetRasterXSize() - 1);
    right = std::clamp(right, left + 1, dataset->GetRasterXSize());
    top = std::clamp(top, 0, dataset->GetRasterYSize() - 1);
    bottom = std::clamp(bottom, top + 1, dataset->GetRasterYSize());

    const int sourceWidth = right - left;
    const int sourceHeight = bottom - top;

    // Target window, in screen pixels, never larger than the source: reading
    // more samples than the screen can show is work thrown away.
    const QPointF cornerA = transform.toScreen(wanted.topLeft());
    const QPointF cornerB = transform.toScreen(wanted.bottomRight());
    const QRectF target = QRectF(cornerA, cornerB).normalized();

    const int width =
      std::clamp(static_cast<int>(std::ceil(target.width())), 1, sourceWidth);
    const int height =
      std::clamp(static_cast<int>(std::ceil(target.height())), 1, sourceHeight);

    QImage image(width, height, QImage::Format_ARGB32);

    if (m_colorImage)
    {
      std::vector<uchar> buffer(static_cast<size_t>(width) * height * 3);

      for (int band = 1; band <= 3; ++band)
      {
        if (dataset->GetRasterBand(band)->RasterIO(
              GF_Read, left, top, sourceWidth, sourceHeight,
              buffer.data() + (band - 1), width, height, GDT_Byte, 3,
              static_cast<GSpacing>(width) * 3)
            != CE_None)
        {
          return;
        }
      }

      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
        {
          const size_t index = (static_cast<size_t>(y) * width + x) * 3;
          image.setPixelColor(x, y,
                              QColor(buffer[index], buffer[index + 1],
                                     buffer[index + 2]));
        }
      }
    }
    else
    {
      std::vector<float> buffer(static_cast<size_t>(width) * height);

      if (dataset->GetRasterBand(1)->RasterIO(
            GF_Read, left, top, sourceWidth, sourceHeight, buffer.data(),
            width, height, GDT_Float32, 0, 0)
          != CE_None)
      {
        return;
      }

      int hasNoData = 0;
      const double noData =
        dataset->GetRasterBand(1)->GetNoDataValue(&hasNoData);

      const double span = m_maximum - m_minimum;

      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
        {
          const double value = buffer[static_cast<size_t>(y) * width + x];

          // No-data is left transparent rather than shaded: painting it the
          // ramp's lowest colour invents ground that is not there.
          if ((hasNoData && qFuzzyCompare(value + 1.0, noData + 1.0))
              || !std::isfinite(value))
          {
            image.setPixelColor(x, y, Qt::transparent);
            continue;
          }

          image.setPixelColor(x, y,
                              m_ramp.colorAt((value - m_minimum) / span));
        }
      }
    }

    m_lastImage = image;
    painter.drawImage(target, image);
  }

  const ISceneSource *GdalRasterLayer::sceneSource() const
  {
    return this;
  }

  Bounds3D GdalRasterLayer::sceneBounds() const
  {
    Bounds3D bounds;

    const QRectF box = m_extent.normalized();

    if (box.isEmpty())
    {
      return bounds;
    }

    // At ground level, and flat: the terrain the raster is laid on carries
    // the relief, and sampling a surface here would cost a full drape just to
    // frame the view — which is the one thing bounds exist to avoid.
    bounds.expandTo(QVector3D(float(box.left()), float(box.top()), 0.0f));
    bounds.expandTo(QVector3D(float(box.right()), float(box.bottom()), 0.0f));

    return bounds;
  }

  QVector<SceneGeometry> GdalRasterLayer::sceneGeometry(
    const SceneContext &context) const
  {
    QVector<SceneGeometry> batches;

    if (!m_groundValid)
    {
      // A raster is a picture of a *place*, so it shows all of itself rather
      // than only the part the rest of the scene happens to cover.
      //
      // The const_cast is the layer drawing itself into its own cache. Its
      // render() is non-const because drawing into a painter records what was
      // drawn, and a const caller asking for geometry is not changing the
      // layer — it is paying for work not yet done, exactly as
      // FeatureLayer::projectedFeatures() does.
      m_ground = renderLayerToImage(const_cast<GdalRasterLayer &>(*this),
                                    m_extent, kGroundTexturePixels);
      m_groundValid = true;
    }

    // drapeTarget(), not the context's terrain outright: a surface set
    // Flat is asking to lie at z = 0 even though the stack has a terrain
    // in it, and passing the terrain regardless is what made draping
    // unconditional before C5d.
    SceneGeometry ground = buildGroundPlane(m_ground, drapeTarget(context));

    if (!ground.isEmpty())
    {
      batches.append(std::move(ground));
    }

    return batches;
  }

} // namespace HydroCouple::Composer
