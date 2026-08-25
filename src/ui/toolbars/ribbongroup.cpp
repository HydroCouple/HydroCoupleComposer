#include "ui/toolbars/ribbongroup.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{

  RibbonGroup::RibbonGroup(const QString &caption, QWidget *parent)
    : QWidget(parent),
      m_caption(caption)
  {
    setObjectName(QStringLiteral("ribbonGroup_") + caption);
    setFixedHeight(kRibbonRowHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(4, 2, 0, 2);
    outer->setSpacing(0);

    m_content = new QWidget(this);
    outer->addWidget(m_content);

    auto *column = new QVBoxLayout(m_content);
    column->setContentsMargins(2, 2, 2, 0);
    column->setSpacing(2);

    m_row = new QHBoxLayout;
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(2);
    column->addLayout(m_row, 1);

    m_captionLabel = new QLabel(caption, m_content);
    m_captionLabel->setObjectName(QStringLiteral("ribbonCaption"));
    m_captionLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    // Mid carries the hint-text token, so the caption follows the theme.
    m_captionLabel->setForegroundRole(QPalette::Mid);

    QFont captionFont = m_captionLabel->font();
    captionFont.setPointSizeF(captionFont.pointSizeF() * 0.85);
    m_captionLabel->setFont(captionFont);

    column->addWidget(m_captionLabel, 0);

    // The trailing rule is what visually closes a group, in place of the
    // heavier boxed panels older ribbons used.
    m_separator = new QFrame(this);
    m_separator->setObjectName(QStringLiteral("ribbonSeparator"));
    m_separator->setFrameShape(QFrame::VLine);
    m_separator->setFrameShadow(QFrame::Plain);
    m_separator->setForegroundRole(QPalette::Dark);
    outer->addWidget(m_separator);
  }

  QString RibbonGroup::caption() const
  {
    return m_caption;
  }

  QToolButton *RibbonGroup::addAction(QAction *action,
                                      const QString &shortLabel)
  {
    if (!action)
    {
      return nullptr;
    }

    if (!shortLabel.isEmpty())
    {
      // iconText is Qt's native short-label channel: the button face uses it
      // while menus keep the action's full text.
      action->setIconText(shortLabel);
    }

    auto *button = new QToolButton(m_content);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(kRibbonIconFull, kRibbonIconFull));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::TabFocus);
    button->setObjectName(QStringLiteral("ribbonButton_") +
                          action->objectName());

    m_row->addWidget(button, 0, Qt::AlignTop);
    m_buttons.append(button);

    return button;
  }

  void RibbonGroup::addWidget(QWidget *widget, int stretch)
  {
    if (!widget)
    {
      return;
    }

    widget->setParent(m_content);
    m_row->addWidget(widget, stretch, Qt::AlignVCenter);
  }

  QToolButton *RibbonGroup::buttonForAction(const QAction *action) const
  {
    for (QToolButton *button : m_buttons)
    {
      if (button->defaultAction() == action)
      {
        return button;
      }
    }

    return nullptr;
  }

  RibbonMode RibbonGroup::mode() const
  {
    return m_mode;
  }

  void RibbonGroup::setMode(RibbonMode mode)
  {
    if (m_mode == mode)
    {
      return;
    }

    m_mode = mode;
    applyMode(mode);
  }

  void RibbonGroup::applyMode(RibbonMode mode)
  {
    const bool full = mode == RibbonMode::Full;
    const int edge = full ? kRibbonIconFull : kRibbonIconCompact;
    const Qt::ToolButtonStyle style =
      full ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonTextBesideIcon;

    for (QToolButton *button : m_buttons)
    {
      button->setToolButtonStyle(style);
      button->setIconSize(QSize(edge, edge));
    }

    // The caption only earns its row in Full mode; in Compact the labels sit
    // beside the icons and the row is already self-describing.
    m_captionLabel->setVisible(full);

    setFixedHeight(full ? kRibbonRowHeight : kRibbonRowHeight / 2);
  }

} // namespace HydroCouple::Composer
