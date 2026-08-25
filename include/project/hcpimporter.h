/*!
 * \file   hcpimporter.h
 * \author Caleb Buahin
 * \brief  HcpImporter — one-way import of HydroCouple 1.x `.hcp` projects.
 *
 * The v1 project format is XML and is not carried forward: Composition
 * Specification v1 replaced it, and the SDK's composition plan retired XML
 * deliberately. This importer exists so existing projects are not stranded,
 * and it is one-way by design — Composer never writes `.hcp`.
 *
 * \par What converts faithfully
 * Components (with their library and component-info references, captions and
 * descriptions), connections including adapted-output chains, canvas
 * positions, and the trigger component.
 *
 * \par What cannot convert, and is reported instead
 * **Argument payloads.** A v1 argument is opaque text in the owning
 * component's own dialect — either raw string content or a path whose file is
 * in that dialect. A v2 payload is JSON handed verbatim to
 * `IArgument::initialize()`, and what a component accepts is that component's
 * business. There is no general translation between the two, and inventing
 * one would produce documents that parse and then fail at run time. Every
 * argument therefore becomes a reported issue carrying its original value, so
 * it can be re-entered against the real component in the configurator.
 *
 * Workflow components and compute-resource allocations have no Composition
 * Spec v1 equivalent and are reported the same way. Nothing is dropped
 * silently.
 */

#ifndef HYDROCOUPLECOMPOSER_PROJECT_HCPIMPORTER_H
#define HYDROCOUPLECOMPOSER_PROJECT_HCPIMPORTER_H

#include "project/presentation.h"

#include "hydrocouplesdk/io/compositionspec.h"

#include <QList>
#include <QString>

namespace HydroCouple::Composer
{

  /*!
   * \brief Something the importer could not carry across, or had to decide.
   */
  struct ImportIssue
  {
      enum class Severity
      {
        //! Converted, but a choice was made the user should confirm.
        Info,
        //! Not converted; the original value is preserved in \c detail.
        Warning,
        //! The document is malformed at this point.
        Error
      };

      Severity severity = Severity::Warning;
      //! Where in the document, e.g. "ModelComponent[1] 'Main Channel'".
      QString location;
      QString message;
      //! The original value, when something could not be carried across.
      QString detail;

      [[nodiscard]] QString toString() const;
  };

  /*!
   * \brief The outcome of importing one `.hcp` file.
   */
  struct ImportResult
  {
      bool succeeded = false;
      HydroCouple::SDK::IO::CompositionSpec spec;
      Presentation presentation;
      QList<ImportIssue> issues;

      /*!
       * \brief Issues at or above \a severity.
       */
      [[nodiscard]] QList<ImportIssue> issuesOfAtLeast(
        ImportIssue::Severity severity) const;

      [[nodiscard]] bool hasErrors() const;
  };

  /*!
   * \brief Reads a HydroCouple 1.x project into a v2 composition.
   */
  class HcpImporter
  {
    public:
      /*!
       * \brief Imports the `.hcp` document at \a filePath.
       *
       * A result may succeed while carrying warnings — that is the normal
       * case, because arguments never convert.
       */
      [[nodiscard]] static ImportResult importFile(const QString &filePath);

      /*!
       * \brief Imports `.hcp` XML text directly.
       * \param xml The document text.
       * \param documentPath Path used to resolve relative library references;
       *        may be empty.
       */
      [[nodiscard]] static ImportResult importXml(const QByteArray &xml,
                                                  const QString &documentPath);
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PROJECT_HCPIMPORTER_H
