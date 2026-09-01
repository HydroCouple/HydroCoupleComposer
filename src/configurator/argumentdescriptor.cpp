#include "configurator/argumentdescriptor.h"

#include "hydrocouplesdk/io/uriresolver.h"

namespace HydroCouple::Composer
{

  namespace
  {
    bool isIntegerKind(HydroCouple::DataKind kind)
    {
      switch (kind)
      {
        case HydroCouple::DataKind::Int8:
        case HydroCouple::DataKind::UInt8:
        case HydroCouple::DataKind::Int16:
        case HydroCouple::DataKind::UInt16:
        case HydroCouple::DataKind::Int32:
        case HydroCouple::DataKind::UInt32:
        case HydroCouple::DataKind::Int64:
        case HydroCouple::DataKind::UInt64:
          return true;
        default:
          return false;
      }
    }

    bool isRealKind(HydroCouple::DataKind kind)
    {
      return kind == HydroCouple::DataKind::Float32 ||
             kind == HydroCouple::DataKind::Float64;
    }
  } // namespace

  bool ArgumentDescriptor::isScalar() const
  {
    return rows <= 1 && columns <= 1;
  }

  nlohmann::json readArgumentPayload(HydroCouple::IArgument *argument,
                                     QString &message)
  {
    if (!argument)
    {
      message = QStringLiteral("no argument");
      return {};
    }

    std::string serialised;
    std::string failure;

    if (!argument->serialize(HydroCouple::IArgument::ArgumentInputType::JSON,
                             serialised, failure))
    {
      message = QString::fromStdString(failure);
      return {};
    }

    try
    {
      return nlohmann::json::parse(serialised);
    }
    catch (const nlohmann::json::parse_error &error)
    {
      message = QStringLiteral("component produced invalid JSON: %1")
                  .arg(QString::fromUtf8(error.what()));
      return {};
    }
  }

  bool writeArgumentPayload(HydroCouple::IArgument *argument,
                            const nlohmann::json &payload, QString &message)
  {
    if (!argument)
    {
      message = QStringLiteral("no argument");
      return false;
    }

    std::string failure;

    // The component parses its own payload; Composer never interprets it.
    if (!argument->initialize(payload.dump(),
                              HydroCouple::IArgument::ArgumentInputType::JSON,
                              failure))
    {
      message = failure.empty()
                  ? QStringLiteral("the component rejected the value")
                  : QString::fromStdString(failure);
      return false;
    }

    return true;
  }

  bool writeArgumentReference(HydroCouple::IArgument *argument,
                              const QString &reference, QString &message)
  {
    if (!argument)
    {
      message = QStringLiteral("no argument");
      return false;
    }

    const std::string target = reference.toStdString();

    // The SDK's rule, not a second one: a scheme is two characters or more,
    // so a Windows drive letter is a path and not a URI.
    const bool remote =
      !HydroCouple::SDK::IO::uriScheme(target).empty();

    std::string failure;

    if (!argument->initialize(
          target,
          remote ? HydroCouple::IArgument::ArgumentInputType::URL
                 : HydroCouple::IArgument::ArgumentInputType::File,
          failure))
    {
      message = failure.empty()
                  ? QStringLiteral("the component could not read it")
                  : QString::fromStdString(failure);
      return false;
    }

    return true;
  }

  QString fileDialogFilter(const QStringList &fileFilters)
  {
    QStringList entries = fileFilters;
    entries.append(QStringLiteral("All Files (*)"));

    return entries.join(QStringLiteral(";;"));
  }

  ArgumentDescriptor describeArgument(HydroCouple::IArgument *argument)
  {
    ArgumentDescriptor descriptor;

    if (!argument)
    {
      return descriptor;
    }

    descriptor.id = QString::fromStdString(argument->id());
    descriptor.caption = QString::fromStdString(argument->caption());
    descriptor.description = QString::fromStdString(argument->description());
    descriptor.isOptional = argument->isOptional();
    descriptor.isReadOnly = argument->isReadOnly();
    descriptor.dataKind = argument->dataKind();

    if (descriptor.caption.isEmpty())
    {
      descriptor.caption = descriptor.id;
    }

    for (const std::string &filter : argument->fileFilters())
    {
      descriptor.fileFilters.append(QString::fromStdString(filter));
    }

    const std::vector<int64_t> shape = argument->shape();

    if (!shape.empty())
    {
      descriptor.rows = static_cast<int>(shape[0]);
      descriptor.columns =
        shape.size() > 1 ? static_cast<int>(shape[1]) : 1;
    }

    // The value definition decides the editor before rank does: a categorical
    // argument is a choice whether it holds one value or a hundred.
    HydroCouple::IValueDefinition *definition = argument->valueDefinition();

    if (auto *quality = dynamic_cast<HydroCouple::IQuality *>(definition))
    {
      for (const std::string &category : quality->categories())
      {
        descriptor.categories.append(QString::fromStdString(category));
      }

      descriptor.categoriesOrdered = quality->isOrdered();
    }
    else if (auto *quantity = dynamic_cast<HydroCouple::IQuantity *>(definition))
    {
      if (HydroCouple::IUnit *unit = quantity->unit())
      {
        descriptor.unit = QString::fromStdString(unit->caption());
      }
    }

    QString message;
    const nlohmann::json payload = readArgumentPayload(argument, message);

    if (!payload.is_null())
    {
      descriptor.payload = payload;
    }

    // ── Choose the editor ──────────────────────────────────────────────────
    if (!descriptor.categories.isEmpty() && descriptor.isScalar())
    {
      descriptor.kind = ArgumentEditorKind::Categorical;
    }
    else if (!descriptor.fileFilters.isEmpty())
    {
      descriptor.kind = ArgumentEditorKind::FilePath;
    }
    else if (!descriptor.isScalar())
    {
      descriptor.kind = ArgumentEditorKind::Table;
    }
    else if (descriptor.dataKind == HydroCouple::DataKind::Boolean)
    {
      descriptor.kind = ArgumentEditorKind::Boolean;
    }
    else if (isIntegerKind(descriptor.dataKind))
    {
      descriptor.kind = ArgumentEditorKind::Integer;
    }
    else if (isRealKind(descriptor.dataKind))
    {
      descriptor.kind = ArgumentEditorKind::Number;
    }
    else if (descriptor.dataKind == HydroCouple::DataKind::String)
    {
      descriptor.kind = ArgumentEditorKind::Text;
    }
    else
    {
      // Opaque or unknown: the component knows what it means and we do not,
      // so the raw JSON pane is the honest editor.
      descriptor.kind = ArgumentEditorKind::Raw;
    }

    return descriptor;
  }

} // namespace HydroCouple::Composer
