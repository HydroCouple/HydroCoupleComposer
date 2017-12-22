#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include <QStandardItemModel>

PreferencesDialog::PreferencesDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::PreferencesDialog)
{
  ui->setupUi(this);
  ui->fontDialog->setWindowFlags(Qt::Widget);
  ui->fontDialog->setOption(QFontDialog::DontUseNativeDialog, true);
  ui->fontDialog->setOption(QFontDialog::NoButtons, true);
  ui->fontDialog->setCurrentFont(qApp->font());
  ui->fontDialog->setFont(qApp->font());

  m_directoriesModel = new QStandardItemModel(ui->listViewSearchDirectories);
  m_fileExtensionsModel = new QStandardItemModel(ui->listViewFileExtensions);

  QStringList temp; temp << "";
  m_directoriesModel->setHorizontalHeaderLabels(temp);
  m_directoriesModel->setSortRole(Qt::DisplayRole);

  m_fileExtensionsModel->setHorizontalHeaderLabels(temp);
  m_fileExtensionsModel->setSortRole(Qt::DisplayRole);

  m_directoriesModel->appendRow(new QStandardItem(qApp->applicationDirPath()));

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
  m_fileExtensionsModel->appendRow(new QStandardItem("*.dll"));
#elif __unix__ // all unices, not all compilers
 m_fileExtensionsModel->appendRow(new QStandardItem("*.so"));
#elif __linux__
  m_fileExtensionsModel->appendRow(new QStandardItem("*.so"));
#elif __APPLE__
  m_fileExtensionsModel->appendRow(new QStandardItem("*.dylib"));
#endif


  ui->listViewSearchDirectories->setModel(m_directoriesModel);
  ui->listViewFileExtensions->setModel(m_fileExtensionsModel);
  connect(this, SIGNAL(rejected()), this, SLOT(onClosing()));
}

PreferencesDialog::~PreferencesDialog()
{
  delete ui;
}

void PreferencesDialog::readSettings()
{
  m_settings.beginGroup("HydroCoupleComposer::PreferencesDialog");
  {
    this->restoreGeometry(m_settings.value("HydroCoupleComposer::PreferencesDialog::WindowState", this->saveGeometry()).toByteArray());
    this->setWindowState((Qt::WindowState)m_settings.value("HydroCoupleComposer::PreferencesDialog::WindowStateEnum", (int)this->windowState()).toInt());
    this->setGeometry(m_settings.value("HydroCoupleComposer::PreferencesDialog::Geometry", this->geometry()).toRect());
  }
  m_settings.endGroup();
}

void PreferencesDialog::writeSettings()
{
  m_settings.beginGroup("HydroCoupleComposer::PreferencesDialog");
  m_settings.setValue("HydroCoupleComposer::PreferencesDialog::WindowState", this->saveGeometry());
  m_settings.setValue("HydroCoupleComposer::PreferencesDialog::WindowStateEnum", (int)this->windowState());
  m_settings.setValue("HydroCoupleComposer::PreferencesDialog::Geometry", this->geometry());
  m_settings.endGroup();
}

void PreferencesDialog::clearSettings()
{
  m_settings.remove("HydroCoupleComposer::PreferencesDialog");
  ui->radioButtonLight->setChecked(true);
  ui->radioButtonFusion->setChecked(true);
  QFont font;
  font.setFamily(font.defaultFamily());
  font.setPixelSize(12);
  ui->fontDialog->setCurrentFont(font);
  m_directoriesModel->clear();
  m_fileExtensionsModel->clear();
  onClosing();
}

void PreferencesDialog::onClosing()
{

  qApp->setFont(ui->fontDialog->currentFont());

  if(ui->radioButtonDark->isChecked())
  {
    QPalette palette;
    palette.setColor(QPalette::Window,QColor(53,53,53));
    palette.setColor(QPalette::WindowText,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::WindowText,QColor(127,127,127));
    palette.setColor(QPalette::Base,QColor(42,42,42));
    palette.setColor(QPalette::AlternateBase,QColor(66,66,66));
    palette.setColor(QPalette::ToolTipBase,Qt::white);
    palette.setColor(QPalette::ToolTipText,Qt::white);
    palette.setColor(QPalette::Text,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::Text,QColor(127,127,127));
    palette.setColor(QPalette::Dark,QColor(35,35,35));
    palette.setColor(QPalette::Shadow,QColor(20,20,20));
    palette.setColor(QPalette::Button,QColor(53,53,53));
    palette.setColor(QPalette::ButtonText,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor(127,127,127));
    palette.setColor(QPalette::BrightText,Qt::red);
    palette.setColor(QPalette::Link,QColor(42,130,218));
    palette.setColor(QPalette::Highlight,QColor(42,130,218));
    palette.setColor(QPalette::Disabled,QPalette::Highlight,QColor(80,80,80));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::HighlightedText,QColor(127,127,127));
    qApp->setPalette(palette);
  }
  else
  {
    qApp->setPalette(qApp->style()->standardPalette());
  }

  if(ui->radioButtonFusion->isChecked())
  {
    qApp->setStyle("fusion");
  }
  else if(ui->radioButtonMacintosh->isChecked())
  {
    qApp->setStyle("macintosh");
  }
  else if(ui->radioButtonWindows->isChecked())
  {
    qApp->setStyle("windows");
  }
  else if(ui->radioButtonWindowsXP->isChecked())
  {
    qApp->setStyle("windowsxp");
  }
  else if(ui->radioButtonWindowsVista->isChecked())
  {
    qApp->setStyle("windowsvista");
  }
}
