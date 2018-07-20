/*!
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
 * This file contains the core interface definitions for the
 * HydroCouple component-based modeling definitions.
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU Lesser General Public License as published by the Free Software Foundation;
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

#include "stdafx.h"
#include "argumentdialog.h"

#include <QtWidgets>

using namespace  HydroCouple ;


ArgumentDialog::ArgumentDialog(QWidget *parent)
   :QDialog(parent),
     m_component(nullptr),
     m_adaptedOutput(nullptr)
{
   setupUi(this);


   connect(pushButtonClose , SIGNAL(clicked()) , this , SLOT(close()));
   connect(pushButtonReadValues, SIGNAL(clicked()) , this , SLOT(onReadArgument()));
   connect(pushButtonInitializeComponent, SIGNAL(clicked()), this, SLOT(onInitializeComponent()));
   connect(pushButtonBrowse, SIGNAL(clicked()), this, SLOT(onBrowseForFile()));
   connect(comboBoxArguments, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectedArgumentChanged(int)));

   readSettings();
}

ArgumentDialog::~ArgumentDialog()
{
  writeSettings();
}

void ArgumentDialog::setComponent(GModelComponent *component)
{
   m_isComponent = true;

   pushButtonInitializeComponent->setVisible(true);

   m_component = component;
   setWindowTitle("Model Component " + m_component->modelComponent()->id() + "'s Arguments");

   comboBoxArguments->clear();
   m_arguments.clear();

   for(IArgument* argument : component->modelComponent()->arguments())
   {
      m_arguments[argument->id()] = argument;
      comboBoxArguments->addItem(argument->caption() , argument->id());
   }
}

void ArgumentDialog::setAdaptedOutput(GAdaptedOutput *adaptedOutput)
{
   m_isComponent = false;

   pushButtonInitializeComponent->setVisible(false);

   m_adaptedOutput = adaptedOutput;
   setWindowTitle("AdaptedOutput " + m_adaptedOutput->id() + "'s Arguments");


   comboBoxArguments->clear();
   m_arguments.clear();

   for(IArgument* argument : m_adaptedOutput->adaptedOutput()->arguments())
   {
      m_arguments[argument->id()] = argument;
      comboBoxArguments->addItem(argument->caption() , argument->id());
   }
}

void ArgumentDialog::readSettings()
{
  m_settings.beginGroup("HydroCoupleComposer::ArgumentDialog");
  {
    this->restoreGeometry(m_settings.value("HydroCoupleComposer::ArgumentDialog::WindowState", this->saveGeometry()).toByteArray());
    this->setWindowState((Qt::WindowState)m_settings.value("HydroCoupleComposer::ArgumentDialog::WindowStateEnum", (int)this->windowState()).toInt());
    this->setGeometry(m_settings.value("HydroCoupleComposer::ArgumentDialog::Geometry", this->geometry()).toRect());
  }
  m_settings.endGroup();

}

void ArgumentDialog::writeSettings()
{
  m_settings.beginGroup("HydroCoupleComposer::ArgumentDialog");
  m_settings.setValue("HydroCoupleComposer::ArgumentDialog::WindowState", this->saveGeometry());
  m_settings.setValue("HydroCoupleComposer::ArgumentDialog::WindowStateEnum", (int)this->windowState());
  m_settings.setValue("HydroCoupleComposer::ArgumentDialog::Geometry", this->geometry());
  m_settings.endGroup();
}

void ArgumentDialog::clearSettings()
{
  m_settings.remove("HydroCoupleComposer::ArgumentDialog");
}

void ArgumentDialog::onReadArgument()
{
   if(m_isComponent)
   {
      if(m_component->modelComponent()->status() == IModelComponent::Created ||
            m_component->modelComponent()->status() == IModelComponent::Failed ||
            m_component->modelComponent()->status() == IModelComponent::Initialized)
      {
         int index;

         if(m_arguments.size() && (index = comboBoxArguments->currentIndex()) > -1)
         {
            QVariant arg = comboBoxArguments->itemData(index);
            IArgument* argument = m_arguments[arg.toString()];

            QString message;

            if(radioButtonFileInput->isChecked())// dont care maybe clear && !lineEditFileInput->text().isEmpty() && !lineEditFileInput->text().isNull())
            {

               if(argument->readValues(lineEditFileInput->text(), message, true))
               {
                  labelStatus->setText("Input read successfully: " + message);
                  textEditInput->setText(argument->toString());
               }
               else
               {
                  labelStatus->setText("Input could not be read!: " + message);
               }
            }
            else //if(!textEditInput->toPlainText().isEmpty() && !textEditInput->toPlainText().isNull())
            {
               if(argument->readValues(textEditInput->toPlainText(), message))
               {
                  labelStatus->setText("Input read successfully: " + message);
               }
               else
               {
                  labelStatus->setText("Input could not be read!: " + message);
               }

               QTimer::singleShot(5000, this , SLOT(onRefreshStatus()));
            }
         }
      }
   }
   else
   {
      int index;

      if(m_arguments.size() && (index = comboBoxArguments->currentIndex()) > -1)
      {
         QVariant arg = comboBoxArguments->itemData(index);
         IArgument* argument = m_arguments[arg.toString()];

         QString message;

         if(radioButtonFileInput->isChecked() && !lineEditFileInput->text().isEmpty() && !lineEditFileInput->text().isNull())
         {
            if(argument->readValues(lineEditFileInput->text(), message, true))
            {
               labelStatus->setText("Input read successfully: " + message);
            }
            else
            {
               labelStatus->setText("Input could not be read Successfully");
            }
         }
         else if(!textEditInput->toPlainText().isEmpty() && !textEditInput->toPlainText().isNull())
         {
            if(argument->readValues(textEditInput->toPlainText(), message))
            {
               labelStatus->setText("Input read successfully: " + message);
            }
            else
            {
               labelStatus->setText("Input could not be read Successfully: " + message);
            }

            QTimer::singleShot(5000, this , SLOT(onRefreshStatus()));
         }
      }
   }
}

void ArgumentDialog::onInitializeComponent()
{
   if(m_component->modelComponent()->status() == IModelComponent::Created ||
         m_component->modelComponent()->status() == IModelComponent::Failed ||
         m_component->modelComponent()->status() == IModelComponent::Initialized
         )
   {
      m_component->modelComponent()->initialize();
   }
}

void ArgumentDialog::onBrowseForFile()
{
   int index;
   if(m_arguments.size() && (index = comboBoxArguments->currentIndex()) > -1)
   {
      QVariant arg = comboBoxArguments->itemData(index);
      IArgument* argument = m_arguments[arg.toString()];

      QString filter ="";

      for(const QString &cfilter : argument->fileFilters())
      {
         if(filter.isEmpty())
         {
            filter = cfilter;
         }
         else
         {
            filter = filter + ";;" + cfilter;
         }
      }

      QString file = QFileDialog::getOpenFileName(this, "Open Argument File","", filter);

      if (!file.isEmpty() && QFileInfo(file).dir().exists())
      {
         lineEditFileInput->setText(file);
      }
   }
}

void ArgumentDialog::onSelectedArgumentChanged(int index)
{
   if(index > -1)
   {
      QVariant arg = comboBoxArguments->itemData(index);
      IArgument* argument = m_arguments[arg.toString()];

      if(argument->canReadFromFile() && !argument->canReadFromString())
      {
         radioButtonFileInput->setEnabled(true);
         radioButtonTextInput->setEnabled(false);
         radioButtonTextInput->setChecked(false);

      }
      else if(!argument->canReadFromFile() && argument->canReadFromString())
      {
        radioButtonFileInput->setEnabled(false);
        radioButtonTextInput->setEnabled(true);
        radioButtonFileInput->setChecked(false);
      }
      else
      {
        radioButtonFileInput->setEnabled(true);
        radioButtonTextInput->setEnabled(true);
      }

      if(argument->currentArgumentIOType() == IArgument::String)
      {
         radioButtonTextInput->setChecked(true);
         QString text = argument->toString();
         textEditInput->setText(text);
         lineEditFileInput->setText("");
      }
      else
      {
         radioButtonFileInput->setChecked(true);
         lineEditFileInput->setText(argument->toString());
         textEditInput->setText("");
      }

      textEditArgumentDescription->setHtml(argument->description());
   }
   else
   {
      lineEditFileInput->clear();
      textEditInput->clear();
      textEditArgumentDescription->clear();
   }
}

void ArgumentDialog::onRefreshStatus()
{
   labelStatus->setText("");
}
