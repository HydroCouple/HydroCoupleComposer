#ifndef ARGUMENTDIALOG_H
#define ARGUMENTDIALOG_H

#include <QDialog>
#include <QTimer>
#include "ui_argumentdialog.h"
#include "gmodelcomponent.h"

class ArgumentDialog : public QDialog , public Ui::ArgumentDialog
{
      Q_OBJECT

   public:

      ArgumentDialog(QDialog* parent = nullptr);

      virtual ~ArgumentDialog();

      void setComponent(GModelComponent *component);

      void setAdaptedOutput(GAdaptedOutput *adaptedOutput);

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

};



#endif // ARGUMENTDIALOG_H

