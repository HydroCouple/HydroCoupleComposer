#include "project/presentation.h"

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
  }

  void Presentation::renameComponent(const QString &fromId, const QString &toId)
  {
    if (fromId == toId || !m_components.contains(fromId))
    {
      return;
    }

    m_components.insert(toId, m_components.take(fromId));
  }

  QStringList Presentation::componentIds() const
  {
    QStringList ids = m_components.keys();
    ids.sort();
    return ids;
  }

  bool Presentation::isEmpty() const
  {
    // The domain counts. Saving is gated on this, so a domain drawn over a
    // composition that has no components yet — which is the order anyone
    // building a model actually works in — would otherwise be written
    // nowhere and be gone at the next open.
    return m_components.isEmpty() && m_meshDomain.isEmpty();
  }

  void Presentation::clear()
  {
    m_components.clear();
    m_meshDomain = MeshDomain{};
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
