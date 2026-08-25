/*!
 * \file   compositionscene.h
 * \author Caleb Buahin
 * \brief  CompositionScene — the composition graph, rendered and edited.
 *
 * The scene is a *view* in the MVC sense: it holds no composition state of its
 * own, it rebuilds from CompositionDocument's change signals, and every edit
 * it originates is pushed as a command on the document's undo stack. Dragging
 * a node, drawing a connection and deleting a selection are therefore all
 * undoable, and all immediately visible to every other view of the same
 * document.
 */

#ifndef HYDROCOUPLECOMPOSER_CANVAS_COMPOSITIONSCENE_H
#define HYDROCOUPLECOMPOSER_CANVAS_COMPOSITIONSCENE_H

#include "canvas/canvasitems.h"
#include "project/componentinstances.h"
#include "project/compositiondocument.h"

#include <QGraphicsScene>
#include <QHash>

class QGraphicsPathItem;

namespace HydroCouple::Composer
{

  /*!
   * \brief Renders and edits a CompositionDocument as a node graph.
   */
  class CompositionScene : public QGraphicsScene
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds a scene over \a document.
       * \param document The composition to render and edit.
       * \param instances Supplies each component's ports; may be nullptr, in
       *        which case boxes are drawn without ports.
       * \param parent Optional Qt parent.
       */
      CompositionScene(CompositionDocument *document,
                       ComponentInstances *instances,
                       QObject *parent = nullptr);

      ~CompositionScene() override;

      /*!
       * \brief The node for \a componentId, or nullptr.
       * \param componentId Component to look up.
       */
      [[nodiscard]] ComponentNodeItem *node(const QString &componentId) const;

      /*!
       * \brief Every connection edge currently drawn.
       */
      [[nodiscard]] QList<ConnectionEdgeItem *> edges() const;

      /*!
       * \brief Adds a component to the document at a canvas position.
       *
       * This is what a drop from the component palette performs.
       *
       * \param componentInfoId Registry id of the component to instantiate.
       * \param instanceId Desired document id; made unique if taken.
       * \param position Canvas position for the new node.
       * \returns The id actually assigned, or an empty string on failure.
       */
      QString addComponentAt(const QString &componentInfoId,
                             const QString &instanceId,
                             const QPointF &position);

      /*!
       * \brief Connects two exchange items, as a completed port drag does.
       * \param fromComponent Producer component id.
       * \param output Producer output id.
       * \param toComponent Consumer component id.
       * \param input Consumer input id.
       * \returns true when the connection was added.
       */
      bool connectPorts(const QString &fromComponent, const QString &output,
                        const QString &toComponent, const QString &input);

      /*!
       * \brief Removes whatever is selected, as pressing Delete does.
       * \returns The number of components and connections removed.
       */
      int removeSelection();

      /*!
       * \brief Rebuilds every item from the document.
       */
      void rebuild();

    protected:
      void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
      void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
      void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
      void refreshEdges();
      void applyPlacement(const QString &componentId);
      [[nodiscard]] PortItem *portAt(const QPointF &scenePosition) const;
      [[nodiscard]] QString uniqueComponentId(const QString &desired) const;

      CompositionDocument *m_document = nullptr;
      ComponentInstances *m_instances = nullptr;

      QHash<QString, ComponentNodeItem *> m_nodes;
      QList<ConnectionEdgeItem *> m_edges;

      // In-progress port drag.
      PortItem *m_dragSource = nullptr;
      PortItem *m_dragTarget = nullptr;
      QGraphicsPathItem *m_dragPreview = nullptr;

      bool m_rebuilding = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CANVAS_COMPOSITIONSCENE_H
