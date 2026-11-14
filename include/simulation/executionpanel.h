/*!
 * \file   executionpanel.h
 * \author Caleb Buahin
 * \brief  ExecutionPanel — the document's orchestration and execution modes.
 *
 * The first UI over WorkflowSpec: strategy, the pull-mode trigger, sweep
 * iterations and the step cap, plus each component's ExecutionMode
 * (run/open/resume with its results manifest) and a read-only preview of
 * the staged initialization order from SDK bindingStages().
 *
 * The panel binds to the whole CompositionDocument, not to the canvas
 * selection: every edit goes back through setWorkflow() /
 * setComponentExecution() so it lands on the undo stack, and the panel
 * re-reads the document on change like every other view.
 */

#ifndef HYDROCOUPLECOMPOSER_SIMULATION_EXECUTIONPANEL_H
#define HYDROCOUPLECOMPOSER_SIMULATION_EXECUTIONPANEL_H

#include "project/compositiondocument.h"

#include <QHash>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QVBoxLayout;

namespace HydroCouple::Composer
{

  /*!
   * \brief Edits the workflow block and per-component execution modes;
   *        previews the staged initialization order.
   */
  class ExecutionPanel : public QWidget
  {
      Q_OBJECT

    public:
      explicit ExecutionPanel(CompositionDocument *document,
                              QWidget *parent = nullptr);

    private:
      /*!
       * Re-reads the document. Rows are rebuilt only when the component id
       * list changed (which this panel's own widgets cannot cause), and
       * updated in place otherwise — so a commit made from inside one of
       * these widgets never deletes the widget mid-signal.
       */
      void refresh();

      void rebuildComponentRows(const QStringList &ids);

      //! Writes the workflow widgets back to the document when they differ.
      void commitWorkflow();

      //! Writes one component's mode + manifest back; visible refusal.
      void commitExecution(const QString &componentId);

      [[nodiscard]] QString stagesText() const;

      CompositionDocument *m_document = nullptr;
      bool m_refreshing = false;

      QComboBox *m_strategy = nullptr;
      QComboBox *m_triggerComponent = nullptr;
      QLineEdit *m_triggerInput = nullptr;
      QSpinBox *m_iterations = nullptr;
      QSpinBox *m_maxSteps = nullptr;

      QGroupBox *m_componentsGroup = nullptr;
      QVBoxLayout *m_componentsLayout = nullptr;
      QWidget *m_rows = nullptr;
      QStringList m_rowIds;
      QHash<QString, QComboBox *> m_modeOf;
      QHash<QString, QLineEdit *> m_manifestOf;

      QLabel *m_stages = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_SIMULATION_EXECUTIONPANEL_H
