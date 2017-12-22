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

