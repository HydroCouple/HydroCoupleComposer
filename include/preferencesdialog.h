#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QDir>
#include <QSettings>

namespace Ui {
  class PreferencesDialog;
}

class QStandardItemModel;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

    Q_ENUMS(Theme Style)

  public:

    enum Theme
    {
      Light,
      Dark
    };

    enum Style
    {
      Fusion,
      Macintosh,
      Windows,
      WindowsXP,
      WindowsVista
    };


    explicit PreferencesDialog(QWidget *parent = 0);

    ~PreferencesDialog();

    void readSettings();

    void writeSettings();

    void clearSettings();

  signals:

    void propertyChanged(const QString &propertyName);

  private slots:


    void onClosing();

  private:
    QSettings m_settings;
    Ui::PreferencesDialog *ui;
    QStandardItemModel *m_directoriesModel, *m_fileExtensionsModel;
    Theme m_theme;
    Style m_style;
    QList<QDir> m_searchDirectories;
    QList<QString> m_searchFileExtensions;
    QFont m_font;
};

#endif // PREFERENCESDIALOG_H
