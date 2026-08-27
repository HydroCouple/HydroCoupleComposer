/*!
 * \file   maptool.h
 * \author Caleb Buahin
 * \brief  MapTool — what a press, drag and release do on the map.
 *
 * The canvas hard-coded its interactions: left drag panned, release picked,
 * wheel zoomed. Adding a fourth behaviour to that chain is where it stops
 * scaling, so the gestures move out into small objects the canvas switches
 * between.
 *
 * Two tools, not the three the plan named. Pan already picks on a click that
 * did not move the view, so a separate Identify tool would duplicate it
 * exactly — and FeatureLayer holds one selection at a time, so there is no
 * box-select for it to offer instead. A mode that does nothing its neighbour
 * does not is a control that teaches the user nothing.
 *
 * Wheel zoom stays on the canvas: it works under every tool, and moving it
 * into both of them would be the same code twice.
 */

#ifndef HYDROCOUPLECOMPOSER_MAP_MAPTOOL_H
#define HYDROCOUPLECOMPOSER_MAP_MAPTOOL_H

#include <QCursor>
#include <QPoint>

#include <memory>

class QMouseEvent;
class QRubberBand;

namespace HydroCouple::Composer
{
  class MapCanvas;

  /*!
   * \brief Which gesture set the map is under.
   */
  enum class MapToolKind
  {
    //! Click to identify one feature; drag a band to take every feature it
    //! crosses.
    Select,

    //! Drag to pan. The default, and what the map has always done.
    Pan,

    //! Drag a rectangle to zoom to it; click to zoom in about the point.
    ZoomIn,

    /*!
     * \brief Drag a rectangle to zoom out into it.
     *
     * The GIS convention, and the inverse of ZoomIn on the same rectangle:
     * what is on screen now is shrunk to fit inside the box drawn, so a
     * small box zooms out a long way and a nearly-full-screen one barely
     * moves.
     */
    ZoomOut,

    /*!
     * \brief Drag a line to cut a section through the layered mesh.
     *
     * A drag, not a click-per-vertex polyline: a section is read against
     * one straight run of ground, and a tool that needs to be told when the
     * user has finished is a tool that can be left half-drawn.
     */
    Transect,
  };

  /*!
   * \brief One set of mouse gestures over the map.
   */
  class MapTool
  {
    public:
      /*!
       * \brief Constructs a tool over \a canvas.
       * \param canvas The canvas to act on; must outlive the tool.
       */
      explicit MapTool(MapCanvas *canvas);

      virtual ~MapTool();

      MapTool(const MapTool &) = delete;
      MapTool &operator=(const MapTool &) = delete;

      /*!
       * \brief Handles a mouse press.
       * \param event The press.
       * \returns True when the tool took it.
       */
      virtual bool press(QMouseEvent *event) = 0;

      /*!
       * \brief Handles a mouse move.
       * \param event The move.
       * \returns True when the tool took it.
       */
      virtual bool move(QMouseEvent *event) = 0;

      /*!
       * \brief Handles a mouse release.
       * \param event The release.
       * \returns True when the tool took it.
       */
      virtual bool release(QMouseEvent *event) = 0;

      //! \returns The cursor this tool wants while idle.
      [[nodiscard]] virtual QCursor idleCursor() const;

      /*!
       * \brief Abandons a gesture in progress.
       *
       * Called before the canvas swaps this tool out, and never during the
       * canvas's own destruction — which is why it is a method rather than
       * a destructor. A tool that undid its half-finished work from ~Tool()
       * would reach into a canvas that is being torn down, and the only
       * observable form of that is heap corruption in whatever runs next.
       */
      virtual void cancel();

    protected:
      MapCanvas *m_canvas = nullptr;
  };

  /*!
   * \brief Drag to pan; click to identify what is underneath.
   *
   * A click still identifies under Pan, as it always has. Select exists for
   * people who want the mode named and want to drag a band; taking the click
   * away from Pan would remove a thing the map has always done.
   */
  class PanTool : public MapTool
  {
    public:
      using MapTool::MapTool;

      bool press(QMouseEvent *event) override;
      bool move(QMouseEvent *event) override;
      bool release(QMouseEvent *event) override;

      [[nodiscard]] QCursor idleCursor() const override;

    private:
      bool m_panning = false;
      QPoint m_pressPosition;
      QPoint m_lastPosition;
  };

  /*!
   * \brief A tool that drags a rectangle out on the canvas.
   *
   * Three of the four do this and differ only in what they do with the
   * rectangle at the end, so the band — showing it, resizing it, telling a
   * drag from a click — lives here once.
   */
  class RubberBandTool : public MapTool
  {
    public:
      explicit RubberBandTool(MapCanvas *canvas);

      ~RubberBandTool() override;

      bool press(QMouseEvent *event) override;
      bool move(QMouseEvent *event) override;
      bool release(QMouseEvent *event) override;

    protected:
      /*!
       * \brief Acts on a rectangle the user dragged out.
       * \param rectangle The band's final geometry, in widget pixels.
       */
      virtual void useRectangle(const QRect &rectangle) = 0;

      /*!
       * \brief Acts on a press and release too close together to be a drag.
       * \param pixel Where the click landed.
       */
      virtual void useClick(const QPoint &pixel) = 0;

    private:
      void endGesture();

      bool m_dragging = false;
      QPoint m_origin;
      std::unique_ptr<QRubberBand> m_band;
  };

  /*!
   * \brief Click to identify one feature; drag a band to take several.
   */
  class SelectTool : public RubberBandTool
  {
    public:
      using RubberBandTool::RubberBandTool;

      [[nodiscard]] QCursor idleCursor() const override;

    protected:
      void useRectangle(const QRect &rectangle) override;
      void useClick(const QPoint &pixel) override;
  };

  /*!
   * \brief Drag a rectangle to zoom to it; click to zoom in about the point.
   */
  class ZoomInTool : public RubberBandTool
  {
    public:
      using RubberBandTool::RubberBandTool;

      [[nodiscard]] QCursor idleCursor() const override;

    protected:
      void useRectangle(const QRect &rectangle) override;
      void useClick(const QPoint &pixel) override;
  };

  /*!
   * \brief Drag a rectangle to zoom out into it; click to zoom out.
   */
  class ZoomOutTool : public RubberBandTool
  {
    public:
      using RubberBandTool::RubberBandTool;

      [[nodiscard]] QCursor idleCursor() const override;

    protected:
      void useRectangle(const QRect &rectangle) override;
      void useClick(const QPoint &pixel) override;
  };

  /*!
   * \brief Drag a line; the map cuts a section along it.
   *
   * Not a RubberBandTool: what is being dragged out is a line, and the band
   * base draws — and reasons about — a rectangle. The line is published to
   * the canvas as it is dragged, so the section under it is visible where it
   * was cut rather than only in the panel that reads it.
   */
  class TransectTool : public MapTool
  {
    public:
      using MapTool::MapTool;

      bool press(QMouseEvent *event) override;
      bool move(QMouseEvent *event) override;
      bool release(QMouseEvent *event) override;

      [[nodiscard]] QCursor idleCursor() const override;

      /*!
       * \brief Takes back a half-drawn line.
       *
       * The preview lives on the canvas rather than in a child widget, so
       * unlike a rubber band it does not disappear when the tool does.
       * Switching tools mid-drag would otherwise leave a line on the map
       * belonging to a gesture nobody is making — and a line the user
       * finished drawing is not half-finished, so it stays.
       */
      void cancel() override;

    private:
      bool m_drawing = false;
      QPoint m_origin;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MAP_MAPTOOL_H
