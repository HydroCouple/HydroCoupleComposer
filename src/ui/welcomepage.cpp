#include "ui/welcomepage.h"

#include "core/composerapplication.h"
#include "core/preferencesmanager.h"
#include "ui/recentcompositions.h"

#include <QAction>
#include <QCheckBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{

  WelcomePage::WelcomePage(RecentCompositions *recent, QWidget *parent)
    : QWidget(parent), m_recent(recent)
  {
    setObjectName(QStringLiteral("welcomePage"));

    // Scrolled, so this page's natural size never becomes the minimum of
    // the workspace it is a tab of: every page in a QTabWidget shares one
    // geometry, and a tall start page would resize the map and 3D views
    // sitting behind it.
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    scroll->setWidget(content);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("HydroCouple Composer"), content);
    title->setObjectName(QStringLiteral("welcomeTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.8);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *version = new QLabel(
      tr("Version %1").arg(ComposerApplication::versionString()), content);
    version->setEnabled(false);
    layout->addWidget(version);

    layout->addSpacing(12);

    auto *start = new QLabel(tr("Start"), content);
    QFont sectionFont = start->font();
    sectionFont.setBold(true);
    start->setFont(sectionFont);
    layout->addWidget(start);

    // A composition needs components before it needs anything else, so
    // loading them is offered here beside New and Open rather than left to
    // be discovered under a menu.
    struct Entry
    {
        QString text;
        QString objectName;
        void (WelcomePage::*signal)();
    };

    const Entry entries[] = {
      {tr("New composition"), QStringLiteral("welcomeNewButton"),
       &WelcomePage::newRequested},
      {tr("Open composition…"), QStringLiteral("welcomeOpenButton"),
       &WelcomePage::openRequested},
      {tr("Load component libraries…"),
       QStringLiteral("welcomeLoadComponentsButton"),
       &WelcomePage::loadComponentsRequested}};

    for (const Entry &entry : entries)
    {
      auto *button = new QPushButton(entry.text, content);
      button->setObjectName(entry.objectName);
      button->setFlat(true);
      button->setCursor(Qt::PointingHandCursor);
      button->setStyleSheet(QStringLiteral("text-align: left;"));

      connect(button, &QPushButton::clicked, this,
              [this, signal = entry.signal] { (this->*signal)(); });

      layout->addWidget(button);
    }

    layout->addSpacing(12);

    // The heading carries the one action that acts on the whole list. A
    // remembered document that has moved is kept and marked rather than
    // dropped, which is right — but it means the list only ever grows, so
    // there has to be a way to empty it.
    auto *recentHeading = new QHBoxLayout;
    recentHeading->setContentsMargins(0, 0, 0, 0);

    auto *recentLabel = new QLabel(tr("Recent"), content);
    recentLabel->setFont(sectionFont);
    recentHeading->addWidget(recentLabel);
    recentHeading->addStretch(1);

    m_clearRecent = new QPushButton(tr("Clear list"), content);
    m_clearRecent->setObjectName(QStringLiteral("welcomeClearRecentButton"));
    m_clearRecent->setFlat(true);
    m_clearRecent->setCursor(Qt::PointingHandCursor);
    recentHeading->addWidget(m_clearRecent);

    connect(m_clearRecent, &QPushButton::clicked, this,
            [this]
            {
              if (m_recent)
              {
                m_recent->clear();
              }
            });

    layout->addLayout(recentHeading);

    m_list = new QListWidget(content);
    m_list->setObjectName(QStringLiteral("welcomeRecentList"));
    m_list->setAlternatingRowColors(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);

    connect(m_list, &QListWidget::customContextMenuRequested, this,
            &WelcomePage::showRecentMenu);

    // A single click, not a double: this is a list of links, and everything
    // else on this page opens on one click too.
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item)
            {
              if (item)
              {
                Q_EMIT openRecentRequested(
                  item->data(Qt::UserRole).toString());
              }
            });

    m_showOnStartUp = new QCheckBox(tr("Show this page on start up"), content);
    m_showOnStartUp->setObjectName(QStringLiteral("welcomeShowOnStartUp"));
    layout->addWidget(m_showOnStartUp);

    // The same preference the dialog edits, so the two cannot disagree: the
    // box writes the preference and follows it, rather than owning a copy.
    PreferencesManager *prefs = PreferencesManager::instance();
    m_showOnStartUp->setChecked(prefs->showWelcomeOnStartUp());

    connect(m_showOnStartUp, &QCheckBox::toggled, prefs,
            [prefs](bool shows) { prefs->setShowWelcomeOnStartUp(shows); });
    connect(prefs, &PreferencesManager::preferenceChanged, this,
            [this, prefs](const QString &group, const QString &name)
            {
              if (group == QLatin1String("General")
                  && name == QLatin1String("showWelcomeOnStartUp"))
              {
                m_showOnStartUp->setChecked(prefs->showWelcomeOnStartUp());
              }
            });

    if (m_recent)
    {
      connect(m_recent, &RecentCompositions::changed, this,
              &WelcomePage::refresh);
    }

    refresh();
  }

  void WelcomePage::refresh()
  {
    m_list->clear();

    if (!m_recent)
    {
      m_clearRecent->setEnabled(false);

      return;
    }

    const QStringList paths = m_recent->paths();

    // Nothing to clear when there is nothing in it: a live button over an
    // empty list is an offer that does nothing.
    m_clearRecent->setEnabled(!paths.isEmpty());

    if (paths.isEmpty())
    {
      auto *empty = new QListWidgetItem(tr("Nothing opened yet."), m_list);
      empty->setFlags(Qt::NoItemFlags);

      return;
    }

    for (const QString &path : paths)
    {
      const QFileInfo info(path);

      // The name leads and the folder follows, because several
      // compositions of a study are routinely called the same thing in
      // different directories.
      auto *item = new QListWidgetItem(
        tr("%1 — %2").arg(info.fileName(), info.absolutePath()), m_list);
      item->setData(Qt::UserRole, path);
      item->setToolTip(path);

      if (!info.exists())
      {
        // Kept and marked rather than dropped: "it was here and is gone"
        // is worth telling the user.
        item->setForeground(QColor(150, 90, 90));
        item->setToolTip(tr("%1 is no longer there.").arg(path));
      }
    }
  }

  QMenu *WelcomePage::recentMenuFor(QListWidgetItem *item)
  {
    if (!item || !m_recent)
    {
      return nullptr;
    }

    const QString path = item->data(Qt::UserRole).toString();

    if (path.isEmpty())
    {
      // The "nothing opened yet" placeholder is a message, not an entry.
      return nullptr;
    }

    auto *menu = new QMenu(this);

    // Opening is asked of the window, because that is the one path that
    // also records the document as recent. Forgetting is done to the store
    // here: nothing about it needs the window's help.
    connect(menu->addAction(tr("Open")), &QAction::triggered, this,
            [this, path] { Q_EMIT openRecentRequested(path); });
    connect(menu->addAction(tr("Remove from List")), &QAction::triggered,
            this, [this, path] { m_recent->forget(path); });

    return menu;
  }

  void WelcomePage::showRecentMenu(const QPoint &point)
  {
    if (QMenu *menu = recentMenuFor(m_list->itemAt(point)))
    {
      menu->setAttribute(Qt::WA_DeleteOnClose);
      menu->popup(m_list->viewport()->mapToGlobal(point));
    }
  }

} // namespace HydroCouple::Composer
