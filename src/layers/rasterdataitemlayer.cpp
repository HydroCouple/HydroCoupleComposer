#include "layers/rasterdataitemlayer.h"

#include "gis/spatialreference.h"
#include "map/maptransform.h"

#include "hydrocouple.h"
#include "hydrocouplespatial.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HydroCouple::Composer
{

  namespace
  {
    using HydroCouple::Spatial::IRasterComponentDataItem;

    QString itemName(const HydroCouple::IComponentDataItem *item)
    {
      if (!item)
      {
        return QObject::tr("Raster");
      }

      const QString caption = QString::fromStdString(item->caption());

      return caption.isEmpty() ? QString::fromStdString(item->id()) : caption;
    }
  } // namespace

  RasterDataItemLayer::RasterDataItemLayer(const QString &name,
                                           IRasterComponentDataItem *item)
    : MapLayer(name),
      m_item(item),
      m_ramp(ColorRamp::builtin(QStringLiteral("Viridis")))
  {
  }

  RasterDataItemLayer::~RasterDataItemLayer() = default;

  bool RasterDataItemLayer::isRaster(const HydroCouple::IComponentDataItem *item)
  {
    const auto *raster = dynamic_cast<const IRasterComponentDataItem *>(item);

    return raster && raster->raster();
  }

  std::unique_ptr<RasterDataItemLayer> RasterDataItemLayer::create(
    HydroCouple::IComponentDataItem *item, QString &message)
  {
    auto *raster = dynamic_cast<IRasterComponentDataItem *>(item);

    if (!raster || !raster->raster())
    {
      message = QObject::tr("This data item carries no raster.");
      return nullptr;
    }

    HydroCouple::Spatial::IRaster *grid = raster->raster();

    if (grid->xSize() <= 0 || grid->ySize() <= 0)
    {
      message = QObject::tr("This raster has no cells.");
      return nullptr;
    }

    std::unique_ptr<RasterDataItemLayer> layer(
      new RasterDataItemLayer(itemName(item), raster));

    layer->m_width = grid->xSize();
    layer->m_height = grid->ySize();

    // geoTransformation() is not const on the interface, so the raster is
    // asked here rather than from render(). It does not change under us: a
    // component may republish values, but not move its own grid.
    grid->geoTransformation(layer->m_geoTransform.data());

    if (qFuzzyIsNull(layer->m_geoTransform[1])
        || qFuzzyIsNull(layer->m_geoTransform[5]))
    {
      message = QObject::tr("This raster's cells have no size.");
      return nullptr;
    }

    if (!layer->readBand(message))
    {
      return nullptr;
    }

    if (HydroCouple::Spatial::ISpatialReferenceSystem *reference =
          grid->spatialReferenceSystem())
    {
      if (!reference->srText().empty())
      {
        QString crsMessage;
        layer->setCrs(SpatialReference::fromDefinition(
          QString::fromStdString(reference->srText()), crsMessage));
      }
    }

    layer->setSourceDescription(QString::fromStdString(item->id()));

    return layer;
  }

  bool RasterDataItemLayer::readBand(QString &message)
  {
    const std::vector<int64_t> shape = m_item->shape();

    // Band, row, column -- the ordering the interface fixes. Anything else is
    // an item that says it is a raster and is not one.
    if (shape.size() != 3)
    {
      message = QObject::tr("A raster item is band, row and column; this one "
                            "has %1 dimension(s).")
                  .arg(shape.size());
      return false;
    }

    if (m_band >= shape[0])
    {
      m_band = 0;
    }

    const int64_t count = shape[1] * shape[2];

    if (count <= 0)
    {
      message = QObject::tr("This raster has no cells.");
      return false;
    }

    std::vector<double> buffer(static_cast<size_t>(count), 0.0);
    const int64_t bufferShape = count;

    HydroCouple::BufferDescriptor destination;
    destination.data = buffer.data();
    destination.kind = HydroCouple::DataKind::Float64;
    destination.rank = 1;
    destination.shape = &bufferShape;

    const std::array<int64_t, 3> start{m_band, 0, 0};
    const std::array<int64_t, 3> extent{1, shape[1], shape[2]};

    std::string reason;

    if (!m_item->getValuesInto(destination, start, extent, &reason))
    {
      message = QString::fromStdString(reason);
      return false;
    }

    m_values = std::move(buffer);
    m_height = static_cast<int>(shape[1]);
    m_width = static_cast<int>(shape[2]);

    m_minimum = std::numeric_limits<double>::max();
    m_maximum = std::numeric_limits<double>::lowest();

    // No-data needs no skipping here: std::min and std::max both answer
    // with the value they already had when the comparison involves a NaN,
    // so holes fall out of the range on their own. Only the all-holes case
    // below has to be said out loud.
    for (double value : m_values)
    {
      m_minimum = std::min(m_minimum, value);
      m_maximum = std::max(m_maximum, value);
    }

    if (m_minimum > m_maximum)
    {
      // Every cell is no-data. Drawable, and empty, which is the truth.
      m_minimum = 0.0;
      m_maximum = 0.0;
    }

    return true;
  }

  HydroCouple::IComponentDataItem *RasterDataItemLayer::dataItem() const
  {
    return m_item;
  }

  int RasterDataItemLayer::band() const
  {
    return m_band;
  }

  void RasterDataItemLayer::setBand(int band)
  {
    const std::vector<int64_t> shape = m_item ? m_item->shape()
                                              : std::vector<int64_t>();

    if (shape.size() != 3 || band < 0 || band >= shape[0] || band == m_band)
    {
      return;
    }

    m_band = band;

    QString message;

    if (readBand(message))
    {
      notifyAppearanceChanged();
    }
  }

  const ColorRamp &RasterDataItemLayer::ramp() const
  {
    return m_ramp;
  }

  void RasterDataItemLayer::setRamp(const ColorRamp &ramp)
  {
    m_ramp = ramp;
    notifyAppearanceChanged();
  }

  QPair<double, double> RasterDataItemLayer::valueRange() const
  {
    return {m_minimum, m_maximum};
  }

  QRectF RasterDataItemLayer::extent() const
  {
    const double left = m_geoTransform[0];
    const double top = m_geoTransform[3];
    const double right = left + m_geoTransform[1] * m_width;
    const double bottom = top + m_geoTransform[5] * m_height;

    return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
  }

  const QImage &RasterDataItemLayer::lastImage() const
  {
    return m_lastImage;
  }

  void RasterDataItemLayer::render(QPainter &painter,
                                   const MapTransform &transform)
  {
    if (m_values.empty() || !transform.isValid())
    {
      return;
    }

    const QRectF whole = extent();
    const QRectF wanted = whole.intersected(transform.visibleExtent());

    if (wanted.isEmpty())
    {
      return;
    }

    // The band is already in memory -- it came through the hyperslab API as
    // one read -- so unlike a GeoTIFF there is no window to narrow before
    // reading. The image is built at the whole raster's resolution and Qt
    // scales it, which for a component's grid is the smaller cost.
    QImage image(m_width, m_height, QImage::Format_ARGB32);

    const double span = m_maximum - m_minimum;

    for (int row = 0; row < m_height; ++row)
    {
      for (int column = 0; column < m_width; ++column)
      {
        const double value =
          m_values[static_cast<size_t>(row) * m_width + column];

        if (std::isnan(value))
        {
          // No-data is nothing, not a colour at one end of the ramp.
          image.setPixelColor(column, row, QColor(0, 0, 0, 0));
          continue;
        }

        const double position =
          qFuzzyIsNull(span) ? 0.5 : (value - m_minimum) / span;

        image.setPixelColor(column, row, m_ramp.colorAt(position));
      }
    }

    m_lastImage = image;

    const QPointF cornerA = transform.toScreen(whole.topLeft());
    const QPointF cornerB = transform.toScreen(whole.bottomRight());

    painter.drawImage(QRectF(cornerA, cornerB).normalized(), image);
  }

} // namespace HydroCouple::Composer
