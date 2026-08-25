/*!
 * \file   compositioncanvas.h
 * \author Caleb Buahin
 * \brief  CompositionCanvas — the composition scene's viewport.
 *
 * Adds the interactions that belong to the view rather than the graph: zoom,
 * rubber-band selection, deleting the selection, and accepting drops from the
 * component palette.
 */

#ifndef HYDROCOUPLECOMPOSER_CANVAS_COMPOSITIONCANVAS_H
#define HYDROCOUPLECOMPOSER_CANVAS_COMPOSITIONCANVAS_H

#include "canvas/compositionscene.h"

#include <QGraphicsView>

namespace HydroCouple::Composer
{

  //! The MIME type a component palette drag carries.
  inline constexpr const char *kComponentMimeType =
    "application/x-hydrocouple-component-id";

  /*!
   * \brief The composition editor's viewport.
   */
  class CompositionCanvas : public QGraphicsView
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds a canvas showing \a scene.
       * \param scene The composition scene to display.
       * \param parent Optional parent widget.
       */
      explicit CompositionCanvas(CompositionScene *scene,
                                 QWidget *parent = nullptr);

      /*!
       * \brief The scene being displayed.
       */
      [[nodiscard]] CompositionScene *compositionScene() const;

    protected:
      void keyPressEvent(QKeyEvent *event) override;
      void wheelEvent(QWheelEvent *event) override;
      void dragEnterEvent(QDragEnterEvent *event) override;
      void dragMoveEvent(QDragMoveEvent *event) override;
      void dropEvent(QDropEvent *event) override;

    private:
      CompositionScene *m_scene = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CANVAS_COMPOSITIONCANVAS_H
