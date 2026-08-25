#include "map/mapcanvas.h"

#include "gis/spatialreference.h"
#include "map/extentmath.h"
#include "map/layerstackmodel.h"
#include "map/maplayer.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QVector>
#include <QWheelEvent>

#include <cmath>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Wheel notches are 120 eighths of a degree; one notch is one step.
    constexpr double kWheelZoomFactor = 1.2;

    /*!
     * \brief Reprojects a rectangle by sampling its boundary.
     *
     * Corners alone are not enough: a projection bends straight lines, so the
     * extreme of a reprojected edge frequently lies at its middle rather than
     * at either end, and a corners-only bound cuts geometry off the map.
     * Sampling the midpoints as well costs four more points and catches the
     * common single-bulge case.
     */
    QRectF reprojectExtent(const QRectF &extent,
                           const CoordinateTransform &transform)
    {
      if (extent.isNull())
      {
        return {};
      }

      QVector<QPointF> samples = {
        extent.topLeft(),     extent.topRight(),
        extent.bottomLeft(),  extent.bottomRight(),
        QPointF(extent.center().x(), extent.top()),
        QPointF(extent.center().x(), extent.bottom()),
        QPointF(extent.left(), extent.center().y()),
        QPointF(extent.right(), extent.center().y())};

      const int failed = transform.transformInPlace(samples);

      // A partly-failed reprojection would bound a mixture of two coordinate
      // systems, which is worse than declining to answer.
      if (failed > 0)
      {
        return {};
      }

      QRectF bounds(samples.first(), samples.first());

      for (const QPointF &point : samples)
      {
        bounds.setLeft(std::min(bounds.left(), point.x()));
        bounds.setRight(std::max(bounds.right(), point.x()));
        bounds.setTop(std::min(bounds.top(), point.y()));
        bounds.setBottom(std::max(bounds.bottom(), point.y()));
      }

      return bounds;
    }
  }

  MapCanvas::MapCanvas(QWidget *parent)
    : QWidget(parent),
      m_background(Qt::white)
  {
    setObjectName(QStringLiteral("mapCanvas"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Qt only delivers a background otherwise, and the canvas paints every
    // pixel itself.
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_transform.setViewport(QSizeF(size()));
  }

  MapCanvas::~MapCanvas() = default;

  void MapCanvas::setModel(LayerStackModel *model)
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
      // One connection covers order, membership, visibility and content: the
      // stack folds all of them into renderChanged precisely so a view cannot
      // subscribe to some and silently miss others.
      connect(m_model, &LayerStackModel::renderChanged, this,
              &MapCanvas::onStackChanged);

      // A layer arriving has to be told what CRS it is being drawn into
      // before it first paints, or it reprojects on the second frame and the
      // first one lands somewhere else entirely.
      connect(m_model, &LayerStackModel::rowsInserted, this,
              [this](const QModelIndex &parent, int first, int last)
              {
                if (parent.isValid())
                {
                  return;
                }

                for (int row = first; row <= last; ++row)
                {
                  if (MapLayer *layer = m_model->layerAt(row))
                  {
                    layer->setMapCrs(m_crs);
                  }
                }
              });

      publishCrs();
    }

    update();
  }

  LayerStackModel *MapCanvas::model() const
  {
    return m_model;
  }

  const SpatialReference *MapCanvas::crs() const
  {
    return m_crs.get();
  }

  void MapCanvas::setCrs(std::shared_ptr<SpatialReference> crs)
  {
    m_crs = std::move(crs);

    publishCrs();
    update();
  }

  void MapCanvas::publishCrs()
  {
    if (!m_model)
    {
      return;
    }

    // Every layer reprojects into the map's CRS, so every layer has to be
    // told when it changes — including the ones already loaded.
    for (MapLayer *layer : m_model->layers())
    {
      layer->setMapCrs(m_crs);
    }
  }

  const MapTransform &MapCanvas::transform() const
  {
    syncViewport();

    return m_transform;
  }

  void MapCanvas::syncViewport() const
  {
    const QSizeF current(size());

    if (current == m_transform.viewport())
    {
      return;
    }

    m_transform.setViewport(current);

    // Re-apply the standing framing at the new size. This lives here rather
    // than in resizeEvent because Qt does not deliver a resize event to a
    // widget that has never been shown — which is precisely the case that
    // needs it, a map framed while its tab was still hidden.
    if (!m_viewMovedByUser && !m_framedExtent.isNull())
    {
      m_transform.fit(m_framedExtent, current);
    }
  }

  void MapCanvas::setVisibleExtent(const QRectF &extent)
  {
    if (extent.isNull())
    {
      return;
    }

    syncViewport();
    m_transform.fit(extent, QSizeF(size()));

    m_fitted = true;
    m_framedExtent = extent;
    m_viewMovedByUser = false;

    Q_EMIT transformChanged();
    update();
  }

  QRectF MapCanvas::fullExtent() const
  {
    if (!m_model)
    {
      return {};
    }

    QRectF united;
    QRectF backdrop;
    bool unitedValid = false;
    bool backdropValid = false;

    for (const MapLayer *layer : m_model->layers())
    {
      if (!layer->isVisible())
      {
        continue;
      }

      const QRectF extent = layerExtentInMapCrs(layer);

      if (extent.isNull())
      {
        continue;
      }

      // A basemap covers the whole world, so including it would make
      // "zoom to everything" always mean "zoom to the planet" and never to
      // the data drawn on top of it.
      if (layer->isBasemap())
      {
        expandTo(backdrop, backdropValid, extent);
        continue;
      }

      expandTo(united, unitedValid, extent);
    }

    // With nothing but a basemap loaded, the world is the right answer.
    return unitedValid ? united : backdrop;
  }

  void MapCanvas::zoomToFullExtent()
  {
    setVisibleExtent(fullExtent());
  }

  void MapCanvas::zoomToLayer(const MapLayer *layer)
  {
    if (!layer)
    {
      return;
    }

    setVisibleExtent(layerExtentInMapCrs(layer));
  }

  void MapCanvas::zoomBy(double factor)
  {
    syncViewport();
    m_transform.zoomAt(factor, QPointF(width() * 0.5, height() * 0.5));
    m_viewMovedByUser = true;

    Q_EMIT transformChanged();
    update();
  }

  QColor MapCanvas::backgroundColor() const
  {
    return m_background;
  }

  void MapCanvas::setBackgroundColor(const QColor &color)
  {
    m_background = color;
    update();
  }

  QSize MapCanvas::sizeHint() const
  {
    return QSize(640, 480);
  }

  void MapCanvas::paintEvent(QPaintEvent *event)
  {
    Q_UNUSED(event)

    syncViewport();

    QPainter painter(this);
    painter.fillRect(rect(), m_background);

    if (!m_model || !m_transform.isValid())
    {
      return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Bottom of the stack first, so row 0 — the top of the layer tree — is
    // drawn last and lands on top, as the tree shows it.
    for (MapLayer *layer : m_model->renderOrder())
    {
      if (!layer->isVisible() || layer->opacity() <= 0.0)
      {
        continue;
      }

      // Saving around each layer means one layer's pen, brush or clip cannot
      // leak into the next one's drawing.
      painter.save();
      painter.setOpacity(layer->opacity());
      layer->render(painter, m_transform);
      painter.restore();
    }

    paintAttribution(painter);
  }

  void MapCanvas::paintAttribution(QPainter &painter)
  {
    QStringList credits;

    for (const MapLayer *layer : m_model->layers())
    {
      const QString credit = layer->attribution();

      // Hidden layers are not on the map, so their provider is owed nothing.
      if (layer->isVisible() && !credit.isEmpty()
          && !credits.contains(credit))
      {
        credits.append(credit);
      }
    }

    if (credits.isEmpty())
    {
      return;
    }

    const QString text = credits.join(QStringLiteral(" · "));

    QFont font = painter.font();
    font.setPointSizeF(qMax(7.0, font.pointSizeF() - 2.0));

    const QFontMetricsF metrics(font);
    const QRectF box(width() - metrics.horizontalAdvance(text) - 10.0,
                     height() - metrics.height() - 4.0,
                     metrics.horizontalAdvance(text) + 6.0,
                     metrics.height() + 2.0);

    // Drawn last and over everything, on its own backing: a licence
    // condition that a data layer can paint over is not met.
    painter.save();
    painter.setOpacity(1.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 190));
    painter.drawRect(box);

    painter.setFont(font);
    painter.setPen(QColor(40, 40, 40));
    painter.drawText(box, Qt::AlignCenter, text);
    painter.restore();
  }

  void MapCanvas::resizeEvent(QResizeEvent *event)
  {
    QWidget::resizeEvent(event);

    // Through syncViewport, never setViewport directly: the viewport change
    // is what triggers re-framing, and setting it here would leave the sizes
    // already matching so the later sync had nothing left to notice.
    syncViewport();

    if (!m_fitted)
    {
      onStackChanged();
    }

    Q_EMIT transformChanged();
  }

  void MapCanvas::onStackChanged()
  {
    // The first layer to arrive frames the view. Without this the canvas
    // shows an empty white rectangle after a successful load, since the
    // default view is around the origin and most data is nowhere near it.
    syncViewport();

    if (!m_fitted && width() > 0 && height() > 0)
    {
      const QRectF extent = fullExtent();

      if (!extent.isNull())
      {
        m_transform.fit(extent, QSizeF(size()));

        m_fitted = true;
        m_framedExtent = extent;

        Q_EMIT transformChanged();
      }
    }

    update();
  }

  void MapCanvas::mousePressEvent(QMouseEvent *event)
  {
    syncViewport();

    if (event->button() == Qt::LeftButton && m_transform.isValid())
    {
      m_panning = true;
      m_lastPanPosition = event->pos();
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }

    QWidget::mousePressEvent(event);
  }

  void MapCanvas::mouseMoveEvent(QMouseEvent *event)
  {
    syncViewport();

    if (m_transform.isValid())
    {
      Q_EMIT cursorMoved(m_transform.toWorld(QPointF(event->pos())));
    }

    if (m_panning)
    {
      m_transform.panByPixels(QPointF(event->pos() - m_lastPanPosition));
      m_lastPanPosition = event->pos();
      m_viewMovedByUser = true;

      Q_EMIT transformChanged();
      update();

      event->accept();
      return;
    }

    QWidget::mouseMoveEvent(event);
  }

  void MapCanvas::mouseReleaseEvent(QMouseEvent *event)
  {
    if (m_panning && event->button() == Qt::LeftButton)
    {
      m_panning = false;
      unsetCursor();
      event->accept();
      return;
    }

    QWidget::mouseReleaseEvent(event);
  }

  void MapCanvas::wheelEvent(QWheelEvent *event)
  {
    syncViewport();

    if (!m_transform.isValid())
    {
      QWidget::wheelEvent(event);
      return;
    }

    const double notches = event->angleDelta().y() / 120.0;

    if (qFuzzyIsNull(notches))
    {
      QWidget::wheelEvent(event);
      return;
    }

    // Anchored at the pointer: zooming about the centre slides whatever the
    // user is pointing at off-screen, which is what makes a map feel wrong.
    m_transform.zoomAt(std::pow(kWheelZoomFactor, notches),
                       event->position());
    m_viewMovedByUser = true;

    Q_EMIT transformChanged();
    update();

    event->accept();
  }

  QRectF MapCanvas::layerExtentInMapCrs(const MapLayer *layer) const
  {
    if (!layer)
    {
      return {};
    }

    const QRectF extent = layer->extent();

    if (extent.isNull())
    {
      return {};
    }

    const SpatialReference *source = layer->crs();

    // With either side unknown there is nothing to reproject between, so the
    // extent is taken at face value — the single-CRS case, which is most of
    // them.
    if (!source || !m_crs || source->isSameAs(*m_crs))
    {
      return extent;
    }

    QString message;
    const std::unique_ptr<CoordinateTransform> transform =
      CoordinateTransform::between(*source, *m_crs, message);

    if (!transform)
    {
      return {};
    }

    return reprojectExtent(extent, *transform);
  }

} // namespace HydroCouple::Composer
