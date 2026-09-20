#include "configurator/argumentdescriptor.h"

#include "hydrocouplesdk/io/uriresolver.h"

// The typed data-item interfaces an argument may answer to. Included
// here rather than in the header, because nothing in the descriptor's
// declaration mentions them: the header says what an argument *is like*
// and this file is where that is worked out.
#include "hydrocouplespatial.h"
#include "hydrocoupletemporal.h"

#include <typeinfo>

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

    /*!
     * \brief Whether \a unit measures time and nothing else.
     *
     * Time to the first power and every other fundamental dimension to the
     * zeroth. (The accessor is IUnitDimensions::power(); the interface's
     * own doc comment calls it getPower, which it is not.) Seconds and hours qualify; a velocity does not, and neither
     * does a frequency — one per second is a rate, and an editor offering
     * to show that in hours would be offering nonsense.
     */
    bool unitIsPureTime(HydroCouple::IUnit *unit)
    {
      HydroCouple::IUnitDimensions *dimensions =
        unit ? unit->dimensions() : nullptr;

      if (!dimensions)
      {
        return false;
      }

      using Fundamental =
        HydroCouple::IUnitDimensions::FundamentalUnitDimension;

      constexpr Fundamental others[] = {
        Fundamental::Length,            Fundamental::Mass,
        Fundamental::ElectricCurrent,   Fundamental::Temperature,
        Fundamental::AmountOfSubstance, Fundamental::LuminousIntensity,
        Fundamental::Currency,
      };

      for (Fundamental dimension : others)
      {
        if (dimensions->power(dimension) != 0.0)
        {
          return false;
        }
      }

      return dimensions->power(Fundamental::Time) == 1.0;
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

  ArgumentEditorKind chooseEditorKind(const ArgumentFacts &facts)
  {
    // ── what the argument *is*, as the component declares it ─────────────
    //
    // Interfaces first. A component that implements
    // IPolyhedralSurfaceComponentDataItem has told us its argument is a
    // surface; deciding from rank instead would call it a table and hand
    // the user a spreadsheet of coordinates.
    //
    // Space beats time where an argument is both. A time-varying mesh is a
    // mesh whose values move — the geometry is the hard thing to edit and
    // the time axis can be a column inside that editor, whereas a time
    // series editor has nowhere to put twelve thousand faces.
    if (facts.isRaster)
    {
      return ArgumentEditorKind::Raster;
    }

    if (facts.isPolyhedralSurface)
    {
      return ArgumentEditorKind::Mesh;
    }

    if (facts.isGeometry)
    {
      return ArgumentEditorKind::Geometry;
    }

    if (facts.isTimeSeries)
    {
      return ArgumentEditorKind::TimeSeries;
    }

    // After the spatial ones, because an id-based *and* time-based item is
    // a table of series and the time axis is the one worth plotting.
    if (facts.isIdBased)
    {
      return ArgumentEditorKind::IdTable;
    }

    // ── what its value definition says ───────────────────────────────────
    //
    // A categorical argument is a choice whether it holds one value or a
    // hundred, which is why this precedes the rank test below.
    if (facts.hasCategories && facts.isScalar)
    {
      return ArgumentEditorKind::Categorical;
    }

    // A CRS is a string the component will only accept as a spatial
    // reference, and it says so through validComponentDataItemTypes().
    // Read from that rather than from the caption, so an argument called
    // "projection" that holds free text stays text.
    if (facts.acceptsSpatialReference
        && facts.dataKind == HydroCouple::DataKind::String)
    {
      return ArgumentEditorKind::Crs;
    }

    // A quantity whose unit has time in it and nothing else is a length of
    // time, not a number: "3600 s" wants an editor that can also say "1 h".
    if (facts.isScalar && facts.unitIsPureTime
        && (isRealKind(facts.dataKind) || isIntegerKind(facts.dataKind)))
    {
      return ArgumentEditorKind::Duration;
    }

    // Any other scalar with a unit gets the converting spin box.
    if (facts.isScalar && facts.hasUnit
        && (isRealKind(facts.dataKind) || isIntegerKind(facts.dataKind)))
    {
      return ArgumentEditorKind::Quantity;
    }

    // ── text, files, and the shapes we are left to guess from ────────────
    //
    // A string argument that also reads files is text that happens to live
    // in one — FVQual's kinetics block is the case. It gets the text pane
    // rather than a path box, and that pane keeps a file chooser, so this
    // judgement costs nothing if it is wrong in either direction.
    if (facts.hasFileFilters
        && facts.dataKind == HydroCouple::DataKind::String
        && facts.isScalar)
    {
      return ArgumentEditorKind::LongText;
    }

    if (facts.hasFileFilters)
    {
      return ArgumentEditorKind::FilePath;
    }

    if (!facts.isScalar)
    {
      return ArgumentEditorKind::Table;
    }

    if (facts.dataKind == HydroCouple::DataKind::Boolean)
    {
      return ArgumentEditorKind::Boolean;
    }

    if (isIntegerKind(facts.dataKind))
    {
      return ArgumentEditorKind::Integer;
    }

    if (isRealKind(facts.dataKind))
    {
      return ArgumentEditorKind::Number;
    }

    if (facts.dataKind == HydroCouple::DataKind::String)
    {
      return ArgumentEditorKind::Text;
    }

    // Opaque or unknown: the component knows what it means and we do not,
    // so the raw JSON pane is the honest editor.
    return ArgumentEditorKind::Raw;
  }

  ArgumentEditorKind genericEditorKind(const ArgumentFacts &facts)
  {
    // Deliberately a copy of the old chain rather than a call into the new
    // one with the typed flags cleared. The two are supposed to diverge —
    // that divergence is the feature — and expressing one in terms of the
    // other would make every future change to the new chain silently
    // change what the dock falls back to.
    if (facts.hasCategories && facts.isScalar)
    {
      return ArgumentEditorKind::Categorical;
    }

    if (facts.hasFileFilters)
    {
      return ArgumentEditorKind::FilePath;
    }

    if (!facts.isScalar)
    {
      return ArgumentEditorKind::Table;
    }

    if (facts.dataKind == HydroCouple::DataKind::Boolean)
    {
      return ArgumentEditorKind::Boolean;
    }

    if (isIntegerKind(facts.dataKind))
    {
      return ArgumentEditorKind::Integer;
    }

    if (isRealKind(facts.dataKind))
    {
      return ArgumentEditorKind::Number;
    }

    if (facts.dataKind == HydroCouple::DataKind::String)
    {
      return ArgumentEditorKind::Text;
    }

    return ArgumentEditorKind::Raw;
  }

  QString argumentEditorKindName(ArgumentEditorKind kind)
  {
    switch (kind)
    {
      case ArgumentEditorKind::Categorical:
        return QStringLiteral("Categorical");
      case ArgumentEditorKind::Number:
        return QStringLiteral("Number");
      case ArgumentEditorKind::Integer:
        return QStringLiteral("Integer");
      case ArgumentEditorKind::Boolean:
        return QStringLiteral("Boolean");
      case ArgumentEditorKind::Text:
        return QStringLiteral("Text");
      case ArgumentEditorKind::FilePath:
        return QStringLiteral("FilePath");
      case ArgumentEditorKind::Table:
        return QStringLiteral("Table");
      case ArgumentEditorKind::Quantity:
        return QStringLiteral("Quantity");
      case ArgumentEditorKind::TimeSeries:
        return QStringLiteral("TimeSeries");
      case ArgumentEditorKind::Mesh:
        return QStringLiteral("Mesh");
      case ArgumentEditorKind::Geometry:
        return QStringLiteral("Geometry");
      case ArgumentEditorKind::Raster:
        return QStringLiteral("Raster");
      case ArgumentEditorKind::IdTable:
        return QStringLiteral("IdTable");
      case ArgumentEditorKind::Duration:
        return QStringLiteral("Duration");
      case ArgumentEditorKind::Crs:
        return QStringLiteral("Crs");
      case ArgumentEditorKind::LongText:
        return QStringLiteral("LongText");
      case ArgumentEditorKind::Raw:
        break;
    }

    return QStringLiteral("Raw");
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
    //
    // Gathered here, decided in chooseEditorKind(). The split is what lets
    // the decision be tested at all: this half needs a loaded component
    // library to exercise, and that half needs nothing.
    ArgumentFacts facts;

    facts.hasCategories = !descriptor.categories.isEmpty();
    facts.isScalar = descriptor.isScalar();
    facts.hasFileFilters = !descriptor.fileFilters.isEmpty();
    facts.dataKind = descriptor.dataKind;
    facts.hasUnit = !descriptor.unit.isEmpty();

    // An argument *is* an IComponentDataItem (hydrocouple.h), so a typed
    // argument can answer the typed interfaces directly. Nothing here
    // requires it to: an argument answering none of them falls through to
    // the value definition exactly as it did before this existed.
    facts.isTimeSeries =
      dynamic_cast<HydroCouple::Temporal::ITimeSeriesComponentDataItem *>(
        argument)
      != nullptr;

    facts.isPolyhedralSurface =
      dynamic_cast<
        HydroCouple::Spatial::IPolyhedralSurfaceComponentDataItem *>(argument)
      != nullptr;

    facts.isRaster =
      dynamic_cast<HydroCouple::Spatial::IRasterComponentDataItem *>(argument)
        != nullptr
      || dynamic_cast<
           HydroCouple::Spatial::IRegularGrid2DComponentDataItem *>(argument)
           != nullptr;

    // A network is geometry too: it is edges with coordinates, and the
    // editor that picks features on the map is the right one for both.
    facts.isGeometry =
      dynamic_cast<HydroCouple::Spatial::IGeometryComponentDataItem *>(
        argument)
        != nullptr
      || dynamic_cast<HydroCouple::Spatial::INetworkComponentDataItem *>(
           argument)
           != nullptr;

    facts.isIdBased =
      dynamic_cast<HydroCouple::IIdBasedComponentDataItem *>(argument)
      != nullptr;

    if (auto *quantity =
          dynamic_cast<HydroCouple::IQuantity *>(argument->valueDefinition()))
    {
      facts.unitIsPureTime = unitIsPureTime(quantity->unit());
    }

    // The CRS signal, read from the component rather than from the
    // argument's name: it lists the data-item types it will accept, and one
    // of them naming a spatial reference is the component saying so.
    for (const std::type_info *type : argument->validComponentDataItemTypes())
    {
      if (type
          && *type == typeid(HydroCouple::Spatial::ISpatialReferenceSystem))
      {
        facts.acceptsSpatialReference = true;

        break;
      }
    }

    descriptor.kind = chooseEditorKind(facts);
    descriptor.inlineKind = genericEditorKind(facts);

    return descriptor;
  }

} // namespace HydroCouple::Composer
