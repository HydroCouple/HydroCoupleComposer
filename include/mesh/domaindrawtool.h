/*!
 * \file   domaindrawtool.h
 * \author Caleb Buahin
 * \brief  DomainDrawTool — drawing a domain's parts on the map.
 *
 * One tool for all four parts, because the gesture is the same one: click to
 * put down a vertex, right-click to finish. Only what the finished vertices
 * become differs, and that is a switch at the end rather than four tools
 * that would each have to re-implement the drawing.
 *
 * A point finishes on the first click, since there is nothing to accumulate.
 *
 * Right-click to finish rather than double-click: the canvas forwards
 * presses of every button to the tool already, so this needs no new event
 * plumbing — and a double-click is indistinguishable from two vertices
 * placed in a hurry.
 *
 * The tool writes to the model and nowhere else. What appears on the map is
 * the layers re-reading the model, so a domain drawn here and a domain
 * loaded from a file arrive on screen by exactly the same path.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_DOMAINDRAWTOOL_H
#define HYDROCOUPLECOMPOSER_MESH_DOMAINDRAWTOOL_H

#include "layers/domainlayer.h"
#include "map/maptool.h"

#include <QPolygonF>

namespace HydroCouple::Composer
{
  class MeshDomainModel;

  /*!
   * \brief Draws one part of a mesh domain on the map.
   */
  class DomainDrawTool : public MapTool
  {
    public:
      /*!
       * \brief Constructs a tool drawing \a part into \a model.
       * \param canvas The canvas to draw on; must outlive the tool.
       * \param model The domain to write to; must outlive the tool.
       * \param part Which part this tool draws.
       */
      DomainDrawTool(MapCanvas *canvas, MeshDomainModel *model,
                     DomainPart part);

      bool press(QMouseEvent *event) override;
      bool move(QMouseEvent *event) override;
      bool release(QMouseEvent *event) override;

      [[nodiscard]] QCursor idleCursor() const override;

      /*!
       * \brief Abandons a shape that was still being drawn.
       *
       * Nothing half-drawn reaches the domain: a boundary abandoned after
       * two clicks is not a boundary, and committing it would leave the user
       * to discover a degenerate ring they never finished.
       */
      void cancel() override;

      //! \returns Which part this tool draws.
      [[nodiscard]] DomainPart part() const;

      //! \returns The vertices placed so far.
      [[nodiscard]] const QPolygonF &pending() const;

    private:
      //! Commits the pending shape to the model, if it has enough vertices.
      void finish();

      //! Shows the pending shape on the canvas.
      void refreshSketch(const QPointF &cursor, bool hasCursor);

      /*!
       * \brief \a world moved onto a domain vertex within reach of it.
       *
       * So that a breakline can be made to start exactly on the boundary it
       * divides, rather than a hair off it — a gap of half a pixel is
       * invisible on the map and is a hole the triangulator meshes through.
       *
       * \param world Where the pointer is, in world coordinates.
       */
      [[nodiscard]] QPointF snapped(const QPointF &world) const;

      MeshDomainModel *m_model = nullptr;
      DomainPart m_part = DomainPart::Boundary;
      QPolygonF m_pending;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_DOMAINDRAWTOOL_H
