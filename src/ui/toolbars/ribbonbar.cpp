#include "ui/toolbars/ribbonbar.h"

#include <QHBoxLayout>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{

  RibbonBar::RibbonBar(QWidget *parent)
    : QWidget(parent)
  {
    setObjectName(QStringLiteral("ribbonBar"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabs = new QTabBar(this);
    m_tabs->setObjectName(QStringLiteral("ribbonTabs"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    layout->addWidget(m_tabs, 0);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("ribbonPages"));
    layout->addWidget(m_pages, 0);

    connect(m_tabs, &QTabBar::currentChanged, this,
            [this](int index)
            {
              m_pages->setCurrentIndex(index);
              Q_EMIT currentTabChanged(currentTab());
            });
  }

  QWidget *RibbonBar::addTab(const QString &id, const QString &title)
  {
    if (const auto it = m_indexById.constFind(id); it != m_indexById.constEnd())
    {
      return m_pages->widget(it.value());
    }

    auto *page = new QWidget(m_pages);
    page->setObjectName(QStringLiteral("ribbonPage_") + id);

    auto *row = new QHBoxLayout(page);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(0);
    // Groups pack from the left; the stretch keeps them there rather than
    // spreading them across the width.
    row->addStretch(1);

    const int index = m_pages->addWidget(page);
    m_tabs->addTab(title);
    m_indexById.insert(id, index);

    if (m_indexById.size() == 1)
    {
      m_tabs->setCurrentIndex(0);
      m_pages->setCurrentIndex(0);
    }

    return page;
  }

  RibbonGroup *RibbonBar::addGroup(const QString &tabId, const QString &caption)
  {
    QWidget *page = addTab(tabId, tabId);

    auto *row = qobject_cast<QHBoxLayout *>(page->layout());

    if (!row)
    {
      return nullptr;
    }

    auto *group = new RibbonGroup(caption, page);
    group->setMode(m_mode);

    // Insert before the trailing stretch so groups stay left-packed.
    row->insertWidget(row->count() - 1, group, 0, Qt::AlignTop);

    return group;
  }

  QString RibbonBar::currentTab() const
  {
    const int index = m_tabs->currentIndex();

    for (auto it = m_indexById.constBegin(); it != m_indexById.constEnd(); ++it)
    {
      if (it.value() == index)
      {
        return it.key();
      }
    }

    return {};
  }

  void RibbonBar::setCurrentTab(const QString &id)
  {
    if (const auto it = m_indexById.constFind(id); it != m_indexById.constEnd())
    {
      m_tabs->setCurrentIndex(it.value());
    }
  }

  QList<RibbonGroup *> RibbonBar::groups(const QString &tabId) const
  {
    const auto it = m_indexById.constFind(tabId);

    if (it == m_indexById.constEnd())
    {
      return {};
    }

    QWidget *page = m_pages->widget(it.value());

    return page ? page->findChildren<RibbonGroup *>(
                    QString(), Qt::FindDirectChildrenOnly)
                : QList<RibbonGroup *>{};
  }

  void RibbonBar::setMode(RibbonMode mode)
  {
    m_mode = mode;

    for (RibbonGroup *group : findChildren<RibbonGroup *>())
    {
      group->setMode(mode);
    }
  }

} // namespace HydroCouple::Composer
