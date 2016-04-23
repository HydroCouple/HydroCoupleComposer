#include "stdafx.h"
#include "argumentdialog.h"

using namespace  HydroCouple ;


ArgumentDialog::ArgumentDialog(QDialog *parent)
   :QDialog(parent)
{
    setupUi(this);

    connect(pushButtonClose , SIGNAL(clicked()) , this , SLOT(close()));
    connect(pushButtonInitialize, SIGNAL(clicked()) , this , SLOT(onInitializeComponent()));
    connect(pushButtonBrowse, SIGNAL(clicked()), this, SLOT(onBrowseForFile()));
}

ArgumentDialog::~ArgumentDialog()
{

}

void ArgumentDialog::setComponent(GModelComponent *component)
{
   m_component = component;
   for(IArgument* argument : component->modelComponent()->arguments())
   {
      QAction* action = new QAction(argument->caption() , comboBoxArguments);
      action->setData(QVariant::fromValue(argument));
      comboBoxArguments->addAction(action);
   }
}



void ArgumentDialog::onInitializeComponent()
{
   m_component->modelComponent()->initialize();
   QList<QString> messages = m_component->modelComponent()->validate();
   textEditInputValidation->clear();

   for(QString message : messages)
   {
      textEditInputValidation->append(m_component->modelComponent()->id() + " Validation Message :" + message);
   }
}

void ArgumentDialog::onBrowseForFile()
{

}
