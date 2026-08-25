#include "project/hcpimporter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>

namespace HydroCouple::Composer
{

  using CompositionSpec = HydroCouple::SDK::IO::CompositionSpec;
  using ComponentSpec = HydroCouple::SDK::IO::ComponentSpec;
  using ConnectionSpec = HydroCouple::SDK::IO::ConnectionSpec;
  using AdaptedOutputSpec = HydroCouple::SDK::IO::AdaptedOutputSpec;

  namespace
  {
    //! One v1 component, as read, before ids are assigned.
    struct LegacyComponent
    {
        QString infoId;      //!< v1 "Name" — the IComponentInfo id.
        QString caption;
        QString description;
        QString library;
        bool isTrigger = false;
        QPointF position;
        QString assignedId;  //!< Synthesised v2 instance id.
    };

    //! One connection, still referring to components by v1 index.
    struct LegacyConnection
    {
        int fromIndex = -1;
        QString output;
        int toIndex = -1;
        QString input;
        QList<AdaptedOutputSpec> adaptedOutputs;
    };

    /*!
     * \brief Turns a caption into an identifier usable as a component id.
     *
     * v1 identified components by their position in the file, not by name, so
     * v2 instance ids have to be synthesised. Deriving them from the caption
     * keeps the imported document readable instead of full of "component_0".
     */
    QString slugify(const QString &text)
    {
      static const QRegularExpression invalid(QStringLiteral("[^A-Za-z0-9]+"));

      QString slug = text.simplified().replace(invalid, QStringLiteral("_"));

      while (slug.startsWith(QLatin1Char('_')))
      {
        slug.remove(0, 1);
      }

      while (slug.endsWith(QLatin1Char('_')))
      {
        slug.chop(1);
      }

      return slug.toLower();
    }

    void addIssue(ImportResult &result, ImportIssue::Severity severity,
                  const QString &location, const QString &message,
                  const QString &detail = QString())
    {
      result.issues.append({severity, location, message, detail});
    }

    QString componentLocation(int index, const LegacyComponent &component)
    {
      return QStringLiteral("ModelComponent[%1] '%2'")
        .arg(index)
        .arg(component.caption.isEmpty() ? component.infoId
                                         : component.caption);
    }

    //! Reads an <Arguments> block, reporting each argument as uncarried.
    void reportArguments(QXmlStreamReader &xml, ImportResult &result,
                         const QString &location)
    {
      while (!xml.atEnd())
      {
        xml.readNext();

        if (xml.isEndElement() &&
            xml.name().compare(QLatin1String("Arguments"),
                               Qt::CaseInsensitive) == 0)
        {
          return;
        }

        if (xml.isStartElement() &&
            xml.name().compare(QLatin1String("Argument"),
                               Qt::CaseInsensitive) == 0)
        {
          const QXmlStreamAttributes attributes = xml.attributes();
          const QString id = attributes.value(QLatin1String("Id")).toString();
          const QString ioType =
            attributes.value(QLatin1String("ArgumentIOType")).toString();
          const QString value = xml.readElementText().trimmed();

          addIssue(
            result, ImportIssue::Severity::Warning,
            QStringLiteral("%1 / Argument '%2'").arg(location, id),
            QStringLiteral(
              "argument not carried across: a v1 %1 payload is in the "
              "component's own dialect and has no general v2 equivalent — "
              "re-enter it against the component in the configurator")
              .arg(ioType.isEmpty() ? QStringLiteral("String") : ioType),
            value);
        }
      }
    }

    //! Reads the <Connections> block beneath one output exchange item.
    void readConnections(QXmlStreamReader &xml, ImportResult &result,
                         LegacyConnection prototype,
                         QList<LegacyConnection> &connections,
                         const QString &location)
    {
      while (!xml.atEnd())
      {
        xml.readNext();

        if (xml.isEndElement() &&
            xml.name().compare(QLatin1String("Connections"),
                               Qt::CaseInsensitive) == 0)
        {
          return;
        }

        if (!xml.isStartElement())
        {
          continue;
        }

        const QXmlStreamAttributes attributes = xml.attributes();

        if (xml.name().compare(QLatin1String("InputExchangeItem"),
                               Qt::CaseInsensitive) == 0)
        {
          LegacyConnection connection = prototype;
          connection.toIndex =
            attributes.value(QLatin1String("ModelComponentIndex")).toInt();
          connection.input =
            attributes.value(QLatin1String("InputExchangeItemId")).toString();
          connections.append(connection);
        }
        else if (xml.name().compare(QLatin1String("AdaptedOutputExchangeItem"),
                                    Qt::CaseInsensitive) == 0)
        {
          LegacyConnection connection = prototype;

          AdaptedOutputSpec adapted;
          adapted.id =
            attributes.value(QLatin1String("AdaptedOutputExchangeItemId"))
              .toString()
              .toStdString();
          adapted.factory =
            attributes.value(QLatin1String("AdaptedOutputFactoryId"))
              .toString()
              .toStdString();
          connection.adaptedOutputs.append(adapted);

          connection.toIndex =
            attributes
              .value(QLatin1String("InputExchangeItemModelComponentIndex"))
              .toInt();
          connection.input =
            attributes.value(QLatin1String("InputExchangeItemId")).toString();

          connections.append(connection);

          // Adapted outputs carry their own arguments, equally uncarriable.
          const QString adaptedLocation =
            QStringLiteral("%1 / AdaptedOutput '%2'")
              .arg(location,
                   QString::fromStdString(adapted.id));

          while (!xml.atEnd())
          {
            xml.readNext();

            if (xml.isEndElement() &&
                xml.name().compare(QLatin1String("AdaptedOutputExchangeItem"),
                                   Qt::CaseInsensitive) == 0)
            {
              break;
            }

            if (xml.isStartElement() &&
                xml.name().compare(QLatin1String("Arguments"),
                                   Qt::CaseInsensitive) == 0)
            {
              reportArguments(xml, result, adaptedLocation);
            }
          }
        }
      }
    }
  } // namespace

  // ── ImportIssue / ImportResult ───────────────────────────────────────────

  QString ImportIssue::toString() const
  {
    const QString prefix = severity == Severity::Error     ? QStringLiteral("error")
                           : severity == Severity::Warning ? QStringLiteral("warning")
                                                           : QStringLiteral("note");

    QString text = QStringLiteral("%1: %2: %3").arg(prefix, location, message);

    if (!detail.isEmpty())
    {
      text += QStringLiteral(" [was: %1]").arg(detail);
    }

    return text;
  }

  QList<ImportIssue> ImportResult::issuesOfAtLeast(
    ImportIssue::Severity severity) const
  {
    QList<ImportIssue> filtered;

    for (const ImportIssue &issue : issues)
    {
      if (issue.severity >= severity)
      {
        filtered.append(issue);
      }
    }

    return filtered;
  }

  bool ImportResult::hasErrors() const
  {
    return !issuesOfAtLeast(ImportIssue::Severity::Error).isEmpty();
  }

  // ── HcpImporter ──────────────────────────────────────────────────────────

  ImportResult HcpImporter::importFile(const QString &filePath)
  {
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
      ImportResult result;
      addIssue(result, ImportIssue::Severity::Error, filePath,
               QStringLiteral("cannot open: %1").arg(file.errorString()));
      return result;
    }

    const QByteArray xml = file.readAll();
    file.close();

    return importXml(xml, filePath);
  }

  ImportResult HcpImporter::importXml(const QByteArray &xml,
                                      const QString &documentPath)
  {
    ImportResult result;

    QList<LegacyComponent> components;
    QList<LegacyConnection> connections;

    QXmlStreamReader reader(xml);

    while (!reader.atEnd())
    {
      reader.readNext();

      if (!reader.isStartElement())
      {
        continue;
      }

      const QXmlStreamAttributes attributes = reader.attributes();

      if (reader.name().compare(QLatin1String("ModelComponent"),
                                Qt::CaseInsensitive) == 0)
      {
        LegacyComponent component;
        component.infoId = attributes.value(QLatin1String("Name")).toString();
        component.caption =
          attributes.value(QLatin1String("Caption")).toString();
        component.description =
          attributes.value(QLatin1String("Description")).toString();
        component.isTrigger =
          attributes.value(QLatin1String("IsTrigger"))
            .compare(QLatin1String("True"), Qt::CaseInsensitive) == 0;
        component.position =
          QPointF(attributes.value(QLatin1String("XPos")).toDouble(),
                  attributes.value(QLatin1String("YPos")).toDouble());

        component.library =
          attributes.value(QLatin1String("ModelComponentLibrary")).toString();

        const int index = static_cast<int>(components.size());
        const QString location = componentLocation(index, component);

        // A component exported to its own file rather than a library: the
        // reference is to a v1 export, which v2 cannot read.
        if (component.library.isEmpty() &&
            attributes.hasAttribute(QLatin1String("ModelComponentFile")))
        {
          addIssue(
            result, ImportIssue::Severity::Warning, location,
            QStringLiteral("component was stored as a v1 component file, "
                           "which has no v2 equivalent; set its library in "
                           "the composition"),
            attributes.value(QLatin1String("ModelComponentFile")).toString());
        }

        components.append(component);
      }
      else if (reader.name().compare(QLatin1String("ComputeResourceAllocations"),
                                     Qt::CaseInsensitive) == 0)
      {
        const int index = static_cast<int>(components.size()) - 1;

        addIssue(result, ImportIssue::Severity::Warning,
                 index >= 0 ? componentLocation(index, components[index])
                            : QStringLiteral("ModelComponent"),
                 QStringLiteral("compute resource allocations (MPI/GPU) have "
                                "no Composition Specification v1 equivalent "
                                "and were not carried across"));
      }
      else if (reader.name().compare(QLatin1String("Arguments"),
                                     Qt::CaseInsensitive) == 0)
      {
        const int index = static_cast<int>(components.size()) - 1;

        if (index >= 0)
        {
          reportArguments(reader, result,
                          componentLocation(index, components[index]));
        }
      }
      else if (reader.name().compare(QLatin1String("WorkflowComponent"),
                                     Qt::CaseInsensitive) == 0)
      {
        addIssue(
          result, ImportIssue::Severity::Warning,
          QStringLiteral("WorkflowComponent"),
          QStringLiteral("v1 workflow components were plugin libraries; v2 "
                         "selects a workflow strategy instead — choose one in "
                         "the composition's workflow settings"),
          attributes.value(QLatin1String("WorkflowComponentLibrary")).toString());
      }
      else if (reader.name().compare(QLatin1String("ModelComponentConnection"),
                                     Qt::CaseInsensitive) == 0)
      {
        LegacyConnection prototype;
        prototype.fromIndex =
          attributes.value(QLatin1String("SourceModelComponentIndex")).toInt();

        // Walk this connection's outputs.
        while (!reader.atEnd())
        {
          reader.readNext();

          if (reader.isEndElement() &&
              reader.name().compare(QLatin1String("ModelComponentConnection"),
                                    Qt::CaseInsensitive) == 0)
          {
            break;
          }

          if (reader.isStartElement() &&
              reader.name().compare(QLatin1String("OutputExchangeItem"),
                                    Qt::CaseInsensitive) == 0)
          {
            LegacyConnection outputPrototype = prototype;
            outputPrototype.output =
              reader.attributes()
                .value(QLatin1String("OutputExchangeItemId"))
                .toString();

            const QString location =
              QStringLiteral("Connection from component[%1] output '%2'")
                .arg(prototype.fromIndex)
                .arg(outputPrototype.output);

            while (!reader.atEnd())
            {
              reader.readNext();

              if (reader.isEndElement() &&
                  reader.name().compare(QLatin1String("OutputExchangeItem"),
                                        Qt::CaseInsensitive) == 0)
              {
                break;
              }

              if (reader.isStartElement() &&
                  reader.name().compare(QLatin1String("Connections"),
                                        Qt::CaseInsensitive) == 0)
              {
                readConnections(reader, result, outputPrototype, connections,
                                location);
              }
            }
          }
        }
      }
    }

    if (reader.hasError())
    {
      addIssue(result, ImportIssue::Severity::Error,
               documentPath.isEmpty() ? QStringLiteral("document")
                                      : documentPath,
               QStringLiteral("malformed XML at line %1: %2")
                 .arg(reader.lineNumber())
                 .arg(reader.errorString()));
      return result;
    }

    if (components.isEmpty())
    {
      addIssue(result, ImportIssue::Severity::Error,
               documentPath.isEmpty() ? QStringLiteral("document")
                                      : documentPath,
               QStringLiteral("no ModelComponent elements found — is this a "
                              "HydroCouple 1.x project?"));
      return result;
    }

    // ── Assign stable ids ──────────────────────────────────────────────────
    QSet<QString> used;

    for (int index = 0; index < components.size(); ++index)
    {
      LegacyComponent &component = components[index];

      QString base = slugify(component.caption);

      if (base.isEmpty())
      {
        base = slugify(component.infoId);
      }

      if (base.isEmpty())
      {
        base = QStringLiteral("component");
      }

      QString candidate = base;
      int suffix = 2;

      while (used.contains(candidate))
      {
        candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++);
      }

      used.insert(candidate);
      component.assignedId = candidate;

      addIssue(result, ImportIssue::Severity::Info,
               componentLocation(index, component),
               QStringLiteral("v1 projects identify components by position, "
                              "so this one was given the id '%1'")
                 .arg(candidate));
    }

    // ── Build the composition ──────────────────────────────────────────────
    for (const LegacyComponent &component : components)
    {
      ComponentSpec spec;
      spec.id = component.assignedId.toStdString();
      spec.caption = component.caption.toStdString();
      spec.info.library = component.library.toStdString();
      spec.info.componentInfoId = component.infoId.toStdString();

      result.spec.components.push_back(spec);
      result.presentation.setComponent(component.assignedId,
                                       {component.position});

      if (component.isTrigger)
      {
        // Tempting to set workflow.strategy = PullDriven here, but the schema
        // requires a pull-driven workflow to name both a trigger component
        // *and* a trigger input, and v1 records no input. Selecting the
        // strategy would therefore produce a document that fails to load —
        // the same "don't invent what v1 did not record" rule the arguments
        // follow. The fact is reported instead, and the user completes the
        // workflow in the configurator.
        addIssue(result, ImportIssue::Severity::Info,
                 QStringLiteral("Workflow"),
                 QStringLiteral("'%1' was the v1 trigger component; v2 also "
                                "needs the trigger input, which v1 did not "
                                "record, so no workflow strategy was set — "
                                "choose one before running")
                   .arg(component.assignedId));
      }
    }

    for (const LegacyConnection &connection : connections)
    {
      const bool fromValid = connection.fromIndex >= 0 &&
                             connection.fromIndex < components.size();
      const bool toValid =
        connection.toIndex >= 0 && connection.toIndex < components.size();

      if (!fromValid || !toValid)
      {
        addIssue(result, ImportIssue::Severity::Error,
                 QStringLiteral("Connection '%1' -> '%2'")
                   .arg(connection.output, connection.input),
                 QStringLiteral("references component index %1, but the "
                                "project declares %2 component(s)")
                   .arg(fromValid ? connection.toIndex : connection.fromIndex)
                   .arg(components.size()));
        continue;
      }

      ConnectionSpec spec;
      spec.fromComponent =
        components[connection.fromIndex].assignedId.toStdString();
      spec.output = connection.output.toStdString();
      spec.toComponent = components[connection.toIndex].assignedId.toStdString();
      spec.input = connection.input.toStdString();

      for (const AdaptedOutputSpec &adapted : connection.adaptedOutputs)
      {
        spec.adaptedOutputs.push_back(adapted);
      }

      result.spec.connections.push_back(spec);
    }

    if (!documentPath.isEmpty())
    {
      result.spec.metadata.caption =
        QFileInfo(documentPath).completeBaseName().toStdString();
    }

    result.succeeded = !result.hasErrors();

    return result;
  }

} // namespace HydroCouple::Composer
