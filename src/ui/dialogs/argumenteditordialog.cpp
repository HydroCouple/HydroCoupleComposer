#include "ui/dialogs/argumenteditordialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{
  ArgumentEditorDialog::ArgumentEditorDialog(ArgumentDescriptor descriptor,
                                             QWidget *parent)
    : QDialog(parent), m_descriptor(std::move(descriptor))
  {
    setObjectName(QStringLiteral("argumentDialog_") + m_descriptor.id);
    setWindowTitle(m_descriptor.caption);

    // Modeless, and it must be deleted when it closes: these are opened
    // from a dock row that may itself go away when the user selects a
    // different component, and a window outliving the argument it edits
    // would offer payloads for something no longer there.
    setAttribute(Qt::WA_DeleteOnClose);

    auto *root = new QVBoxLayout(this);

    auto *caption = new QLabel(m_descriptor.caption, this);
    caption->setObjectName(QStringLiteral("argumentCaption"));

    QFont heading = caption->font();
    heading.setBold(true);
    caption->setFont(heading);
    root->addWidget(caption);

    if (!m_descriptor.description.isEmpty())
    {
      auto *description = new QLabel(m_descriptor.description, this);
      description->setObjectName(QStringLiteral("argumentDescription"));
      description->setWordWrap(true);
      root->addWidget(description);
    }

    m_body = new QVBoxLayout;
    root->addLayout(m_body, 1);

    // The strip is built empty and kept, rather than shown and hidden: a
    // strip that appears resizes the window under the user's cursor at
    // the exact moment they are reading why their value was refused.
    m_strip = new QLabel(this);
    m_strip->setObjectName(QStringLiteral("argumentRefusal"));
    m_strip->setWordWrap(true);
    m_strip->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_strip);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Apply
                                       | QDialogButtonBox::Cancel
                                       | QDialogButtonBox::Ok,
                                     this);
    m_buttons->setObjectName(QStringLiteral("argumentButtons"));
    root->addWidget(m_buttons);

    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, [this] { apply(); });

    connect(m_buttons, &QDialogButtonBox::accepted, this,
            [this]
            {
              // OK is Apply and then close — but only closes if the
              // component took it. A window that shut on a refusal would
              // throw the user's work away and leave the message it was
              // about on a strip nobody can still see.
              if (apply())
              {
                accept();
              }
            });

    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  }

  ArgumentEditorDialog::~ArgumentEditorDialog() = default;

  const ArgumentDescriptor &ArgumentEditorDialog::descriptor() const
  {
    return m_descriptor;
  }

  void ArgumentEditorDialog::setCommitter(ArgumentCommitter committer)
  {
    m_committer = std::move(committer);
  }

  void ArgumentEditorDialog::setEditor(QWidget *editor)
  {
    if (!editor || m_editor)
    {
      return;
    }

    m_editor = editor;
    m_body->addWidget(editor, 1);
  }

  void ArgumentEditorDialog::hydrateFromDescriptor()
  {
    hydrate(m_descriptor.payload);
  }

  bool ArgumentEditorDialog::apply()
  {
    if (!m_committer)
    {
      // Said rather than silently doing nothing. A dialog with nothing
      // listening is a bug in the caller, and a dead Apply button that
      // reports success is the hardest kind to notice.
      showRefusal(tr("This editor is not connected to its component."));

      return false;
    }

    QString message;

    // The dialog's own objection first. Handing text that is not JSON to
    // the component so that *it* can refuse would report a typing
    // mistake as a modelling one.
    if (!validate(message))
    {
      showRefusal(message.isEmpty() ? tr("This value is not usable.")
                                    : message);

      return false;
    }

    const nlohmann::json offered = payload();

    if (!m_committer(m_descriptor.id, offered, message))
    {
      showRefusal(message.isEmpty()
                    ? tr("The component refused this value.")
                    : message);

      return false;
    }

    clearRefusal();

    // The accepted payload becomes the baseline, so a later Cancel is
    // measured against what the component actually holds rather than
    // against what it held when the window opened.
    m_descriptor.payload = offered;
    m_committed = true;

    return true;
  }

  bool ArgumentEditorDialog::validate(QString &message) const
  {
    Q_UNUSED(message)

    return true;
  }

  QString ArgumentEditorDialog::refusal() const
  {
    return m_strip->text();
  }

  bool ArgumentEditorDialog::hasCommitted() const
  {
    return m_committed;
  }

  void ArgumentEditorDialog::showRefusal(const QString &message)
  {
    m_strip->setText(message);
  }

  void ArgumentEditorDialog::clearRefusal()
  {
    m_strip->clear();
  }

} // namespace HydroCouple::Composer
