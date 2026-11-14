#include "ui/dialogs/rawargumentdialog.h"

#include <QFontDatabase>
#include <QPlainTextEdit>

namespace HydroCouple::Composer
{
  RawArgumentDialog::RawArgumentDialog(ArgumentDescriptor descriptor,
                                       QWidget *parent)
    : ArgumentEditorDialog(std::move(descriptor), parent)
  {
    m_editor = new QPlainTextEdit(this);
    m_editor->setObjectName(QStringLiteral("rawPayload"));

    // A fixed-width font, because this is the one editor where the shape
    // of the text carries meaning: a misplaced brace is found by looking
    // down a column.
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    setEditor(m_editor);
    hydrateFromDescriptor();
  }

  void RawArgumentDialog::hydrate(const nlohmann::json &payload)
  {
    m_opened = payload;
    m_editor->setPlainText(
      QString::fromStdString(payload.dump(2)));
  }

  QString RawArgumentDialog::text() const
  {
    return m_editor->toPlainText();
  }

  void RawArgumentDialog::setText(const QString &text)
  {
    m_editor->setPlainText(text);
  }

  bool RawArgumentDialog::validate(QString &message) const
  {
    const nlohmann::json parsed =
      nlohmann::json::parse(text().toStdString(), nullptr, false);

    if (parsed.is_discarded())
    {
      message = tr("This is not valid JSON.");

      return false;
    }

    return true;
  }

  nlohmann::json RawArgumentDialog::payload() const
  {
    // Parsed without throwing. The alternative — letting the exception
    // out of payload() — would make every caller of a *const accessor*
    // handle an exception, including the base's apply(), which has a
    // refusal strip precisely so that it does not have to.
    nlohmann::json parsed =
      nlohmann::json::parse(text().toStdString(), nullptr, false);

    if (parsed.is_discarded())
    {
      // What the window opened with, so a caller that ignores the
      // refusal cannot accidentally write nonsense. validate() refuses
      // before apply() ever gets here; this is the belt to its braces.
      return m_opened;
    }

    return parsed;
  }

} // namespace HydroCouple::Composer
