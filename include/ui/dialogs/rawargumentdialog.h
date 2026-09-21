/*!
 * \file   rawargumentdialog.h
 * \author Caleb Buahin
 * \brief  The editor for an argument nothing else understands.
 *
 * The payload as JSON, in a text box. It is the fallback for
 * ArgumentEditorKind::Raw — an opaque argument whose meaning the
 * component knows and we do not — and it is also the escape hatch every
 * other dialog offers, because a typed editor that cannot express what
 * the user needs should not be the end of the road.
 *
 * It refuses to hand back text that is not JSON. That refusal is its own,
 * not the component's: sending malformed text on to be rejected there
 * would report a parse error as a modelling error.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_RAWARGUMENTDIALOG_H
#define HYDROCOUPLECOMPOSER_UI_RAWARGUMENTDIALOG_H

#include "ui/dialogs/argumenteditordialog.h"

class QPlainTextEdit;

namespace HydroCouple::Composer
{
  class RawArgumentDialog : public ArgumentEditorDialog
  {
      Q_OBJECT

    public:
      explicit RawArgumentDialog(ArgumentDescriptor descriptor,
                                 QWidget *parent = nullptr);

      /*!
       * \brief The text, parsed.
       *
       * Unparseable text returns the payload the window opened with, and
       * apply() is what refuses — see text() for why the two are split.
       */
      [[nodiscard]] nlohmann::json payload() const override;

      //! The text exactly as typed, valid or not.
      [[nodiscard]] QString text() const;

      //! Replaces the text, for a test and for the Raw… button.
      void setText(const QString &text);

      //! Refuses text that is not JSON, before the component sees it.
      [[nodiscard]] bool validate(QString &message) const override;

    protected:
      void hydrate(const nlohmann::json &payload) override;

    private:
      QPlainTextEdit *m_editor = nullptr;
      nlohmann::json m_opened = nlohmann::json::object();
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_RAWARGUMENTDIALOG_H
