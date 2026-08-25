/*!
 * \file   documentcommands.h
 * \author Caleb Buahin
 * \brief  Undoable edits to a CompositionDocument.
 *
 * Every command works the same way: it captures the whole composition
 * specification before and after its edit and swaps between them. That is
 * deliberately coarse. A composition is a small document — tens of components,
 * not millions of cells — and whole-spec snapshots make undo exactly correct
 * for edits with wide effects, such as removing a component that several
 * connections referenced. Fine-grained inverse operations would have to
 * reconstruct that collateral by hand, which is precisely where undo bugs
 * live.
 *
 * Presentation-only edits are separate commands, so moving a box on the canvas
 * does not mark the composition itself dirty.
 */

#ifndef HYDROCOUPLECOMPOSER_PROJECT_DOCUMENTCOMMANDS_H
#define HYDROCOUPLECOMPOSER_PROJECT_DOCUMENTCOMMANDS_H

#include "project/compositiondocument.h"

#include <QPointF>
#include <QString>
#include <QUndoCommand>

namespace HydroCouple::Composer
{

  /*!
   * \brief Swaps the document's specification between two whole states.
   */
  class SpecChangeCommand : public QUndoCommand
  {
    public:
      SpecChangeCommand(CompositionDocument *document,
                        CompositionDocument::CompositionSpec before,
                        CompositionDocument::CompositionSpec after,
                        const QString &text);

      void undo() override;
      void redo() override;

    private:
      CompositionDocument *m_document;
      CompositionDocument::CompositionSpec m_before;
      CompositionDocument::CompositionSpec m_after;
  };

  /*!
   * \brief Adds a component, together with its initial canvas placement.
   */
  class AddComponentCommand : public SpecChangeCommand
  {
    public:
      AddComponentCommand(CompositionDocument *document,
                          CompositionDocument::CompositionSpec before,
                          CompositionDocument::CompositionSpec after,
                          QString componentId,
                          ComponentPresentation placement);

      void undo() override;
      void redo() override;

    private:
      CompositionDocument *m_document;
      QString m_componentId;
      ComponentPresentation m_placement;
  };

  /*!
   * \brief Removes a component, restoring its placement on undo.
   */
  class RemoveComponentCommand : public SpecChangeCommand
  {
    public:
      RemoveComponentCommand(CompositionDocument *document,
                             CompositionDocument::CompositionSpec before,
                             CompositionDocument::CompositionSpec after,
                             QString componentId,
                             ComponentPresentation placement,
                             bool hadPlacement);

      void undo() override;
      void redo() override;

    private:
      CompositionDocument *m_document;
      QString m_componentId;
      ComponentPresentation m_placement;
      bool m_hadPlacement;
  };

  /*!
   * \brief Moves a component on the canvas. Presentation only.
   *
   * Consecutive moves of one component merge, so a drag is a single undo step
   * rather than one per mouse-move event.
   */
  class MoveComponentCommand : public QUndoCommand
  {
    public:
      enum
      {
        Id = 0x43'4d'56'01
      };

      MoveComponentCommand(CompositionDocument *document, QString componentId,
                           QPointF before, QPointF after);

      void undo() override;
      void redo() override;

      [[nodiscard]] int id() const override;

      bool mergeWith(const QUndoCommand *other) override;

    private:
      CompositionDocument *m_document;
      QString m_componentId;
      QPointF m_before;
      QPointF m_after;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PROJECT_DOCUMENTCOMMANDS_H
