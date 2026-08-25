/*!
 * \file   ribbongroup.h
 * \author Caleb Buahin
 * \brief  RibbonGroup — one captioned cluster of ribbon buttons.
 *
 * The ribbon style is shared with openswmm.gui so the two applications read as
 * one suite: a fixed-height row of large icon-over-label buttons, grouped under
 * a small centred caption and closed by a trailing rule (ArcGIS Pro
 * convention). The metrics and layout are taken from openswmm.gui's
 * `ui/toolbars/ribbongroup.h` rather than re-derived.
 *
 * \par What is deliberately not ported
 * openswmm.gui's ribbon also carries a responsive layout solver, a width
 * compactor and last-used split buttons — roughly 3.7k lines that exist to
 * degrade a very dense toolbar gracefully on narrow windows. Composer's
 * toolbar is a fraction of that size, so porting the machinery now would be
 * carrying complexity for a problem it does not yet have. The Full/Compact
 * mode switch is kept, because that is what the appearance depends on; the
 * automatic solver can follow if and when the ribbon grows dense enough to
 * need it.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_TOOLBARS_RIBBONGROUP_H
#define HYDROCOUPLECOMPOSER_UI_TOOLBARS_RIBBONGROUP_H

#include <QString>
#include <QWidget>

class QAction;
class QFrame;
class QHBoxLayout;
class QLabel;
class QToolButton;

namespace HydroCouple::Composer
{

  //! Fixed height of the ribbon content plus caption row (ArcGIS scale).
  inline constexpr int kRibbonRowHeight = 100;

  //! Icon edge in Full mode.
  inline constexpr int kRibbonIconFull = 32;

  //! Icon edge in Compact mode.
  inline constexpr int kRibbonIconCompact = 24;

  /*!
   * \brief How densely a ribbon group draws itself.
   */
  enum class RibbonMode
  {
    Full,    //!< 32 px icons with labels beneath.
    Compact  //!< 24 px icons with labels beside.
  };

  /*!
   * \brief A captioned row of related ribbon buttons.
   */
  class RibbonGroup : public QWidget
  {
      Q_OBJECT

    public:
      /*!
       * \brief Builds an empty group.
       * \param caption Text shown beneath the buttons.
       * \param parent Optional parent widget.
       */
      explicit RibbonGroup(const QString &caption, QWidget *parent = nullptr);

      /*!
       * \brief The group's caption.
       */
      [[nodiscard]] QString caption() const;

      /*!
       * \brief Adds a button for \a action.
       * \param action The action to expose.
       * \param shortLabel Overrides the button face text when the action's own
       *        text is too long for a ribbon face; menus keep the full text
       *        because this is stored as the action's iconText.
       * \returns The button created.
       */
      QToolButton *addAction(QAction *action,
                             const QString &shortLabel = QString());

      /*!
       * \brief Adopts a member widget, such as a combo box.
       * \param widget Widget to place in the group.
       * \param stretch Layout stretch factor.
       */
      void addWidget(QWidget *widget, int stretch = 0);

      /*!
       * \brief The button hosting \a action, or nullptr.
       * \param action Action to look up.
       */
      [[nodiscard]] QToolButton *buttonForAction(const QAction *action) const;

      /*!
       * \brief Switches between Full and Compact presentation.
       * \param mode Presentation to apply.
       */
      void setMode(RibbonMode mode);

      [[nodiscard]] RibbonMode mode() const;

    private:
      void applyMode(RibbonMode mode);

      QString m_caption;
      RibbonMode m_mode = RibbonMode::Full;

      QWidget *m_content = nullptr;
      QHBoxLayout *m_row = nullptr;
      QLabel *m_captionLabel = nullptr;
      QFrame *m_separator = nullptr;
      QList<QToolButton *> m_buttons;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_TOOLBARS_RIBBONGROUP_H
