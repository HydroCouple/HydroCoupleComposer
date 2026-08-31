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
    //! Nothing better fits; the raw JSON pane is the editor.
    Raw
  };

  /*!
   * \brief Everything an editor needs about one argument.
   */
  struct ArgumentDescriptor
  {
      QString id;
      QString caption;
      QString description;

      ArgumentEditorKind kind = ArgumentEditorKind::Raw;

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
   * \brief Points an argument at a file and lets it read itself.
   *
   * A path is not a payload: the composition document's only channel to an
   * argument is `initialize(..., JSON, ...)`, so a path recorded there would
   * be handed back to the component as JSON and rejected. A chooser therefore
   * loads the file through the component now, and what gets recorded is the
   * values the component read out of it.
   *
   * \param argument The argument to load; must not be null.
   * \param path The file to read.
   * \param[out] message Diagnostic when the component cannot read it.
   * \returns true when the component read the file.
   */
  [[nodiscard]] bool writeArgumentFile(HydroCouple::IArgument *argument,
                                       const QString &path, QString &message);

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
