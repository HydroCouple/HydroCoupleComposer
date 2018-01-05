/*!
 * \file   hydrocouple.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
 * This header file contains the core interface definitions for the
 * HydroCouple component-based modeling definitions.
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

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
