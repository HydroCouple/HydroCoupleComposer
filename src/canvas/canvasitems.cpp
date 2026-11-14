#include "canvas/canvasitems.h"

#include <QFontMetricsF>

#include <algorithm>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

namespace HydroCouple::Composer
{

  namespace
  {
    constexpr qreal kPortRadius = 5.0;
    constexpr qreal kPortSpacing = 20.0;
    constexpr qreal kHeaderHeight = 26.0;
    constexpr qreal kMinimumWidth = 160.0;
    constexpr qreal kMaximumWidth = 260.0;
    constexpr qreal kHorizontalPadding = 16.0;
  } // namespace

  // ── PortItem ─────────────────────────────────────────────────────────────

  PortItem::PortItem(ComponentNodeItem *node, ExchangeItemDescriptor descriptor,
                     Direction direction)
    : QGraphicsItem(node),
      m_node(node),
      m_descriptor(std::move(descriptor)),
      m_direction(direction)
  {
    setAcceptHoverEvents(true);
    setToolTip(m_descriptor.caption);
    setData(0, m_descriptor.id);
  }

  int PortItem::type() const
  {
    return Type;
  }

  QRectF PortItem::boundingRect() const
  {
    return QRectF(-kPortRadius, -kPortRadius, kPortRadius * 2.0,
                  kPortRadius * 2.0);
  }

  void PortItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                       QWidget *)
  {
    const QColor fill = m_highlighted
                          ? QColor(90, 190, 120)
                          : (m_direction == Direction::Input
                               ? QColor(80, 130, 200)
                               : QColor(210, 145, 60));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(fill);
    painter->setPen(QPen(QColor(40, 40, 40), 1.0));

    if (m_descriptor.isMultiInput)
    {
      // A multi-input accepts several providers; drawn square so the
      // difference is visible without hovering.
      painter->drawRect(boundingRect().adjusted(0.5, 0.5, -0.5, -0.5));
    }
    else
    {
      painter->drawEllipse(boundingRect());
    }
  }

  QString PortItem::itemId() const
  {
    return m_descriptor.id;
  }

  PortItem::Direction PortItem::direction() const
  {
    return m_direction;
  }

  ComponentNodeItem *PortItem::node() const
  {
    return m_node;
  }

  QPointF PortItem::anchor() const
  {
    return mapToScene(QPointF(0.0, 0.0));
  }

  void PortItem::setHighlighted(bool highlighted)
  {
    if (m_highlighted == highlighted)
    {
      return;
    }

    m_highlighted = highlighted;
    update();
  }

  // ── ComponentNodeItem ────────────────────────────────────────────────────

  ComponentNodeItem::ComponentNodeItem(
    QString componentId, const QString &caption,
    const QList<ExchangeItemDescriptor> &inputs,
    const QList<ExchangeItemDescriptor> &outputs)
    : m_componentId(std::move(componentId)),
      m_caption(caption.isEmpty() ? m_componentId : caption)
  {
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemSendsGeometryChanges, true);
    setData(0, m_componentId);

    for (const ExchangeItemDescriptor &descriptor : inputs)
    {
      m_inputs.append(new PortItem(this, descriptor, PortItem::Direction::Input));
    }

    for (const ExchangeItemDescriptor &descriptor : outputs)
    {
      m_outputs.append(
        new PortItem(this, descriptor, PortItem::Direction::Output));
    }

    layoutPorts();
  }

  int ComponentNodeItem::type() const
  {
    return Type;
  }

  void ComponentNodeItem::layoutPorts()
  {
    const int rows = std::max(
      {static_cast<int>(m_inputs.size()), static_cast<int>(m_outputs.size()), 1});

    // Wide enough to read the port names on both sides at once. A port
    // whose name is clipped — "…_temperature" for "air_temperature" — is
    // the one thing a user has to be sure of before dragging a connection
    // to it, so the box grows to fit rather than eliding. Clamped, because
    // a component with one very long item id must not push every other box
    // off the canvas.
    const QFontMetricsF metrics((QFont()));

    qreal widest = 0.0;

    for (int row = 0; row < rows; ++row)
    {
      const qreal left =
        row < m_inputs.size()
          ? metrics.horizontalAdvance(m_inputs[row]->itemId())
          : 0.0;
      const qreal right =
        row < m_outputs.size()
          ? metrics.horizontalAdvance(m_outputs[row]->itemId())
          : 0.0;

      // Both labels share one row, so it is the pair that has to fit.
      widest = std::max(widest, left + right);
    }

    widest = std::max(widest, metrics.horizontalAdvance(m_caption));

    m_size = QSizeF(std::clamp(widest + kHorizontalPadding * 2.0,
                               kMinimumWidth, kMaximumWidth),
                    kHeaderHeight + (rows * kPortSpacing) + kPortSpacing * 0.5);

    for (int index = 0; index < m_inputs.size(); ++index)
    {
      m_inputs[index]->setPos(
        0.0, kHeaderHeight + kPortSpacing * (index + 0.5));
    }

    for (int index = 0; index < m_outputs.size(); ++index)
    {
      m_outputs[index]->setPos(
        m_size.width(), kHeaderHeight + kPortSpacing * (index + 0.5));
    }
  }

  QRectF ComponentNodeItem::boundingRect() const
  {
    return QRectF(-kPortRadius, 0.0, m_size.width() + kPortRadius * 2.0,
                  m_size.height());
  }

  void ComponentNodeItem::paint(QPainter *painter,
                                const QStyleOptionGraphicsItem *option,
                                QWidget *)
  {
    const QRectF body(0.0, 0.0, m_size.width(), m_size.height());
    const bool selected = option->state & QStyle::State_Selected;

    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setBrush(m_unavailableReason.isEmpty() ? QColor(250, 250, 252)
                                                    : QColor(252, 244, 244));
    painter->setPen(QPen(selected ? QColor(60, 120, 220) : QColor(120, 120, 130),
                         selected ? 2.0 : 1.0));
    painter->drawRoundedRect(body, 6.0, 6.0);

    QRectF header(0.0, 0.0, m_size.width(), kHeaderHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_unavailableReason.isEmpty() ? QColor(232, 238, 248)
                                                    : QColor(248, 226, 226));
    painter->drawRoundedRect(header, 6.0, 6.0);
    painter->drawRect(QRectF(0.0, kHeaderHeight - 6.0, m_size.width(), 6.0));

    painter->setPen(QColor(30, 30, 40));

    const QFontMetricsF metrics(painter->font());
    const QString elided = metrics.elidedText(
      m_caption, Qt::ElideRight, m_size.width() - kHorizontalPadding);

    painter->drawText(header.adjusted(kHorizontalPadding * 0.5, 0.0,
                                      -kHorizontalPadding * 0.5, 0.0),
                      Qt::AlignVCenter | Qt::AlignLeft, elided);

    // Port labels, drawn inside the body on their respective sides.
    painter->setPen(QColor(70, 70, 80));

    // Drawn a row at a time, because the two labels share the row and the
    // box was sized for the PAIR: splitting it down the middle instead
    // would elide a lone long name inside a box already wide enough for
    // it. Each side takes what it needs; when the pair does not fit — the
    // width is clamped — they give way in proportion, elided rather than
    // clipped, because "air_temper…" says a name was shortened where a
    // hard cut just looks like a different port.
    const qreal available = m_size.width() - kHorizontalPadding;
    const int rows =
      std::max(static_cast<int>(m_inputs.size()),
               static_cast<int>(m_outputs.size()));

    for (int row = 0; row < rows; ++row)
    {
      PortItem *input = row < m_inputs.size() ? m_inputs[row] : nullptr;
      PortItem *output = row < m_outputs.size() ? m_outputs[row] : nullptr;

      const QString inText = input ? input->itemId() : QString();
      const QString outText = output ? output->itemId() : QString();

      qreal inWidth = metrics.horizontalAdvance(inText);
      qreal outWidth = metrics.horizontalAdvance(outText);

      if (inWidth + outWidth > available && inWidth + outWidth > 0.0)
      {
        const qreal share = available / (inWidth + outWidth);
        inWidth *= share;
        outWidth *= share;
      }

      const qreal y = (input ? input->pos().y() : output->pos().y())
                      - kPortSpacing * 0.5;

      if (input)
      {
        painter->drawText(
          QRectF(kHorizontalPadding * 0.5, y, inWidth, kPortSpacing),
          Qt::AlignVCenter | Qt::AlignLeft,
          metrics.elidedText(inText, Qt::ElideRight, inWidth));
      }

      if (output)
      {
        painter->drawText(
          QRectF(m_size.width() - kHorizontalPadding * 0.5 - outWidth, y,
                 outWidth, kPortSpacing),
          Qt::AlignVCenter | Qt::AlignRight,
          metrics.elidedText(outText, Qt::ElideRight, outWidth));
      }
    }
  }

  QString ComponentNodeItem::componentId() const
  {
    return m_componentId;
  }

  PortItem *ComponentNodeItem::port(const QString &itemId,
                                    PortItem::Direction direction) const
  {
    const QList<PortItem *> &ports =
      direction == PortItem::Direction::Input ? m_inputs : m_outputs;

    for (PortItem *port : ports)
    {
      if (port->itemId() == itemId)
      {
        return port;
      }
    }

    return nullptr;
  }

  void ComponentNodeItem::setUnavailable(const QString &reason)
  {
    m_unavailableReason = reason;
    setToolTip(reason);
    update();
  }

  QVariant ComponentNodeItem::itemChange(GraphicsItemChange change,
                                         const QVariant &value)
  {
    if (change == ItemPositionHasChanged)
    {
      m_moving = true;
    }

    return QGraphicsObject::itemChange(change, value);
  }

  void ComponentNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
  {
    QGraphicsObject::mouseReleaseEvent(event);

    // The move is committed on release, so a drag becomes one undo entry
    // rather than one per mouse-move. Acting on release rather than press
    // also matches the rule that anything modal is opened from a release —
    // opening from a press wedges input handling on macOS.
    if (m_moving)
    {
      m_moving = false;
      Q_EMIT moved(m_componentId, pos());
    }
  }

  // ── ConnectionEdgeItem ───────────────────────────────────────────────────

  ConnectionEdgeItem::ConnectionEdgeItem(
    HydroCouple::SDK::IO::ConnectionSpec connection, PortItem *from,
    PortItem *to)
    : m_connection(std::move(connection)),
      m_from(from),
      m_to(to)
  {
    setFlag(ItemIsSelectable, true);
    setZValue(-1.0);
    refresh();
  }

  int ConnectionEdgeItem::type() const
  {
    return Type;
  }

  void ConnectionEdgeItem::setAdapters(const QList<AdapterNodeItem *> &adapters)
  {
    m_adapters = adapters;
    refresh();
  }

  QPainterPath ConnectionEdgeItem::buildPath() const
  {
    QPainterPath path;

    if (!m_from || !m_to)
    {
      return path;
    }

    // The waypoint list pairs off into legs: port → first adapter's inlet,
    // each adapter's outlet → the next one's inlet, last outlet → the input
    // port. The gap across each adapter body is deliberate — the node draws
    // itself there.
    QList<QPointF> points;
    points.append(m_from->anchor());

    for (AdapterNodeItem *adapter : m_adapters)
    {
      if (!adapter)
      {
        continue;
      }

      points.append(adapter->anchorIn());
      points.append(adapter->anchorOut());
    }

    points.append(m_to->anchor());

    for (int i = 0; i + 1 < points.size(); i += 2)
    {
      const QPointF start = points[i];
      const QPointF end = points[i + 1];

      // A horizontal-tangent cubic keeps legs readable when boxes stack.
      const qreal reach =
        std::max(40.0, std::abs(end.x() - start.x()) * 0.5);

      path.moveTo(start);
      path.cubicTo(start + QPointF(reach, 0.0), end - QPointF(reach, 0.0),
                   end);
    }

    return path;
  }

  void ConnectionEdgeItem::refresh()
  {
    prepareGeometryChange();
    m_path = buildPath();
    update();
  }

  QRectF ConnectionEdgeItem::boundingRect() const
  {
    return m_path.boundingRect().adjusted(-6.0, -6.0, 6.0, 6.0);
  }

  QPainterPath ConnectionEdgeItem::shape() const
  {
    QPainterPathStroker stroker;
    stroker.setWidth(8.0);
    return stroker.createStroke(m_path);
  }

  void ConnectionEdgeItem::paint(QPainter *painter,
                                 const QStyleOptionGraphicsItem *option,
                                 QWidget *)
  {
    const bool selected = option->state & QStyle::State_Selected;
    const bool adapted = !m_connection.adaptedOutputs.empty();

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    QPen pen(selected ? QColor(60, 120, 220) : QColor(110, 110, 120),
             selected ? 2.5 : 1.6);

    // An adapted-output chain is a different kind of link, not decoration —
    // it is drawn dashed so it reads as one at a glance.
    if (adapted)
    {
      pen.setStyle(Qt::DashLine);
    }

    painter->setPen(pen);
    painter->drawPath(m_path);
  }

  const HydroCouple::SDK::IO::ConnectionSpec &
  ConnectionEdgeItem::connection() const
  {
    return m_connection;
  }


  // ── AdapterNodeItem ──────────────────────────────────────────────────────

  namespace
  {
    constexpr qreal kAdapterWidth = 120.0;
    constexpr qreal kAdapterHeight = 34.0;
  }

  AdapterNodeItem::AdapterNodeItem(
    HydroCouple::SDK::IO::ConnectionSpec connection, int stepIndex,
    HydroCouple::SDK::IO::AdaptedOutputSpec step)
    : m_connection(std::move(connection)),
      m_stepIndex(stepIndex),
      m_step(std::move(step)),
      m_size(kAdapterWidth, kAdapterHeight)
  {
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemSendsGeometryChanges, true);

    const QString factory = m_step.factory.empty()
                              ? QStringLiteral("any factory")
                              : QString::fromStdString(m_step.factory);
    setToolTip(QStringLiteral("adapter '%1' (%2) — step %3 of the "
                              "chain from %4.%5")
                 .arg(QString::fromStdString(m_step.id), factory)
                 .arg(m_stepIndex + 1)
                 .arg(QString::fromStdString(m_connection.fromComponent),
                      QString::fromStdString(m_connection.output)));
  }

  int AdapterNodeItem::type() const
  {
    return Type;
  }

  QRectF AdapterNodeItem::boundingRect() const
  {
    // Centred on the origin, so pos() is the node's centre — the same point
    // the sidecar stores and the default along-the-edge placement computes.
    return {-m_size.width() / 2.0, -m_size.height() / 2.0, m_size.width(),
            m_size.height()};
  }

  void AdapterNodeItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *)
  {
    const bool selected = option->state & QStyle::State_Selected;
    const QRectF box = boundingRect();

    painter->setRenderHint(QPainter::Antialiasing, true);

    // Amber, unmistakably not a component box: a connector spliced into
    // the wire.
    painter->setBrush(QColor(252, 246, 232));
    painter->setPen(QPen(selected ? QColor(60, 120, 220)
                                  : QColor(200, 150, 60),
                         selected ? 2.0 : 1.4));
    painter->drawRoundedRect(box, 6.0, 6.0);

    painter->setPen(QColor(90, 70, 30));
    const QFontMetricsF metrics(painter->font());
    painter->drawText(box.adjusted(8.0, 0.0, -8.0, 0.0),
                      Qt::AlignCenter,
                      metrics.elidedText(QString::fromStdString(m_step.id),
                                         Qt::ElideMiddle,
                                         box.width() - 16.0));
  }

  const HydroCouple::SDK::IO::ConnectionSpec &AdapterNodeItem::connection() const
  {
    return m_connection;
  }

  int AdapterNodeItem::stepIndex() const
  {
    return m_stepIndex;
  }

  const HydroCouple::SDK::IO::AdaptedOutputSpec &AdapterNodeItem::step() const
  {
    return m_step;
  }

  QPointF AdapterNodeItem::anchorIn() const
  {
    return mapToScene(QPointF(-m_size.width() / 2.0, 0.0));
  }

  QPointF AdapterNodeItem::anchorOut() const
  {
    return mapToScene(QPointF(m_size.width() / 2.0, 0.0));
  }

  QVariant AdapterNodeItem::itemChange(GraphicsItemChange change,
                                       const QVariant &value)
  {
    if (change == ItemPositionHasChanged)
    {
      m_moving = true;
    }

    return QGraphicsObject::itemChange(change, value);
  }

  void AdapterNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
  {
    QGraphicsObject::mouseReleaseEvent(event);

    if (m_moving)
    {
      m_moving = false;
      Q_EMIT moved(m_connection, m_stepIndex, pos());
    }
  }

  // ── BindingEdgeItem ───────────────────────────────────────────────────────

  BindingEdgeItem::BindingEdgeItem(
    HydroCouple::SDK::IO::ArgumentBindingSpec binding,
    ComponentNodeItem *provider, ComponentNodeItem *consumer)
    : m_binding(std::move(binding)),
      m_provider(provider),
      m_consumer(consumer)
  {
    setFlag(ItemIsSelectable, true); // Selectable, so Delete can remove it.
    setZValue(-2.0); // Behind the exchange edges: initialization underlies.
    setToolTip(QStringLiteral("%1.%2 \u27f5 %3.%4 \u2014 resolves when the "
                              "composition runs")
                 .arg(QString::fromStdString(m_binding.component),
                      QString::fromStdString(m_binding.argument),
                      QString::fromStdString(m_binding.provider),
                      QString::fromStdString(m_binding.output)));
    refresh();
  }

  int BindingEdgeItem::type() const
  {
    return Type;
  }

  const HydroCouple::SDK::IO::ArgumentBindingSpec &
  BindingEdgeItem::binding() const
  {
    return m_binding;
  }

  QPainterPath BindingEdgeItem::buildPath() const
  {
    QPainterPath path;

    if (!m_provider || !m_consumer)
    {
      return path;
    }

    const QRectF from = m_provider->sceneBoundingRect();
    const QRectF to = m_consumer->sceneBoundingRect();
    const QPointF start(from.right(), from.center().y());
    const QPointF end(to.left(), to.center().y());
    const qreal reach = std::max(40.0, std::abs(end.x() - start.x()) * 0.5);

    path.moveTo(start);
    path.cubicTo(start + QPointF(reach, 0.0), end - QPointF(reach, 0.0), end);

    return path;
  }

  void BindingEdgeItem::refresh()
  {
    prepareGeometryChange();
    m_path = buildPath();
    update();
  }

  QRectF BindingEdgeItem::boundingRect() const
  {
    return m_path.boundingRect().adjusted(-6.0, -6.0, 6.0, 6.0);
  }

  QPainterPath BindingEdgeItem::shape() const
  {
    QPainterPathStroker stroker;
    stroker.setWidth(8.0);
    return stroker.createStroke(m_path);
  }

  void BindingEdgeItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *)
  {
    const bool selected = option->state & QStyle::State_Selected;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    QPen pen(selected ? QColor(60, 120, 220) : QColor(140, 110, 200),
             selected ? 2.5 : 1.6);
    pen.setDashPattern({5.0, 4.0});
    painter->setPen(pen);
    painter->drawPath(m_path);
  }

} // namespace HydroCouple::Composer
