/*!
 * \file   canvasitems.h
 * \author Caleb Buahin
 * \brief  Graphics items for the composition canvas.
 *
 * Three item kinds make up the graph: a box per component, a port per exchange
 * item, and an edge per connection. They are deliberately dumb — they draw and
 * they report interaction, but they never mutate the composition. Every edit
 * goes back through CompositionDocument's commands, which is what keeps the
 * canvas consistent with the other views and undoable.
 */

#ifndef HYDROCOUPLECOMPOSER_CANVAS_CANVASITEMS_H
#define HYDROCOUPLECOMPOSER_CANVAS_CANVASITEMS_H

#include "project/componentinstances.h"

#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QString>

namespace HydroCouple::Composer
{

  class ComponentNodeItem;

  /*!
   * \brief One exchange item's connection point on a component box.
   */
  class PortItem : public QGraphicsItem
  {
    public:
      enum class Direction
      {
        Input,
        Output
      };

      enum
      {
        Type = UserType + 1
      };

      PortItem(ComponentNodeItem *node, ExchangeItemDescriptor descriptor,
               Direction direction);

      [[nodiscard]] int type() const override;

      [[nodiscard]] QRectF boundingRect() const override;

      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

      [[nodiscard]] QString itemId() const;

      [[nodiscard]] Direction direction() const;

      [[nodiscard]] ComponentNodeItem *node() const;

      /*!
       * \brief Where an edge should attach, in scene coordinates.
       */
      [[nodiscard]] QPointF anchor() const;

      /*!
       * \brief Highlights the port while a connection is being dragged to it.
       * \param highlighted Whether to draw the port highlighted.
       */
      void setHighlighted(bool highlighted);

    private:
      ComponentNodeItem *m_node = nullptr;
      ExchangeItemDescriptor m_descriptor;
      Direction m_direction;
      bool m_highlighted = false;
  };

  /*!
   * \brief One component, drawn as a titled box carrying its ports.
   */
  class ComponentNodeItem : public QGraphicsObject
  {
      Q_OBJECT

    public:
      enum
      {
        Type = UserType + 2
      };

      /*!
       * \brief Builds a node for \a componentId.
       * \param componentId The document's component id.
       * \param caption Display caption.
       * \param inputs Input ports to create.
       * \param outputs Output ports to create.
       */
      ComponentNodeItem(QString componentId, const QString &caption,
                        const QList<ExchangeItemDescriptor> &inputs,
                        const QList<ExchangeItemDescriptor> &outputs);

      [[nodiscard]] int type() const override;

      [[nodiscard]] QRectF boundingRect() const override;

      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

      [[nodiscard]] QString componentId() const;

      /*!
       * \brief The port for \a itemId in \a direction, or nullptr.
       * \param itemId Exchange item id.
       * \param direction Whether to look among inputs or outputs.
       */
      [[nodiscard]] PortItem *port(const QString &itemId,
                                   PortItem::Direction direction) const;

      /*!
       * \brief Marks the component as unavailable, e.g. its library is missing.
       * \param reason Explanation shown on the box; empty clears the mark.
       */
      void setUnavailable(const QString &reason);

    Q_SIGNALS:
      /*!
       * \brief Emitted when the user finishes moving this node.
       * \param componentId The node's component.
       * \param position The node's new position.
       */
      void moved(const QString &componentId, const QPointF &position);

    protected:
      QVariant itemChange(GraphicsItemChange change,
                          const QVariant &value) override;

      void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
      void layoutPorts();

      QString m_componentId;
      QString m_caption;
      QString m_unavailableReason;
      QList<PortItem *> m_inputs;
      QList<PortItem *> m_outputs;
      QSizeF m_size;
      bool m_moving = false;
  };

  class AdapterNodeItem;

  /*!
   * \brief One connection, drawn between an output port and an input port.
   *
   * One edge owns the WHOLE connection, spliced adapters included: the path
   * routes output → adapter(s) → input, but selection of the edge still
   * means "the connection", while selection of an adapter node means "that
   * chain step". Splitting the edge into per-leg items would invent
   * identities nothing else has.
   */
  class ConnectionEdgeItem : public QGraphicsItem
  {
    public:
      enum
      {
        Type = UserType + 3
      };

      /*!
       * \brief Builds an edge for a connection.
       * \param connection The connection this edge represents.
       * \param from Producer port.
       * \param to Consumer port.
       */
      ConnectionEdgeItem(HydroCouple::SDK::IO::ConnectionSpec connection,
                         PortItem *from, PortItem *to);

      [[nodiscard]] int type() const override;

      [[nodiscard]] QRectF boundingRect() const override;

      [[nodiscard]] QPainterPath shape() const override;

      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

      [[nodiscard]] const HydroCouple::SDK::IO::ConnectionSpec &connection() const;

      /*!
       * \brief The spliced adapter nodes to route through, in chain order.
       */
      void setAdapters(const QList<AdapterNodeItem *> &adapters);

      /*!
       * \brief Recomputes the edge geometry from its ports and adapters.
       */
      void refresh();

    private:
      [[nodiscard]] QPainterPath buildPath() const;

      HydroCouple::SDK::IO::ConnectionSpec m_connection;
      PortItem *m_from = nullptr;
      PortItem *m_to = nullptr;
      QList<AdapterNodeItem *> m_adapters;
      QPainterPath m_path;
  };

  /*!
   * \brief One adaptation-chain step, drawn as its own connector node
   *        spliced into its connection's edge.
   *
   * Addressed as (connection identity, chain index) — the same coordinates
   * the document's chain edits use — never by item pointer: a rebuild
   * invalidates every item, and whatever holds one of these must re-find it
   * by identity afterwards.
   */
  class AdapterNodeItem : public QGraphicsObject
  {
      Q_OBJECT

    public:
      enum
      {
        Type = UserType + 5
      };

      AdapterNodeItem(HydroCouple::SDK::IO::ConnectionSpec connection,
                      int stepIndex,
                      HydroCouple::SDK::IO::AdaptedOutputSpec step);

      [[nodiscard]] int type() const override;

      [[nodiscard]] QRectF boundingRect() const override;

      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

      [[nodiscard]] const HydroCouple::SDK::IO::ConnectionSpec &connection() const;

      [[nodiscard]] int stepIndex() const;

      [[nodiscard]] const HydroCouple::SDK::IO::AdaptedOutputSpec &step() const;

      //! Where the incoming leg attaches (left mid, scene coordinates).
      [[nodiscard]] QPointF anchorIn() const;

      //! Where the outgoing leg leaves (right mid, scene coordinates).
      [[nodiscard]] QPointF anchorOut() const;

    Q_SIGNALS:
      /*!
       * \brief Emitted when the user finishes moving this node — on release,
       *        so a drag is one undo entry (and nothing modal ever opens
       *        from a press).
       */
      void moved(const HydroCouple::SDK::IO::ConnectionSpec &connection,
                 int stepIndex, const QPointF &position);

    protected:
      QVariant itemChange(GraphicsItemChange change,
                          const QVariant &value) override;

      void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
      HydroCouple::SDK::IO::ConnectionSpec m_connection;
      int m_stepIndex = 0;
      HydroCouple::SDK::IO::AdaptedOutputSpec m_step;
      QSizeF m_size;
      bool m_moving = false;
  };


  /*!
   * \brief An @from argument binding, drawn as its own kind of edge.
   *
   * Node to node, not port to port: a binding initializes an argument, and
   * arguments have no ports — the value flows once, before the consumer
   * initializes, not every step. The dashed stroke says exactly that at a
   * glance, next to the solid exchange edges.
   */
  class BindingEdgeItem : public QGraphicsItem
  {
    public:
      enum
      {
        Type = UserType + 4
      };

      /*!
       * \brief Builds an edge for one binding.
       * \param binding The binding this edge represents.
       * \param provider The providing component's node.
       * \param consumer The consuming component's node.
       */
      BindingEdgeItem(HydroCouple::SDK::IO::ArgumentBindingSpec binding,
                      ComponentNodeItem *provider,
                      ComponentNodeItem *consumer);

      [[nodiscard]] int type() const override;

      //! The binding this edge draws.
      [[nodiscard]] const HydroCouple::SDK::IO::ArgumentBindingSpec &
      binding() const;

      //! Recomputes the path after a node moved.
      void refresh();

      [[nodiscard]] QRectF boundingRect() const override;

      [[nodiscard]] QPainterPath shape() const override;

      void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

    private:
      [[nodiscard]] QPainterPath buildPath() const;

      HydroCouple::SDK::IO::ArgumentBindingSpec m_binding;
      ComponentNodeItem *m_provider = nullptr;
      ComponentNodeItem *m_consumer = nullptr;
      QPainterPath m_path;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CANVAS_CANVASITEMS_H
