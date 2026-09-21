/*!
 * \file   argumenteditorfactory.h
 * \author Caleb Buahin
 * \brief  Which window edits which kind of argument.
 *
 * One function, so that the mapping from kind to dialog lives in one
 * place and a kind with no dialog yet has one obvious answer rather than
 * a different improvisation at each call site.
 *
 * As U2c lands its dialogs, each takes over a line here. Until then every
 * kind arrives at the raw JSON editor — which is a working editor for
 * every argument, not a placeholder: it is what the dock's raw pane
 * already offered, now reachable per argument rather than per component.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_ARGUMENTEDITORFACTORY_H
#define HYDROCOUPLECOMPOSER_UI_ARGUMENTEDITORFACTORY_H

#include "configurator/argumentdescriptor.h"

class QWidget;

namespace HydroCouple::Composer
{
  class ArgumentEditorDialog;

  /*!
   * \brief A window that can edit \a descriptor's argument.
   *
   * Never null: an argument the Composer cannot type is still an
   * argument the user may need to change, and refusing to open anything
   * would make it uneditable.
   *
   * The caller owns the result, which deletes itself when closed.
   *
   * \param descriptor The argument, as the component described it.
   * \param parent Owner window.
   */
  [[nodiscard]] ArgumentEditorDialog *createArgumentEditor(
    const ArgumentDescriptor &descriptor, QWidget *parent = nullptr);

  /*!
   * \brief Whether \a kind has an editor of its own yet.
   *
   * False means createArgumentEditor() will return the raw JSON window.
   * The dock uses this to decide whether *Edit…* promises a typed editor
   * or a JSON one, so that the button's tooltip does not overstate what
   * is behind it.
   */
  [[nodiscard]] bool hasTypedEditor(ArgumentEditorKind kind);

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_ARGUMENTEDITORFACTORY_H
