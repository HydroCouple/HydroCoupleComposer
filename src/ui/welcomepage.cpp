#include "ui/welcomepage.h"

#include "core/composerapplication.h"
#include "core/preferencesmanager.h"
#include "ui/recentcompositions.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
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

    auto *recentLabel = new QLabel(tr("Recent"), content);
    recentLabel->setFont(sectionFont);
    layout->addWidget(recentLabel);

    m_list = new QListWidget(content);
    m_list->setObjectName(QStringLiteral("welcomeRecentList"));
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

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
      return;
    }

    const QStringList paths = m_recent->paths();

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

} // namespace HydroCouple::Composer
