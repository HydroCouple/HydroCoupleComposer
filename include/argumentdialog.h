#ifndef ARGUMENTDIALOG_H
#define ARGUMENTDIALOG_H

#include <QDialog>
#include "ui_argumentdialog.h"
#include "gmodelcomponent.h"

class ArgumentDialog : public QDialog , public Ui::ArgumentDialog
{
      Q_OBJECT

   public:
      ArgumentDialog(QDialog* parent = nullptr);

      virtual ~ArgumentDialog();

      void setComponent(GModelComponent* component);


   signals:
      void postValidationMessage(const QString& message);

  private slots:
      void onInitializeComponent();

      void onBrowseForFile();

   private:

      void onClose();

   private:
      GModelComponent* m_component;

};



#endif // ARGUMENTDIALOG_H

