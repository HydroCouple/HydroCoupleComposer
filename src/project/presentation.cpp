#include "project/presentation.h"

#include "hydrocouplesdk/io/compositionspec.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace HydroCouple::Composer
{

  namespace
  {
    constexpr int kSidecarVersion = 1;
  }

  QString Presentation::sidecarPathFor(const QString &documentPath)
  {
    const QFileInfo info(documentPath);

    QString stem = info.completeBaseName();

    if (stem.isEmpty())
    {
      stem = info.fileName();
    }

    return QDir(info.absolutePath())
      .absoluteFilePath(stem + QStringLiteral(".composer.json"));
  }

  bool Presentation::hasComponent(const QString &componentId) const
  {
    return m_components.contains(componentId);
  }

  ComponentPresentation Presentation::component(
    const QString &componentId) const
  {
    return m_components.value(componentId);
  }

  void Presentation::setComponent(const QString &componentId,
                                  const ComponentPresentation &presentation)
  {
    m_components.insert(componentId, presentation);
  }

  void Presentation::removeComponent(const QString &componentId)
  {
    m_components.remove(componentId);

    // Chains whose identity names the component would otherwise linger as
    // orphans in every future save. (Undoing the removal restores the
    // component's placement but not these positions — the nodes fall back
    // to default spots along the edge, which is cosmetic and self-heals on
    // the next drag.)
    const QStringList keys = m_adapterChains.keys();

    for (const QString &key : keys)
    {
      const AdapterChainEntry &entry = m_adapterChains[key];

      if (entry.fromComponent == componentId ||
          entry.toComponent == componentId)
      {
        m_adapterChains.remove(key);
      }
    }
  }

  void Presentation::renameComponent(const QString &fromId, const QString &toId)
  {
    if (fromId == toId)
    {
      return;
    }

    if (m_components.contains(fromId))
    {
      m_components.insert(toId, m_components.take(fromId));
    }

    // Adapter chains carry the component's id on either side of their
    // identity; rekey the ones that name it.
    const QStringList keys = m_adapterChains.keys();

    for (const QString &key : keys)
    {
      AdapterChainEntry entry = m_adapterChains.value(key);

      if (entry.fromComponent != fromId && entry.toComponent != fromId)
      {
        continue;
      }

      m_adapterChains.remove(key);

      if (entry.fromComponent == fromId)
      {
        entry.fromComponent = toId;
      }
      if (entry.toComponent == fromId)
      {
        entry.toComponent = toId;
      }

      m_adapterChains.insert(chainKey(entry.fromComponent, entry.output,
                                      entry.toComponent, entry.input,
                                      entry.role),
                             entry);
    }
  }

  QString Presentation::chainKey(const QString &fromComponent,
                                 const QString &output,
                                 const QString &toComponent,
                                 const QString &input, const QString &role)
  {
    const QChar separator(0x1f);
    return fromComponent + separator + output + separator + toComponent +
           separator + input + separator + role;
  }

  QList<QPointF> Presentation::adapterChain(
    const HydroCouple::SDK::IO::ConnectionSpec &connection) const
  {
    return m_adapterChains
      .value(chainKey(QString::fromStdString(connection.fromComponent),
                      QString::fromStdString(connection.output),
                      QString::fromStdString(connection.toComponent),
                      QString::fromStdString(connection.input),
                      QString::fromStdString(connection.role)))
      .positions;
  }

  void Presentation::setAdapterChain(
    const HydroCouple::SDK::IO::ConnectionSpec &connection,
    const QList<QPointF> &positions)
  {
    AdapterChainEntry entry;
    entry.fromComponent = QString::fromStdString(connection.fromComponent);
    entry.output = QString::fromStdString(connection.output);
    entry.toComponent = QString::fromStdString(connection.toComponent);
    entry.input = QString::fromStdString(connection.input);
    entry.role = QString::fromStdString(connection.role);
    entry.positions = positions;

    const QString key = chainKey(entry.fromComponent, entry.output,
                                 entry.toComponent, entry.input, entry.role);

    if (positions.isEmpty())
    {
      m_adapterChains.remove(key);
    }
    else
    {
      m_adapterChains.insert(key, entry);
    }
  }

  void Presentation::setAdapterPosition(
    const HydroCouple::SDK::IO::ConnectionSpec &connection, int index,
    const QPointF &position)
  {
    if (index < 0)
    {
      return;
    }

    QList<QPointF> positions = adapterChain(connection);

    while (positions.size() <= index)
    {
      positions.append(position);
    }

    positions[index] = position;
    setAdapterChain(connection, positions);
  }

  void Presentation::removeAdapterChain(
    const HydroCouple::SDK::IO::ConnectionSpec &connection)
  {
    setAdapterChain(connection, {});
  }

  QStringList Presentation::componentIds() const
  {
    QStringList ids = m_components.keys();
    ids.sort();
    return ids;
  }

  const QJsonArray &Presentation::layers() const
  {
    return m_layers;
  }

  void Presentation::setLayers(const QJsonArray &layers)
  {
    m_layers = layers;
  }

  bool Presentation::isEmpty() const
  {
    // The domain counts. Saving is gated on this, so a domain drawn over a
    // composition that has no components yet — which is the order anyone
    // building a model actually works in — would otherwise be written
    // nowhere and be gone at the next open.
    // Layers count for the same reason the domain does: a basemap added to
    // a composition with nothing else in it yet is work, and saving gated
    // on this would throw it away at the next open.
    return m_components.isEmpty() && m_adapterChains.isEmpty()
           && m_meshDomain.isEmpty() && m_layers.isEmpty();
  }

  void Presentation::clear()
  {
    m_components.clear();
    m_adapterChains.clear();
    m_meshDomain = MeshDomain{};
    m_layers = QJsonArray{};
  }

  bool Presentation::hasMeshDomain() const
  {
    return !m_meshDomain.isEmpty();
  }

  const MeshDomain &Presentation::meshDomain() const
  {
    return m_meshDomain;
  }

  void Presentation::setMeshDomain(const MeshDomain &domain)
  {
    m_meshDomain = domain;
  }

  QByteArray Presentation::toJson() const
  {
    QJsonObject components;

    // Sorted so the sidecar diffs cleanly; QHash iteration order is arbitrary
    // and would otherwise reshuffle the file on every save.
    for (const QString &id : componentIds())
    {
      const ComponentPresentation presentation = m_components.value(id);

      QJsonObject entry;
      entry.insert(QStringLiteral("x"), presentation.position.x());
      entry.insert(QStringLiteral("y"), presentation.position.y());

      components.insert(id, entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("sidecar_version"), kSidecarVersion);
    root.insert(QStringLiteral("components"), components);

    // Adapter-node positions, one entry per connection, identity fields
    // spelled out (never a composed string key — component ids may contain
    // anything). Sorted for diff stability; written only when present.
    if (!m_adapterChains.isEmpty())
    {
      QStringList keys = m_adapterChains.keys();
      keys.sort();

      QJsonArray adapters;

      for (const QString &key : keys)
      {
        const AdapterChainEntry &entry = m_adapterChains[key];

        QJsonArray positions;
        for (const QPointF &position : entry.positions)
        {
          QJsonObject point;
          point.insert(QStringLiteral("x"), position.x());
          point.insert(QStringLiteral("y"), position.y());
          positions.append(point);
        }

        QJsonObject block;
        block.insert(QStringLiteral("from"), entry.fromComponent);
        block.insert(QStringLiteral("output"), entry.output);
        block.insert(QStringLiteral("to"), entry.toComponent);
        block.insert(QStringLiteral("input"), entry.input);
        if (!entry.role.isEmpty())
        {
          block.insert(QStringLiteral("role"), entry.role);
        }
        block.insert(QStringLiteral("positions"), positions);

        adapters.append(block);
      }

      root.insert(QStringLiteral("adapters"), adapters);
    }

    // Written only when there are some, so a composition that added no
    // layers keeps a sidecar that says nothing about them.
    if (!m_layers.isEmpty())
    {
      root.insert(QStringLiteral("layers"), m_layers);
    }

    // Written only when there is one, so a composition that never drew a
    // domain keeps a sidecar that says nothing about meshes rather than one
    // carrying an empty section that reads as a domain someone cleared.
    if (hasMeshDomain())
    {
      root.insert(QStringLiteral("mesh_domain"), m_meshDomain.toJson());
    }

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
  }

  bool Presentation::fromJson(const QByteArray &json)
  {
    clear();

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
      return false;
    }

    const QJsonObject components =
      document.object().value(QStringLiteral("components")).toObject();

    for (auto it = components.constBegin(); it != components.constEnd(); ++it)
    {
      const QJsonObject entry = it.value().toObject();

      ComponentPresentation presentation;
      presentation.position = QPointF(entry.value(QStringLiteral("x")).toDouble(),
                                      entry.value(QStringLiteral("y")).toDouble());

      m_components.insert(it.key(), presentation);
    }

    const QJsonValue adapters =
      document.object().value(QStringLiteral("adapters"));

    if (adapters.isArray())
    {
      for (const QJsonValue &value : adapters.toArray())
      {
        const QJsonObject block = value.toObject();

        AdapterChainEntry entry;
        entry.fromComponent = block.value(QStringLiteral("from")).toString();
        entry.output = block.value(QStringLiteral("output")).toString();
        entry.toComponent = block.value(QStringLiteral("to")).toString();
        entry.input = block.value(QStringLiteral("input")).toString();
        entry.role = block.value(QStringLiteral("role")).toString();

        for (const QJsonValue &point :
             block.value(QStringLiteral("positions")).toArray())
        {
          const QJsonObject coordinates = point.toObject();
          entry.positions.append(
            QPointF(coordinates.value(QStringLiteral("x")).toDouble(),
                    coordinates.value(QStringLiteral("y")).toDouble()));
        }

        if (!entry.positions.isEmpty())
        {
          m_adapterChains.insert(chainKey(entry.fromComponent, entry.output,
                                          entry.toComponent, entry.input,
                                          entry.role),
                                 entry);
        }
      }
    }

    const QJsonValue layers = document.object().value(QStringLiteral("layers"));

    if (layers.isArray())
    {
      m_layers = layers.toArray();
    }

    const QJsonValue domain = document.object().value(
      QStringLiteral("mesh_domain"));

    if (domain.isObject())
    {
      QString message;

      // A sidecar whose mesh section will not parse is not a sidecar that
      // fails: the canvas positions beside it are still good, and throwing
      // them away over a domain would cost more than it saved.
      if (!MeshDomain::fromJson(domain.toObject(), m_meshDomain, message))
      {
        m_meshDomain = MeshDomain{};
      }
    }

    return true;
  }

} // namespace HydroCouple::Composer
