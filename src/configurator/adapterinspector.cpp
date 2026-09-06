#include "configurator/adapterinspector.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{

  namespace
  {
    bool sameIdentity(const HydroCouple::SDK::IO::ConnectionSpec &lhs,
                      const HydroCouple::SDK::IO::ConnectionSpec &rhs)
    {
      return lhs.fromComponent == rhs.fromComponent &&
             lhs.output == rhs.output && lhs.toComponent == rhs.toComponent &&
             lhs.input == rhs.input && lhs.role == rhs.role;
    }
  } // namespace

  AdapterInspector::AdapterInspector(CompositionDocument *document,
                                     QWidget *parent)
    : QWidget(parent),
      m_document(document)
  {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);

    m_title = new QLabel(tr("Nothing selected."), this);
    m_title->setObjectName(QStringLiteral("adapterInspectorTitle"));
    m_title->setWordWrap(true);
    m_layout->addWidget(m_title);
    m_layout->addStretch(1);

    if (m_document)
    {
      // The address survives edits; the content is re-read from the
      // document each time it changes, whichever view changed it.
      connect(m_document, &CompositionDocument::connectionsChanged, this,
              &AdapterInspector::refresh);
      connect(m_document, &CompositionDocument::compositionChanged, this,
              &AdapterInspector::refresh);
    }
  }

  void AdapterInspector::setChainStep(
    const HydroCouple::SDK::IO::ConnectionSpec &connection, int stepIndex)
  {
    m_hasAddress = true;
    m_stepMode = true;
    m_identity = connection;
    m_stepIndex = stepIndex;
    refresh();
  }

  void AdapterInspector::showConnection(
    const HydroCouple::SDK::IO::ConnectionSpec &connection)
  {
    m_hasAddress = true;
    m_stepMode = false;
    m_identity = connection;
    m_stepIndex = 0;
    refresh();
  }

  void AdapterInspector::clearSelection()
  {
    m_hasAddress = false;
    refresh();
  }

  const HydroCouple::SDK::IO::ConnectionSpec *
  AdapterInspector::findConnection() const
  {
    if (!m_document)
    {
      return nullptr;
    }

    for (const HydroCouple::SDK::IO::ConnectionSpec &link :
         m_document->spec().connections)
    {
      if (sameIdentity(link, m_identity))
      {
        return &link;
      }
    }

    return nullptr;
  }

  void AdapterInspector::refresh()
  {
    delete m_rows;
    m_rows = nullptr;

    if (!m_hasAddress)
    {
      m_title->setText(tr("Nothing selected."));
      return;
    }

    const HydroCouple::SDK::IO::ConnectionSpec *link = findConnection();

    if (!link ||
        (m_stepMode &&
         (m_stepIndex < 0 ||
          m_stepIndex >= static_cast<int>(link->adaptedOutputs.size()))))
    {
      // The address no longer resolves — the connection or the step was
      // removed from under the panel. Empty beats stale.
      m_hasAddress = false;
      m_title->setText(tr("Nothing selected."));
      return;
    }

    m_rows = new QWidget(this);
    auto *form = new QFormLayout(m_rows);
    form->setContentsMargins(0, 0, 0, 0);

    const QString endpoints =
      QStringLiteral("%1.%2 → %3.%4")
        .arg(QString::fromStdString(link->fromComponent),
             QString::fromStdString(link->output),
             QString::fromStdString(link->toComponent),
             QString::fromStdString(link->input));

    if (m_stepMode)
    {
      const HydroCouple::SDK::IO::AdaptedOutputSpec &step =
        link->adaptedOutputs[static_cast<size_t>(m_stepIndex)];

      m_title->setText(
        tr("Adapter '%1'%2 — step %3 of %4 on %5")
          .arg(QString::fromStdString(step.id),
               step.factory.empty()
                 ? QString()
                 : tr(" (%1)").arg(QString::fromStdString(step.factory)))
          .arg(m_stepIndex + 1)
          .arg(link->adaptedOutputs.size())
          .arg(endpoints));

      // One row per argument payload, edited as compact JSON. The document
      // is the single source of truth: an accepted edit goes through the
      // undo stack, and this panel re-reads it like every other view.
      const HydroCouple::SDK::IO::ConnectionSpec identity = m_identity;
      const int stepIndex = m_stepIndex;

      for (const auto &[key, payload] : step.arguments.items())
      {
        auto *editor = new QLineEdit(
          QString::fromStdString(payload.dump()), m_rows);
        editor->setObjectName(QStringLiteral("adapter_argument_%1")
                                .arg(QString::fromStdString(key)));

        const QString argumentId = QString::fromStdString(key);
        connect(editor, &QLineEdit::editingFinished, this,
                [this, editor, identity, stepIndex, argumentId]
                {
                  try
                  {
                    const nlohmann::json payload =
                      nlohmann::json::parse(editor->text().toStdString());
                    editor->setStyleSheet(QString());
                    m_document->setConnectionAdapterArgument(
                      identity, stepIndex, argumentId, payload);
                  }
                  catch (const nlohmann::json::exception &error)
                  {
                    // Refused, visibly: the document never learns about
                    // JSON that does not parse.
                    editor->setStyleSheet(
                      QStringLiteral("border: 1px solid #c04040;"));
                    editor->setToolTip(QString::fromUtf8(error.what()));
                  }
                });

        form->addRow(argumentId, editor);
      }

      if (step.arguments.empty())
      {
        form->addRow(new QLabel(tr("This adapter takes no arguments."),
                                m_rows));
      }
    }
    else
    {
      m_title->setText(
        link->role.empty()
          ? tr("Connection %1").arg(endpoints)
          : tr("Connection %1 (role '%2')")
              .arg(endpoints, QString::fromStdString(link->role)));

      if (link->adaptedOutputs.empty())
      {
        form->addRow(new QLabel(tr("Direct — no adapters."), m_rows));
      }
      else
      {
        int position = 1;
        for (const HydroCouple::SDK::IO::AdaptedOutputSpec &step :
             link->adaptedOutputs)
        {
          form->addRow(
            tr("Step %1").arg(position++),
            new QLabel(QString::fromStdString(step.id), m_rows));
        }
      }
    }

    m_layout->insertWidget(1, m_rows);
  }

} // namespace HydroCouple::Composer
