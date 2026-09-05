#include "canvas/compositionscene.h"

#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QPen>

namespace HydroCouple::Composer
{

  using ComponentSpec = HydroCouple::SDK::IO::ComponentSpec;
  using ConnectionSpec = HydroCouple::SDK::IO::ConnectionSpec;

  CompositionScene::CompositionScene(CompositionDocument *document,
                                     ComponentInstances *instances,
                                     QObject *parent)
    : QGraphicsScene(parent),
      m_document(document),
      m_instances(instances)
  {
    if (m_document)
    {
      connect(m_document, &CompositionDocument::componentsChanged, this,
              &CompositionScene::rebuild);
      connect(m_document, &CompositionDocument::connectionsChanged, this,
              &CompositionScene::rebuild);
      connect(m_document, &CompositionDocument::placementChanged, this,
              &CompositionScene::applyPlacement);
    }

    if (m_instances)
    {
      // A component whose library only became available later must gain its
      // ports without the user reopening the document.
      connect(m_instances, &ComponentInstances::instanceChanged, this,
              [this](const QString &) { rebuild(); });
    }

    rebuild();
  }

  CompositionScene::~CompositionScene() = default;

  void CompositionScene::rebuild()
  {
    if (!m_document || m_rebuilding)
    {
      return;
    }

    m_rebuilding = true;

    clear();
    m_nodes.clear();
    m_edges.clear();
    m_bindingEdges.clear();
    m_dragSource = nullptr;
    m_dragTarget = nullptr;
    m_dragPreview = nullptr;

    for (const QString &componentId : m_document->componentIds())
    {
      const std::optional<ComponentSpec> spec = m_document->component(componentId);

      QList<ExchangeItemDescriptor> inputs;
      QList<ExchangeItemDescriptor> outputs;

      if (m_instances)
      {
        inputs = m_instances->inputs(componentId);
        outputs = m_instances->outputs(componentId);
      }

      const QString caption =
        spec && !spec->caption.empty()
          ? QString::fromStdString(spec->caption)
          : componentId;

      auto *node = new ComponentNodeItem(componentId, caption, inputs, outputs);

      if (m_instances && !m_instances->failure(componentId).isEmpty())
      {
        node->setUnavailable(m_instances->failure(componentId));
      }

      connect(node, &ComponentNodeItem::moved, this,
              [this](const QString &id, const QPointF &position)
              {
                // The canvas never writes placement directly; it asks the
                // document, so the move joins the undo history.
                m_document->moveComponent(id, position);
              });

      addItem(node);
      node->setPos(m_document->presentation().component(componentId).position);
      m_nodes.insert(componentId, node);
    }

    for (const ConnectionSpec &connection : m_document->spec().connections)
    {
      ComponentNodeItem *from =
        m_nodes.value(QString::fromStdString(connection.fromComponent));
      ComponentNodeItem *to =
        m_nodes.value(QString::fromStdString(connection.toComponent));

      if (!from || !to)
      {
        continue;
      }

      PortItem *fromPort = from->port(QString::fromStdString(connection.output),
                                      PortItem::Direction::Output);
      PortItem *toPort = to->port(QString::fromStdString(connection.input),
                                  PortItem::Direction::Input);

      // A connection whose ports are unknown — because the component could not
      // be instantiated — is kept in the document but simply not drawn.
      if (!fromPort || !toPort)
      {
        continue;
      }

      auto *edge = new ConnectionEdgeItem(connection, fromPort, toPort);
      addItem(edge);
      m_edges.append(edge);
    }

    // Binding edges, node to node: an argument has no port, and the value
    // flows once, before the consumer initializes.
    for (const HydroCouple::SDK::IO::ArgumentBindingSpec &binding :
         HydroCouple::SDK::IO::argumentBindings(m_document->spec()))
    {
      ComponentNodeItem *provider =
        m_nodes.value(QString::fromStdString(binding.provider));
      ComponentNodeItem *consumer =
        m_nodes.value(QString::fromStdString(binding.component));

      if (!provider || !consumer)
      {
        continue;
      }

      auto *edge = new BindingEdgeItem(binding, provider, consumer);
      addItem(edge);
      m_bindingEdges.append(edge);
    }

    m_rebuilding = false;
  }

  void CompositionScene::applyPlacement(const QString &componentId)
  {
    ComponentNodeItem *node = m_nodes.value(componentId);

    if (!node)
    {
      return;
    }

    const QPointF position =
      m_document->presentation().component(componentId).position;

    if (node->pos() != position)
    {
      node->setPos(position);
    }

    refreshEdges();
  }

  void CompositionScene::refreshEdges()
  {
    for (ConnectionEdgeItem *edge : m_edges)
    {
      edge->refresh();
    }

    for (BindingEdgeItem *edge : m_bindingEdges)
    {
      edge->refresh();
    }
  }

  ComponentNodeItem *CompositionScene::node(const QString &componentId) const
  {
    return m_nodes.value(componentId);
  }

  QList<ConnectionEdgeItem *> CompositionScene::edges() const
  {
    return m_edges;
  }

  QList<BindingEdgeItem *> CompositionScene::bindingEdges() const
  {
    return m_bindingEdges;
  }

  QString CompositionScene::uniqueComponentId(const QString &desired) const
  {
    QString base = desired.isEmpty() ? QStringLiteral("component") : desired;
    QString candidate = base;
    int suffix = 2;

    const QStringList taken = m_document->componentIds();

    while (taken.contains(candidate))
    {
      candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++);
    }

    return candidate;
  }

  QString CompositionScene::addComponentAt(const QString &componentInfoId,
                                           const QString &instanceId,
                                           const QPointF &position)
  {
    if (!m_document)
    {
      return QString();
    }

    // A KNOWN non-model library is refused here rather than left to die as
    // a red "unavailable" box; an id the registry has never seen still goes
    // in — a document may legitimately name components that are not
    // installed on this machine.
    if (m_instances && m_instances->registry())
    {
      HydroCouple::IComponentInfo *info =
        m_instances->registry()->entry(componentInfoId);

      const ComponentRegistry::ComponentKind kind =
        info ? ComponentRegistry::kindOf(info)
             : ComponentRegistry::ComponentKind::Model;

      if (kind != ComponentRegistry::ComponentKind::Model)
      {
        Q_EMIT componentRefused(
          kind == ComponentRegistry::ComponentKind::AdapterFactory
            ? QStringLiteral("'%1' is an adapted-output factory; adapters "
                             "attach to connections, they are not placed "
                             "as components")
                .arg(componentInfoId)
            : QStringLiteral("'%1' is not a model component")
                .arg(componentInfoId));
        return QString();
      }
    }

    ComponentSpec spec;
    spec.id = uniqueComponentId(instanceId.isEmpty() ? componentInfoId
                                                     : instanceId)
                .toStdString();
    spec.info.componentInfoId = componentInfoId.toStdString();

    if (!m_document->addComponent(spec, {position}))
    {
      return QString();
    }

    return QString::fromStdString(spec.id);
  }

  bool CompositionScene::connectPorts(const QString &fromComponent,
                                      const QString &output,
                                      const QString &toComponent,
                                      const QString &input)
  {
    if (!m_document)
    {
      return false;
    }

    ConnectionSpec connection;
    connection.fromComponent = fromComponent.toStdString();
    connection.output = output.toStdString();
    connection.toComponent = toComponent.toStdString();
    connection.input = input.toStdString();

    return m_document->addConnection(connection);
  }

  int CompositionScene::removeSelection()
  {
    if (!m_document)
    {
      return 0;
    }

    // Collect first: removing mutates the document, which rebuilds the scene
    // and invalidates every item pointer under our feet.
    QStringList components;
    QList<ConnectionSpec> connections;

    for (QGraphicsItem *item : selectedItems())
    {
      if (auto *node = qgraphicsitem_cast<ComponentNodeItem *>(item))
      {
        components.append(node->componentId());
      }
      else if (item->type() == ConnectionEdgeItem::Type)
      {
        connections.append(static_cast<ConnectionEdgeItem *>(item)->connection());
      }
    }

    int removed = 0;

    // Connections first: removing a component already takes its connections,
    // and doing it the other way round would count them twice.
    for (const ConnectionSpec &connection : connections)
    {
      if (m_document->removeConnection(connection))
      {
        ++removed;
      }
    }

    for (const QString &componentId : components)
    {
      if (m_document->removeComponent(componentId))
      {
        ++removed;
      }
    }

    return removed;
  }

  PortItem *CompositionScene::portAt(const QPointF &scenePosition) const
  {
    const QList<QGraphicsItem *> hits = items(scenePosition);

    for (QGraphicsItem *item : hits)
    {
      if (item->type() == PortItem::Type)
      {
        return static_cast<PortItem *>(item);
      }
    }

    return nullptr;
  }

  void CompositionScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
  {
    if (event->button() == Qt::LeftButton)
    {
      if (PortItem *port = portAt(event->scenePos()))
      {
        // Connections are drawn from an output towards an input.
        if (port->direction() == PortItem::Direction::Output)
        {
          m_dragSource = port;

          m_dragPreview = new QGraphicsPathItem;
          m_dragPreview->setPen(QPen(QColor(90, 150, 220), 1.5, Qt::DashLine));
          m_dragPreview->setZValue(1.0);
          addItem(m_dragPreview);

          event->accept();
          return;
        }
      }
    }

    QGraphicsScene::mousePressEvent(event);
  }

  void CompositionScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
  {
    if (m_dragSource && m_dragPreview)
    {
      QPainterPath path;
      path.moveTo(m_dragSource->anchor());
      path.lineTo(event->scenePos());
      m_dragPreview->setPath(path);

      PortItem *candidate = portAt(event->scenePos());

      if (candidate && candidate->direction() != PortItem::Direction::Input)
      {
        candidate = nullptr;
      }

      if (candidate != m_dragTarget)
      {
        if (m_dragTarget)
        {
          m_dragTarget->setHighlighted(false);
        }

        m_dragTarget = candidate;

        if (m_dragTarget)
        {
          m_dragTarget->setHighlighted(true);
        }
      }

      event->accept();
      return;
    }

    QGraphicsScene::mouseMoveEvent(event);

    if (!selectedItems().isEmpty())
    {
      refreshEdges();
    }
  }

  void CompositionScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
  {
    if (m_dragSource)
    {
      PortItem *target = portAt(event->scenePos());

      if (m_dragTarget)
      {
        m_dragTarget->setHighlighted(false);
      }

      if (m_dragPreview)
      {
        removeItem(m_dragPreview);
        delete m_dragPreview;
        m_dragPreview = nullptr;
      }

      if (target && target->direction() == PortItem::Direction::Input &&
          target->node() != m_dragSource->node())
      {
        connectPorts(m_dragSource->node()->componentId(),
                     m_dragSource->itemId(), target->node()->componentId(),
                     target->itemId());
      }

      m_dragSource = nullptr;
      m_dragTarget = nullptr;

      event->accept();
      return;
    }

    QGraphicsScene::mouseReleaseEvent(event);
    refreshEdges();
  }

} // namespace HydroCouple::Composer
