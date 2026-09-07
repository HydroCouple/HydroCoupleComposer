#include "canvas/compositionscene.h"

#include "canvas/graphlayout.h"

#include <QGraphicsPathItem>
#include <QUndoStack>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QMenu>
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
      connect(m_document, &CompositionDocument::adapterPlacementChanged, this,
              &CompositionScene::applyAdapterPlacements);
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

    // Selection survives the teardown by IDENTITY, never by item pointer:
    // every item below is about to be destroyed, and in-place edits (a
    // chain argument changed in the inspector) rebuild mid-interaction.
    QStringList selectedComponents;
    QList<ConnectionSpec> selectedConnections;
    QList<QPair<ConnectionSpec, int>> selectedAdapters;

    for (QGraphicsItem *item : selectedItems())
    {
      if (auto *node = qgraphicsitem_cast<ComponentNodeItem *>(item))
      {
        selectedComponents.append(node->componentId());
      }
      else if (auto *edge = qgraphicsitem_cast<ConnectionEdgeItem *>(item))
      {
        selectedConnections.append(edge->connection());
      }
      else if (auto *adapter = qgraphicsitem_cast<AdapterNodeItem *>(item))
      {
        selectedAdapters.append({adapter->connection(),
                                 adapter->stepIndex()});
      }
    }

    clear();
    m_nodes.clear();
    m_edges.clear();
    m_bindingEdges.clear();
    m_adapterNodes.clear();
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

      // One spliced node per chain step. Saved positions come from the
      // sidecar; unplaced steps sit at even fractions along the straight
      // port-to-port line — a missing or short list is never an error.
      const QList<QPointF> saved =
        m_document->presentation().adapterChain(connection);
      const int stepCount = static_cast<int>(connection.adaptedOutputs.size());

      QList<AdapterNodeItem *> adapters;

      for (int index = 0; index < stepCount; ++index)
      {
        auto *adapter = new AdapterNodeItem(
          connection, index, connection.adaptedOutputs[
                               static_cast<size_t>(index)]);

        connect(adapter, &AdapterNodeItem::moved, this,
                [this](const ConnectionSpec &identity, int stepIndex,
                       const QPointF &position)
                {
                  // Through the document, so the move joins undo history.
                  m_document->moveConnectionAdapter(identity, stepIndex,
                                                    position);
                });

        addItem(adapter);

        if (index < saved.size() && saved[index] != QPointF())
        {
          adapter->setPos(saved[index]);
        }
        else
        {
          const QPointF start = fromPort->anchor();
          const QPointF end = toPort->anchor();
          const qreal fraction =
            static_cast<qreal>(index + 1) / static_cast<qreal>(stepCount + 1);
          adapter->setPos(start + (end - start) * fraction);
        }

        adapters.append(adapter);
        m_adapterNodes.append(adapter);
      }

      edge->setAdapters(adapters);
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

    // Re-select what was selected, by identity.
    for (const QString &componentId : selectedComponents)
    {
      if (ComponentNodeItem *node = m_nodes.value(componentId))
      {
        node->setSelected(true);
      }
    }

    const auto sameIdentity = [](const ConnectionSpec &lhs,
                                 const ConnectionSpec &rhs)
    {
      return lhs.fromComponent == rhs.fromComponent &&
             lhs.output == rhs.output &&
             lhs.toComponent == rhs.toComponent && lhs.input == rhs.input &&
             lhs.role == rhs.role;
    };

    for (const ConnectionSpec &identity : selectedConnections)
    {
      for (ConnectionEdgeItem *edge : m_edges)
      {
        if (sameIdentity(edge->connection(), identity))
        {
          edge->setSelected(true);
        }
      }
    }

    for (const QPair<ConnectionSpec, int> &address : selectedAdapters)
    {
      for (AdapterNodeItem *adapter : m_adapterNodes)
      {
        if (adapter->stepIndex() == address.second &&
            sameIdentity(adapter->connection(), address.first))
        {
          adapter->setSelected(true);
        }
      }
    }

    m_rebuilding = false;
  }

  void CompositionScene::applyAdapterPlacements()
  {
    for (AdapterNodeItem *adapter : m_adapterNodes)
    {
      const QList<QPointF> saved =
        m_document->presentation().adapterChain(adapter->connection());

      if (adapter->stepIndex() < saved.size())
      {
        adapter->setPos(saved[adapter->stepIndex()]);
      }
    }

    refreshEdges();
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

  QList<AdapterNodeItem *> CompositionScene::adapterNodes() const
  {
    return m_adapterNodes;
  }

  int CompositionScene::applyAutoLayout()
  {
    if (!m_document)
    {
      return 0;
    }

    // Measured from the items rather than assumed: a component's box is as
    // wide as the port names it has to show, so the columns can only be
    // spaced correctly by asking what is actually drawn.
    QHash<QString, QSizeF> sizes;

    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it)
    {
      if (it.value())
      {
        sizes.insert(it.key(), it.value()->boundingRect().size());
      }
    }

    const QHash<QString, QPointF> positions =
      layoutComposition(m_document->spec(), sizes);

    if (positions.isEmpty())
    {
      return 0;
    }

    int moved = 0;

    m_document->undoStack()->beginMacro(tr("Lay out components"));

    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
    {
      if (m_document->moveComponent(it.key(), it.value()))
      {
        ++moved;
      }
    }

    // A spliced adapter keeps whatever position it was dragged to, which
    // after a re-layout is nowhere near the connection it belongs to. The
    // null point is what rebuild() reads as "unplaced" (see the sidecar
    // branch above), so writing it back is how a step is returned to
    // sitting along its edge — undoably, inside this same macro.
    for (const HydroCouple::SDK::IO::ConnectionSpec &connection :
         m_document->spec().connections)
    {
      for (int index = 0;
           index < static_cast<int>(connection.adaptedOutputs.size()); ++index)
      {
        m_document->moveConnectionAdapter(connection, index, QPointF());
      }
    }

    m_document->undoStack()->endMacro();

    return moved;
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
    QList<QPair<ConnectionSpec, int>> adapters;
    QList<QPair<QString, QString>> bindings;

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
      else if (auto *adapter = qgraphicsitem_cast<AdapterNodeItem *>(item))
      {
        adapters.append({adapter->connection(), adapter->stepIndex()});
      }
      else if (auto *binding = qgraphicsitem_cast<BindingEdgeItem *>(item))
      {
        bindings.append(
          {QString::fromStdString(binding->binding().component),
           QString::fromStdString(binding->binding().argument)});
      }
    }

    const auto sameIdentity = [](const ConnectionSpec &lhs,
                                 const ConnectionSpec &rhs)
    {
      return lhs.fromComponent == rhs.fromComponent &&
             lhs.output == rhs.output &&
             lhs.toComponent == rhs.toComponent && lhs.input == rhs.input &&
             lhs.role == rhs.role;
    };

    int removed = 0;

    // Adapter steps first, highest index first, so earlier removals never
    // renumber a later one out from under us; steps whose whole connection
    // is also selected are skipped — the connection removal takes its
    // chain, and removing them here too would count them twice.
    std::sort(adapters.begin(), adapters.end(),
              [](const QPair<ConnectionSpec, int> &lhs,
                 const QPair<ConnectionSpec, int> &rhs)
              { return lhs.second > rhs.second; });

    for (const QPair<ConnectionSpec, int> &address : adapters)
    {
      const bool parentSelected =
        std::any_of(connections.begin(), connections.end(),
                    [&address, &sameIdentity](const ConnectionSpec &link)
                    { return sameIdentity(link, address.first); });

      if (!parentSelected &&
          m_document->removeConnectionAdapter(address.first, address.second))
      {
        ++removed;
      }
    }

    // Connections before components: removing a component already takes its
    // connections, and doing it the other way round would count them twice.
    for (const ConnectionSpec &connection : connections)
    {
      if (m_document->removeConnection(connection))
      {
        ++removed;
      }
    }

    for (const QPair<QString, QString> &binding : bindings)
    {
      if (m_document->clearArgument(binding.first, binding.second))
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

  PortItem *CompositionScene::portNear(const QPointF &scenePosition,
                                       qreal radius) const
  {
    if (PortItem *exact = portAt(scenePosition))
    {
      return exact;
    }

    PortItem *best = nullptr;
    qreal bestDistance = radius;

    const QList<QGraphicsItem *> hits =
      items(QRectF(scenePosition - QPointF(radius, radius),
                   QSizeF(radius * 2.0, radius * 2.0)));

    for (QGraphicsItem *item : hits)
    {
      if (item->type() != PortItem::Type)
      {
        continue;
      }

      auto *port = static_cast<PortItem *>(item);
      const qreal distance =
        QLineF(scenePosition, port->anchor()).length();

      if (distance <= bestDistance)
      {
        best = port;
        bestDistance = distance;
      }
    }

    return best;
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

      PortItem *candidate = portNear(event->scenePos(), 12.0);

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
      PortItem *target = portNear(event->scenePos(), 12.0);

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

  // ── Adapter insertion and context menus (C4) ─────────────────────────────

  HydroCouple::IOutput *CompositionScene::liveOutput(
    const ConnectionSpec &connection) const
  {
    if (!m_instances)
    {
      return nullptr;
    }

    HydroCouple::IModelComponent *component = m_instances->instance(
      QString::fromStdString(connection.fromComponent));

    if (!component)
    {
      return nullptr;
    }

    for (HydroCouple::IOutput *output : component->outputs())
    {
      if (output && output->id() == connection.output)
      {
        return output;
      }
    }

    return nullptr;
  }

  HydroCouple::IInput *CompositionScene::liveInput(
    const ConnectionSpec &connection) const
  {
    if (!m_instances)
    {
      return nullptr;
    }

    HydroCouple::IModelComponent *component = m_instances->instance(
      QString::fromStdString(connection.toComponent));

    if (!component)
    {
      return nullptr;
    }

    for (HydroCouple::IInput *input : component->inputs())
    {
      if (input && input->id() == connection.input)
      {
        return input;
      }
    }

    return nullptr;
  }

  QList<CompositionScene::AdapterOffering> CompositionScene::adapterOfferings(
    const ConnectionSpec &connection)
  {
    QList<AdapterOffering> offerings;

    HydroCouple::IOutput *output = liveOutput(connection);

    if (!output)
    {
      return offerings;
    }

    HydroCouple::IInput *input = liveInput(connection);

    // Component-info factories first — a component may deliberately shadow
    // a standalone factory id, and the run pipeline searches in this order.
    std::vector<HydroCouple::IAdaptedOutputFactory *> factories;

    if (output->modelComponent() && output->modelComponent()->componentInfo())
    {
      for (HydroCouple::IAdaptedOutputFactory *factory :
           output->modelComponent()->componentInfo()->adaptedOutputFactories())
      {
        if (factory)
        {
          factories.push_back(factory);
        }
      }
    }

    if (m_instances)
    {
      for (HydroCouple::IAdaptedOutputFactory *factory :
           m_instances->adapterFactories())
      {
        if (factory)
        {
          factories.push_back(factory);
        }
      }
    }

    for (HydroCouple::IAdaptedOutputFactory *factory : factories)
    {
      for (HydroCouple::IIdentity *offering :
           factory->getAvailableAdaptedOutputIds(output, input))
      {
        if (!offering)
        {
          continue;
        }

        AdapterOffering entry;
        entry.factoryId = QString::fromStdString(factory->id());
        entry.adapterId = QString::fromStdString(offering->id());
        entry.caption = QString::fromStdString(offering->caption());
        offerings.append(entry);
      }
    }

    return offerings;
  }

  bool CompositionScene::insertAdapter(const ConnectionSpec &connection,
                                       int index, const QString &factoryId,
                                       const QString &adapterId)
  {
    if (!m_document)
    {
      return false;
    }

    HydroCouple::IOutput *output = liveOutput(connection);
    HydroCouple::IInput *input = liveInput(connection);

    HydroCouple::SDK::IO::AdaptedOutputSpec step;
    step.factory = factoryId.toStdString();
    step.id = adapterId.toStdString();

    // A scratch instance captures the argument keys and their defaults, so
    // the document carries editable payloads from the start. Transient by
    // contract: unregistered from the provider (its registry is non-owning)
    // and destroyed before the edit.
    if (output)
    {
      std::vector<HydroCouple::IAdaptedOutputFactory *> factories;

      if (output->modelComponent() &&
          output->modelComponent()->componentInfo())
      {
        for (HydroCouple::IAdaptedOutputFactory *factory :
             output->modelComponent()->componentInfo()
               ->adaptedOutputFactories())
        {
          factories.push_back(factory);
        }
      }
      if (m_instances)
      {
        for (HydroCouple::IAdaptedOutputFactory *factory :
             m_instances->adapterFactories())
        {
          factories.push_back(factory);
        }
      }

      for (HydroCouple::IAdaptedOutputFactory *factory : factories)
      {
        if (!factory || factory->id() != step.factory)
        {
          continue;
        }

        for (HydroCouple::IIdentity *offering :
             factory->getAvailableAdaptedOutputIds(output, input))
        {
          if (!offering || offering->id() != step.id)
          {
            continue;
          }

          std::unique_ptr<HydroCouple::IAdaptedOutput> scratch =
            factory->createAdaptedOutput(offering, output, input);

          if (scratch)
          {
            for (HydroCouple::IArgument *argument : scratch->arguments())
            {
              if (!argument)
              {
                continue;
              }

              std::string value;
              std::string message;
              if (argument->serialize(
                    HydroCouple::IArgument::ArgumentInputType::JSON, value,
                    message))
              {
                try
                {
                  step.arguments[argument->id()] =
                    nlohmann::json::parse(value);
                }
                catch (const nlohmann::json::exception &)
                {
                  // An argument that will not serialise cleanly simply
                  // keeps its default when the run builds the real chain.
                }
              }
            }

            output->removeAdaptedOutput(scratch.get());
          }

          break;
        }

        break;
      }
    }

    return m_document->insertConnectionAdapter(connection, index, step);
  }

  void CompositionScene::insertAdapterInteractively(
    const ConnectionSpec &connection, int index)
  {
    const QList<AdapterOffering> offerings = adapterOfferings(connection);

    if (offerings.isEmpty())
    {
      Q_EMIT componentRefused(
        tr("no adapter is available for output '%1' of '%2'%3")
          .arg(QString::fromStdString(connection.output),
               QString::fromStdString(connection.fromComponent),
               m_instances &&
                   !m_instances
                      ->failure(QString::fromStdString(
                        connection.fromComponent))
                      .isEmpty()
                 ? tr(" — %1").arg(m_instances->failure(
                     QString::fromStdString(connection.fromComponent)))
                 : QString()));
      return;
    }

    QStringList labels;
    for (const AdapterOffering &offering : offerings)
    {
      labels.append(offering.caption.isEmpty()
                      ? QStringLiteral("%1 — %2").arg(offering.adapterId,
                                                      offering.factoryId)
                      : QStringLiteral("%1 — %2 (%3)")
                          .arg(offering.adapterId, offering.factoryId,
                               offering.caption));
    }

    bool accepted = false;
    const QString choice = QInputDialog::getItem(
      nullptr, tr("Insert adapter"), tr("Adapted output:"), labels, 0,
      false, &accepted);

    if (!accepted)
    {
      return;
    }

    const int chosen = labels.indexOf(choice);

    if (chosen >= 0)
    {
      insertAdapter(connection, index, offerings[chosen].factoryId,
                    offerings[chosen].adapterId);
    }
  }

  void CompositionScene::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *event)
  {
    if (!m_document)
    {
      QGraphicsScene::contextMenuEvent(event);
      return;
    }

    QGraphicsItem *hit = nullptr;

    for (QGraphicsItem *item : items(event->scenePos()))
    {
      const int type = item->type();

      if (type == AdapterNodeItem::Type || type == ComponentNodeItem::Type ||
          type == ConnectionEdgeItem::Type || type == BindingEdgeItem::Type)
      {
        hit = item;
        break;
      }
    }

    if (!hit)
    {
      QGraphicsScene::contextMenuEvent(event);
      return;
    }

    QMenu menu;

    if (auto *adapter = qgraphicsitem_cast<AdapterNodeItem *>(hit))
    {
      const ConnectionSpec identity = adapter->connection();
      const int index = adapter->stepIndex();

      menu.addAction(tr("Insert adapter before…"), this,
                     [this, identity, index]
                     { insertAdapterInteractively(identity, index); });
      menu.addAction(tr("Insert adapter after…"), this,
                     [this, identity, index]
                     { insertAdapterInteractively(identity, index + 1); });
      menu.addSeparator();
      menu.addAction(tr("Remove adapter"), this,
                     [this, identity, index]
                     { m_document->removeConnectionAdapter(identity, index); });
    }
    else if (auto *edge = qgraphicsitem_cast<ConnectionEdgeItem *>(hit))
    {
      const ConnectionSpec identity = edge->connection();

      menu.addAction(
        tr("Insert adapter…"), this,
        [this, identity]
        {
          insertAdapterInteractively(
            identity, static_cast<int>(identity.adaptedOutputs.size()));
        });

      // Roles only mean something to a multi-input, and only the live
      // input knows its labels.
      if (auto *multi =
            dynamic_cast<HydroCouple::IMultiInput *>(liveInput(identity)))
      {
        const std::vector<HydroCouple::IIdentity *> labels =
          multi->providerLabels();

        if (!labels.empty())
        {
          QMenu *roles = menu.addMenu(tr("Set provider role"));

          for (HydroCouple::IIdentity *label : labels)
          {
            if (!label)
            {
              continue;
            }

            const QString role = QString::fromStdString(label->id());
            QAction *action = roles->addAction(role);
            action->setCheckable(true);
            action->setChecked(identity.role == label->id());
            connect(action, &QAction::triggered, this,
                    [this, identity, role]
                    { m_document->setConnectionRole(identity, role); });
          }
        }
      }

      menu.addSeparator();
      menu.addAction(tr("Disconnect"), this,
                     [this, identity]
                     { m_document->removeConnection(identity); });
    }
    else if (auto *binding = qgraphicsitem_cast<BindingEdgeItem *>(hit))
    {
      const QString component =
        QString::fromStdString(binding->binding().component);
      const QString argument =
        QString::fromStdString(binding->binding().argument);

      menu.addAction(tr("Remove binding"), this,
                     [this, component, argument]
                     { m_document->clearArgument(component, argument); });
    }
    else if (auto *node = qgraphicsitem_cast<ComponentNodeItem *>(hit))
    {
      const QString componentId = node->componentId();

      menu.addAction(tr("Remove '%1'").arg(componentId), this,
                     [this, componentId]
                     { m_document->removeComponent(componentId); });
    }

    menu.exec(event->screenPos());
    event->accept();
  }

} // namespace HydroCouple::Composer
