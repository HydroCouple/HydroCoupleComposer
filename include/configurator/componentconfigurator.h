/*!
 * \file   componentconfigurator.h
 * \author Caleb Buahin
 * \brief  ComponentConfigurator — edits one component's arguments.
 *
 * Builds an editor per argument from ArgumentDescriptor introspection and
 * keeps a raw JSON pane in step with it, because a generated form can never
 * cover every payload a component might accept: the form is the convenience,
 * the raw pane is the guarantee.
 *
 * Every accepted edit is written to the CompositionDocument as an undoable
 * command, so the configurator, the canvas and any other view stay consistent.
 * A component that ships its own editor (`IUIProvider`) is offered it instead
 * of — not as well as — the generated form for that component.
 */

#ifndef HYDROCOUPLECOMPOSER_CONFIGURATOR_COMPONENTCONFIGURATOR_H
#define HYDROCOUPLECOMPOSER_CONFIGURATOR_COMPONENTCONFIGURATOR_H

#include "configurator/argumentdescriptor.h"
#include "project/componentinstances.h"
#include "project/compositiondocument.h"

#include <QHash>
#include <QUrl>
#include <QWidget>

#include <functional>

class QFormLayout;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;

namespace HydroCouple::Composer
{

  /*!
   * \brief Argument editors and a raw payload pane for one component.
   */
  class ComponentConfigurator : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds a configurator bound to a document.
       * \param document Composition receiving the edits.
       * \param instances Supplies the live components to introspect.
       * \param parent Optional parent widget.
       */
      ComponentConfigurator(CompositionDocument *document,
                            ComponentInstances *instances,
                            QWidget *parent = nullptr);

      ~ComponentConfigurator() override;

      /*!
       * \brief Shows the arguments of \a componentId; empty clears the panel.
       * \param componentId Component to configure.
       */
      void setComponent(const QString &componentId);

      /*!
       * \brief The component currently shown.
       */
      [[nodiscard]] QString componentId() const;

      /*!
       * \brief The descriptors currently driving the form.
       */
      [[nodiscard]] QList<ArgumentDescriptor> descriptors() const;

      /*!
       * \brief Applies a payload to one argument, as an editor does.
       *
       * Offers it to the live component first: a payload the component
       * rejects never reaches the document, so the document cannot hold a
       * value the model would refuse.
       *
       * \param argumentId Argument to set.
       * \param payload The new payload.
       * \param[out] message Diagnostic when rejected.
       * \returns true when accepted and recorded.
       */
      bool applyArgument(const QString &argumentId,
                         const nlohmann::json &payload, QString &message);

      /*!
       * \brief Loads a file or URI into one argument, as the buttons do.
       *
       * The reference is read by the component first; the document then
       * records what the component holds afterwards. For a file that is the
       * values, because a path is not something the load path could act on;
       * for a URI the component records the URI beside them, so a reopened
       * composition goes back to the service rather than running on a copy.
       *
       * \param argumentId Argument to load.
       * \param reference The file or URI to read.
       * \param[out] message Diagnostic when the component cannot read it.
       * \returns true when it was read and recorded.
       */
      bool applyArgumentReference(const QString &argumentId,
                                  const QString &reference, QString &message);

      /*!
       * \brief A loaded map layer offered as a source for an argument.
       */
      struct LayerSource
      {
          QString name;
          QUrl uri;
      };

      /*!
       * \brief Tells the configurator which map layers can be read from.
       *
       * A callback rather than a list, because the layer stack changes while
       * the panel is open and a snapshot taken when a component was selected
       * would offer layers that have since been closed.
       *
       * \param provider Returns the layers that can name a source.
       */
      void setLayerSources(std::function<QVector<LayerSource>()> provider);

      /*!
       * \brief Another component's output offered as an argument's source.
       */
      struct OutputSource
      {
          QString componentId;
          QString outputId;
          QString caption;
      };

      /*!
       * \brief The outputs an argument could be bound to.
       *
       * Every output of every OTHER component in the document. Deliberately
       * unfiltered by type: there is no non-mutating way to ask an argument
       * whether it could adopt a given item, and a heuristic (kind, rank)
       * would wrongly exclude honest pairs — a mesh output's hyperslab
       * plane says nothing about the surface a mesh argument reads. The
       * run pipeline validates the pairing and names a mismatch.
       *
       * \param argumentId The argument the binding is for.
       */
      [[nodiscard]] QVector<OutputSource> bindableOutputsFor(
        const QString &argumentId) const;

      /*!
       * \brief Binds an argument to another component's output.
       *
       * Records the reference — never a value: opening a composition shows
       * provenance, and the value exists only after the provider has run,
       * which is the run pipeline's job (the SDK's staged apply). The live
       * component is deliberately not touched here.
       *
       * \param argumentId Argument to bind.
       * \param providerId The providing component's document id.
       * \param outputId The provider's output id.
       * \param[out] message Diagnostic when refused.
       * \returns true when the reference was recorded.
       */
      bool applyArgumentBinding(const QString &argumentId,
                                const QString &providerId,
                                const QString &outputId, QString &message);

      /*!
       * \brief The raw JSON pane's current text.
       */
      [[nodiscard]] QString rawText() const;

      /*!
       * \brief Applies the raw pane's text as the component's arguments.
       * \param[out] message Diagnostic when the text is invalid or rejected.
       * \returns true when every argument in the text was accepted.
       */
      bool applyRawText(QString &message);

      /*!
       * \brief Sets the raw pane's text without applying it.
       * \param text JSON text to show.
       */
      void setRawText(const QString &text);

      /*!
       * \brief Whether the shown component offers its own editor.
       */
      [[nodiscard]] bool hasComponentEditor() const;

      /*!
       * \brief Opens the component's own editor, when it has one.
       * \returns true when an editor was shown.
       */
      bool showComponentEditor();

    Q_SIGNALS:
      /*!
       * \brief Emitted after an argument is successfully changed.
       * \param componentId The component whose argument changed.
       * \param argumentId The argument that changed.
       */
      void argumentChanged(const QString &componentId, const QString &argumentId);

    private:
      void rebuild();
      void refreshRawPane();
      void chooseLayerFor(const QString &argumentId, QLineEdit *line);
      void chooseOutputFor(const QString &argumentId, QLineEdit *line);
      [[nodiscard]] HydroCouple::IArgument *argument(
        const QString &argumentId) const;

      CompositionDocument *m_document = nullptr;
      ComponentInstances *m_instances = nullptr;

      QString m_componentId;
      QList<ArgumentDescriptor> m_descriptors;

      QFormLayout *m_form = nullptr;
      QPlainTextEdit *m_rawPane = nullptr;
      QLabel *m_status = nullptr;
      QPushButton *m_componentEditorButton = nullptr;

      std::function<QVector<LayerSource>()> m_layerSources;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_CONFIGURATOR_COMPONENTCONFIGURATOR_H
