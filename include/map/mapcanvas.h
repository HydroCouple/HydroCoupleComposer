/*!
 * \file   mapcanvas.h
 * \author Caleb Buahin
 * \brief  MapCanvas — the widget that draws a layer stack.
 *
 * The canvas is a view, not a store: it owns the transform describing where
 * the map is looking and nothing else. Layers, their order and their
 * visibility all live in the LayerStackModel, so the layer tree and the canvas
 * are two views of one model and cannot disagree.
 *
 * Rendering is QPainter over the whole stack. The QSG/QRhi path arrives with
 * the 3D scene in C3, where it earns its complexity; using it here, for a
 * handful of vector layers, would mean maintaining a shader pipeline to draw
 * what QPainter draws in a line.
 */

#ifndef HYDROCOUPLECOMPOSER_MAP_MAPCANVAS_H
#define HYDROCOUPLECOMPOSER_MAP_MAPCANVAS_H

#include "map/maplayer.h"
#include "map/maptransform.h"

#include <QColor>
#include <QPoint>
#include <QWidget>

#include <memory>

namespace HydroCouple::Composer
{
  class FeatureLayer;
  class LayerStackModel;
  class SpatialReference;

  /*!
   * \brief Draws a layer stack, with pan and zoom.
   */
  class MapCanvas : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs a canvas with no layers.
       * \param parent Parent widget.
       */
      explicit MapCanvas(QWidget *parent = nullptr);

      ~MapCanvas() override;

      /*!
       * \brief Sets the layer stack to draw.
       * \param model The stack; the canvas does not take ownership.
       */
      void setModel(LayerStackModel *model);

      /*!
       * \brief The layer stack being drawn, or nullptr.
       */
      [[nodiscard]] LayerStackModel *model() const;

      /*!
       * \brief The CRS the map is drawn in.
       *
       * Layer extents in other systems are reprojected into it; layer
       * geometry is the layer's own responsibility.
       */
      [[nodiscard]] const SpatialReference *crs() const;

      /*!
       * \brief Sets the CRS the map is drawn in.
       * \param crs The map's coordinate reference system.
       */
      void setCrs(std::shared_ptr<SpatialReference> crs);

      /*!
       * \brief The current world-to-pixel mapping.
       */
      [[nodiscard]] const MapTransform &transform() const;

      /*!
       * \brief Shows \a extent, preserving aspect ratio.
       *
       * Exactly \a extent by default, with no air around it. Breathing room
       * belongs to the *commands* that frame data — zoomToFullExtent() and
       * zoomToLayer() ask for it — and not to the primitive they are built
       * on: a margin here is charged again on every hand-off from the 3D
       * view, and a user flipping between the tabs zooms steadily out.
       *
       * \param extent World rectangle in the map's CRS.
       * \param marginFraction Extra space around it, as a fraction.
       */
      void setVisibleExtent(const QRectF &extent,
                            double marginFraction = 0.0);

      /*!
       * \brief The union of every visible layer's extent, in the map's CRS.
       *
       * Empty when nothing visible has geometry.
       */
      [[nodiscard]] QRectF fullExtent() const;

      /*!
       * \brief Zooms to show every visible layer.
       */
      void zoomToFullExtent();

      /*!
       * \brief Zooms to show one layer.
       * \param layer Layer to frame; ignored when null or extentless.
       */
      void zoomToLayer(const MapLayer *layer);

      /*!
       * \brief Zooms about the centre of the viewport.
       * \param factor Values above 1 zoom in.
       */
      void zoomBy(double factor);

      /*!
       * \brief The background colour drawn beneath the layers.
       */
      [[nodiscard]] QColor backgroundColor() const;

      /*!
       * \brief Sets the background colour drawn beneath the layers.
       * \param color The colour to fill with.
       */
      void setBackgroundColor(const QColor &color);

      [[nodiscard]] QSize sizeHint() const override;

      /*!
       * \brief The feature under \a screen, and the layer it belongs to.
       *
       * Walked from the top of the stack down, so the answer is the feature
       * the user can see rather than one hidden beneath it.
       *
       * \param screen Widget position, as a click gives.
       * \param[out] feature Index within the layer returned.
       * \returns The layer picked, or nullptr when nothing was under it.
       */
      [[nodiscard]] FeatureLayer *pickAt(const QPoint &screen,
                                         int &feature) const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when a click selects a feature, or selects nothing.
       *
       * Carries the base type, not the concrete one: a signal's parameter has
       * to be a complete type where moc reads it, and naming the concrete
       * layer here would pull the whole layers tier into every translation
       * unit that draws a map. A listener that needs more casts.
       *
       * \param layer The layer picked, or nullptr when the click was on
       *        empty map.
       * \param feature Index within \a layer, or -1.
       */
      void featurePicked(MapLayer *layer, int feature);

      /*!
       * \brief Emitted when the view moves or its scale changes.
       */
      void transformChanged();

      /*!
       * \brief Emitted as the pointer moves over the map.
       * \param world Pointer position in the map's CRS.
       */
      void cursorMoved(const QPointF &world);

    protected:
      void paintEvent(QPaintEvent *event) override;

      void resizeEvent(QResizeEvent *event) override;

      void mousePressEvent(QMouseEvent *event) override;

      void mouseMoveEvent(QMouseEvent *event) override;

      void mouseReleaseEvent(QMouseEvent *event) override;

      void wheelEvent(QWheelEvent *event) override;

    private:
      /*!
       * \brief Extent of \a layer expressed in the map's CRS.
       * \param layer Layer to measure.
       */
      [[nodiscard]] QRectF layerExtentInMapCrs(const MapLayer *layer) const;

      /*!
       * \brief Repaints, and frames the first layer to arrive.
       */
      void onStackChanged();

      /*!
       * \brief Tells every layer what CRS the map is drawn in.
       */
      void publishCrs();

      /*!
       * \brief Draws the credits every visible layer requires.
       * \param painter Painter to draw with.
       */
      void paintAttribution(QPainter &painter);

      /*!
       * \brief Brings the transform's viewport up to date with the widget.
       *
       * Called from every entry point that reads or moves the view rather
       * than only from resizeEvent, because Qt defers the resize event of a
       * widget that has never been shown — an offscreen render or a
       * zoom-to-extent before the first show would otherwise be computed
       * against a viewport of the wrong size.
       */
      void syncViewport() const;

      LayerStackModel *m_model = nullptr;
      std::shared_ptr<SpatialReference> m_crs;

      //! Mutable so the viewport can be synchronised from const accessors;
      //! the view it describes is a cache of the widget's geometry, not
      //! state a caller sets.
      mutable MapTransform m_transform;
      QColor m_background;

      //! Whether the view has been framed on anything yet. Tracked rather
      //! than inferred from the transform, whose default scale is already a
      //! usable number and so cannot distinguish "framed" from "untouched".
      bool m_fitted = false;

      //! The extent the view was last asked to show, re-applied on resize
      //! until the user takes the view somewhere else. Without it, framing
      //! an extent before the widget has been laid out — opening a project
      //! while another tab is showing — fits into a placeholder-sized
      //! viewport and the wrong scale survives the real layout.
      QRectF m_framedExtent;

      //! Set once the user pans or zooms, after which a resize preserves the
      //! scale they chose instead of re-framing over it.
      bool m_viewMovedByUser = false;

      bool m_panning = false;
      QPoint m_pressPosition;
      QPoint m_lastPanPosition;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MAP_MAPCANVAS_H
