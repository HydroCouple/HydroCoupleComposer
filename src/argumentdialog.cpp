#include "stdafx.h"
#include "argumentdialog.h"

using namespace  HydroCouple ;


ArgumentDialog::ArgumentDialog(QDialog *parent)
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
}

ArgumentDialog::~ArgumentDialog()
{

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

void ArgumentDialog::onReadArgument()
{
   if(m_isComponent)
   {
      if(m_component->modelComponent()->status() == HydroCouple::Created ||
            m_component->modelComponent()->status() == HydroCouple::Failed ||
            m_component->modelComponent()->status() == HydroCouple::Initialized)
      {
         int index;

         if(m_arguments.size() && (index = comboBoxArguments->currentIndex()) > -1)
         {
            QVariant arg = comboBoxArguments->itemData(index);
            IArgument* argument = m_arguments[arg.toString()];

            if(radioButtonFileInput->isChecked() && !lineEditFileInput->text().isEmpty() && !lineEditFileInput->text().isNull())
            {
               if(argument->readValues(lineEditFileInput->text(), true))
               {
                  labelStatus->setText("Input read successfully");
               }
               else
               {
                  labelStatus->setText("Input could not be read Successfully");
               }
            }
            else if(!textEditInput->toPlainText().isEmpty() && !textEditInput->toPlainText().isNull())
            {
               if(argument->readValues(textEditInput->toPlainText()))
               {
                  labelStatus->setText("Input read successfully");
               }
               else
               {
                  labelStatus->setText("Input could not be read Successfully");
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

         if(radioButtonFileInput->isChecked() && !lineEditFileInput->text().isEmpty() && !lineEditFileInput->text().isNull())
         {
            if(argument->readValues(lineEditFileInput->text(), true))
            {
               labelStatus->setText("Input read successfully");
            }
            else
            {
               labelStatus->setText("Input could not be read Successfully");
            }
         }
         else if(!textEditInput->toPlainText().isEmpty() && !textEditInput->toPlainText().isNull())
         {
            if(argument->readValues(textEditInput->toPlainText()))
            {
               labelStatus->setText("Input read successfully");
            }
            else
            {
               labelStatus->setText("Input could not be read Successfully");
            }

            QTimer::singleShot(5000, this , SLOT(onRefreshStatus()));
         }
      }
   }
}

void ArgumentDialog::onInitializeComponent()
{
   if(m_component->modelComponent()->status() == HydroCouple::Created ||
         m_component->modelComponent()->status() == HydroCouple::Failed ||
         m_component->modelComponent()->status() == HydroCouple::Initialized
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

      for(const QString &cfilter : argument->inputFileTypeFilters())
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
         radioButtonFileInput->setChecked(true);
         groupBoxInputType->setVisible(false);
         groupBoxTextInput->setVisible(false);
         groupBoxInputFile->setVisible(true);
      }
      else if(!argument->canReadFromFile() && argument->canReadFromString())
      {
         radioButtonFileInput->setChecked(false);
         groupBoxInputType->setVisible(false);
         groupBoxTextInput->setVisible(true);
         groupBoxInputFile->setVisible(false);
      }
      else
      {
         groupBoxInputType->setVisible(true);
         groupBoxTextInput->setVisible(true);
         groupBoxInputFile->setVisible(true);
      }

      if(argument->currentArgumentIOType() == HydroCouple::String)
      {
         radioButtonTextInput->setChecked(true);
         textEditInput->setText(argument->toString());
      }
      else
      {
         radioButtonTextInput->setChecked(false);
         lineEditFileInput->setText(argument->toString());
      }
   }
   else
   {
      lineEditFileInput->clear();
      textEditInput->clear();
   }
}

void ArgumentDialog::onRefreshStatus()
{
   labelStatus->setText("");
}
