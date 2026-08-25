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
    return m_components.isEmpty();
  }

  void Presentation::clear()
  {
    m_components.clear();
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

    return true;
  }

} // namespace HydroCouple::Composer
