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
       * \brief Selects the node for \a componentId, and only it.
       *
       * An empty id, or one with no node, clears the selection: the caller
       * is saying "nothing here", and leaving the previous node lit would
       * make the canvas claim a relationship that does not hold.
       *
       * \param componentId The component to select.
       * \returns Whether a node was found and selected.
       */
      bool selectComponent(const QString &componentId);

      /*!
       * \brief Every connection edge currently drawn.
       */
      [[nodiscard]] QList<ConnectionEdgeItem *> edges() const;

      /*!
       * \brief The @from binding edges currently drawn.
       */
      [[nodiscard]] QList<BindingEdgeItem *> bindingEdges() const;

      /*!
       * \brief The spliced adapter nodes currently drawn.
       */
      [[nodiscard]] QList<AdapterNodeItem *> adapterNodes() const;

      /*!
       * \brief Arranges the whole composition left to right by dependency.
       *
       * One undo entry for the lot: a layout the user does not like is a
       * single Ctrl+Z, not one per component. Adapter nodes give up any
       * position they were dragged to and return to sitting along their
       * connection, because a spliced node left where the old edge used to
       * run is stranded rather than placed.
       *
       * \returns How many components were moved.
       */
      int applyAutoLayout();

      /*!
       * \brief One adapter a factory offers for splicing into a connection.
       */
      struct AdapterOffering
      {
          QString factoryId;
          QString adapterId;
          QString caption;
      };

      /*!
       * \brief What the Insert adapter… menu lists for a connection:
       *        offerings from the provider component's own factories first
       *        (they win at run time), then the standalone ones — each
       *        filtered through getAvailableAdaptedOutputIds() against the
       *        LIVE provider output and consumer input.
       *
       * Availability is asked of the raw output, not an edit-time chain:
       * edit-time never builds chains, and the run pipeline validates the
       * real thing with its own messages.
       */
      [[nodiscard]] QList<AdapterOffering> adapterOfferings(
        const HydroCouple::SDK::IO::ConnectionSpec &connection);

      /*!
       * \brief Inserts a chain step, capturing the offering's argument
       *        defaults through a scratch instance — what the Insert
       *        adapter… menu action performs.
       *
       * The scratch adapted output is unregistered and destroyed before the
       * document edit; capturing defaults at insertion is what lets the
       * inspector stay document-pure.
       */
      bool insertAdapter(const HydroCouple::SDK::IO::ConnectionSpec &connection, int index,
                         const QString &factoryId, const QString &adapterId);

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

    Q_SIGNALS:
      /*!
       * \brief A component add was refused (e.g. an adapter-factory library
       *        dropped on the canvas); \a reason says why, for the log.
       */
      void componentRefused(const QString &reason);

    protected:
      void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
      void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
      void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
      void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    private:
      void refreshEdges();
      void applyPlacement(const QString &componentId);
      void applyAdapterPlacements();

      //! The live provider output / consumer input of a connection, or null.
      [[nodiscard]] HydroCouple::IOutput *liveOutput(
        const HydroCouple::SDK::IO::ConnectionSpec &connection) const;
      [[nodiscard]] HydroCouple::IInput *liveInput(
        const HydroCouple::SDK::IO::ConnectionSpec &connection) const;

      //! Runs the Insert adapter… picker and applies the choice.
      void insertAdapterInteractively(const HydroCouple::SDK::IO::ConnectionSpec &connection,
                                      int index);
      [[nodiscard]] PortItem *portAt(const QPointF &scenePosition) const;

      /*!
       * \brief The exact-hit port, or the nearest one within \a radius.
       *
       * Used by the connection drag's move and release — a 5 px dot is a
       * hard target. The press keeps exact hits, so starting a drag on a
       * node body still moves the node.
       */
      [[nodiscard]] PortItem *portNear(const QPointF &scenePosition,
                                       qreal radius) const;
      [[nodiscard]] QString uniqueComponentId(const QString &desired) const;

      CompositionDocument *m_document = nullptr;
      ComponentInstances *m_instances = nullptr;

      QHash<QString, ComponentNodeItem *> m_nodes;
      QList<ConnectionEdgeItem *> m_edges;
      QList<BindingEdgeItem *> m_bindingEdges;
      QList<AdapterNodeItem *> m_adapterNodes;

      // In-progress port drag.
      PortItem *m_dragSource = nullptr;
      PortItem *m_dragTarget = nullptr;
      QGraphicsPathItem *m_dragPreview = nullptr;

      bool m_rebuilding = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CANVAS_COMPOSITIONSCENE_H
