/*!
 * \file   compositiondocument.h
 * \author Caleb Buahin
 * \brief  CompositionDocument — the single source of truth for one composition.
 *
 * Everything that edits a composition — the canvas, the argument editors, the
 * property panels, and (later) Python scripts — mutates this one object
 * through commands on its undo stack. That is what keeps several views of the
 * same data synchronised: a view never holds its own copy, it observes the
 * document's change signals and re-reads.
 *
 * The document owns two artefacts that travel together but are stored apart:
 *
 *  - the **composition**, a Composition Specification v1 document owned by the
 *    SDK (`HydroCouple::SDK::IO::CompositionSpec`), which every other host can
 *    read; and
 *  - the **presentation** sidecar, this program's private view state.
 *
 * Mutating methods return the command they pushed (or nullptr when the edit
 * was rejected), so callers can compose larger operations in a macro.
 */

#ifndef HYDROCOUPLECOMPOSER_PROJECT_COMPOSITIONDOCUMENT_H
#define HYDROCOUPLECOMPOSER_PROJECT_COMPOSITIONDOCUMENT_H

#include "project/presentation.h"

#include "hydrocouplesdk/io/compositionspec.h"

#include <QObject>
#include <QString>

#include <optional>

class QUndoStack;

namespace HydroCouple::Composer
{

  /*!
   * \brief One editable composition, with undo and change notification.
   */
  class CompositionDocument : public QObject
  {
      Q_OBJECT

    public:
      using CompositionSpec = HydroCouple::SDK::IO::CompositionSpec;
      using ComponentSpec = HydroCouple::SDK::IO::ComponentSpec;
      using ConnectionSpec = HydroCouple::SDK::IO::ConnectionSpec;

      explicit CompositionDocument(QObject *parent = nullptr);

      ~CompositionDocument() override;

      // ── Document lifecycle ──────────────────────────────────────────────

      /*!
       * \brief Discards all content and returns to an untitled document.
       */
      void clear();

      /*!
       * \brief Loads a composition document and its sidecar, if present.
       * \param filePath Composition document to read.
       * \param[out] message Actionable diagnostic on failure.
       * \returns false when the file cannot be read or does not validate. The
       *          document is left untouched in that case — never half-applied.
       */
      [[nodiscard]] bool load(const QString &filePath, QString &message);

      /*!
       * \brief Parses composition JSON text without touching the filesystem.
       * \param json Composition document text.
       * \param[out] message Actionable diagnostic on failure.
       */
      [[nodiscard]] bool loadFromJson(const QByteArray &json, QString &message);

      /*!
       * \brief Writes the composition and, when non-empty, its sidecar.
       * \param filePath Destination; empty means "the current file".
       * \param[out] message Actionable diagnostic on failure.
       */
      [[nodiscard]] bool save(const QString &filePath, QString &message);

      /*!
       * \brief The composition document text this document would write.
       */
      [[nodiscard]] QByteArray toJson() const;

      // ── State ───────────────────────────────────────────────────────────

      [[nodiscard]] QString filePath() const;

      [[nodiscard]] bool isModified() const;

      [[nodiscard]] QUndoStack *undoStack() const;

      [[nodiscard]] const CompositionSpec &spec() const;

      [[nodiscard]] Presentation &presentation();
      [[nodiscard]] const Presentation &presentation() const;

      // ── Queries ─────────────────────────────────────────────────────────

      [[nodiscard]] QStringList componentIds() const;

      /*!
       * \brief Replaces the orchestration block, undoably.
       * \param workflow The workflow configuration to record.
       */
      bool setWorkflow(const HydroCouple::SDK::IO::WorkflowSpec &workflow);

      /*!
       * \brief Sets one component's execution mode, undoably.
       *
       * Mode "open" requires \a resultsManifest — the spec refuses an open
       * block with nothing to open.
       *
       * \param componentId Component whose execution changes.
       * \param mode run, open, or resume.
       * \param resultsManifest Run manifest path for mode "open".
       */
      bool setComponentExecution(
        const QString &componentId, HydroCouple::SDK::IO::ExecutionMode mode,
        const QString &resultsManifest);

      [[nodiscard]] std::optional<ComponentSpec> component(
        const QString &componentId) const;

      [[nodiscard]] int connectionCount() const;

      // ── Edits (each pushes one undoable command) ─────────────────────────

      /*!
       * \brief Adds a component block.
       * \param component The component block to add.
       * \param placement Initial canvas placement for the new component.
       * \returns false when the id is empty or already present.
       */
      bool addComponent(const ComponentSpec &component,
                        const ComponentPresentation &placement = {});

      /*!
       * \brief Removes a component and every connection touching it.
       * \param componentId Identifier of the component to remove.
       */
      bool removeComponent(const QString &componentId);

      /*!
       * \brief Connects an output to an input.
       * \param connection The connection to add.
       * \returns false when either endpoint names an unknown component, or the
       *          identical connection already exists.
       */
      bool addConnection(const ConnectionSpec &connection);

      bool removeConnection(const ConnectionSpec &connection);

      // A connection's IDENTITY is its endpoints plus role — exactly what
      // sameConnection() compares. The adaptation chain is content: the
      // chain-editing calls below address a step as (identity, index).

      /*!
       * \brief Inserts one adaptation step into a connection's chain.
       * \param connection Names the connection (endpoints + role).
       * \param index Position in the chain, 0..size; the step before the
       *        consumer is the chain's last.
       * \param step The step to insert; its id must be non-empty (the spec
       *        refuses id-less steps at parse time).
       */
      bool insertConnectionAdapter(
        const ConnectionSpec &connection, int index,
        const HydroCouple::SDK::IO::AdaptedOutputSpec &step);

      /*!
       * \brief Removes one adaptation step; later steps renumber down.
       */
      bool removeConnectionAdapter(const ConnectionSpec &connection,
                                   int index);

      /*!
       * \brief Replaces one argument payload on a chain step.
       */
      bool setConnectionAdapterArgument(const ConnectionSpec &connection,
                                        int index, const QString &argumentId,
                                        const nlohmann::json &payload);

      /*!
       * \brief Changes a connection's provider role.
       * \returns false when the connection is unknown, the role is
       *          unchanged, or another connection already holds the
       *          resulting identity.
       */
      bool setConnectionRole(const ConnectionSpec &connection,
                             const QString &role);

      /*!
       * \brief Deletes one argument payload entirely (e.g. an \@from
       *        binding), restoring the component's own default.
       */
      bool clearArgument(const QString &componentId,
                         const QString &argumentId);

      /*!
       * \brief Replaces one argument payload on a component.
       * \param componentId Component owning the argument.
       * \param argumentId Argument to replace.
       * \param payload The argument's JSON value, as authored.
       */
      bool setArgument(const QString &componentId, const QString &argumentId,
                       const nlohmann::json &payload);

      /*!
       * \brief Moves a component on the canvas (presentation only).
       * \param componentId Component to move.
       * \param position New canvas position.
       */
      bool moveComponent(const QString &componentId, const QPointF &position);

      /*!
       * \brief Moves a spliced adapter node (presentation only).
       * \param connection The connection whose chain holds the step.
       * \param index Chain step; must exist in the connection's chain.
       * \param position New canvas position.
       */
      bool moveConnectionAdapter(const ConnectionSpec &connection, int index,
                                 const QPointF &position);

      // ── Command plumbing ────────────────────────────────────────────────
      //
      // Commands apply their effects through these; they are not the editing
      // API. Use the methods above.

      void applySpec(const CompositionSpec &spec);

      void applyComponentPlacement(const QString &componentId,
                                   const ComponentPresentation &placement);

      void applyComponentRemovedFromPresentation(const QString &componentId);

      void applyAdapterPlacement(const ConnectionSpec &connection, int index,
                                 const QPointF &position);

    Q_SIGNALS:
      //! Emitted whenever the composition's content changes in any way.
      void compositionChanged();

      //! Emitted when a component is added or removed.
      void componentsChanged();

      //! Emitted when connections change.
      void connectionsChanged();

      //! Emitted when a component's placement changes.
      void placementChanged(const QString &componentId);

      //! Emitted when a spliced adapter node's placement changes.
      void adapterPlacementChanged();

      void filePathChanged(const QString &filePath);

      void modifiedChanged(bool modified);

    private:
      void setFilePath(const QString &filePath);
      void refreshModified();

      CompositionSpec m_spec;
      Presentation m_presentation;
      QUndoStack *m_undoStack = nullptr;
      QString m_filePath;
      bool m_modified = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PROJECT_COMPOSITIONDOCUMENT_H
