/*!
 * \file   domainedittool.h
 * \author Caleb Buahin
 * \brief  DomainEditTool — moving, adding and removing a domain's vertices.
 *
 * The counterpart to drawing: a domain is almost never right the first time,
 * and without this the only correction available is to draw the whole shape
 * again.
 *
 * Three gestures over one set of handles — drag a vertex to move it, click an
 * edge to put a vertex there and drag it in the same motion, right-click a
 * vertex to take it out. They are the gestures every GIS node editor uses,
 * which is the reason to use them rather than better ones.
 *
 * A drag writes each position **through the model**, so what is on the map
 * mid-drag is the domain itself. The alternative — dragging a copy and
 * committing it at the end — is a second piece of geometry that can disagree
 * with the first, which is the thing the model exists to prevent.
 */

#ifndef HYDROCOUPLECOMPOSER_MESH_DOMAINEDITTOOL_H
#define HYDROCOUPLECOMPOSER_MESH_DOMAINEDITTOOL_H

#include "map/maptool.h"
#include "mesh/meshdomain.h"

#include <QObject>
#include <QPointF>
#include <QVector>

namespace HydroCouple::Composer
{
  class MeshDomainModel;

  /*!
   * \brief Edits the vertices of a mesh domain on the map.
   */
  class DomainEditTool : public QObject, public MapTool
  {
      Q_OBJECT

    public:
      /*!
       * \brief Constructs an editor over \a model.
       * \param canvas The canvas to act on; must outlive the tool.
       * \param model The domain to edit; must outlive the tool.
       */
      DomainEditTool(MapCanvas *canvas, MeshDomainModel *model);

      ~DomainEditTool() override;

      bool press(QMouseEvent *event) override;
      bool move(QMouseEvent *event) override;
      bool release(QMouseEvent *event) override;

      [[nodiscard]] QCursor idleCursor() const override;

      /*!
       * \brief Abandons a drag, putting the vertex back where it started.
       *
       * A drag interrupted by switching tools was not finished, and the
       * vertex following the pointer to wherever it happened to be when the
       * user reached for the ribbon is not an edit anybody made.
       */
      void cancel() override;

      //! \returns The vertex being dragged; invalid when none is.
      [[nodiscard]] const DomainVertex &grabbed() const;

    private:
      /*!
       * \brief Republishes the handles the canvas draws.
       *
       * Called on every domain change, the tool's own edits included, so the
       * handle under the pointer follows the vertex it belongs to.
       */
      void refreshHandles();

      //! \returns How far from a vertex still counts, in world units.
      [[nodiscard]] double tolerance() const;

      MeshDomainModel *m_model = nullptr;

      //! Where each published handle lives, parallel to the canvas's list.
      QVector<DomainVertex> m_handles;

      DomainVertex m_grabbed;
      DomainVertex m_hovered;

      //! Where the dragged vertex was before the drag, for cancel().
      QPointF m_original;

      bool m_dragging = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_MESH_DOMAINEDITTOOL_H
