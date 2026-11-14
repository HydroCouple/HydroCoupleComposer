#include "simulation/executionpanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{

  using HydroCouple::SDK::IO::ExecutionMode;
  using HydroCouple::SDK::IO::WorkflowSpec;
  using HydroCouple::SDK::IO::WorkflowStrategy;

  namespace
  {
    // WorkflowSpec carries no operator==; the panel compares before it
    // pushes so a focus-out that changed nothing never pollutes the undo
    // stack with a no-op command.
    bool sameWorkflow(const WorkflowSpec &lhs, const WorkflowSpec &rhs)
    {
      return lhs.strategy == rhs.strategy && lhs.id == rhs.id &&
             lhs.triggerComponent == rhs.triggerComponent &&
             lhs.triggerInput == rhs.triggerInput &&
             lhs.iterationsPerGroup == rhs.iterationsPerGroup &&
             lhs.maxSteps == rhs.maxSteps;
    }
  } // namespace

  ExecutionPanel::ExecutionPanel(CompositionDocument *document,
                                 QWidget *parent)
    : QWidget(parent),
      m_document(document)
  {
    // Scrolled content: the panel's natural width must never dictate the
    // dock area's minimum — a wide form here narrows the central canvas
    // for the whole window — and a many-component document needs the
    // overflow anyway.
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    scroll->setWidget(content);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);

    // ── Workflow ─────────────────────────────────────────────────────────
    auto *workflowGroup = new QGroupBox(tr("Workflow"), content);
    auto *form = new QFormLayout(workflowGroup);

    m_strategy = new QComboBox(workflowGroup);
    m_strategy->setObjectName(QStringLiteral("workflowStrategy"));
    m_strategy->addItem(tr("Host-driven (none)"),
                        static_cast<int>(WorkflowStrategy::None));
    m_strategy->addItem(tr("Pull-driven"),
                        static_cast<int>(WorkflowStrategy::PullDriven));
    m_strategy->addItem(tr("Time-stepped"),
                        static_cast<int>(WorkflowStrategy::TimeStepped));
    form->addRow(tr("Strategy"), m_strategy);

    m_triggerComponent = new QComboBox(workflowGroup);
    m_triggerComponent->setObjectName(
      QStringLiteral("workflowTriggerComponent"));
    form->addRow(tr("Trigger component"), m_triggerComponent);

    m_triggerInput = new QLineEdit(workflowGroup);
    m_triggerInput->setObjectName(QStringLiteral("workflowTriggerInput"));
    m_triggerInput->setPlaceholderText(tr("input id"));
    form->addRow(tr("Trigger input"), m_triggerInput);

    m_iterations = new QSpinBox(workflowGroup);
    m_iterations->setObjectName(
      QStringLiteral("workflowIterationsPerGroup"));
    m_iterations->setRange(1, 999);
    form->addRow(tr("Iterations per group"), m_iterations);

    m_maxSteps = new QSpinBox(workflowGroup);
    m_maxSteps->setObjectName(QStringLiteral("workflowMaxSteps"));
    m_maxSteps->setRange(0, 1000000000);
    m_maxSteps->setSpecialValueText(tr("Until done"));
    form->addRow(tr("Max steps"), m_maxSteps);

    layout->addWidget(workflowGroup);

    // ── Component execution ──────────────────────────────────────────────
    m_componentsGroup = new QGroupBox(tr("Component execution"), content);
    m_componentsLayout = new QVBoxLayout(m_componentsGroup);
    layout->addWidget(m_componentsGroup);

    // ── Staged initialization order ──────────────────────────────────────
    auto *stagesGroup = new QGroupBox(tr("Initialization order"), content);
    auto *stagesLayout = new QVBoxLayout(stagesGroup);
    m_stages = new QLabel(stagesGroup);
    m_stages->setObjectName(QStringLiteral("executionStages"));
    m_stages->setWordWrap(true);
    stagesLayout->addWidget(m_stages);
    layout->addWidget(stagesGroup);

    layout->addStretch(1);

    // User-committed edits only: combos commit on index change (guarded
    // against programmatic sets), text and spin fields on editingFinished —
    // never on every keystroke, so one edit is one undo entry.
    connect(m_strategy, &QComboBox::currentIndexChanged, this,
            [this] { commitWorkflow(); });
    connect(m_triggerComponent, &QComboBox::currentIndexChanged, this,
            [this] { commitWorkflow(); });
    connect(m_triggerInput, &QLineEdit::editingFinished, this,
            &ExecutionPanel::commitWorkflow);
    connect(m_iterations, &QSpinBox::editingFinished, this,
            &ExecutionPanel::commitWorkflow);
    connect(m_maxSteps, &QSpinBox::editingFinished, this,
            &ExecutionPanel::commitWorkflow);

    if (m_document)
    {
      connect(m_document, &CompositionDocument::compositionChanged, this,
              &ExecutionPanel::refresh);
    }

    refresh();
  }

  void ExecutionPanel::refresh()
  {
    if (!m_document)
    {
      return;
    }

    m_refreshing = true;

    const auto &spec = m_document->spec();
    const WorkflowSpec &workflow = spec.workflow;

    m_strategy->setCurrentIndex(
      m_strategy->findData(static_cast<int>(workflow.strategy)));

    // The trigger candidates are the document's components; a hand-authored
    // trigger naming an unknown component still shows rather than silently
    // rebinding.
    const QStringList ids = m_document->componentIds();
    const QString trigger = QString::fromStdString(workflow.triggerComponent);

    m_triggerComponent->clear();
    m_triggerComponent->addItem(QString());
    m_triggerComponent->addItems(ids);
    if (!trigger.isEmpty() && !ids.contains(trigger))
    {
      m_triggerComponent->addItem(trigger);
    }
    m_triggerComponent->setCurrentText(trigger);

    m_triggerInput->setText(QString::fromStdString(workflow.triggerInput));
    m_iterations->setValue(workflow.iterationsPerGroup);
    m_maxSteps->setValue(workflow.maxSteps);

    const bool pull = workflow.strategy == WorkflowStrategy::PullDriven;
    const bool stepped = workflow.strategy == WorkflowStrategy::TimeStepped;
    m_triggerComponent->setEnabled(pull);
    m_triggerInput->setEnabled(pull);
    m_iterations->setEnabled(stepped);
    m_maxSteps->setEnabled(workflow.strategy != WorkflowStrategy::None);

    if (ids != m_rowIds)
    {
      rebuildComponentRows(ids);
    }

    for (const auto &component : spec.components)
    {
      const QString id = QString::fromStdString(component.id);
      if (QComboBox *mode = m_modeOf.value(id))
      {
        mode->setCurrentIndex(
          mode->findData(static_cast<int>(component.mode)));
      }
      if (QLineEdit *manifest = m_manifestOf.value(id))
      {
        manifest->setText(
          QString::fromStdString(component.resultsManifest));
        manifest->setEnabled(component.mode != ExecutionMode::Run);
      }
    }

    m_stages->setText(stagesText());

    m_refreshing = false;
  }

  void ExecutionPanel::rebuildComponentRows(const QStringList &ids)
  {
    delete m_rows;
    m_rows = new QWidget(m_componentsGroup);
    m_modeOf.clear();
    m_manifestOf.clear();
    m_rowIds = ids;

    auto *form = new QFormLayout(m_rows);
    form->setContentsMargins(0, 0, 0, 0);

    if (ids.isEmpty())
    {
      form->addRow(new QLabel(tr("No components."), m_rows));
    }

    for (const QString &id : ids)
    {
      auto *row = new QWidget(m_rows);
      auto *rowLayout = new QVBoxLayout(row);
      rowLayout->setContentsMargins(0, 0, 0, 0);

      auto *mode = new QComboBox(row);
      mode->setObjectName(QStringLiteral("execution_mode_%1").arg(id));
      mode->addItem(tr("Run"), static_cast<int>(ExecutionMode::Run));
      mode->addItem(tr("Open results"),
                    static_cast<int>(ExecutionMode::Open));
      mode->addItem(tr("Resume"), static_cast<int>(ExecutionMode::Resume));
      rowLayout->addWidget(mode);

      auto *manifest = new QLineEdit(row);
      manifest->setObjectName(
        QStringLiteral("execution_manifest_%1").arg(id));
      manifest->setPlaceholderText(tr("run manifest path"));
      rowLayout->addWidget(manifest);

      m_modeOf.insert(id, mode);
      m_manifestOf.insert(id, manifest);

      connect(mode, &QComboBox::currentIndexChanged, this,
              [this, id] { commitExecution(id); });
      connect(manifest, &QLineEdit::editingFinished, this,
              [this, id] { commitExecution(id); });

      form->addRow(id, row);
    }

    m_componentsLayout->addWidget(m_rows);
  }

  void ExecutionPanel::commitWorkflow()
  {
    if (m_refreshing || !m_document)
    {
      return;
    }

    WorkflowSpec workflow = m_document->spec().workflow; // keeps its id
    workflow.strategy =
      static_cast<WorkflowStrategy>(m_strategy->currentData().toInt());
    workflow.triggerComponent =
      m_triggerComponent->currentText().toStdString();
    workflow.triggerInput = m_triggerInput->text().toStdString();
    workflow.iterationsPerGroup = m_iterations->value();
    workflow.maxSteps = m_maxSteps->value();

    if (sameWorkflow(workflow, m_document->spec().workflow))
    {
      return;
    }

    m_document->setWorkflow(workflow);
  }

  void ExecutionPanel::commitExecution(const QString &componentId)
  {
    if (m_refreshing || !m_document)
    {
      return;
    }

    QComboBox *modeBox = m_modeOf.value(componentId);
    QLineEdit *manifestEdit = m_manifestOf.value(componentId);
    if (!modeBox || !manifestEdit)
    {
      return;
    }

    const auto mode =
      static_cast<ExecutionMode>(modeBox->currentData().toInt());
    const QString manifest = manifestEdit->text();

    manifestEdit->setEnabled(mode != ExecutionMode::Run);

    const auto current =
      m_document->component(componentId);
    if (current && current->mode == mode &&
        QString::fromStdString(current->resultsManifest) == manifest)
    {
      return;
    }

    if (m_document->setComponentExecution(componentId, mode, manifest))
    {
      manifestEdit->setStyleSheet(QString());
      manifestEdit->setToolTip(QString());
    }
    else
    {
      // Refused — visibly. Mode "open" with no manifest is the one refusal
      // a user can cause from here; the document stays untouched until a
      // path is supplied.
      manifestEdit->setStyleSheet(
        QStringLiteral("border: 1px solid #c04040;"));
      manifestEdit->setToolTip(
        tr("Mode 'open' needs a results manifest path."));
    }
  }

  QString ExecutionPanel::stagesText() const
  {
    const auto &spec = m_document->spec();
    if (spec.components.empty())
    {
      return tr("No components.");
    }

    std::vector<std::vector<std::string>> stages;
    std::string message;
    if (!HydroCouple::SDK::IO::bindingStages(spec, stages, message))
    {
      // Cycles cannot be loaded, but argument edits can create one; the
      // SDK's message names the offending edges.
      return QString::fromStdString(message);
    }

    QStringList lines;
    int position = 1;
    for (const std::vector<std::string> &stage : stages)
    {
      QStringList names;
      for (const std::string &id : stage)
      {
        names.append(QString::fromStdString(id));
      }
      lines.append(
        QStringLiteral("%1. %2").arg(position++).arg(names.join(", ")));
    }

    return lines.join(QStringLiteral("\n"));
  }

} // namespace HydroCouple::Composer
