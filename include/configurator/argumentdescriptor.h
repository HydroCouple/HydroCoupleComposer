/*!
 * \file   argumentdescriptor.h
 * \author Caleb Buahin
 * \brief  ArgumentDescriptor — what an editor needs to know about an argument.
 *
 * \par Why this is introspection and not a schema
 * Composition Specification v1 validates the *document*; it declares each
 * component's `arguments` object as `additionalProperties: true`, passed
 * verbatim to `IArgument::initialize()`. There is deliberately no per-argument
 * JSON Schema anywhere in the interfaces or the SDK, because what a payload
 * may contain is the component's business, not the document format's.
 *
 * So a form cannot be generated from a schema — it is generated from what the
 * argument itself advertises through the v2 data plane: its rank and shape,
 * its `DataKind`, and its value definition. That last one is the interesting
 * part: an `IQuality` value definition enumerates the permitted categories,
 * which is exactly an enumeration, while an `IQuantity` carries a unit. This
 * yields better forms than a schema would, because it reflects the component's
 * own model of its inputs.
 *
 * The payload itself is always read and written through
 * `serialize(JSON, …)` / `initialize(…, JSON, …)`, so Composer never invents
 * a payload shape — the same rule the `.hcp` importer follows.
 */

#ifndef HYDROCOUPLECOMPOSER_CONFIGURATOR_ARGUMENTDESCRIPTOR_H
#define HYDROCOUPLECOMPOSER_CONFIGURATOR_ARGUMENTDESCRIPTOR_H

#include "hydrocouple.h"

#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

namespace HydroCouple::Composer
{

  /*!
   * \brief The editor an argument should be given.
   */
  enum class ArgumentEditorKind
  {
    //! One value chosen from an IQuality's categories.
    Categorical,
    //! A single number.
    Number,
    //! A single whole number.
    Integer,
    //! A single true/false value.
    Boolean,
    //! A single line of text.
    Text,
    //! A path, offered with the argument's file filters.
    FilePath,
    //! A rank-1 or rank-2 grid of values.
    Table,

    // ── the typed kinds (U2a) ────────────────────────────────────────────
    //
    // Chosen from the data-item interfaces the argument answers to, then
    // from its value definition, and only then from rank and data kind —
    // in that order, because an interface is the component telling us what
    // the argument *is*, while a rank is us guessing from its shape.
    //
    // Never from the argument's id or caption. That rule is B5a's and it
    // stands: an argument called "startDate" that holds a number is a
    // number, and a component whose author spells captions differently
    // must not get a different editor for it.

    //! A scalar with a unit, editable in a unit of the user's choosing.
    Quantity,
    //! Values against times; the argument answers ITimeSeriesComponentDataItem.
    TimeSeries,
    //! A surface or mesh; IPolyhedralSurfaceComponentDataItem.
    Mesh,
    //! Features with geometry; IGeometryComponentDataItem or a network.
    Geometry,
    //! A raster; IRasterComponentDataItem or a regular grid.
    Raster,
    //! Values against identifiers; IIdBasedComponentDataItem.
    IdTable,
    //! A length of time: a scalar whose unit has only time in it.
    Duration,
    //! A coordinate reference system.
    Crs,
    //! Text long enough to want its own pane, and often read from a file.
    LongText,

    //! Nothing better fits; the raw JSON pane is the editor.
    Raw
  };

  /*!
   * \brief What an argument says about itself, as plain values.
   *
   * The half of describeArgument() that talks to HydroCouple's interfaces
   * hands this to the half that decides — chooseEditorKind() — which then
   * needs no component, no library and no interfaces to test. The order of
   * that decision chain is the thing most likely to be wrong, and order is
   * pure logic: a mesh that also varies in time must not arrive at the
   * time-series editor and lose its geometry, and a categorical argument
   * must not be read as a table merely because it holds a hundred values.
   */
  struct ArgumentFacts
  {
      //! Its IQuality offers a fixed set of values.
      bool hasCategories = false;
      //! Its payload is one value rather than a grid.
      bool isScalar = true;
      //! It reads files, and says which.
      bool hasFileFilters = false;

      HydroCouple::DataKind dataKind = HydroCouple::DataKind::Unknown;

      // ── the typed interfaces it answers to ─────────────────────────────
      bool isTimeSeries = false;
      bool isPolyhedralSurface = false;
      bool isRaster = false;
      bool isGeometry = false;
      bool isIdBased = false;

      //! Its IQuantity carries an IUnit.
      bool hasUnit = false;
      //! That unit's dimensions are time and nothing else.
      bool unitIsPureTime = false;
      //! validComponentDataItemTypes() names ISpatialReferenceSystem.
      bool acceptsSpatialReference = false;
  };

  /*!
   * \brief The editor \a facts call for.
   *
   * Pure, total and order-sensitive; see ArgumentFacts.
   */
  [[nodiscard]] ArgumentEditorKind chooseEditorKind(const ArgumentFacts &facts);

  /*!
   * \brief The editor \a facts called for before the typed kinds existed.
   *
   * The pre-U2a chain, kept and still used. A typed kind names what an
   * argument *is*; until that kind has a dialog of its own (U2b, U2c) the
   * dock still has to draw something, and the honest something is what it
   * drew yesterday. Without this, widening the enum would make every
   * newly-typed argument fall through the configurator's switch and
   * render nothing at all — a component's meteorology would simply
   * disappear from its form, which is a far worse outcome than a table.
   *
   * Each dialog that lands deletes one line of the descriptor's use of
   * this, and when the last one lands this goes with it.
   */
  [[nodiscard]] ArgumentEditorKind genericEditorKind(
    const ArgumentFacts &facts);

  //! A stable token for \a kind, for messages and for settings.
  [[nodiscard]] QString argumentEditorKindName(ArgumentEditorKind kind);

  /*!
   * \brief Everything an editor needs about one argument.
   */
  struct ArgumentDescriptor
  {
      QString id;
      QString caption;
      QString description;

      /*!
       * \brief What the argument is, as its interfaces declare it.
       *
       * The summary text and, once it exists, the dialog.
       */
      ArgumentEditorKind kind = ArgumentEditorKind::Raw;

      /*!
       * \brief What the dock draws inline, until \c kind has its dialog.
       *
       * Equal to \c kind for every kind that had an editor before U2a.
       * See genericEditorKind().
       */
      ArgumentEditorKind inlineKind = ArgumentEditorKind::Raw;

      bool isOptional = true;
      bool isReadOnly = false;

      //! Rank-1 length, or rows for rank 2. Zero when unknown.
      int rows = 0;
      //! Columns for rank 2; 1 otherwise.
      int columns = 1;

      HydroCouple::DataKind dataKind = HydroCouple::DataKind::Unknown;

      //! Permitted values when kind is Categorical.
      QStringList categories;
      //! True when the categories have a meaningful order.
      bool categoriesOrdered = false;

      //! Unit caption from an IQuantity, when there is one.
      QString unit;

      //! File filters the argument accepts, when it reads files.
      QStringList fileFilters;

      //! The argument's current payload, as the component serialises it.
      nlohmann::json payload = nlohmann::json::object();

      /*!
       * \brief True when the payload is a single value rather than a grid.
       */
      [[nodiscard]] bool isScalar() const;
  };

  /*!
   * \brief Describes \a argument for the configurator.
   * \param argument The argument to introspect; must not be null.
   */
  [[nodiscard]] ArgumentDescriptor describeArgument(
    HydroCouple::IArgument *argument);

  /*!
   * \brief Reads an argument's current payload as JSON.
   * \param argument The argument to read.
   * \param[out] message Diagnostic when the argument cannot serialise.
   * \returns The payload, or a null json on failure.
   */
  [[nodiscard]] nlohmann::json readArgumentPayload(
    HydroCouple::IArgument *argument, QString &message);

  /*!
   * \brief Points an argument at a file or a URI and lets it read itself.
   *
   * A path is not a payload: the composition document's only channel to an
   * argument is `initialize(..., JSON, ...)`, so a path recorded there would
   * be handed back to the component as JSON and rejected. A chooser therefore
   * loads the reference through the component now, and what gets recorded is
   * what the component read.
   *
   * The two input types are told apart by whether the reference has a URI
   * scheme, using the SDK's own rule so that `C:\data\x.json` stays a path.
   * The distinction is recorded on the argument rather than acted on: a
   * remote reference is what the argument remembers as its source, and a
   * local one is inlined.
   *
   * \param argument The argument to load; must not be null.
   * \param reference The file or URI to read.
   * \param[out] message Diagnostic when the component cannot read it.
   * \returns true when the component read it.
   */
  [[nodiscard]] bool writeArgumentReference(HydroCouple::IArgument *argument,
                                            const QString &reference,
                                            QString &message);

  /*!
   * \brief Builds a QFileDialog filter string from an argument's filters.
   *
   * "All Files" is always offered last, because a filter is what a component
   * says it reads and not what a server named the file.
   *
   * \param fileFilters The argument's own filters, in its own order.
   */
  [[nodiscard]] QString fileDialogFilter(const QStringList &fileFilters);

  /*!
   * \brief Applies a JSON payload to an argument.
   * \param argument The argument to write.
   * \param payload The payload to apply.
   * \param[out] message Diagnostic when the component rejects the payload.
   * \returns true when the component accepted it.
   */
  [[nodiscard]] bool writeArgumentPayload(HydroCouple::IArgument *argument,
                                          const nlohmann::json &payload,
                                          QString &message);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CONFIGURATOR_ARGUMENTDESCRIPTOR_H
