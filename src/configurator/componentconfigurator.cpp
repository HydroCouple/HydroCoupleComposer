#include "configurator/componentconfigurator.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{

  namespace
  {
    //! The value slot of a payload, whichever shape the component used.
    const nlohmann::json &valuesOf(const nlohmann::json &payload)
    {
      static const nlohmann::json empty = nlohmann::json::array();

      if (payload.is_object() && payload.contains("values"))
      {
        return payload["values"];
      }

      return payload.is_array() ? payload : empty;
    }

    //! Rewraps edited values the way the component serialised them.
    nlohmann::json withValues(const nlohmann::json &original,
                              nlohmann::json values)
    {
      if (original.is_object() && original.contains("values"))
      {
        nlohmann::json updated = original;
        updated["values"] = std::move(values);
        return updated;
      }

      return values;
    }

    nlohmann::json firstValue(const nlohmann::json &payload)
    {
      const nlohmann::json &values = valuesOf(payload);

      if (values.is_array() && !values.empty())
      {
        return values.front();
      }

      return values.is_array() ? nlohmann::json() : values;
    }

    nlohmann::json scalarPayload(const nlohmann::json &original,
                                 nlohmann::json value)
    {
      const nlohmann::json &values = valuesOf(original);

      if (values.is_array())
      {
        return withValues(original, nlohmann::json::array({std::move(value)}));
      }

      return withValues(original, std::move(value));
    }
  } // namespace

  ComponentConfigurator::ComponentConfigurator(CompositionDocument *document,
                                               ComponentInstances *instances,
                                               QWidget *parent)
    : QWidget(parent),
      m_document(document),
      m_instances(instances)
  {
    setObjectName(QStringLiteral("componentConfigurator"));

    auto *layout = new QVBoxLayout(this);

    m_form = new QFormLayout;
    m_form->setObjectName(QStringLiteral("argumentForm"));
    layout->addLayout(m_form);

    m_componentEditorButton =
      new QPushButton(tr("Open the component's own editor…"), this);
    m_componentEditorButton->setObjectName(QStringLiteral("componentEditorButton"));
    m_componentEditorButton->setVisible(false);
    layout->addWidget(m_componentEditorButton);

    connect(m_componentEditorButton, &QPushButton::clicked, this,
            [this]
            {
              // Fired from the button's clicked() — a release, never a press.
              // Opening a modal editor from a mouse press wedges input
              // handling on macOS.
              showComponentEditor();
            });

    layout->addWidget(new QLabel(tr("Raw arguments (JSON)"), this));

    m_rawPane = new QPlainTextEdit(this);
    m_rawPane->setObjectName(QStringLiteral("rawArgumentsPane"));
    layout->addWidget(m_rawPane, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("configuratorStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    if (m_document)
    {
      connect(m_document, &CompositionDocument::compositionChanged, this,
              [this] { refreshRawPane(); });
    }
  }

  ComponentConfigurator::~ComponentConfigurator() = default;

  QString ComponentConfigurator::componentId() const
  {
    return m_componentId;
  }

  QList<ArgumentDescriptor> ComponentConfigurator::descriptors() const
  {
    return m_descriptors;
  }

  HydroCouple::IArgument *ComponentConfigurator::argument(
    const QString &argumentId) const
  {
    if (!m_instances || m_componentId.isEmpty())
    {
      return nullptr;
    }

    HydroCouple::IModelComponent *component =
      m_instances->instance(m_componentId);

    if (!component)
    {
      return nullptr;
    }

    for (HydroCouple::IArgument *candidate : component->arguments())
    {
      if (candidate &&
          QString::fromStdString(candidate->id()) == argumentId)
      {
        return candidate;
      }
    }

    return nullptr;
  }

  void ComponentConfigurator::setComponent(const QString &componentId)
  {
    m_componentId = componentId;
    rebuild();
  }

  void ComponentConfigurator::rebuild()
  {
    // QFormLayout::takeAt() warns on an invalid index rather than simply
    // returning nullptr, so the loop is driven by count(). Rows own both a
    // label and a field widget; removeRow deletes both.
    while (m_form->rowCount() > 0)
    {
      m_form->removeRow(0);
    }

    m_descriptors.clear();
    m_status->clear();

    if (!m_instances || m_componentId.isEmpty())
    {
      m_componentEditorButton->setVisible(false);
      m_rawPane->clear();
      return;
    }

    HydroCouple::IModelComponent *component =
      m_instances->instance(m_componentId);

    if (!component)
    {
      m_componentEditorButton->setVisible(false);
      m_rawPane->clear();
      m_status->setText(tr("This component could not be loaded: %1")
                          .arg(m_instances->failure(m_componentId)));
      return;
    }

    m_componentEditorButton->setVisible(hasComponentEditor());

    for (HydroCouple::IArgument *argument : component->arguments())
    {
      if (!argument)
      {
        continue;
      }

      const ArgumentDescriptor descriptor = describeArgument(argument);
      m_descriptors.append(descriptor);

      QWidget *editor = nullptr;

      switch (descriptor.kind)
      {
        case ArgumentEditorKind::Categorical:
        {
          auto *combo = new QComboBox(this);
          combo->addItems(descriptor.categories);

          const nlohmann::json current = firstValue(descriptor.payload);

          if (current.is_string())
          {
            combo->setCurrentText(
              QString::fromStdString(current.get<std::string>()));
          }

          connect(combo, &QComboBox::currentTextChanged, this,
                  [this, id = descriptor.id, payload = descriptor.payload](
                    const QString &text)
                  {
                    QString message;
                    applyArgument(
                      id, scalarPayload(payload, text.toStdString()), message);
                  });

          editor = combo;
          break;
        }

        case ArgumentEditorKind::Integer:
        {
          auto *spin = new QSpinBox(this);
          spin->setRange(std::numeric_limits<int>::lowest() / 2,
                         std::numeric_limits<int>::max() / 2);

          const nlohmann::json current = firstValue(descriptor.payload);

          if (current.is_number())
          {
            spin->setValue(current.get<int>());
          }

          connect(spin, &QSpinBox::valueChanged, this,
                  [this, id = descriptor.id, payload = descriptor.payload](
                    int value)
                  {
                    QString message;
                    applyArgument(id, scalarPayload(payload, value), message);
                  });

          editor = spin;
          break;
        }

        case ArgumentEditorKind::Number:
        {
          auto *spin = new QDoubleSpinBox(this);
          spin->setDecimals(6);
          spin->setRange(-1e12, 1e12);

          const nlohmann::json current = firstValue(descriptor.payload);

          if (current.is_number())
          {
            spin->setValue(current.get<double>());
          }

          connect(spin, &QDoubleSpinBox::valueChanged, this,
                  [this, id = descriptor.id, payload = descriptor.payload](
                    double value)
                  {
                    QString message;
                    applyArgument(id, scalarPayload(payload, value), message);
                  });

          editor = spin;
          break;
        }

        case ArgumentEditorKind::Boolean:
        {
          auto *check = new QCheckBox(this);
          const nlohmann::json current = firstValue(descriptor.payload);

          if (current.is_boolean())
          {
            check->setChecked(current.get<bool>());
          }

          connect(check, &QCheckBox::toggled, this,
                  [this, id = descriptor.id, payload = descriptor.payload](
                    bool value)
                  {
                    QString message;
                    applyArgument(id, scalarPayload(payload, value), message);
                  });

          editor = check;
          break;
        }

        case ArgumentEditorKind::FilePath:
        {
          // A path box on its own would be a text box that happens to hold a
          // path; the filters the argument advertises are only worth anything
          // through a dialog that uses them.
          auto *chooser = new QWidget(this);
          auto *row = new QHBoxLayout(chooser);
          row->setContentsMargins(0, 0, 0, 0);

          auto *line = new QLineEdit(chooser);
          line->setObjectName(QStringLiteral("argument_") + descriptor.id +
                              QStringLiteral("_path"));
          line->setPlaceholderText(tr("Choose a file…"));

          auto *browse = new QPushButton(tr("Browse…"), chooser);
          browse->setObjectName(QStringLiteral("argument_") + descriptor.id +
                                QStringLiteral("_browse"));

          row->addWidget(line, 1);
          row->addWidget(browse);

          connect(line, &QLineEdit::editingFinished, this,
                  [this, line, id = descriptor.id]
                  {
                    if (line->text().isEmpty())
                    {
                      return;
                    }

                    QString message;
                    applyArgumentFile(id, line->text(), message);
                  });

          connect(browse, &QPushButton::clicked, this,
                  [this, line, id = descriptor.id,
                   filters = descriptor.fileFilters]
                  {
                    const QString chosen = QFileDialog::getOpenFileName(
                      this, tr("Choose a file for '%1'").arg(id), line->text(),
                      fileDialogFilter(filters));

                    if (chosen.isEmpty())
                    {
                      return;
                    }

                    line->setText(chosen);

                    QString message;
                    applyArgumentFile(id, chosen, message);
                  });

          editor = chooser;
          break;
        }

        case ArgumentEditorKind::Text:
        {
          auto *line = new QLineEdit(this);
          const nlohmann::json current = firstValue(descriptor.payload);

          if (current.is_string())
          {
            line->setText(QString::fromStdString(current.get<std::string>()));
          }

          connect(line, &QLineEdit::editingFinished, this,
                  [this, line, id = descriptor.id,
                   payload = descriptor.payload]
                  {
                    QString message;
                    applyArgument(
                      id, scalarPayload(payload, line->text().toStdString()),
                      message);
                  });

          editor = line;
          break;
        }

        case ArgumentEditorKind::Table:
        {
          auto *table = new QTableWidget(descriptor.rows, descriptor.columns,
                                         this);
          const nlohmann::json &values = valuesOf(descriptor.payload);

          // A rank-2 argument serialises its values as an array of rows, not
          // as one flattened run; a rank-1 argument is flat. Read whichever
          // the component actually produced rather than assuming.
          const bool nested = values.is_array() && !values.empty() &&
                              values.front().is_array();

          for (int row = 0; row < descriptor.rows; ++row)
          {
            for (int column = 0; column < descriptor.columns; ++column)
            {
              QString text;

              if (nested)
              {
                const size_t r = static_cast<size_t>(row);

                if (r < values.size() && values[r].is_array() &&
                    static_cast<size_t>(column) < values[r].size())
                {
                  text = QString::fromStdString(values[r][column].dump());
                }
              }
              else if (values.is_array())
              {
                const size_t flat =
                  static_cast<size_t>(row) *
                    static_cast<size_t>(descriptor.columns) +
                  static_cast<size_t>(column);

                if (flat < values.size())
                {
                  text = QString::fromStdString(values[flat].dump());
                }
              }

              table->setItem(row, column, new QTableWidgetItem(text));
            }
          }

          editor = table;
          break;
        }

        case ArgumentEditorKind::Raw:
          // Handled by the raw pane; no generated widget claims to know better.
          break;
      }

      if (!editor)
      {
        continue;
      }

      editor->setObjectName(QStringLiteral("argument_") + descriptor.id);
      editor->setEnabled(!descriptor.isReadOnly);
      editor->setToolTip(descriptor.description);

      QString label = descriptor.caption;

      if (!descriptor.unit.isEmpty())
      {
        label += QStringLiteral(" (%1)").arg(descriptor.unit);
      }

      if (!descriptor.isOptional)
      {
        label += QStringLiteral(" *");
      }

      m_form->addRow(label, editor);
    }

    refreshRawPane();
  }

  void ComponentConfigurator::refreshRawPane()
  {
    if (m_componentId.isEmpty() || !m_document)
    {
      return;
    }

    const std::optional<CompositionDocument::ComponentSpec> spec =
      m_document->component(m_componentId);

    if (!spec)
    {
      return;
    }

    const QString text =
      QString::fromStdString(spec->arguments.dump(2));

    if (m_rawPane->toPlainText() != text)
    {
      m_rawPane->setPlainText(text);
    }
  }

  bool ComponentConfigurator::applyArgument(const QString &argumentId,
                                            const nlohmann::json &payload,
                                            QString &message)
  {
    HydroCouple::IArgument *target = argument(argumentId);

    if (!target)
    {
      message = tr("no argument '%1' on '%2'").arg(argumentId, m_componentId);
      m_status->setText(message);
      return false;
    }

    // Offer it to the component first. A payload the model refuses must never
    // reach the document, or the document would hold a value that cannot run.
    if (!writeArgumentPayload(target, payload, message))
    {
      m_status->setText(tr("'%1' rejected: %2").arg(argumentId, message));
      return false;
    }

    if (!m_document->setArgument(m_componentId, argumentId, payload))
    {
      message = tr("could not record '%1'").arg(argumentId);
      m_status->setText(message);
      return false;
    }

    m_status->clear();
    Q_EMIT argumentChanged(m_componentId, argumentId);

    return true;
  }

  bool ComponentConfigurator::applyArgumentFile(const QString &argumentId,
                                                const QString &path,
                                                QString &message)
  {
    HydroCouple::IArgument *target = argument(argumentId);

    if (!target)
    {
      message = tr("no argument '%1' on '%2'").arg(argumentId, m_componentId);
      m_status->setText(message);
      return false;
    }

    if (!writeArgumentFile(target, path, message))
    {
      m_status->setText(tr("'%1' rejected '%2': %3")
                          .arg(argumentId, path, message));
      return false;
    }

    // The file has already changed the component, so what is recorded has to
    // be read back out of it rather than assumed -- the component decides
    // what the file meant.
    const nlohmann::json payload = readArgumentPayload(target, message);

    if (payload.is_null())
    {
      m_status->setText(tr("'%1' read '%2' but cannot serialise it: %3")
                          .arg(argumentId, path, message));
      return false;
    }

    if (!m_document->setArgument(m_componentId, argumentId, payload))
    {
      message = tr("could not record '%1'").arg(argumentId);
      m_status->setText(message);
      return false;
    }

    m_status->clear();
    Q_EMIT argumentChanged(m_componentId, argumentId);

    return true;
  }

  QString ComponentConfigurator::rawText() const
  {
    return m_rawPane->toPlainText();
  }

  void ComponentConfigurator::setRawText(const QString &text)
  {
    m_rawPane->setPlainText(text);
  }

  bool ComponentConfigurator::applyRawText(QString &message)
  {
    nlohmann::json parsed;

    try
    {
      parsed = nlohmann::json::parse(rawText().toStdString());
    }
    catch (const nlohmann::json::parse_error &error)
    {
      message = tr("not valid JSON: %1").arg(QString::fromUtf8(error.what()));
      m_status->setText(message);
      return false;
    }

    if (!parsed.is_object())
    {
      message = tr("the arguments block must be a JSON object");
      m_status->setText(message);
      return false;
    }

    // All-or-nothing: validate every argument against the component before
    // recording any of them, so a typo in the third argument cannot leave the
    // first two applied.
    for (auto it = parsed.begin(); it != parsed.end(); ++it)
    {
      const QString argumentId = QString::fromStdString(it.key());
      HydroCouple::IArgument *target = argument(argumentId);

      if (!target)
      {
        message = tr("no argument '%1' on '%2'").arg(argumentId, m_componentId);
        m_status->setText(message);
        return false;
      }

      if (!writeArgumentPayload(target, it.value(), message))
      {
        message = tr("'%1' rejected: %2").arg(argumentId, message);
        m_status->setText(message);
        return false;
      }
    }

    for (auto it = parsed.begin(); it != parsed.end(); ++it)
    {
      m_document->setArgument(m_componentId,
                              QString::fromStdString(it.key()), it.value());
    }

    m_status->clear();
    rebuild();

    return true;
  }

  bool ComponentConfigurator::hasComponentEditor() const
  {
    if (!m_instances || m_componentId.isEmpty())
    {
      return false;
    }

    HydroCouple::IModelComponent *component =
      m_instances->instance(m_componentId);

    auto *provider = dynamic_cast<HydroCouple::IUIProvider *>(component);

    return provider && provider->hasEditor();
  }

  bool ComponentConfigurator::showComponentEditor()
  {
    if (!hasComponentEditor())
    {
      return false;
    }

    auto *provider = dynamic_cast<HydroCouple::IUIProvider *>(
      m_instances->instance(m_componentId));

    // The opaque pointer is this widget: a component that builds Qt UI can
    // parent it correctly, and one that does not simply ignores it.
    provider->showEditor(this);

    return true;
  }

} // namespace HydroCouple::Composer
