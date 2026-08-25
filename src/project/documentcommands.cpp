#include "project/documentcommands.h"

namespace HydroCouple::Composer
{

  // ── SpecChangeCommand ──────────────────────────────────────────────────

  SpecChangeCommand::SpecChangeCommand(
    CompositionDocument *document,
    CompositionDocument::CompositionSpec before,
    CompositionDocument::CompositionSpec after, const QString &text)
    : QUndoCommand(text),
      m_document(document),
      m_before(std::move(before)),
      m_after(std::move(after))
  {
  }

  void SpecChangeCommand::undo()
  {
    m_document->applySpec(m_before);
  }

  void SpecChangeCommand::redo()
  {
    m_document->applySpec(m_after);
  }

  // ── AddComponentCommand ────────────────────────────────────────────────

  AddComponentCommand::AddComponentCommand(
    CompositionDocument *document,
    CompositionDocument::CompositionSpec before,
    CompositionDocument::CompositionSpec after, QString componentId,
    ComponentPresentation placement)
    : SpecChangeCommand(document, std::move(before), std::move(after),
                        QObject::tr("Add component '%1'").arg(componentId)),
      m_document(document),
      m_componentId(std::move(componentId)),
      m_placement(placement)
  {
  }

  void AddComponentCommand::undo()
  {
    SpecChangeCommand::undo();
    m_document->applyComponentRemovedFromPresentation(m_componentId);
  }

  void AddComponentCommand::redo()
  {
    SpecChangeCommand::redo();
    m_document->applyComponentPlacement(m_componentId, m_placement);
  }

  // ── RemoveComponentCommand ─────────────────────────────────────────────

  RemoveComponentCommand::RemoveComponentCommand(
    CompositionDocument *document,
    CompositionDocument::CompositionSpec before,
    CompositionDocument::CompositionSpec after, QString componentId,
    ComponentPresentation placement, bool hadPlacement)
    : SpecChangeCommand(document, std::move(before), std::move(after),
                        QObject::tr("Remove component '%1'").arg(componentId)),
      m_document(document),
      m_componentId(std::move(componentId)),
      m_placement(placement),
      m_hadPlacement(hadPlacement)
  {
  }

  void RemoveComponentCommand::undo()
  {
    SpecChangeCommand::undo();

    if (m_hadPlacement)
    {
      m_document->applyComponentPlacement(m_componentId, m_placement);
    }
  }

  void RemoveComponentCommand::redo()
  {
    SpecChangeCommand::redo();
    m_document->applyComponentRemovedFromPresentation(m_componentId);
  }

  // ── MoveComponentCommand ───────────────────────────────────────────────

  MoveComponentCommand::MoveComponentCommand(CompositionDocument *document,
                                             QString componentId,
                                             QPointF before, QPointF after)
    : QUndoCommand(QObject::tr("Move component '%1'").arg(componentId)),
      m_document(document),
      m_componentId(std::move(componentId)),
      m_before(before),
      m_after(after)
  {
  }

  void MoveComponentCommand::undo()
  {
    m_document->applyComponentPlacement(m_componentId, {m_before});
  }

  void MoveComponentCommand::redo()
  {
    m_document->applyComponentPlacement(m_componentId, {m_after});
  }

  int MoveComponentCommand::id() const
  {
    return Id;
  }

  bool MoveComponentCommand::mergeWith(const QUndoCommand *other)
  {
    const auto *move = static_cast<const MoveComponentCommand *>(other);

    if (move->m_componentId != m_componentId)
    {
      return false;
    }

    m_after = move->m_after;
    return true;
  }

} // namespace HydroCouple::Composer
