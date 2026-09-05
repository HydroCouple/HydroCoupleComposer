#include "project/compositiondocument.h"
#include "project/documentcommands.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUndoStack>

#include <algorithm>

namespace HydroCouple::Composer
{

  namespace
  {
    //! Serialises a spec the way this program writes composition documents.
    QByteArray specToJsonText(
      const HydroCouple::SDK::IO::CompositionSpec &spec)
    {
      // Two-space indent, and nlohmann's ordered dump — the SDK's toJson()
      // decides the content, this only decides the formatting, and doing it in
      // exactly one place is what makes re-saves byte-stable.
      return QByteArray::fromStdString(spec.toJson().dump(2) + "\n");
    }

    // Connection IDENTITY: endpoints plus role. The adaptation chain is
    // content, deliberately excluded — two chains on one (output, input,
    // role) would be a double-wire, so hosts deduplicate by identity and
    // edit the chain in place.
    bool sameConnection(const HydroCouple::SDK::IO::ConnectionSpec &lhs,
                        const HydroCouple::SDK::IO::ConnectionSpec &rhs)
    {
      return lhs.fromComponent == rhs.fromComponent &&
             lhs.output == rhs.output && lhs.toComponent == rhs.toComponent &&
             lhs.input == rhs.input && lhs.role == rhs.role;
    }

    HydroCouple::SDK::IO::ConnectionSpec *findConnection(
      HydroCouple::SDK::IO::CompositionSpec &spec,
      const HydroCouple::SDK::IO::ConnectionSpec &identity)
    {
      for (HydroCouple::SDK::IO::ConnectionSpec &link : spec.connections)
      {
        if (sameConnection(link, identity))
        {
          return &link;
        }
      }

      return nullptr;
    }
  } // namespace

  CompositionDocument::CompositionDocument(QObject *parent)
    : QObject(parent),
      m_undoStack(new QUndoStack(this))
  {
    connect(m_undoStack, &QUndoStack::cleanChanged, this,
            [this](bool) { refreshModified(); });
  }

  CompositionDocument::~CompositionDocument() = default;

  // ── Lifecycle ────────────────────────────────────────────────────────────

  void CompositionDocument::clear()
  {
    m_spec = CompositionSpec{};
    m_presentation.clear();
    m_undoStack->clear();
    m_undoStack->setClean();

    setFilePath(QString());
    refreshModified();

    Q_EMIT componentsChanged();
    Q_EMIT connectionsChanged();
    Q_EMIT compositionChanged();
  }

  bool CompositionDocument::loadFromJson(const QByteArray &json,
                                         QString &message)
  {
    nlohmann::json parsed;

    try
    {
      parsed = nlohmann::json::parse(json.toStdString());
    }
    catch (const nlohmann::json::parse_error &error)
    {
      message = QStringLiteral("not valid JSON: %1")
                  .arg(QString::fromUtf8(error.what()));
      return false;
    }

    CompositionSpec spec;
    std::string specMessage;

    // Validate into a *separate* spec, so a document that fails validation
    // leaves the one currently open untouched rather than half-replaced.
    if (!CompositionSpec::parse(parsed, spec, specMessage))
    {
      message = QString::fromStdString(specMessage);
      return false;
    }

    m_spec = std::move(spec);
    m_presentation.clear();
    m_undoStack->clear();
    m_undoStack->setClean();
    refreshModified();

    Q_EMIT componentsChanged();
    Q_EMIT connectionsChanged();
    Q_EMIT compositionChanged();

    return true;
  }

  bool CompositionDocument::load(const QString &filePath, QString &message)
  {
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
      message = QStringLiteral("cannot open '%1': %2")
                  .arg(filePath, file.errorString());
      return false;
    }

    const QByteArray json = file.readAll();
    file.close();

    if (!loadFromJson(json, message))
    {
      message = QStringLiteral("%1: %2").arg(filePath, message);
      return false;
    }

    // The sidecar is optional by design: a composition authored by any other
    // host has none, and must still open cleanly.
    QFile sidecar(Presentation::sidecarPathFor(filePath));

    if (sidecar.open(QIODevice::ReadOnly))
    {
      m_presentation.fromJson(sidecar.readAll());
      sidecar.close();
    }

    setFilePath(QFileInfo(filePath).absoluteFilePath());
    refreshModified();

    Q_EMIT compositionChanged();

    return true;
  }

  bool CompositionDocument::save(const QString &filePath, QString &message)
  {
    const QString target = filePath.isEmpty() ? m_filePath : filePath;

    if (target.isEmpty())
    {
      message = QStringLiteral("no destination given and the document is untitled");
      return false;
    }

    QSaveFile file(target);

    if (!file.open(QIODevice::WriteOnly))
    {
      message = QStringLiteral("cannot write '%1': %2")
                  .arg(target, file.errorString());
      return false;
    }

    file.write(specToJsonText(m_spec));

    if (!file.commit())
    {
      message = QStringLiteral("cannot write '%1': %2")
                  .arg(target, file.errorString());
      return false;
    }

    if (!m_presentation.isEmpty())
    {
      QSaveFile sidecar(Presentation::sidecarPathFor(target));

      if (sidecar.open(QIODevice::WriteOnly))
      {
        sidecar.write(m_presentation.toJson());

        if (!sidecar.commit())
        {
          // The composition is what matters; a failed sidecar costs layout,
          // not data, and must not fail the save.
          message = QStringLiteral("saved '%1', but its layout sidecar could "
                                   "not be written")
                      .arg(target);
        }
      }
    }

    setFilePath(QFileInfo(target).absoluteFilePath());
    m_undoStack->setClean();
    refreshModified();

    return true;
  }

  QByteArray CompositionDocument::toJson() const
  {
    return specToJsonText(m_spec);
  }

  // ── State ────────────────────────────────────────────────────────────────

  QString CompositionDocument::filePath() const
  {
    return m_filePath;
  }

  bool CompositionDocument::isModified() const
  {
    return m_modified;
  }

  QUndoStack *CompositionDocument::undoStack() const
  {
    return m_undoStack;
  }

  const CompositionDocument::CompositionSpec &CompositionDocument::spec() const
  {
    return m_spec;
  }

  Presentation &CompositionDocument::presentation()
  {
    return m_presentation;
  }

  const Presentation &CompositionDocument::presentation() const
  {
    return m_presentation;
  }

  // ── Queries ──────────────────────────────────────────────────────────────

  QStringList CompositionDocument::componentIds() const
  {
    QStringList ids;
    ids.reserve(static_cast<int>(m_spec.components.size()));

    for (const ComponentSpec &component : m_spec.components)
    {
      ids.append(QString::fromStdString(component.id));
    }

    return ids;
  }

  std::optional<CompositionDocument::ComponentSpec>
  CompositionDocument::component(const QString &componentId) const
  {
    if (const ComponentSpec *found = m_spec.component(componentId.toStdString()))
    {
      return *found;
    }

    return std::nullopt;
  }

  int CompositionDocument::connectionCount() const
  {
    return static_cast<int>(m_spec.connections.size());
  }

  // ── Edits ────────────────────────────────────────────────────────────────

  bool CompositionDocument::addComponent(const ComponentSpec &component,
                                         const ComponentPresentation &placement)
  {
    if (component.id.empty() || m_spec.component(component.id))
    {
      return false;
    }

    CompositionSpec after = m_spec;
    after.components.push_back(component);

    m_undoStack->push(new AddComponentCommand(
      this, m_spec, std::move(after),
      QString::fromStdString(component.id), placement));

    return true;
  }

  bool CompositionDocument::removeComponent(const QString &componentId)
  {
    const std::string id = componentId.toStdString();

    if (!m_spec.component(id))
    {
      return false;
    }

    CompositionSpec after = m_spec;

    after.components.erase(
      std::remove_if(after.components.begin(), after.components.end(),
                     [&id](const ComponentSpec &c) { return c.id == id; }),
      after.components.end());

    // A dangling reference would make the saved document unparseable — the
    // spec refuses connections and @from bindings that name an undeclared
    // component — so the component's connections AND the bindings that
    // point at it go with it, and come back together on undo.
    for (ComponentSpec &component : after.components)
    {
      for (auto it = component.arguments.begin();
           it != component.arguments.end();)
      {
        if (HydroCouple::SDK::IO::isArgumentBinding(it.value()) &&
            it.value()[HydroCouple::SDK::IO::kArgumentBindingKey]
                .value("component", std::string()) == id)
        {
          it = component.arguments.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }

    after.connections.erase(
      std::remove_if(after.connections.begin(), after.connections.end(),
                     [&id](const ConnectionSpec &c)
                     {
                       return c.fromComponent == id || c.toComponent == id;
                     }),
      after.connections.end());

    const bool hadPlacement = m_presentation.hasComponent(componentId);

    m_undoStack->push(new RemoveComponentCommand(
      this, m_spec, std::move(after), componentId,
      m_presentation.component(componentId), hadPlacement));

    return true;
  }

  bool CompositionDocument::addConnection(const ConnectionSpec &connection)
  {
    if (!m_spec.component(connection.fromComponent) ||
        !m_spec.component(connection.toComponent))
    {
      return false;
    }

    const bool duplicate =
      std::any_of(m_spec.connections.begin(), m_spec.connections.end(),
                  [&connection](const ConnectionSpec &existing)
                  { return sameConnection(existing, connection); });

    if (duplicate)
    {
      return false;
    }

    CompositionSpec after = m_spec;
    after.connections.push_back(connection);

    m_undoStack->push(new SpecChangeCommand(this, m_spec, std::move(after),
                                            tr("Connect")));

    return true;
  }

  bool CompositionDocument::removeConnection(const ConnectionSpec &connection)
  {
    CompositionSpec after = m_spec;

    const auto removed =
      std::remove_if(after.connections.begin(), after.connections.end(),
                     [&connection](const ConnectionSpec &existing)
                     { return sameConnection(existing, connection); });

    if (removed == after.connections.end())
    {
      return false;
    }

    after.connections.erase(removed, after.connections.end());

    m_undoStack->push(new SpecChangeCommand(this, m_spec, std::move(after),
                                            tr("Disconnect")));

    return true;
  }

  bool CompositionDocument::insertConnectionAdapter(
    const ConnectionSpec &connection, int index,
    const HydroCouple::SDK::IO::AdaptedOutputSpec &step)
  {
    if (step.id.empty())
    {
      return false;
    }

    CompositionSpec after = m_spec;
    ConnectionSpec *link = findConnection(after, connection);

    if (!link || index < 0 ||
        index > static_cast<int>(link->adaptedOutputs.size()))
    {
      return false;
    }

    link->adaptedOutputs.insert(link->adaptedOutputs.begin() + index, step);

    m_undoStack->push(new SpecChangeCommand(
      this, m_spec, std::move(after),
      tr("Insert adapter '%1'").arg(QString::fromStdString(step.id))));

    return true;
  }

  bool CompositionDocument::removeConnectionAdapter(
    const ConnectionSpec &connection, int index)
  {
    CompositionSpec after = m_spec;
    ConnectionSpec *link = findConnection(after, connection);

    if (!link || index < 0 ||
        index >= static_cast<int>(link->adaptedOutputs.size()))
    {
      return false;
    }

    link->adaptedOutputs.erase(link->adaptedOutputs.begin() + index);

    m_undoStack->push(new SpecChangeCommand(this, m_spec, std::move(after),
                                            tr("Remove adapter")));

    return true;
  }

  bool CompositionDocument::setConnectionAdapterArgument(
    const ConnectionSpec &connection, int index, const QString &argumentId,
    const nlohmann::json &payload)
  {
    if (argumentId.isEmpty())
    {
      return false;
    }

    CompositionSpec after = m_spec;
    ConnectionSpec *link = findConnection(after, connection);

    if (!link || index < 0 ||
        index >= static_cast<int>(link->adaptedOutputs.size()))
    {
      return false;
    }

    HydroCouple::SDK::IO::AdaptedOutputSpec &step =
      link->adaptedOutputs[static_cast<size_t>(index)];
    step.arguments[argumentId.toStdString()] = payload;

    m_undoStack->push(new SpecChangeCommand(
      this, m_spec, std::move(after),
      tr("Set '%1' on adapter '%2'")
        .arg(argumentId, QString::fromStdString(step.id))));

    return true;
  }

  bool CompositionDocument::setConnectionRole(const ConnectionSpec &connection,
                                              const QString &role)
  {
    const std::string next = role.toStdString();

    if (connection.role == next)
    {
      return false;
    }

    // The role is part of the identity: refuse when another connection
    // already holds the identity this change would produce.
    ConnectionSpec probe = connection;
    probe.role = next;

    if (std::any_of(m_spec.connections.begin(), m_spec.connections.end(),
                    [&probe](const ConnectionSpec &existing)
                    { return sameConnection(existing, probe); }))
    {
      return false;
    }

    CompositionSpec after = m_spec;
    ConnectionSpec *link = findConnection(after, connection);

    if (!link)
    {
      return false;
    }

    link->role = next;

    m_undoStack->push(new SpecChangeCommand(this, m_spec, std::move(after),
                                            tr("Set the provider role")));

    return true;
  }

  bool CompositionDocument::clearArgument(const QString &componentId,
                                          const QString &argumentId)
  {
    const std::string id = componentId.toStdString();
    const std::string argument = argumentId.toStdString();

    CompositionSpec after = m_spec;

    for (ComponentSpec &component : after.components)
    {
      if (component.id != id)
      {
        continue;
      }

      if (!component.arguments.contains(argument))
      {
        return false;
      }

      component.arguments.erase(argument);

      m_undoStack->push(new SpecChangeCommand(
        this, m_spec, std::move(after),
        tr("Clear '%1' on '%2'").arg(argumentId, componentId)));

      return true;
    }

    return false;
  }

  bool CompositionDocument::setWorkflow(
    const HydroCouple::SDK::IO::WorkflowSpec &workflow)
  {
    CompositionSpec after = m_spec;
    after.workflow = workflow;

    m_undoStack->push(new SpecChangeCommand(this, m_spec, std::move(after),
                                            tr("Change the workflow")));
    return true;
  }

  bool CompositionDocument::setComponentExecution(
    const QString &componentId, HydroCouple::SDK::IO::ExecutionMode mode,
    const QString &resultsManifest)
  {
    const std::string id = componentId.toStdString();

    if (!m_spec.component(id))
    {
      return false;
    }

    if (mode == HydroCouple::SDK::IO::ExecutionMode::Open &&
        resultsManifest.isEmpty())
    {
      // The spec refuses an open block with nothing to open; refusing here
      // keeps the document loadable rather than failing at save time.
      return false;
    }

    CompositionSpec after = m_spec;

    for (ComponentSpec &component : after.components)
    {
      if (component.id == id)
      {
        component.mode = mode;
        component.resultsManifest =
          mode == HydroCouple::SDK::IO::ExecutionMode::Run
            ? std::string()
            : resultsManifest.toStdString();
        break;
      }
    }

    m_undoStack->push(new SpecChangeCommand(
      this, m_spec, std::move(after),
      tr("Set how '%1' executes").arg(componentId)));
    return true;
  }

  bool CompositionDocument::setArgument(const QString &componentId,
                                        const QString &argumentId,
                                        const nlohmann::json &payload)
  {
    const std::string id = componentId.toStdString();

    if (argumentId.isEmpty() || !m_spec.component(id))
    {
      return false;
    }

    CompositionSpec after = m_spec;

    for (ComponentSpec &component : after.components)
    {
      if (component.id == id)
      {
        component.arguments[argumentId.toStdString()] = payload;
        break;
      }
    }

    m_undoStack->push(new SpecChangeCommand(
      this, m_spec, std::move(after),
      tr("Set '%1' on '%2'").arg(argumentId, componentId)));

    return true;
  }

  bool CompositionDocument::moveComponent(const QString &componentId,
                                          const QPointF &position)
  {
    if (!m_spec.component(componentId.toStdString()))
    {
      return false;
    }

    const QPointF before = m_presentation.component(componentId).position;

    if (before == position)
    {
      return false;
    }

    m_undoStack->push(
      new MoveComponentCommand(this, componentId, before, position));

    return true;
  }

  bool CompositionDocument::moveConnectionAdapter(
    const ConnectionSpec &connection, int index, const QPointF &position)
  {
    // The step must exist in the document's chain; positions for phantom
    // steps would linger in the sidecar with nothing to place.
    const auto link = std::find_if(
      m_spec.connections.begin(), m_spec.connections.end(),
      [&connection](const ConnectionSpec &existing)
      { return sameConnection(existing, connection); });

    if (link == m_spec.connections.end() || index < 0 ||
        index >= static_cast<int>(link->adaptedOutputs.size()))
    {
      return false;
    }

    const QList<QPointF> chain = m_presentation.adapterChain(connection);
    const QPointF before =
      index < chain.size() ? chain[index] : QPointF();

    if (before == position)
    {
      return false;
    }

    m_undoStack->push(
      new MoveAdapterCommand(this, connection, index, before, position));

    return true;
  }

  // ── Command plumbing ─────────────────────────────────────────────────────

  void CompositionDocument::applySpec(const CompositionSpec &spec)
  {
    // Value comparison, not size: an in-place edit — an adapter step
    // inserted into a connection's chain, an argument changed — keeps the
    // counts identical, and a size-only diff would never redraw it.
    const bool componentsDiffer = spec.components != m_spec.components;
    const bool connectionsDiffer = spec.connections != m_spec.connections;

    m_spec = spec;
    refreshModified();

    if (componentsDiffer)
    {
      Q_EMIT componentsChanged();
    }

    if (connectionsDiffer)
    {
      Q_EMIT connectionsChanged();
    }

    Q_EMIT compositionChanged();
  }

  void CompositionDocument::applyComponentPlacement(
    const QString &componentId, const ComponentPresentation &placement)
  {
    m_presentation.setComponent(componentId, placement);
    Q_EMIT placementChanged(componentId);
  }

  void CompositionDocument::applyComponentRemovedFromPresentation(
    const QString &componentId)
  {
    m_presentation.removeComponent(componentId);
    Q_EMIT placementChanged(componentId);
  }

  void CompositionDocument::applyAdapterPlacement(
    const ConnectionSpec &connection, int index, const QPointF &position)
  {
    m_presentation.setAdapterPosition(connection, index, position);
    Q_EMIT adapterPlacementChanged();
  }

  // ── Internals ────────────────────────────────────────────────────────────

  void CompositionDocument::setFilePath(const QString &filePath)
  {
    if (m_filePath == filePath)
    {
      return;
    }

    m_filePath = filePath;
    Q_EMIT filePathChanged(m_filePath);
  }

  void CompositionDocument::refreshModified()
  {
    const bool modified = !m_undoStack->isClean();

    if (modified == m_modified)
    {
      return;
    }

    m_modified = modified;
    Q_EMIT modifiedChanged(m_modified);
  }

} // namespace HydroCouple::Composer
