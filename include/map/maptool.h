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
    //! Drag to pan, click to identify. The default, and what the map has
    //! always done.
    Pan,

    //! Drag a rectangle to zoom to it, click to zoom in about the point.
    Zoom,
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

    protected:
      MapCanvas *m_canvas = nullptr;
  };

  /*!
   * \brief Drag to pan; click to identify what is underneath.
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
   * \brief Drag a rectangle to zoom to it; click to zoom in about the point.
   */
  class ZoomTool : public MapTool
  {
    public:
      explicit ZoomTool(MapCanvas *canvas);

      ~ZoomTool() override;

      bool press(QMouseEvent *event) override;
      bool move(QMouseEvent *event) override;
      bool release(QMouseEvent *event) override;

      [[nodiscard]] QCursor idleCursor() const override;

    private:
      //! Hides the band and forgets the gesture.
      void endGesture();

      bool m_dragging = false;
      QPoint m_origin;
      std::unique_ptr<QRubberBand> m_band;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MAP_MAPTOOL_H
