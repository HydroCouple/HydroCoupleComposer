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
#include <QPolygonF>
#include "map/maptool.h"

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
       * \brief The current scale as the N of a 1:N ratio.
       *
       * Ground metres per pixel divided by the physical size of a screen
       * pixel, so it answers the question a printed scale bar answers:
       * how much smaller than life is this.
       *
       * Two conversions are not optional. The screen's own DPI is read
       * rather than assumed to be 96 — the assumption is wrong by about
       * twice on a Retina display. And the map's CRS decides what a world
       * unit is worth: metres for one projected system, feet for another,
       * degrees for a geographic one, where a degree of longitude is worth
       * less the further from the equator the view sits.
       *
       * \returns The denominator, or 1 when there is no valid view to
       *          measure.
       */
      [[nodiscard]] double scaleDenominator() const;

      /*!
       * \brief Zooms to exactly 1:\a denominator, keeping the centre.
       *
       * The exact inverse of scaleDenominator(), so setting a scale and
       * reading it back returns what was set.
       *
       * \param denominator The N of the 1:N wanted; ignored when not
       *        positive.
       */
      void setScaleDenominator(double denominator);

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

      // ── what the map tools act through ───────────────────────────────────

      /*!
       * \brief Which gesture set the map is under.
       */
      [[nodiscard]] MapToolKind toolKind() const;

      /*!
       * \brief Switches gesture set.
       *
       * A gesture in progress is abandoned rather than carried across, so a
       * rubber band cannot be left on screen belonging to a tool that is no
       * longer active.
       *
       * \param kind The gestures wanted.
       */
      void setToolKind(MapToolKind kind);

      /*!
       * \brief Slides the view by \a pixels.
       * \param pixels Screen-space offset to move the content by.
       */
      void panByPixels(const QPointF &pixels);

      /*!
       * \brief Zooms about a point rather than about the centre.
       *
       * What the wheel does, and what a zoom-tool click does: zooming about
       * the centre slides whatever the user is pointing at off-screen.
       *
       * \param factor Values above 1 zoom in.
       * \param pixel The point to hold still.
       */
      void zoomAtPixel(double factor, const QPoint &pixel);

      /*!
       * \brief Frames the world under a screen rectangle.
       *
       * No margin: the rectangle is what was asked for, and breathing room
       * belongs to the commands that frame *data* — zoomToFullExtent() and
       * zoomToLayer() — not to one the user drew themselves.
       *
       * \param rectangle Screen rectangle; ignored when degenerate.
       */
      void zoomToScreenRect(const QRect &rectangle);

      /*!
       * \brief Shrinks what is on screen to fit inside \a rectangle.
       *
       * The inverse of zoomToScreenRect() on the same box, and the GIS
       * convention for a zoom-out drag: a small box zooms out a long way, a
       * nearly-full-screen one barely moves. Expressed as a ratio of the
       * viewport to the box rather than as a fixed step, which is what makes
       * it the inverse rather than merely the opposite direction.
       *
       * \param rectangle Screen rectangle; ignored when degenerate.
       */
      void zoomOutToScreenRect(const QRect &rectangle);

      /*!
       * \brief Selects every feature \a rectangle catches.
       *
       * In the topmost visible layer that catches anything, so that one
       * layer holds the selection exactly as it does after a click. A band
       * spread over three layers is a selection the table can only ever
       * show a third of.
       *
       * \param rectangle Screen rectangle to select within.
       */
      void selectIn(const QRect &rectangle);

      /*!
       * \brief Identifies what is under \a screen and selects it.
       *
       * Selecting nothing when nothing is there, which is how a user says
       * "nothing" — leaving the last selection standing would make the table
       * beside it describe somewhere they have navigated away from.
       *
       * \param screen Widget position, as a click gives.
       */
      void pickAndSelectAt(const QPoint &screen);

      /*!
       * \brief Sets the section line drawn across the map.
       *
       * The line is map state, not the section panel's: it is a thing the
       * user drew on the map, it stays visible after the gesture, and every
       * view that wants to know where the section was cut reads it from
       * here. An empty polygon clears it.
       *
       * \param world The line in the map's CRS.
       */
      void setTransectLine(const QPolygonF &world);

      //! \returns The section line, in the map's CRS; empty when none.
      [[nodiscard]] const QPolygonF &transectLine() const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when the section line changes, drag included.
       *
       * During the drag as well as at the end of it, so the section under
       * the line is the section of the line being drawn. A cut costs one
       * pass over the mesh, which is what the map itself costs to redraw.
       *
       * \param world The new line, or an empty polygon when cleared.
       */
      void transectDrawn(const QPolygonF &world);

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
       * \brief Emitted when the 1:N scale changes.
       *
       * Separate from transformChanged() because a pan moves the view
       * without changing the scale, and a scale readout that rewrote itself
       * on every pan would fight anyone typing into it.
       *
       * \param denominator The new N of the 1:N.
       */
      void scaleChanged(double denominator);

      /*!
       * \brief Emitted as the pointer moves over the map.
       * \param world Pointer position in the map's CRS.
       */
      void cursorMoved(const QPointF &world);

      /*!
       * \brief Emitted when the map's coordinate reference system changes.
       *
       * What a CRS indicator listens to. Distinct from transformChanged():
       * changing the system redraws every layer without moving the view,
       * and moving the view does not change the system.
       */
      void crsChanged();

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
       * \brief Draws the section line, if one has been cut.
       * \param painter Painter to draw with.
       */
      void paintTransectLine(QPainter &painter) const;

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

      /*!
       * \brief The active tool, built on first use.
       *
       * \returns The tool; never null.
       */
      [[nodiscard]] MapTool *activeTool();

      std::unique_ptr<MapTool> m_tool;
      MapToolKind m_toolKind = MapToolKind::Pan;

      //! Where a section was cut, in the map's CRS; empty when none.
      QPolygonF m_transectLine;

      //! What scaleChanged() last reported, so a pan does not re-announce it.
      mutable double m_lastDenominator = 0.0;

      /*!
       * \brief Emits transformChanged(), and scaleChanged() when it moved.
       *
       * One place, so every route that moves the view — a fit, a wheel, a
       * typed scale — reports it the same way.
       */
      void announceTransformChanged();

      /*!
       * \brief How many metres one unit of the map's CRS is worth.
       *
       * Linear units for a projected system; a degree of longitude at the
       * view's centre latitude for a geographic one.
       */
      [[nodiscard]] double metresPerWorldUnit() const;

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
