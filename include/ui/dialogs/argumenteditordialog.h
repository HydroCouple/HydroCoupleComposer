/*!
 * \file   argumenteditordialog.h
 * \author Caleb Buahin
 * \brief  The base every typed argument editor is a kind of.
 *
 * One argument, one window. The dock's row becomes a summary and an
 * *Edit…* button, and the editing itself happens here — which is what
 * makes a mesh picker, a time-series plot or a kinetics text pane
 * possible at all: none of them fit in a table row, and the two that
 * pick on the map need the map visible while they are open.
 *
 * The contract is deliberately narrow, and it is the whole of U2b:
 *
 *   - a dialog is **given** a payload and **returns** a payload;
 *   - it never writes to a component and never writes to the document;
 *   - committing is the configurator's, through `applyArgument()`, which
 *     offers the payload to the live component first and only records it
 *     if the component accepts (B2's rule, unchanged);
 *   - and nothing is committed until the user asks — no dialog commits
 *     on a keystroke.
 *
 * That last one is the difference between this and the inline editors in
 * the dock, which do commit on every `valueChanged`. It is fine for a
 * spin box and wrong for a window with a Cancel button: a Cancel that
 * left half the typing behind would be a lie.
 *
 * Dialogs are modeless. The Mesh and Geometry editors ask the user to
 * pick features on the map while open, which a modal dialog cannot do.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_ARGUMENTEDITORDIALOG_H
#define HYDROCOUPLECOMPOSER_UI_ARGUMENTEDITORDIALOG_H

#include "configurator/argumentdescriptor.h"

#include <QDialog>
#include <QString>

#include <functional>

class QDialogButtonBox;
class QLabel;
class QVBoxLayout;

namespace HydroCouple::Composer
{
  /*!
   * \brief Offers a payload to whoever owns the argument.
   *
   * A callback rather than a pointer to the configurator, so the dialog
   * knows nothing about who is listening and a test can listen instead
   * (D8: one document, several views, and no view holding another).
   *
   * \param argumentId Which argument.
   * \param payload What the editor now holds.
   * \param[out] message The refusal, when it refuses.
   * \returns True when it was accepted and recorded.
   */
  using ArgumentCommitter =
    std::function<bool(const QString &argumentId,
                       const nlohmann::json &payload, QString &message)>;

  /*!
   * \brief A window that edits one argument.
   */
  class ArgumentEditorDialog : public QDialog
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds the frame; the subclass supplies what goes in it.
       * \param descriptor The argument, as the component described it.
       * \param parent Owner window.
       */
      explicit ArgumentEditorDialog(ArgumentDescriptor descriptor,
                                    QWidget *parent = nullptr);

      ~ArgumentEditorDialog() override;

      //! The argument this window edits.
      [[nodiscard]] const ArgumentDescriptor &descriptor() const;

      //! Where to offer a payload. Without one, Apply refuses and says so.
      void setCommitter(ArgumentCommitter committer);

      /*!
       * \brief What the editor holds right now.
       *
       * Not what was committed: the point of the Apply button is that
       * those two can differ.
       */
      [[nodiscard]] virtual nlohmann::json payload() const = 0;

      /*!
       * \brief Offers the current payload, and reports what happened.
       *
       * Public and not a slot-only affair, so a gate can drive it without
       * a blocking exec() — the same reason PreferencesDialog::apply() is
       * public.
       *
       * \returns False when there is nothing listening, or when the
       *          component refused; refusal() then says why.
       */
      bool apply();

      /*!
       * \brief Whether what the editor holds can be offered at all.
       *
       * The dialog's own check, run before the component's. A parse
       * error is not a modelling error, and reporting one as the other
       * sends the user looking in the wrong place — at their model,
       * rather than at the brace they left open.
       *
       * The default accepts everything: a spin box cannot hold a value
       * it does not accept, so most editors have nothing to add here.
       *
       * \param[out] message Why not, when it says no.
       */
      [[nodiscard]] virtual bool validate(QString &message) const;

      /*!
       * \brief The component's last refusal, or empty.
       */
      [[nodiscard]] QString refusal() const;

      /*!
       * \brief True once a payload has been accepted through this window.
       *
       * What lets the owner tell "the user closed without applying" from
       * "the user applied and then closed".
       */
      [[nodiscard]] bool hasCommitted() const;

    protected:
      /*!
       * \brief Puts \a editor between the header and the buttons.
       *
       * Called once, by the subclass, during its own construction.
       */
      void setEditor(QWidget *editor);

      /*!
       * \brief Loads \a payload into the editor.
       *
       * Called by the base after the subclass has built its editor;
       * subclasses do not call it themselves.
       */
      virtual void hydrate(const nlohmann::json &payload) = 0;

      /*!
       * \brief Hydrates from the descriptor. Subclasses call this last.
       *
       * Not done by the base constructor, because at that point the
       * subclass has not built its editor yet and hydrate() would be a
       * pure virtual call on a half-built object.
       */
      void hydrateFromDescriptor();

      //! Shows \a message in the strip under the editor.
      void showRefusal(const QString &message);

      //! Empties the strip.
      void clearRefusal();

    private:
      ArgumentDescriptor m_descriptor;
      ArgumentCommitter m_committer;

      QVBoxLayout *m_body = nullptr;
      QLabel *m_strip = nullptr;
      QDialogButtonBox *m_buttons = nullptr;
      QWidget *m_editor = nullptr;

      bool m_committed = false;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_ARGUMENTEDITORDIALOG_H
