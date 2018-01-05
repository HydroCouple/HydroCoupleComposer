/*!
 * \file   argumentdialog.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
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

#ifndef ARGUMENTDIALOG_H
#define ARGUMENTDIALOG_H

#include <QDialog>
#include <QTimer>
#include "ui_argumentdialog.h"
#include "gmodelcomponent.h"
#include <QGenericMatrix>
#include <QSettings>

class ArgumentDialog : public QDialog , public Ui::ArgumentDialog
{
      Q_OBJECT

   public:

      ArgumentDialog(QWidget* parent = nullptr);

      virtual ~ArgumentDialog();

      void setComponent(GModelComponent *component);

      void setAdaptedOutput(GAdaptedOutput *adaptedOutput);

      void readSettings();

      void writeSettings();

      void clearSettings();

   signals:

      void postValidationMessage(const QString& message);


  private slots:

      void onReadArgument();

      void onInitializeComponent();

      void onBrowseForFile();

      void onSelectedArgumentChanged(int index);

      void onRefreshStatus();

   private:

      void onClose();

   private:
      bool m_isComponent;
      GModelComponent *m_component;
      GAdaptedOutput *m_adaptedOutput;
      QHash<QString,HydroCouple::IArgument*> m_arguments;
      QSettings m_settings;

};



#endif // ARGUMENTDIALOG_H

