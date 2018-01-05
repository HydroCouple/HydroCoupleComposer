#include "stdafx.h"
#include "hydrocouplecomposer.h"
#include "qpropertyitemdelegate.h"
#include "gmodelcomponent.h"
#include "custompropertyitems.h"
#include "qxmlsyntaxhighlighter.h"
#include "simulationmanager.h"
#include "hydrocouplevis.h"
#include "preferencesdialog.h"

#include <QPolygonF>
#include <iostream>
#include <string>
#include <random>
#include <QPrinter>
#include <QPrintDialog>
#include <QPointF>

#ifdef USE_MPI
#include <mpi.h>
#endif

#ifdef _WIN32

#endif

using namespace HydroCouple;
using namespace std;

const QString HydroCoupleComposer::sc_modelComponentInfoHtml = "<html>"
                                                               "<head>"
                                                               "<title>Component Information</title>"
                                                               "</head>"
                                                               "<body>"
                                                               "<img alt=\"icon\" src='[IconPath]' width=\"50\" align=\"left\" /><h3 align=\"center\"> [Component] </h3>"
                                                               "<h3 align=\"center\"> [Version] </h3>"
                                                               "<h4 align=\"center\"> [Caption] </h4>"
                                                               "<hr>"
                                                               "<p>[Description]</p>"
                                                               "<p align=\"center\"> [License] </p>"
                                                               "<p align=\"center\"><a href=\"[Url]\"> [Vendor] </a></p>"
                                                               "<p align=\"center\"><a href=\"mailto:[Email]?Subject=[Component] | [Version] | [License]\">[Email]</a></p>"
                                                               "<p align=\"center\"> [Copyright] </p>"
                                                               "</body>"
                                                               "</html>";

const QString HydroCoupleComposer::sc_standardInfoHtml = "<html>"
                                                         "<head>"
                                                         "<title>Adapted Output Factory</title>"
                                                         "</head>"
                                                         "<h3 align=\"center\">[Component]</h3>"
                                                         "<body>"
                                                         "<h4 align=\"center\">[Caption]</h4>"
                                                         "<hr>"
                                                         "<p>[Description]</p>"
                                                         "</body>"
                                                         "</html>";

QIcon HydroCoupleComposer::s_categoryIcon = QIcon();
QIcon HydroCoupleComposer::s_adaptedOutput = QIcon();
QIcon HydroCoupleComposer::s_adaptedOutputFactoryIcon = QIcon();
QIcon HydroCoupleComposer::s_adaptedOutputComponentIcon = QIcon();
QIcon HydroCoupleComposer::s_workflowIcon = QIcon();
QIcon HydroCoupleComposer::s_modelComponentIcon = QIcon();

HydroCoupleComposer *HydroCoupleComposer::hydroCoupleComposerWindow = nullptr;

HydroCoupleComposer::HydroCoupleComposer(QWidget *parent)
  : QMainWindow(parent),
    m_connProd(nullptr),
    m_connCons(nullptr),
    m_simulationManager(nullptr)
{

  setupUi(this);

  s_categoryIcon.addFile(":/HydroCoupleComposer/close", QSize(), QIcon::Mode::Normal, QIcon::State::Off);
  s_categoryIcon.addFile(":/HydroCoupleComposer/open" , QSize(), QIcon::Mode::Normal, QIcon::State::On );
  s_adaptedOutput = QIcon(":/HydroCoupleComposer/adaptedoutput");
  s_adaptedOutputFactoryIcon = QIcon(":/HydroCoupleComposer/adaptedoutputfactory");
  s_adaptedOutputComponentIcon = QIcon(":/HydroCoupleComposer/adaptedoutputcomponent");
  s_workflowIcon = QIcon(":/HydroCoupleComposer/workflow");
  s_modelComponentIcon = QIcon(":/HydroCoupleComposer/hydrocouplecomposer");

  m_project = new HydroCoupleProject(this);
  m_simulationManager = new SimulationManager(m_project);

  qRegisterMetaType<QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>>();
  qRegisterMetaType<QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>>("QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>");

  m_argumentDialog = new ArgumentDialog(this);
  m_preferencesDialog = new PreferencesDialog(this);

  new QXMLSyntaxHighlighter(m_argumentDialog->textEditInput);

  m_modelStatusItemModel = new ModelStatusItemModel(this);
  m_modelComponentInfoModel = new QStandardItemModel(this);
  m_adaptedOutputFactoriesModel = new QStandardItemModel(this);
  m_workflowComponentModel = new QStandardItemModel(this);
  m_propertyModel = new QPropertyModel(this);

  m_qVariantPropertyHolder = new QVariantHolderHelper(QVariant() , this);

  m_graphicsContextMenu = new QMenu(this);
  m_treeViewModelComponentInfoContextMenu = new QMenu(this);
  m_treeViewAdaptedOutputFactoryComponentInfoContextMenu = new QMenu(this);
  m_treeViewWorkflowComponentInfoContextMenu = new QMenu(this);

  initializeGUIComponents();

  initializeModelComponentInfoTreeView();
  initializeAdaptedOutputFactoryComponentInfoTreeView();
  initializeWorkflowComponentInfoTreeView();

  initializePropertyGrid();
  initializeActions();
  initializeContextMenus();
  initializeSignalSlotConnections();
  initializeProjectSignalSlotConnections();
  initializeSimulationStatusTreeView();
  readSettings();
  onUpdateRecentFiles();
}

HydroCoupleComposer::~HydroCoupleComposer()
{
  //  m_argumentDialog->close();
  //  delete m_argumentDialog;

  if(m_simulationManager)
  {
    delete m_simulationManager;
    m_simulationManager = nullptr;
  }
}

HydroCoupleProject* HydroCoupleComposer::project() const
{
  return m_project;
}

void HydroCoupleComposer::setProject(HydroCoupleProject *project)
{
  QList<QDir> existingDirectories;
  QList<QFileInfo> existingLibraries;

  if(m_project)
  {

    for(QDir dir : m_project->componentManager()->componentDirectories())
      existingDirectories.append(dir);

    for(QString id : m_project->componentManager()->modelComponentInfoIdentifiers())
    {
      IModelComponentInfo *modelComponentInfo = m_project->componentManager()->findModelComponentInfo(id);
      existingLibraries.append(modelComponentInfo->libraryFilePath());
    }

    for(QString id : m_project->componentManager()->adaptedFactoryComponentInfoIdentifiers())
    {
      IAdaptedOutputFactoryComponentInfo *modelComponentInfo = m_project->componentManager()->findAdaptedOutputFactoryComponentInfo(id);
      existingLibraries.append(modelComponentInfo->libraryFilePath());
    }

    for(QString id : m_project->componentManager()->workflowComponentInfoIdentifiers())
    {
      IWorkflowComponentInfo *modelComponentInfo = m_project->componentManager()->findWorkflowComponentInfo(id);
      existingLibraries.append(modelComponentInfo->libraryFilePath());
    }


    delete m_project;
    m_project = nullptr;


  }

  m_project = project;

  for(QDir dir : existingDirectories)
    project->componentManager()->addComponentDirectory(dir);

  for(QFileInfo file : existingLibraries)
    project->componentManager()->loadComponent(file);

  m_simulationManager = new SimulationManager(m_project);

  setWindowTitle(m_project->projectFile().fileName() + "[*] - HydroCouple Composer");
  setWindowModified(m_project->hasChanges());

  initializeProjectSignalSlotConnections();



  for(GModelComponent* modelComponent : m_project->modelComponents())
  {
    onModelComponentAdded(modelComponent);
  }

  for(const QString &cInfo : m_project->componentManager()->modelComponentInfoIdentifiers())
  {
    onModelComponentInfoLoaded(cInfo);
  }

  for(const QString &adapInfo : m_project->componentManager()->adaptedFactoryComponentInfoIdentifiers())
  {
    onAdaptedOutputFactoryComponentInfoLoaded(adapInfo);
  }

  for(const QString &workflowInfo : m_project->componentManager()->workflowComponentInfoIdentifiers())
  {
    onWorkflowComponentInfoLoaded(workflowInfo);
  }

  connect(m_simulationManager, &SimulationManager::postMessage , this , &HydroCoupleComposer::onPostMessage);
  connect(m_simulationManager, &SimulationManager::setProgress , this , &HydroCoupleComposer::onSetProgress);

  onProjectWorkflowComponentChanged(m_project->workflowComponent());

  onZoomExtent();

}

void HydroCoupleComposer::closeEvent(QCloseEvent* event)
{
  writeSettings();

  if(m_project->hasChanges())
  {
    if (QMessageBox::question(this, "Save ?", "You have changes that need to be saved, Do you want to save ?",
                              QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
    {
      QString file;

      if(m_project->projectFile().exists())
      {
        m_project->onSaveProject();
      }
      else if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,
                                                     "HydroCouple Composer Project (*hcp)")).isEmpty())
      {
        m_project->onSaveProjectAs(QFileInfo(file));
      }
    }
  }

  graphicsViewHydroCoupleComposer->scene()->blockSignals(true);

  delete m_project;
  m_project = nullptr;

  graphicsViewHydroCoupleComposer->scene()->blockSignals(false);

  HydroCoupleVis::getInstance()->close();
  m_argumentDialog->close();

  event->accept();
}

void HydroCoupleComposer::dragMoveEvent(QDragMoveEvent* event)
{
  event->accept();
}

void HydroCoupleComposer::dragEnterEvent(QDragEnterEvent * event)
{
  bool hasValid = false;
  if(event->mimeData()->hasUrls())
  {
    foreach (const QUrl &url, event->mimeData()->urls())
    {
      QFileInfo fileInfo(url.toLocalFile());
      QString extension = fileInfo.suffix();

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
      if(!extension.compare("dll" , Qt::CaseInsensitive))
#elif __unix__ // all unices, not all compilers
      if(!extension.compare("so" , Qt::CaseInsensitive))
#elif __linux__
      if(!extension.compare("so" , Qt::CaseInsensitive))
#elif __APPLE__
      if(!extension.compare("dylib" , Qt::CaseInsensitive))
#endif
      {
        hasValid = true;
      }
      else if(!extension.compare("hcp" , Qt::CaseInsensitive))
      {
        hasValid = true;
      }
      else if(!extension.compare("hcc" , Qt::CaseInsensitive))
      {
        hasValid = true;
      }
    }

    if(hasValid)
    {
      event->acceptProposedAction();
    }
  }
}

void HydroCoupleComposer::dropEvent(QDropEvent * event)
{
  if(event->mimeData()->hasUrls())
  {
    foreach (const QUrl &url, event->mimeData()->urls())
    {
      QFileInfo fileInfo(url.toLocalFile());
      openFile(fileInfo);
    }
  }
}

void HydroCoupleComposer::mousePressEvent(QMouseEvent * event)
{
  event->accept();
}

void HydroCoupleComposer::keyPressEvent(QKeyEvent * event)
{
  if (event->key() == Qt::Key::Key_Delete && (m_selectedModelComponents.count() || m_selectedConnections.count()))
  {

    if (QMessageBox::question(this, "Delete ?", "Are you sure you want to delete selected items ?",
                              QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
    {

      graphicsViewHydroCoupleComposer->scene()->blockSignals(true);

      if(m_selectedConnections.length())
      {
        for(GConnection* connection : m_selectedConnections)
        {
          connection->producer()->deleteConnection(connection);
        }

        m_selectedConnections.clear();
      }


      if(m_selectedModelComponents.length())
      {
        for (GModelComponent* model : m_selectedModelComponents)
        {
          QString id = model->modelComponent()->id();

          if (m_project->deleteModelComponent(model))
          {
            setStatusTip(id + " has been removed");
          }
        }

        m_selectedModelComponents.clear();
      }

      graphicsViewHydroCoupleComposer->scene()->blockSignals(false);

    }

    m_propertyModel->setData(QVariant());
  }
  else if( event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier)
  {

    for(QGraphicsItem* item : graphicsViewHydroCoupleComposer->scene()->items())
    {
      item->setSelected(true);
    }

  }
}

bool HydroCoupleComposer::openFile(const QFileInfo& file)
{

  onPostMessage("Opening file \"" + file.absoluteFilePath() + "\"");
  m_lastOpenedFilePath = file.absoluteFilePath();
  m_lastPath = file.absolutePath();
  bool opened = false;

  QString extension = file.suffix();

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
  if(!extension.compare("dll" , Qt::CaseInsensitive))
#elif __unix__ // all unices, not all compilers
  if(!extension.compare("so" , Qt::CaseInsensitive))
#elif __linux__
  if(!extension.compare("so" , Qt::CaseInsensitive))
#elif __APPLE__
  if(!extension.compare("dylib" , Qt::CaseInsensitive))
#endif
  {
    if(m_project->componentManager()->loadComponent(file))
    {
      opened = true;
    }
  }
  else if(!extension.compare("hcp" , Qt::CaseInsensitive))
  {
    if(m_project->hasChanges())
    {
      if (QMessageBox::question(this, "Save ?", "You have changes that need to be saved, Do you want to save ?",
                                QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
      {
        QString file;

        if(m_project->projectFile().exists())
        {
          m_project->onSaveProject();
        }
        else if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,
                                                       "HydroCouple Composer Project (*.hcp)")).isEmpty())
        {
          m_project->onSaveProjectAs(file);
        }
      }
    }

    if(m_project)
    {
      delete m_project;
      m_project = nullptr;
    }

    HydroCoupleProject* project;

    QList<QString> errorMessages;

    if((project = HydroCoupleProject::readProjectFile(file, errorMessages)))
    {
      setProject(project);
      opened = true;
    }
    else
    {
      project = new HydroCoupleProject(this);
      setProject(project);
    }

    for(QString message : errorMessages)
    {
      onPostMessage(message);
    }
  }
  else if(!extension.compare("hcc" , Qt::CaseInsensitive))
  {
    QList<QString> errorMessages;

    GModelComponent* modelComponent = GModelComponent::readComponentFile(file,m_project,errorMessages);

    if(modelComponent)
    {
      modelComponent->setProject(m_project);
      m_project->addModelComponent(modelComponent);
      opened = true;
    }

    for(QString message : errorMessages)
    {
      onPostMessage(message);
    }
  }

  onUpdateRecentFiles();

  return opened;
}

void HydroCoupleComposer::readSettings()
{
  onPostMessage("Reading settings");

  m_settings.beginGroup("HydroCoupleComposer::MainWindow");
  {
    //!HydroCoupleComposer GUI
    this->restoreState(m_settings.value("HydroCoupleComposer::WindowState", saveState()).toByteArray());
    this->setWindowState((Qt::WindowState)m_settings.value("HydroCoupleComposer::WindowStateEnum", (int)this->windowState()).toInt());
    this->setGeometry(m_settings.value("HydroCoupleComposer::Geometry", geometry()).toRect());

    //!recent files
    m_recentFiles = m_settings.value("HydroCoupleComposer::RecentFiles", m_recentFiles).toStringList();
    m_lastOpenedFilePath = m_settings.value("HydroCoupleComposer::LastPath", m_lastOpenedFilePath).toString();

    onUpdateRecentFiles();

  }
  m_settings.endGroup();

  m_argumentDialog->readSettings();

  //!last path opened

  onPostMessage("Finished reading settings");
}

void HydroCoupleComposer::writeSettings()
{
  onPostMessage("Writing settings");

  m_settings.beginGroup("HydroCoupleComposer::MainWindow");
  m_settings.setValue("HydroCoupleComposer::WindowState", saveState());
  m_settings.setValue("HydroCoupleComposer::WindowStateEnum", (int)this->windowState());
  m_settings.setValue("HydroCoupleComposer::Geometry", geometry());
  m_settings.setValue("HydroCoupleComposer::RecentFiles", m_recentFiles);
  m_settings.setValue("HydroCoupleComposer::LastPath", m_lastOpenedFilePath);
  m_settings.endGroup();


  m_argumentDialog->writeSettings();
  onPostMessage("Finished writing settings");
}

void HydroCoupleComposer::initializeGUIComponents()
{
  //Progressbar

  tabifyDockWidget(dockWidgetModelComponents, dockWidgetAdaptedOutputFactory);
  tabifyDockWidget(dockWidgetAdaptedOutputFactory, dockWidgetWorkflowComponents);
  tabifyDockWidget(dockWidgetSimulationStatus,dockWidgetMessageLogs);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setMinimum(0);
  m_progressBar->setMaximum(100);
  m_progressBar->setVisible(false);
  m_progressBar->setToolTip("Progress bar");
  m_progressBar->setWhatsThis("Sets the progress of a process");
  m_progressBar->setStatusTip("Progress bar");

  m_progressBar->setMaximumWidth(300);
  m_progressBar->setMaximumHeight(18);

  m_progressBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
  statusBarMain->setContentsMargins(0, 0, 0, 0);

  statusBarMain->addPermanentWidget(m_progressBar);

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!

  actionPreferences->setText("Options");
  menuTools->addAction(actionPreferences);
  menuHelp->addAction(actionAbout);

#elif __unix__ // all unices, not all compilers
#elif __linux__
#elif __APPLE__

  for (QObject* child : menuBarMain->children())
  {
    QMenu* macMenu;

    if((macMenu = dynamic_cast<QMenu*>(child)))
    {
      if(!macMenu->title().compare("File" , Qt::CaseInsensitive))
      {
        macMenu->addAction(actionAbout);
        macMenu->addSeparator();
        macMenu->addAction(actionPreferences);
        macMenu->addSeparator();
      }
      else if(!macMenu->title().compare("Help" , Qt::CaseInsensitive))
      {
        macMenu->addAction("");
      }
    }
  }

#endif


}

void HydroCoupleComposer::initializeModelComponentInfoTreeView()
{

  m_modelComponentInfoModel->clear();

  QStandardItem* rootItem = m_modelComponentInfoModel->invisibleRootItem();
  rootItem->setText("Model Components");
  rootItem->setToolTip("Model Components");
  rootItem->setStatusTip("Model Components");
  rootItem->setWhatsThis("Model Components");

  rootItem->setIcon(s_categoryIcon);

  QStringList temp; temp << "";
  m_modelComponentInfoModel->setHorizontalHeaderLabels(temp);
  m_modelComponentInfoModel->setSortRole(Qt::DisplayRole);

  treeViewModelComponentInfos->setModel(m_modelComponentInfoModel);
  treeViewModelComponentInfos->expand(rootItem->index());
}

void HydroCoupleComposer::initializeAdaptedOutputFactoryComponentInfoTreeView()
{
  m_adaptedOutputFactoriesModel->clear();

  QStringList temp; temp << "";
  m_adaptedOutputFactoriesModel->setHorizontalHeaderLabels(temp);
  m_adaptedOutputFactoriesModel->setSortRole(Qt::DisplayRole);

  QStandardItem* rootItem = m_adaptedOutputFactoriesModel->invisibleRootItem();
  rootItem->setText("Adapted Outputs");
  rootItem->setToolTip("Adapted Outputs");
  rootItem->setStatusTip("Adapted Outputs");
  rootItem->setWhatsThis("Adapted Outputs");

  rootItem->setIcon(s_adaptedOutputFactoryIcon);

  m_externalAdaptedOutputFactoriesStandardItem = new QStandardItem("AdaptedOutput Factory Components");
  m_externalAdaptedOutputFactoriesStandardItem->setIcon(s_adaptedOutputFactoryIcon);
  m_externalAdaptedOutputFactoriesStandardItem->setDragEnabled(false);

  m_internalAdaptedOutputFactoriesStandardItem = new QStandardItem("Internal AdaptedOutput Factories");
  m_internalAdaptedOutputFactoriesStandardItem->setIcon(s_adaptedOutputFactoryIcon);
  m_internalAdaptedOutputFactoriesStandardItem->setDragEnabled(false);

  rootItem->appendRow(m_externalAdaptedOutputFactoriesStandardItem);
  rootItem->appendRow(m_internalAdaptedOutputFactoriesStandardItem);

  treeViewAdaptedOutputFactoryComponents->setModel(m_adaptedOutputFactoriesModel);
  treeViewAdaptedOutputFactoryComponents->expand(rootItem->index());
}

void HydroCoupleComposer::initializeWorkflowComponentInfoTreeView()
{
  m_workflowComponentModel->clear();

  QStringList temp; temp << "";
  m_workflowComponentModel->setHorizontalHeaderLabels(temp);
  m_workflowComponentModel->setSortRole(Qt::DisplayRole);

  QStandardItem *rootItem = m_workflowComponentModel->invisibleRootItem();
  rootItem->setText("Workflow Components");
  rootItem->setToolTip("Workflow Components");
  rootItem->setStatusTip("Workflow Components");
  rootItem->setWhatsThis("Workflow Components");

  rootItem->setIcon(s_workflowIcon);

  connect(m_workflowComponentModel, SIGNAL(itemChanged(QStandardItem *)), this, SLOT(onWorkflowComponentCheckStateChanged(QStandardItem *)));

  treeViewWorkflowComponents->setModel(m_workflowComponentModel);
  treeViewWorkflowComponents->expand(rootItem->index());

}

void HydroCoupleComposer::initializeSimulationStatusTreeView()
{
  treeViewSimulationStatus->setModel(m_modelStatusItemModel);
  ModelStatusItemStyledItemDelegate* statusProgressStyledItemDelegate = new ModelStatusItemStyledItemDelegate(treeViewSimulationStatus);
  treeViewSimulationStatus->setItemDelegate(statusProgressStyledItemDelegate);
  treeViewSimulationStatus->header()->resetDefaultSectionSize();
  //  treeViewSimulationStatus->setColumnWidth(3,400);
  //treeViewSimulationStatus->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
}

void HydroCoupleComposer::initializePropertyGrid()
{
  QPropertyItemDelegate* modelDelegate = new QPropertyItemDelegate(m_propertyModel);
  treeViewProperties->setModel(m_propertyModel);

  CustomPropertyItems::registerCustomPropertyItems(m_propertyModel);

  treeViewProperties->setEditTriggers(QAbstractItemView::AllEditTriggers);
  treeViewProperties->setItemDelegate(modelDelegate);
  treeViewProperties->setAlternatingRowColors(true);
  treeViewProperties->expandToDepth(1);
  treeViewProperties->resizeColumnToContents(0);
}

void HydroCoupleComposer::initializeActions()
{
  QActionGroup* actionGroupSelect = new QActionGroup(this);
  actionGroupSelect->addAction(actionDefaultSelect);
  actionGroupSelect->addAction(actionZoomIn);
  actionGroupSelect->addAction(actionZoomOut);
  actionGroupSelect->addAction(actionPan);
  actionDefaultSelect->setChecked(true);

  m_recentFilesMenu = menuFile->addMenu("Recent Files");
  m_recentFilesMenu->setToolTip("Recent Files");
  m_recentFilesMenu->setWhatsThis("Recent Files");
  m_recentFilesMenu->setStatusTip("Recent Files");
  m_recentFilesMenu->setIcon(QIcon(":/HydroCoupleComposer/recentfiles"));
  menuFile->insertMenu(actionClose, m_recentFilesMenu);

  for (int i = 0; i < 20; i++)
  {
    QAction* fileAction = m_recentFilesMenu->addAction(QIcon(":/HydroCoupleComposer/addfile"), "Recent File");
    fileAction->setVisible(false);
    connect(fileAction, SIGNAL(triggered()), this, SLOT(onOpenRecentFile()));
    m_recentFilesActions[i] = fileAction;
  }

  //!Clear Recent Files Action;
  m_recentFilesMenu->addSeparator();
  m_clearRecentFilesAction = m_recentFilesMenu->addAction(QIcon(":/HydroCoupleComposer/clear"), "Clear Recent Files");
  connect(m_clearRecentFilesAction, SIGNAL(triggered()), this, SLOT(onClearRecentFiles()));

  textEdit->addAction(actionClearMessageLogs);
  textEdit->addAction(actionCopyMessageLogsToClipboard);

}

void HydroCoupleComposer::initializeSignalSlotConnections()
{

  connect(actionAddComponentLibraryDirectory, &QAction::triggered, this, &HydroCoupleComposer::onAddComponentLibraryDirectory);

  //ModelComponentInfoTreeview
  connect(treeViewModelComponentInfos, SIGNAL(clicked(const QModelIndex&)), this, SLOT(onModelComponentInfoClicked(const QModelIndex&)));
  connect(treeViewModelComponentInfos, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(onModelComponentInfoDoubleClicked(const QModelIndex&)));


  //ModelComponentStatus
  connect(treeViewSimulationStatus, SIGNAL(clicked(const QModelIndex&)) , this , SLOT(onModelComponentStatusItemClicked(const QModelIndex&)));
  connect(treeViewSimulationStatus, SIGNAL(doubleClicked(const QModelIndex&)) , this , SLOT(onModelComponentStatusItemDoubleClicked(const QModelIndex&)));


  //Adaptedout factory
  connect(treeViewAdaptedOutputFactoryComponents , SIGNAL(clicked(const QModelIndex&)) , SLOT(onAdaptedOutputClicked(const QModelIndex&)));
  connect(treeViewAdaptedOutputFactoryComponents , SIGNAL(doubleClicked(const QModelIndex&)) , SLOT(onAdaptedOutputDoubleClicked(const QModelIndex&)));
  connect(actionInsertAdaptedOutput, &QAction::toggled, this, &HydroCoupleComposer::onInsertAdaptedOutput);

  //WorkflowComponent
  connect(treeViewWorkflowComponents, SIGNAL(clicked(const QModelIndex&)), this, SLOT(onWorkflowComponentClicked(const QModelIndex&)));

  //Toolbar actions
  connect(actionDefaultSelect, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionZoomIn, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionZoomOut, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionPan, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionZoomExtent, SIGNAL(triggered()), this, SLOT(onZoomExtent()));
  connect(actionEditSelected, SIGNAL(triggered()), this, SLOT(onEditSelectedItem()));

  connect(actionNew, SIGNAL(triggered()), this, SLOT(onNewProject()));
  connect(actionClose, SIGNAL(triggered()), this, SLOT(close()));
  connect(actionOpen, SIGNAL(triggered()), this, SLOT(onOpenFiles()));
  connect(actionSave, SIGNAL(triggered()), this, SLOT(onSave()));
  connect(actionSave_As, SIGNAL(triggered()), this, SLOT(onSaveAs()));
  connect(actionExport , SIGNAL(triggered()) , this , SLOT(onExport()));
  connect(actionPrint, SIGNAL(triggered()), this, SLOT(onPrint()));
  connect(actionClearAllSettings, SIGNAL(triggered()), this, SLOT(onClearSettings()));
  connect(actionAddComponent, SIGNAL(triggered()), this, SLOT(onAddModelComponent()));
  connect(actionExecute, SIGNAL(triggered()), this, SLOT(onRunSimulation()));
  connect(actionHydroCoupleVis,  SIGNAL(triggered()), this, SLOT(onShowHydroCoupleVis()));

  connect(actionValidateModelComponentInfo , SIGNAL ( triggered()) , this , SLOT( onValidateComponentLibrary()));
  connect(actionValidateAdaptedOutputFactoryComponentInfo , SIGNAL ( triggered()) , this , SLOT( onValidateComponentLibrary()));
  connect(actionValidateWorkflowComponentInfo , SIGNAL ( triggered()) , this , SLOT( onValidateComponentLibrary()));

  connect(actionBrowseModelComponentInfoFolder , SIGNAL(triggered()) , this , SLOT(onBrowseToComponentLibraryPath()));
  connect(actionBrowseAdaptedOutputFactoryComponentInfoFolder , SIGNAL(triggered()) , this , SLOT(onBrowseToComponentLibraryPath()));
  connect(actionBrowseWorkflowComponentInfoFolder , SIGNAL(triggered()) , this , SLOT(onBrowseToComponentLibraryPath()));

  connect(actionLoadModelComponentLibrary , SIGNAL(triggered()) , this , SLOT(onLoadComponentLibrary()));
  connect(actionUnloadModelComponentLibrary , SIGNAL(triggered()) , this , SLOT(onUnloadComponentLibrary()));
  connect(actionUnloadAdaptedOutputFactoryComponentLibrary, SIGNAL(triggered()) , this , SLOT(onUnloadComponentLibrary()));
  connect(actionUnloadWorkflowComponentLibrary, SIGNAL(triggered()) , this , SLOT(onUnloadComponentLibrary()));

  connect(actionDefaultSelect, SIGNAL(toggled(bool)), actionCreateConnection, SLOT(setEnabled(bool)));
  connect(actionCreateConnection, SIGNAL(toggled(bool)), this, SLOT(onCreateConnection(bool)));
  connect(actionCloneModelComponent, SIGNAL(triggered()), this, SLOT(onCloneModelComponents()));
  connect(actionSetTrigger, SIGNAL(triggered()), this, SLOT(onSetAsTrigger()));
  connect(actionDeleteComponent, SIGNAL(triggered()), this, SLOT(onDeleteSelectedComponents()));
  connect(actionDeleteConnection, SIGNAL(triggered()), this, SLOT(onDeleteSelectedConnections()));
  connect(actionAlignTop, SIGNAL(triggered()), this, SLOT(onAlignTop()));
  connect(actionAlignBottom, SIGNAL(triggered()), this, SLOT(onAlignBottom()));
  connect(actionAlignLeft, SIGNAL(triggered()), this, SLOT(onAlignLeft()));
  connect(actionAlignRight, SIGNAL(triggered()), this, SLOT(onAlignRight()));
  connect(actionAlignVerticalCenters, SIGNAL(triggered()), this, SLOT(onAlignVerticalCenters()));
  connect(actionAlignHorizontalCenters, SIGNAL(triggered()), this, SLOT(onAlignHorizontalCenters()));
  connect(actionDistributeTopEdges, SIGNAL(triggered()), this, SLOT(onDistributeTopEdges()));
  connect(actionDistributeBottomEdges, SIGNAL(triggered()), this, SLOT(onDistributeBottomEdges()));
  connect(actionDistributeLeftEdges, SIGNAL(triggered()), this, SLOT(onDistributeLeftEdges()));
  connect(actionDistributeRightEdges, SIGNAL(triggered()), this, SLOT(onDistributeRightEdges()));
  connect(actionDistributeVerticalCenters, SIGNAL(triggered()), this, SLOT(onDistributeVerticalCenters()));
  connect(actionDistributeHorizontalCenters, SIGNAL(triggered()), this, SLOT(onDistributeHorizontalCenters()));
  connect(actionLayoutComponents , SIGNAL(triggered()) , this , SLOT(onLayoutComponents()));


  connect(treeViewModelComponentInfos , SIGNAL(customContextMenuRequested(const QPoint&)) , this , SLOT(onTreeViewModelComponentInfoContextMenuRequested(const QPoint&)));
  connect(treeViewAdaptedOutputFactoryComponents , SIGNAL(customContextMenuRequested(const QPoint&)) , this , SLOT(onTreeViewAdaptedOutputFactoryComponentContextMenuRequested(const QPoint&)));
  connect(treeViewWorkflowComponents , SIGNAL(customContextMenuRequested(const QPoint&)) , this , SLOT(onTreeViewWorkflowComponentContextMenuRequested(const QPoint&)));

  connect(graphicsViewHydroCoupleComposer , SIGNAL(customContextMenuRequested(const QPoint&)) , this , SLOT(onGraphicsViewHydroCoupleComposerContextMenuRequested(const QPoint&)));

  //graphicsView
  //   connect(graphicsViewHydroCoupleComposer, SIGNAL(statusChanged(const QString&)), this->statusBarMain, SLOT(showMessage(const QString&)));
  connect(graphicsViewHydroCoupleComposer, &GraphicsView::itemDropped, this, &HydroCoupleComposer::onItemDroppedInGraphicsView);

  //graphicsScene
  connect(graphicsViewHydroCoupleComposer->scene(), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));

  //about
  connect(actionAbout , SIGNAL(triggered()) , this,  SLOT(onAbout()));

  //preferences
  connect(actionPreferences, SIGNAL(triggered()) , this,  SLOT(onPreferences()));

  //message logs
  connect(actionClearMessageLogs , SIGNAL(triggered()) , textEdit , SLOT(clear()));
  connect(actionCopyMessageLogsToClipboard , SIGNAL(triggered()) , textEdit , SLOT(copy()));

  connect(actionEditSelected , SIGNAL(triggered()) , this , SLOT(onEditSelectedItem()));
  connect(m_argumentDialog , SIGNAL(postValidationMessage(const QString&)) , this, SLOT(onPostMessage(const QString&)));

  connect(actionInitializeComponent , SIGNAL(triggered()) , this , SLOT(onInitializeComponent()));
  connect(actionValidateComponent , SIGNAL(triggered()) , this , SLOT(onValidateComponent()));

}

void HydroCoupleComposer::initializeProjectSignalSlotConnections()
{
  //HydroCoupleProject
  connect(m_project, SIGNAL(componentAdded(GModelComponent* )), this, SLOT(onModelComponentAdded(GModelComponent* )));
  connect(m_project, SIGNAL(componentDeleting(GModelComponent* )), this, SLOT(onModelComponentDeleting(GModelComponent* )));
  connect(m_project, SIGNAL(stateModified(bool)), this, SLOT(onProjectHasChanges(bool)));
  connect(m_project , SIGNAL(postMessage(const QString&)) , this , SLOT(onPostMessage(const QString&)));
  connect(m_project, SIGNAL(setProgress(bool,int,int,int)), this, SLOT(onSetProgress(bool,int,int,int)));
  connect(actionReloadComponentConnections , SIGNAL(triggered()) , m_project , SLOT(onReloadConnections()));

  //component manager
  connect(m_project->componentManager(), &ComponentManager::modelComponentInfoLoaded, this, &HydroCoupleComposer::onModelComponentInfoLoaded);
  connect(m_project->componentManager(), &ComponentManager::modelComponentInfoUnloaded, this, &HydroCoupleComposer::onModelComponentInfoUnloaded);

  connect(m_project->componentManager(), &ComponentManager::adaptedOutputFactoryComponentInfoLoaded, this, &HydroCoupleComposer::onAdaptedOutputFactoryComponentInfoLoaded);
  connect(m_project->componentManager(), &ComponentManager::adaptedOutputFactoryComponentInfoUnloaded, this, &HydroCoupleComposer::onAdaptedOutputFactoryComponentInfoUnloaded);

  connect(m_project->componentManager(), &ComponentManager::workflowComponentInfoLoaded, this, &HydroCoupleComposer::onWorkflowComponentInfoLoaded);
  connect(m_project->componentManager(), &ComponentManager::workflowComponentInfoUnloaded, this, &HydroCoupleComposer::onWorkflowComponentInfoUnloadedLoaded);

  connect(m_project->componentManager(), &ComponentManager::postMessage, this, &HydroCoupleComposer::onPostMessage);
  connect(m_project->componentManager(), SIGNAL(setProgress(bool,int,int,int)), this, SLOT(onSetProgress(bool,int,int,int)));

  connect(actionCancelSimulation, SIGNAL(triggered()), m_simulationManager, SLOT(onStopSimulation()));
  connect(m_simulationManager, SIGNAL(isBusy(bool)), actionCancelSimulation, SLOT(setEnabled(bool)));

  connect(m_project, &HydroCoupleProject::workflowComponentChanged , this, &HydroCoupleComposer::onProjectWorkflowComponentChanged);
}

void HydroCoupleComposer::initializeContextMenus()
{
  //Component Info
  m_treeViewModelComponentInfoContextMenu->addAction(actionAddComponent);
  m_treeViewModelComponentInfoContextMenu->addAction(actionValidateModelComponentInfo);
  m_treeViewModelComponentInfoContextMenu->addAction(actionLoadModelComponentLibrary);
  m_treeViewModelComponentInfoContextMenu->addAction(actionUnloadModelComponentLibrary);
  m_treeViewModelComponentInfoContextMenu->addAction(actionBrowseModelComponentInfoFolder);

  m_treeViewAdaptedOutputFactoryComponentInfoContextMenu->addAction(actionValidateAdaptedOutputFactoryComponentInfo);
  m_treeViewAdaptedOutputFactoryComponentInfoContextMenu->addAction(actionInsertAdaptedOutput);
  m_treeViewAdaptedOutputFactoryComponentInfoContextMenu->addAction(actionLoadModelComponentLibrary);
  m_treeViewAdaptedOutputFactoryComponentInfoContextMenu->addAction(actionUnloadAdaptedOutputFactoryComponentLibrary);
  m_treeViewAdaptedOutputFactoryComponentInfoContextMenu->addAction(actionBrowseAdaptedOutputFactoryComponentInfoFolder);

  m_treeViewWorkflowComponentInfoContextMenu->addAction(actionValidateWorkflowComponentInfo);
  m_treeViewWorkflowComponentInfoContextMenu->addAction(actionLoadModelComponentLibrary);
  m_treeViewWorkflowComponentInfoContextMenu->addAction(actionUnloadWorkflowComponentLibrary);
  m_treeViewWorkflowComponentInfoContextMenu->addAction(actionBrowseWorkflowComponentInfoFolder);

  m_graphicsContextMenu->addAction(actionInitializeComponent);
  m_graphicsContextMenu->addAction(actionValidateComponent);
  m_graphicsContextMenu->addAction(actionDeleteComponent);
  m_graphicsContextMenu->addAction(actionSetTrigger);
  m_graphicsContextMenu->addAction(actionCloneModelComponent);

  m_graphicsContextMenu->addSeparator();

  m_graphicsContextMenu->addAction(actionCreateConnection);
  m_graphicsContextMenu->addAction(actionDeleteConnection);

  m_graphicsContextMenu->addSeparator();

  QMenu* layout = new QMenu("Component Layout Options",m_graphicsContextMenu);
  layout->addAction(actionLayoutComponents);
  layout->addSeparator();
  layout->addAction(actionAlignTop);
  layout->addAction(actionAlignBottom);
  layout->addAction(actionAlignLeft);
  layout->addAction(actionAlignRight);
  layout->addSeparator();
  layout->addAction(actionAlignVerticalCenters);
  layout->addAction(actionAlignHorizontalCenters);
  layout->addSeparator();
  layout->addAction(actionDistributeTopEdges);
  layout->addAction(actionDistributeBottomEdges);
  layout->addAction(actionDistributeLeftEdges);
  layout->addAction(actionDistributeRightEdges);
  layout->addAction(actionDistributeVerticalCenters);
  layout->addAction(actionDistributeHorizontalCenters);
  m_graphicsContextMenu->addMenu(layout);

  m_graphicsContextMenu->addSeparator();

  m_graphicsContextMenu->addAction(actionPrint);
}

void HydroCoupleComposer::resetAdaptedOutputFactoryModel(QStandardItem *standardItem)
{

  QMap<QString,QVariant> data = standardItem->data(Qt::UserRole + 1).toMap();

  if(data.contains("AdaptedOutputFactoryComponentInfo") )
  {
    if(standardItem->rowCount())
    {
      standardItem->removeRows(0,standardItem->rowCount());
    }

    GOutput *output = nullptr;
    GInput  *input = nullptr;

    if(m_selectedConnections.length() == 1 &&
       (output = dynamic_cast<GOutput*>(m_selectedConnections[0]->producer())) &&
       (input = dynamic_cast<GInput*>(m_selectedConnections[0]->consumer())))
    {

      QString adaptedOutputFactoryComponentId = data["AdaptedOutputFactoryComponentInfo"].toString();
      QList<IIdentity*> availableAdaptors = m_project->componentManager()->getAvailableAdaptedOutputs(adaptedOutputFactoryComponentId,
                                                                                                      output->output(),input->input());
      for(IIdentity* identity : availableAdaptors)
      {
        QStandardItem* adapterId = new QStandardItem(s_adaptedOutput,identity->caption());

        QString desc ="<html>"
                      "<head>"
                      "<title>Adapted Output</title>"
                      "</head>"
                      "<h3 align=\"center\">[Component]</h3>"
                      "<body>"
                      "<h4 align=\"center\">[Caption]</h4>"
                      "<hr>"
                      "<p>[Description]</p>"
                      "</body>"
                      "</html>";

        QString html = QString(desc).replace("[Component]", identity->id())
                       .replace("[Caption]", identity->caption())
                       .replace("[Description]", identity->description());

        adapterId->setToolTip(html);
        adapterId->setWhatsThis(html);
        adapterId->setDragEnabled(true);

        QMap<QString,QVariant> idData;

        idData["AdaptedOutput"] = identity->id();
        idData["AdaptedOutputFactoryComponentInfo"] = adaptedOutputFactoryComponentId;

        adapterId->setData(idData, Qt::UserRole + 1);
        adapterId->setDragEnabled(true);

        standardItem->appendRow(adapterId);
      }

      treeViewAdaptedOutputFactoryComponents->expand(standardItem->index());
    }
  }
  else if(data.contains("AdaptedOutputFactory"))
  {
    if(standardItem->rowCount())
    {
      standardItem->removeRows(0,standardItem->rowCount());
    }

    GOutput *output = nullptr;
    GInput  *input = nullptr;

    if(m_selectedConnections.length() == 1 &&
       (output = dynamic_cast<GOutput*>(m_selectedConnections[0]->producer())) &&
       (input = dynamic_cast<GInput*>(m_selectedConnections[0]->consumer())))
    {

      GModelComponent *producerModelComponent = output->modelComponent();
      QString adaptedOutputFactoryId = data["AdaptedOutputFactory"].toString();
      IAdaptedOutputFactory *adaptedOutputFactory = producerModelComponent->getAdaptedOutputFactory(adaptedOutputFactoryId);

      if(adaptedOutputFactory)
      {
        QList<IIdentity*> availableAdaptors = adaptedOutputFactory->getAvailableAdaptedOutputIds(output->output(),input->input());

        for(IIdentity* identity : availableAdaptors)
        {
          QStandardItem* adapterId = new QStandardItem(s_adaptedOutput,identity->caption());

          QString desc ="<html>"
                        "<head>"
                        "<title>Adapted Output</title>"
                        "</head>"
                        "<h3 align=\"center\">[Component]</h3>"
                        "<body>"
                        "<h4 align=\"center\">[Caption]</h4>"
                        "<hr>"
                        "<p>[Description]</p>"
                        "</body>"
                        "</html>";

          QString html = QString(desc).replace("[Component]", identity->id())
                         .replace("[Caption]", identity->caption())
                         .replace("[Description]", identity->description());

          adapterId->setToolTip(html);
          adapterId->setWhatsThis(html);
          adapterId->setDragEnabled(true);

          QMap<QString,QVariant> idData;

          idData["AdaptedOutput"] = identity->id();
          idData["AdaptedOutputFactory"] = adaptedOutputFactoryId;
          idData["AdaptedOutputFactoryModelComponentInfo"] = producerModelComponent->modelComponent()->componentInfo()->id();

          adapterId->setData(idData, Qt::UserRole + 1);
          adapterId->setDragEnabled(true);

          standardItem->appendRow(adapterId);
        }
      }

      treeViewAdaptedOutputFactoryComponents->expand(standardItem->index());
    }
  }
  else
  {
    for(int i = 0; i < standardItem->rowCount(); i++)
    {
      QStandardItem *item = standardItem->child(i);
      resetAdaptedOutputFactoryModel(item);
    }
  }
}

void HydroCoupleComposer::createConnection(GExchangeItem *producer, GExchangeItem *consumer)
{
  if(producer != consumer &&
     (producer->nodeType() == GNode::Output || producer->nodeType() == GNode::AdaptedOutput) &&
     ( consumer->nodeType() == GNode::Input || consumer->nodeType() == GNode::MultiInput  || consumer->nodeType() == GNode::AdaptedOutput))
  {
    QString message;

    if(!producer->createConnection(consumer))
    {
      QMessageBox::critical(this,"Connection Error",message);
    }
  }

  for(GExchangeItem* exchangeItem : m_selectedExchangeItems)
  {
    exchangeItem->setSelected(false);
  }

  actionCreateConnection->setChecked(false);
}

QStandardItem* HydroCoupleComposer::findStandardItem(const QString &id, QStandardItem *parent, const QString &key, bool recursive)
{
  if (!id.isNull() && !id.isEmpty())
  {
    for (int i = 0; i < parent->rowCount(); i++)
    {
      QStandardItem* child = parent->child(i);

      QMap<QString,QVariant> data = child->data(Qt::UserRole + 1).toMap();

      if(data.contains(key) && !data[key].toString().compare(id, Qt::CaseInsensitive))
      {
        return child;
      }
      else if (recursive)
      {
        QStandardItem* c;

        if (child->hasChildren())
        {
          c = findStandardItem(id, child,key,recursive);

          if (c != child)
          {
            return c;
          }
        }
      }
    }
  }

  return nullptr;
}

QStandardItem *HydroCoupleComposer::findStandardItem(const QList<QString> &identifiers, QStandardItem* parent, const QList<QString> &keys, bool recursive)
{
  if (identifiers.size())
  {
    for (int i = 0; i < parent->rowCount(); i++)
    {
      QStandardItem* child = parent->child(i);

      QMap<QString,QVariant> data = child->data(Qt::UserRole + 1).toMap();

      bool found = false;
      int f = 0;

      for(QString key : keys)
      {
        if(data.contains(key) && !QString::compare(data[key].toString(), identifiers[f]))
        {
          found = true;
        }
        else
        {
          found = false;
          break;
        }

        f++;
      }

      if(found)
      {
        return child;
      }
      else if (recursive)
      {
        QStandardItem* c;

        if (child->hasChildren())
        {
          c = findStandardItem(identifiers, child,keys,recursive);

          if (c != child)
          {
            return c;
          }
        }
      }
    }
  }

  return nullptr;
}

void HydroCoupleComposer::addRemoveNodeToGraphicsView(GNode *node, bool add)
{
  if(add)
  {
    for(GConnection* connection : node->connections())
    {
      graphicsViewHydroCoupleComposer->scene()->addItem(connection);

      if(connection->consumer()->nodeType() != GNode::Input &&
         connection->consumer()->nodeType() != GNode::MultiInput)
      {
        addRemoveNodeToGraphicsView(connection->consumer(),add);
      }
    }

    graphicsViewHydroCoupleComposer->scene()->addItem(node);
  }
  else
  {
    for(GConnection* connection : node->connections())
    {
      graphicsViewHydroCoupleComposer->scene()->removeItem(connection);
      addRemoveNodeToGraphicsView(connection->consumer(),add);
    }
    graphicsViewHydroCoupleComposer->scene()->removeItem(node);
  }

}

#ifdef GRAPHVIZ_LIBRARY

void HydroCoupleComposer::layoutNode(Agraph_t* graph, QHash<GNode*, QString> &identifiers, GNode *node, int &currentIndex, bool addToGraph)
{
  if(addToGraph)
  {
    currentIndex++;
    char* nodeIdP = nullptr;
    QString nodeId = QString::number(currentIndex);
    stringToCharP(nodeId, nodeIdP);
    agnode(graph,nodeIdP,1);
    identifiers[node] = nodeId;
    delete[] nodeIdP;

    for(GConnection* connection : node->connections())
    {
      if(connection->consumer()->nodeType() != GNode::Component)
      {
        layoutNode(graph, identifiers ,connection->consumer() , currentIndex , addToGraph);
      }
    }
  }
  else
  {
    char* nodeIdP = nullptr;
    QString nodeId = identifiers[node];
    stringToCharP(nodeId, nodeIdP);
    Agnode_t* tnode = agnode(graph,nodeIdP,0);

    QPointF pos;
    pos.setX( ND_coord(tnode).x  - node->boundingRect().width() /2);
    pos.setY( ND_coord(tnode).y  - node->boundingRect().height() /2);
    node->setPos(pos);

    delete[] nodeIdP;

    for(GConnection* connection : node->connections())
    {
      if(connection->consumer()->nodeType() != GNode::Component)
      {
        layoutNode(graph, identifiers ,connection->consumer() , currentIndex, addToGraph);
      }
    }
  }

}

void HydroCoupleComposer::layoutEdges(Agraph_t *graph, const QHash<GNode*, QString> &identifiers, GNode *node)
{

  char nullChar[1];
  nullChar[0] = '0';

  QString fromId = identifiers[node];
  char* fromIdP = nullptr;
  stringToCharP(fromId, fromIdP);
  Agnode_t* fn = agnode(graph, fromIdP , 0);

  for(GConnection* connection : node->connections())
  {

    QString toId = identifiers[connection->consumer()];
    char* toIdP = nullptr;
    stringToCharP(toId, toIdP);
    Agnode_t* tn = agnode(graph, toIdP , 0);
    Agedge_t* edge = agedge(graph , fn , tn , nullChar , 0);

    if(!edge)
    {
      agedge(graph , fn , tn , nullChar , 1);
    }
    else
    {
      //qDebug() << "";
    }

    delete[] toIdP;

    if(connection->consumer()->connections().length() && connection->consumer()->nodeType() != GNode::Component)
    {
      layoutEdges(graph,identifiers,connection->consumer());
    }
  }

  delete[] fromIdP;
}

#endif

void HydroCoupleComposer::stringToCharP(const QString& text, char * & output)
{
  output = new char[text.length() + 1];
  strcpy(output , text.toStdString().c_str());
}

void HydroCoupleComposer::setRight(QGraphicsItem * graphicsItem, double right)
{
  double diff = right - graphicsItem->sceneBoundingRect().right();
  graphicsItem->setX(graphicsItem->pos().x() + diff);
}

void HydroCoupleComposer::setHorizontalCenter(QGraphicsItem * graphicsItem, double hcenter)
{
  double diff = hcenter - graphicsItem->sceneBoundingRect().center().x();
  graphicsItem->setX(graphicsItem->pos().x() + diff);
}

void HydroCoupleComposer::setBottom(QGraphicsItem * graphicsItem, double bottom)
{
  double diff = bottom - graphicsItem->sceneBoundingRect().bottom();
  graphicsItem->setY(graphicsItem->pos().y() + diff);
}

void HydroCoupleComposer::setVerticalCenter(QGraphicsItem * graphicsItem, double vcenter)
{
  double diff = vcenter - graphicsItem->sceneBoundingRect().center().y();
  graphicsItem->setY(graphicsItem->pos().y() + diff);
}

bool HydroCoupleComposer::compareLeftEdges(QGraphicsItem*   a, QGraphicsItem *  b)
{
  return a->pos().x() < b->pos().x();
}

bool HydroCoupleComposer::compareHorizontalCenters(QGraphicsItem*   a, QGraphicsItem *  b)
{
  return a->sceneBoundingRect().center().x() < b->sceneBoundingRect().center().x();
}

bool HydroCoupleComposer::compareRightEdges(QGraphicsItem*   a, QGraphicsItem *  b)
{
  return a->sceneBoundingRect().right() < b->sceneBoundingRect().right();
}

bool HydroCoupleComposer::compareTopEdges(QGraphicsItem*   a, QGraphicsItem *  b)
{
  return a->pos().y() < b->pos().y();
}

bool HydroCoupleComposer::compareVerticalCenters(QGraphicsItem*   a, QGraphicsItem *  b)
{
  return a->sceneBoundingRect().center().y() < b->sceneBoundingRect().center().y();
}

bool HydroCoupleComposer::compareBottomEdges(QGraphicsItem*   a, QGraphicsItem *  b)
{
  return a->sceneBoundingRect().bottom() < b->sceneBoundingRect().bottom();
}

void HydroCoupleComposer::onSetProgress(bool visible, int progress , int min , int max)
{
  m_progressBar->setVisible(visible);

  if(visible)
  {
    m_progressBar->setMinimum(min);
    m_progressBar->setMaximum(max);
    m_progressBar->setValue(progress);

    if(min == 0  && max == 0 && progress == 0)
    {
      m_progressBar->setTextVisible(true);
      m_progressBar->setFormat("Busy...");
    }
    else
    {
      m_progressBar->setTextVisible(false);
      m_progressBar->setFormat("");
    }
  }
  else
  {
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFormat("");
  }
}

void HydroCoupleComposer::onPostMessage(const QString& message)
{
  this->statusBarMain->showMessage(message ,50000);
  QString intMessage = QDateTime::currentDateTime().toString(Qt::ISODate) + " : " + message;
  textEdit->append(intMessage);
  textEdit->verticalScrollBar()->setValue(textEdit->verticalScrollBar()->maximum());
  printf("%s\n", intMessage.toStdString().c_str());
}

void HydroCoupleComposer::onPostToStatusBar(const QString& message)
{
  this->statusBarMain->showMessage(message ,50000);
  printf("%s\n", message.toStdString().c_str());}

void HydroCoupleComposer::onNewProject()
{
  if(m_project->hasChanges())
  {
    if (QMessageBox::question(this, "Save ?", "You have changes that need to be saved, Do you want to save ?",
                              QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
    {
      QString file;

      if(m_project->projectFile().exists())
      {
        m_project->onSaveProject();
      }
      else if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,
                                                     "HydroCouple Composer Project (*.hcp)")).isEmpty())
      {
        m_project->onSaveProjectAs(QFileInfo(file));
      }
    }
  }


  HydroCoupleProject* newProject = new HydroCoupleProject(this);
  setProject(newProject);
}

void HydroCoupleComposer::onOpenFiles()
{

#ifdef _WIN32

  QString filter ="All Compatible Files (*.hcp *.hcc *.dll)"
                  ";;HydroCouple Composer Project (*.hcp)"
                  ";;HydroCouple Component (*.hcc)"
                  ";;HydroCouple Component Libraries (*.dll)";

#elif __unix__ // all unices, not all compilers

  QString filter ="All Compatible Files (*.hcp *.hcc *.so)"
                  ";;HydroCouple Composer Project (*.hcp)"
                  ";;HydroCouple Component (*.hcc)"
                  ";;HydroCouple Component Libraries (*.so)";
#elif __linux__

  QString filter ="All Compatible Files (*.hcp *.hcc *.so)"
                  ";;HydroCouple Composer Project (*.hcp)"
                  ";;HydroCouple Component (*.hcc)"
                  ";;HydroCouple Component Libraries (*.so)";

#elif __APPLE__

  QString filter ="All Compatible Files (*.hcp *.hcc *.dylib)"
                  ";;HydroCouple Composer Project (*.hcp)"
                  ";;HydroCouple Component (*.hcc)"
                  ";;HydroCouple Component Libraries (*.dylib)";

#else

  QString filter ="All Compatible Files (*.hcp *.hcc *.dll *.so *.dylib)"
                  ";;HydroCouple Composer Project (*.hcp)"
                  ";;HydroCouple Component (*.hcc)"
                  ";;HydroCouple Component Libraries (*.dll *.so *.dylib)";

#endif

  QFileInfo m_last(m_lastOpenedFilePath);

  QStringList files = QFileDialog::getOpenFileNames(this, "Open", m_last.absoluteDir().absolutePath(), filter);

  if (files.count() > 0)
  {
    for (int i = 0; i < files.count(); i++)
    {
      QFileInfo file(files[i]);
      openFile(file);
    }
  }
}

void HydroCoupleComposer::onSave()
{
  QString file;

  QFileInfo m_last(m_lastOpenedFilePath);

  if(m_project->projectFile().exists())
  {
    m_project->onSaveProject();
    m_lastOpenedFilePath = m_project->projectFile().absoluteFilePath();
    onUpdateRecentFiles();
  }
  else if (!(file = QFileDialog::getSaveFileName(this, "Save", m_last.absoluteDir().absolutePath(),
                                                 "HydroCouple Composer Project (*.hcp)")).isEmpty())
  {
    m_project->onSaveProjectAs(QFileInfo(file));
    m_lastOpenedFilePath = file;
    onUpdateRecentFiles();
  }


}

void HydroCoupleComposer::onSaveAs()
{
  QString file;

  QString filter = "HydroCouple Composer Project (*.hcp)";

  if(m_selectedModelComponents.length() == 1)
  {
    filter = "All Compatible Files (*.hcp *.hcc);;" + filter + ";;HydroCouple Component (*.hcc)";
  }

  if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,filter)).isEmpty())
  {
    QFileInfo inputFile(file);

    if(!inputFile.suffix().compare("hcp" , Qt::CaseInsensitive))
    {
      m_project->onSaveProjectAs(inputFile);
      onUpdateRecentFiles();
    }
    else if(!inputFile.suffix().compare("hcc" , Qt::CaseInsensitive))
    {
      m_selectedModelComponents[0]->writeComponent(inputFile);
    }
  }
}

void HydroCoupleComposer::onExport()
{
  if(m_selectedModelComponents.length() == 1)
  {
    QString file;

    if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,
                                              "HydroCouple Component (*.hcc)")).isEmpty())
    {
      QFileInfo inputFile(file);
    }
  }
}

void HydroCoupleComposer::onPrint()
{
  QPrinter printer;

  if(QPrintDialog(&printer).exec() == QDialog::Accepted)
  {
    QPainter painter (&printer);
    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.setRenderHint(QPainter::TextAntialiasing,true);
    QRectF rect = graphicsViewHydroCoupleComposer->mapToScene(graphicsViewHydroCoupleComposer->viewport()->geometry()).boundingRect();
    graphicsViewHydroCoupleComposer->scene()->render(&painter, QRect(), rect);
  }
}

void HydroCoupleComposer::onOpenRecentFile()
{
  QAction *action = qobject_cast<QAction *>(sender());

  if (action)
  {
    QFileInfo file(action->data().toString());

    if (file.exists())
    {
      openFile(file);
    }
    else
    {
      onUpdateRecentFiles();
    }
  }
}

void HydroCoupleComposer::onRunSimulation()
{
  if(m_project->hasChanges())
  {
    if (QMessageBox::question(this, "Save ?", "You have changes that need to be saved, Do you want to save ?",
                              QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
    {
      QString file;

      if(m_project->projectFile().exists())
      {
        m_project->onSaveProject();
      }
      else if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,
                                                     "HydroCouple Composer Project (*.hcp)")).isEmpty())
      {
        m_project->onSaveProjectAs(QFileInfo(file));
      }
    }
  }

  m_simulationManager->runComposition(true);
}

void HydroCoupleComposer::onAddComponentLibraryDirectory()
{
  QString dirPath = QFileDialog::getExistingDirectory(this, "Open", m_lastPath);
  QDir dir(dirPath);

  if (dir.exists())
  {
    m_project->componentManager()->addComponentDirectory(dir);
    m_lastPath = dir.absolutePath();
  }
}

void HydroCoupleComposer::onUpdateRecentFiles()
{
  QFileInfo m_last(m_lastOpenedFilePath);

  //add new path
  m_recentFiles.removeAll(m_lastOpenedFilePath);
  m_recentFiles.prepend(m_lastOpenedFilePath);

  for(int i = 0 ; i < m_recentFiles.length() ; i++)
  {
    QFileInfo f(m_recentFiles[i]);

    if(!(f.exists() && f.isFile()))
    {
      m_recentFiles.removeAt(i);
      i--;
    }
  }

  //trim
  while (m_recentFiles.size() > 20)
  {
    m_recentFiles.removeLast();
  }

  if (m_recentFiles.count())
  {
    m_clearRecentFilesAction->setVisible(true);
    m_recentFilesMenu->setEnabled(true);

    for (int i = 0; i < 20; i++)
    {
      QAction* action = m_recentFilesActions[i];

      if (i < m_recentFiles.count())
      {
        QFileInfo file(m_recentFiles[i]);
        QString fullPath(file.absoluteFilePath());
        //        QString text = tr("&%1. %2").arg(i + 1).arg(file.absoluteFilePath());
        action->setText(file.filePath());
        action->setToolTip(fullPath);
        action->setStatusTip(fullPath);
        action->setWhatsThis(fullPath);
        action->setVisible(true);
        action->setData(fullPath);
      }
      else
      {
        action->setVisible(false);
        action->setText("");
        action->setToolTip("");
        action->setStatusTip("");
        action->setWhatsThis("");
        action->setData("");
      }
    }
  }
  else
  {
    for (int i = 0; i < 20; i++)
    {
      QAction* action = m_recentFilesActions[i];
      action->setVisible(false);
      action->setText("");
      action->setToolTip("");
      action->setStatusTip("");
      action->setWhatsThis("");
      action->setData("");
    }

    m_clearRecentFilesAction->setVisible(false);
    m_recentFilesMenu->setEnabled(false);
  }

  writeSettings();
}

void HydroCoupleComposer::onClearRecentFiles()
{
  m_recentFiles.clear();
  m_lastOpenedFilePath = "";
  onUpdateRecentFiles();
}

void HydroCoupleComposer::onClearSettings()
{
  onPostMessage("Clearing settings");

  m_settings.remove("HydroCoupleComposer::MainWindow");
  m_argumentDialog->clearSettings();
  m_preferencesDialog->clearSettings();

  onPostMessage("Finished clearing settings");

  readSettings();
}

void HydroCoupleComposer::onEditSelectedItem()
{
  if(m_selectedModelComponents.length() == 1 && m_selectedAdaptedOutputs.length() == 0)
  {
    if(!m_argumentDialog->isVisible())
    {
      GModelComponent *modelComponent = m_selectedModelComponents[0];

      if((modelComponent->modelComponent()->status() == IModelComponent::Created
          || modelComponent->modelComponent()->status() == IModelComponent::Failed
          || modelComponent->modelComponent()->status() == IModelComponent::Initialized ))
      {
        m_argumentDialog->show();
        m_argumentDialog->setComponent(modelComponent);
      }
    }
  }
  else if (m_selectedModelComponents.length() == 0 && m_selectedAdaptedOutputs.length() == 1)
  {
    if(!m_argumentDialog->isVisible())
    {
      m_argumentDialog->show();
      m_argumentDialog->setAdaptedOutput(m_selectedAdaptedOutputs[0]);
    }
  }
}

void HydroCoupleComposer::onAddModelComponent()
{
  QItemSelectionModel* itemSelection = treeViewModelComponentInfos->selectionModel();

  if(itemSelection->selectedRows().length())
  {
    QModelIndexList selectedIndices = itemSelection->selectedIndexes();

    for(int i = 0 ; i < selectedIndices.length() ; i++)
    {
      QVariant value = selectedIndices[i].data(Qt::UserRole);

      if (value.type() != QVariant::Bool)
      {
        QString id = value.toString();

        //        if(m_project->componentManager()->modelComponentInfoById().contains(id))
        //        {
        //          IModelComponentInfo* foundModelComponentInfo = m_project->componentManager()->modelComponentInfoById()[id];

        //          IModelComponent* component = foundModelComponentInfo->createComponentInstance();

        //          GModelComponent* gcomponent = new GModelComponent(component, m_project);

        //          QPointF f = graphicsViewHydroCoupleComposer->mapToScene(graphicsViewHydroCoupleComposer->frameRect().center()) - gcomponent->boundingRect().bottomRight()/2;

        //          gcomponent->setPos(f);

        //          if (m_project->modelComponents().count() == 0)
        //          {
        //            gcomponent->setTrigger(true);
        //          }

        //          m_project->addComponent(gcomponent);
        //          return;
        //        }
      }
    }
  }
}

void HydroCoupleComposer::onCreateConnection(bool create)
{
  if(create)
  {
    if(m_selectedExchangeItems.count() > 2)
    {
      actionCreateConnection->setChecked(false);
    }
    else if (m_selectedExchangeItems.count() == 2)
    {
      if( m_selectedExchangeItems[0]->zValue() <  m_selectedExchangeItems[1]->zValue())
      {
        createConnection(m_selectedExchangeItems[0] , m_selectedExchangeItems[1]);
      }
      else
      {
        createConnection(m_selectedExchangeItems[1] , m_selectedExchangeItems[0]);
      }
    }
    else if (m_selectedExchangeItems.length() ==1 )
    {
      m_connProd = m_selectedExchangeItems[0];
    }
    else
    {
      m_connCons = nullptr;
      m_connProd = nullptr;
    }
  }
  else
  {
    m_connCons = nullptr;
    m_connProd = nullptr;
  }
}

void HydroCoupleComposer::onLayoutComponents()
{
#ifdef GRAPHVIZ_LIBRARY

  for(int i = 0;  i <  2; i++)
  {
    char lay[4];
    strcpy(lay , "lay");
    Agraph_t* G = agopen(lay , Agdirected , 0);


    GVC_t* gvc = gvContext();
    char nullChar[1];
    nullChar[0] = '0';

    if(m_project->modelComponents().length())
    {
      QRectF ff =  graphicsViewHydroCoupleComposer->scene()->itemsBoundingRect() ;

      string g_size =  to_string(ff.width()) + ","+ to_string(ff.height());
      char* g_size_c = new char[g_size.length() + 1];

      strcpy(g_size_c, g_size.c_str());

      char size[5] ;
      strcpy(size , "size");

      agsafeset(G, size, g_size_c, nullChar);

      delete[] g_size_c;

      char nodesep[8] ;
      strcpy(nodesep , "nodesep");

      char ranksep[8] ;
      strcpy(ranksep , "ranksep");

      char rankdir[8] ;
      strcpy(rankdir , "rankdir");

      char num[4];
      strcpy(num , "4.0");
      agsafeset(G , nodesep , num, nullChar);

      strcpy(num , "6.0");
      agsafeset(G , ranksep , num, nullChar);

      char al[3];
      strcpy(al , "LR");
      agsafeset(G , rankdir , al, nullChar);

    }

    int index = 0;
    QHash<GNode*,QString> identifiers;

    for(GModelComponent* component : m_project->modelComponents())
    {
      for(GInput* input : component->inputGraphicObjects().values())
      {
        layoutNode(G,identifiers,input,index,true);
      }

      layoutNode(G, identifiers, component, index , true);
    }


    for(GModelComponent* component : m_project->modelComponents())
    {
      for(GInput* input : component->inputGraphicObjects().values())
      {
        layoutEdges(G,identifiers,input);
      }

      layoutEdges(G,identifiers,component);
    }

    gvLayout(gvc,G,"dot");

    for(GModelComponent* component : m_project->modelComponents())
    {
      for(GInput* input : component->inputGraphicObjects().values())
      {
        layoutNode(G,identifiers,input,index, false);
      }

      layoutNode(G , identifiers , component , index, false);
    }
    gvFreeLayout(gvc,G);
    agclose(G);
    gvFreeContext(gvc);
  }

#endif

  onZoomExtent();

}

void HydroCoupleComposer::onCloneModelComponents()
{
  if(m_selectedModelComponents.length())
  {
    for(GModelComponent* model : m_selectedModelComponents)
    {
      ICloneableModelComponent* cloneableModelComponent = dynamic_cast<ICloneableModelComponent*>(model->modelComponent());

      if(cloneableModelComponent)
        cloneableModelComponent->clone();
    }
  }
}

void HydroCoupleComposer::onSetAsTrigger()
{
  if(m_selectedModelComponents.length() == 1 && !m_selectedModelComponents[0]->trigger())
  {
    m_selectedModelComponents[0]->setTrigger(true);
  }
}

void HydroCoupleComposer::onInitializeComponent()
{
  if(m_selectedModelComponents.length())
  {
    for(GModelComponent* gModelComponent : m_selectedModelComponents)
    {
      if(gModelComponent->modelComponent()->status() == IModelComponent::Created ||
         gModelComponent->modelComponent()->status() == IModelComponent::Initialized ||
         gModelComponent->modelComponent()->status() == IModelComponent::Failed)
      {
        gModelComponent->modelComponent()->initialize();
      }
    }
  }
}

void HydroCoupleComposer::onValidateComponent()
{
  if(m_selectedModelComponents.length())
  {
    for(GModelComponent* gModelComponent : m_selectedModelComponents)
    {
      IModelComponent* modelComponent = gModelComponent->modelComponent();

      if(modelComponent->status() == IModelComponent::Initialized)
      {
        QList<QString> messages = modelComponent->validate();

        for(QString message : messages)
        {
          onPostMessage(message);
        }
      }
    }
  }
}

void HydroCoupleComposer::onDeleteSelectedComponents()
{
  if (m_selectedModelComponents.length() &&
      QMessageBox::question(this, "Delete ?", "Are you sure you want to delete selected Model Components ?",
                            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
  {

    graphicsViewHydroCoupleComposer->scene()->blockSignals(true);

    for (int i = 0 ; i < m_selectedModelComponents.length() ; i++)
    {
      GModelComponent* model = m_selectedModelComponents[i];

      QString id = model->modelComponent()->id();

      if (m_project->deleteModelComponent(model))
      {
        setStatusTip(id + " has been removed");
      }
    }

    graphicsViewHydroCoupleComposer->scene()->blockSignals(false);

    m_selectedModelComponents.clear();

    m_propertyModel->setData(QVariant());
    treeViewProperties->expandToDepth(0);
  }
}

void HydroCoupleComposer::onDeleteSelectedConnections()
{
  if (m_selectedConnections.length() &&
      QMessageBox::question(this, "Delete ?", "Are you sure you want to delete selected connections ?",
                            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
  {
    graphicsViewHydroCoupleComposer->scene()->blockSignals(true);

    for(GConnection* con : m_selectedConnections)
    {
      con->producer()->deleteConnection(con);
    }

    graphicsViewHydroCoupleComposer->scene()->blockSignals(false);

    m_propertyModel->setData(QVariant());
    treeViewProperties->expandToDepth(0);
    m_selectedConnections.clear();
  }

}

void HydroCoupleComposer::onValidateComponentLibrary()
{

  QAction *senderAction = dynamic_cast<QAction*>(sender());

  if(senderAction == actionValidateModelComponentInfo)
  {
    QModelIndex index = treeViewModelComponentInfos->currentIndex();

    if (index.isValid())
    {
      QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

      if(value.contains("ModelComponentInfo"))
      {
        QString id = value["ModelComponentInfo"].toString();
        IModelComponentInfo *modelComponentInfo = m_project->componentManager()->findModelComponentInfo(id);

        if(modelComponentInfo)
        {
          QString validationMessage;

          if(modelComponentInfo->validateLicense(validationMessage))
          {
            onPostMessage("Validation Was Successful ! Message:" + validationMessage);
          }
          else
          {
            onPostMessage("Validation Was Not Successful :( Message:" + validationMessage);
          }

        }
      }
    }
  }
  else if(senderAction == actionValidateAdaptedOutputFactoryComponentInfo)
  {
    QModelIndex index = treeViewAdaptedOutputFactoryComponents->currentIndex();

    if (index.isValid())
    {
      QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

      if(value.contains("AdaptedOutputFactoryComponentInfo"))
      {
        QString id = value["AdaptedOutputFactoryComponentInfo"].toString();
        IAdaptedOutputFactoryComponentInfo *componentInfo = m_project->componentManager()->findAdaptedOutputFactoryComponentInfo(id);

        if(componentInfo)
        {
          QString validationMessage;

          if(componentInfo->validateLicense(validationMessage))
          {
            onPostMessage("Validation Was Successful ! Message:" + validationMessage);
          }
          else
          {
            onPostMessage("Validation Was Not Successful :( Message:" + validationMessage);
          }

        }
      }
    }
  }
  else if(senderAction == actionValidateWorkflowComponentInfo)
  {
    QModelIndex index = treeViewWorkflowComponents->currentIndex();

    if (index.isValid())
    {
      QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

      if(value.contains("WorkflowComponentInfo"))
      {
        QString id = value["WorkflowComponentInfo"].toString();
        IWorkflowComponentInfo *componentInfo = m_project->componentManager()->findWorkflowComponentInfo(id);

        if(componentInfo)
        {
          QString validationMessage;

          if(componentInfo->validateLicense(validationMessage))
          {
            onPostMessage("Validation Was Successful ! Message:" + validationMessage);
          }
          else
          {
            onPostMessage("Validation Was Not Successful :( Message:" + validationMessage);
          }

        }
      }
    }
  }
}

void HydroCoupleComposer::onBrowseToComponentLibraryPath()
{
  QAction *action = dynamic_cast<QAction*>(sender());

  if(actionBrowseModelComponentInfoFolder == action)
  {
    QModelIndex index = treeViewModelComponentInfos->currentIndex();
    QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

    if (value.contains("ModelComponentInfo"))
    {
      QString id = value["ModelComponentInfo"].toString();
      IComponentInfo* foundComponentInfo = m_project->componentManager()->findModelComponentInfo(id);

      if(foundComponentInfo)
      {
        QFileInfo file (foundComponentInfo->libraryFilePath());

        if (file.exists())
        {
          QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
          m_lastPath = file.absolutePath();
        }
      }
    }
  }
  else if(actionBrowseAdaptedOutputFactoryComponentInfoFolder == action)
  {
    QModelIndex index = treeViewAdaptedOutputFactoryComponents->currentIndex();
    QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

    if(value.contains("AdaptedOutputFactoryComponentInfo"))
    {
      QString id = value["AdaptedOutputFactoryComponentInfo"].toString();
      IAdaptedOutputFactoryComponentInfo* foundComponentInfo = m_project->componentManager()->findAdaptedOutputFactoryComponentInfo(id);

      if(foundComponentInfo)
      {
        QFileInfo file (foundComponentInfo->libraryFilePath());

        if (file.exists())
        {
          QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
          m_lastPath = file.absolutePath();
        }
      }
    }
  }
  else if(actionBrowseWorkflowComponentInfoFolder == action)
  {
    QModelIndex index = treeViewWorkflowComponents->currentIndex();
    QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

    if(value.contains("WorkflowComponentInfo"))
    {
      QString id = value["WorkflowComponentInfo"].toString();
      IWorkflowComponentInfo* foundComponentInfo = m_project->componentManager()->findWorkflowComponentInfo(id);

      if(foundComponentInfo)
      {
        QFileInfo file (foundComponentInfo->libraryFilePath());

        if (file.exists())
        {
          QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
          m_lastPath = file.absolutePath();
        }
      }
    }
  }
}

void HydroCoupleComposer::onLoadComponentLibrary()
{
#ifdef _WIN32

  QString filter = "HydroCouple Component Libraries (*.dll)";

#elif __unix__ // all unices, not all compilers

  QString filter = "HydroCouple Component Libraries (*.so)";
#elif __linux__

  QString filter ="HydroCouple Component Libraries (*.so)";

#elif __APPLE__

  QString filter ="HydroCouple Component Libraries (*.dylib)";

#else

  QString filter ="HydroCouple Component Libraries (*.dll *.so *.dylib)";

#endif

  QStringList	files = QFileDialog::getOpenFileNames(this, "Open", m_lastPath, filter);

  if (files.count() > 0)
  {
    for (int i = 0; i < files.count(); i++)
    {
      QFileInfo file(files[i]);
      openFile(file);
    }
  }
}

void HydroCoupleComposer::onUnloadComponentLibrary()
{
  if (QMessageBox::question(this, "Unload ?", "All instances associated with the component library being unloaded will be deleted. Are you sure you want to unload selected component library ? ",
                            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
  {

    if(dynamic_cast<QAction*>(sender()) == actionUnloadModelComponentLibrary)
    {
      QModelIndex index = treeViewModelComponentInfos->currentIndex();
      QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

      if (value.contains("ModelComponentInfo"))
      {
        QString id = value["ModelComponentInfo"].toString();
        QString message;
        m_project->componentManager()->unloadModelComponentInfo(id, message);
        treeViewModelComponentInfos->setCurrentIndex(QModelIndex());
        onModelComponentInfoClicked(QModelIndex());
      }
    }
    else if(dynamic_cast<QAction*>(sender()) == actionUnloadAdaptedOutputFactoryComponentLibrary)
    {
      QModelIndex index = treeViewAdaptedOutputFactoryComponents->currentIndex();
      QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

      if (value.contains("AdaptedOutputFactoryComponentInfo"))
      {
        QString id = value["AdaptedOutputFactoryComponentInfo"].toString();
        QString message;
        m_project->componentManager()->unloadAdaptedOutputFactoryComponentInfo(id, message);
        treeViewAdaptedOutputFactoryComponents->setCurrentIndex(QModelIndex());
        onAdaptedOutputClicked(QModelIndex());
      }
    }
    else if(dynamic_cast<QAction*>(sender()) == actionUnloadWorkflowComponentLibrary)
    {
      QModelIndex index = treeViewModelComponentInfos->currentIndex();
      QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

      if (value.contains("WorkflowComponentInfo"))
      {
        QString id = value["WorkflowComponentInfo"].toString();
        QString message;
        m_project->componentManager()->unloadWorkflowComponentInfo(id, message);
        treeViewWorkflowComponents->setCurrentIndex(QModelIndex());
        onWorkflowComponentClicked(QModelIndex());
      }
    }
  }
}

void HydroCoupleComposer::onModelComponentInfoLoaded(const QString &modelComponentInfoId)
{
  IModelComponentInfo *modelComponentinfo = m_project->componentManager()->findModelComponentInfo(modelComponentInfoId);

  if(modelComponentinfo)
  {
    QStringList categories = modelComponentinfo->category().split('\\', QString::SplitBehavior::SkipEmptyParts);
    QStandardItem* parent = m_modelComponentInfoModel->invisibleRootItem();

    while (categories.count() > 0)
    {
      QString category = categories[0];
      QStandardItem* childCategoryItem = findStandardItem(category, parent, "ModelComponentInfoCategory", false);

      if (childCategoryItem == nullptr)
      {
        QStandardItem* newCategoryItem = new QStandardItem(s_categoryIcon, category);
        newCategoryItem->setToolTip(category);
        newCategoryItem->setStatusTip(category);
        newCategoryItem->setWhatsThis(category);
        newCategoryItem->setDragEnabled(false);
        QMap<QString,QVariant> data;
        data["ModelComponentInfoCategory"] = category;
        newCategoryItem->setData(data, Qt::UserRole + 1);
        parent->appendRow(newCategoryItem);
        treeViewModelComponentInfos->expand(parent->index());
        parent = newCategoryItem;
      }
      else
      {
        parent = childCategoryItem;
      }

      categories.removeAt(0);
    }

    QStandardItem* componentTreeViewItem = findStandardItem(modelComponentInfoId, parent, "ModelComponentInfo");
    bool found = true;

    if(componentTreeViewItem == nullptr)
    {
      componentTreeViewItem = new QStandardItem(modelComponentinfo->caption());
      found = false;
    }

    componentTreeViewItem->setText(modelComponentinfo->caption());
    componentTreeViewItem->setStatusTip(modelComponentinfo->caption());
    componentTreeViewItem->setDragEnabled(true);

    QString iconPath;
    QFileInfo iconFile(QFileInfo(modelComponentinfo->libraryFilePath()).dir(), modelComponentinfo->iconFilePath());

    if (iconFile.isFile() && iconFile.exists())
    {
      iconPath = iconFile.absoluteFilePath();
      QIcon cIcon(iconPath);
      componentTreeViewItem->setIcon(cIcon);
    }
    else
    {
      iconPath = ":/HydroCoupleComposer/hydrocouplecomposer";
      componentTreeViewItem->setIcon(s_modelComponentIcon);
    }

    QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", modelComponentinfo->id())
                   .replace("[Version]", modelComponentinfo->version())
                   .replace("[Url]", modelComponentinfo->url())
                   .replace("[Caption]", modelComponentinfo->caption())
                   .replace("[IconPath]", iconPath)
                   .replace("[Description]", modelComponentinfo->description())
                   .replace("[License]", modelComponentinfo->license())
                   .replace("[Vendor]", modelComponentinfo->vendor())
                   .replace("[Email]", modelComponentinfo->email())
                   .replace("[Copyright]", modelComponentinfo->copyright());


    componentTreeViewItem->setToolTip(html);
    componentTreeViewItem->setWhatsThis(html);
    componentTreeViewItem->setStatusTip(modelComponentinfo->caption());

    QMap<QString,QVariant> data;
    data["ModelComponentInfo"] = modelComponentInfoId;
    componentTreeViewItem->setData(data, Qt::UserRole + 1);

    onPostMessage("Component Library Loaded: " + modelComponentinfo->id());

    if(found == false)
    {
      parent->appendRow(componentTreeViewItem);
    }

    QObject* componentInfoAsObject = dynamic_cast<QObject*>(modelComponentinfo);

    if(componentInfoAsObject)
    {
      connect(componentInfoAsObject , SIGNAL(propertyChanged(const QString&)),
              this,SLOT(onComponentInfoPropertyChanged(const QString&)));
    }

    loadModelComponentInfoAdaptedOutputFactory(modelComponentinfo);

    treeViewModelComponentInfos->expand(parent->index());
  }
}

void HydroCoupleComposer::onModelComponentInfoUnloaded(const QString& modelComponentInfoId)
{
  QStandardItem* childCategoryItem = findStandardItem(modelComponentInfoId, m_modelComponentInfoModel->invisibleRootItem(), "ModelComponentInfo" , true);

  if(childCategoryItem)
  {
    m_modelComponentInfoModel->removeRow(childCategoryItem->index().row() , childCategoryItem->parent()->index());
    m_propertyModel->setData(QVariant());

    IModelComponentInfo *modelComponentInfo = m_project->componentManager()->findModelComponentInfo(modelComponentInfoId);

    if(modelComponentInfo)
    {
      unloadModelComponentInfoAdaptedOutputFactory(modelComponentInfo);
    }
  }
}

void HydroCoupleComposer::loadModelComponentInfoAdaptedOutputFactory(const IModelComponentInfo *modelComponentInfo)
{
  QStandardItem *parent = findStandardItem(modelComponentInfo->id(), m_internalAdaptedOutputFactoriesStandardItem, "ModelComponentInfo");

  if(parent == nullptr)
  {
    QString iconPath = ":/HydroCoupleComposer/hydrocouplecomposer";

    QFileInfo iconFile(QFileInfo(modelComponentInfo->libraryFilePath()).dir(), modelComponentInfo->iconFilePath());

    if (iconFile.isFile() && iconFile.exists())
    {
      iconPath = iconFile.absoluteFilePath();
    }

    QIcon cIcon = QIcon(iconPath);

    parent = new QStandardItem(cIcon, modelComponentInfo->caption());
    parent->setStatusTip(modelComponentInfo->caption());
    parent->setDragEnabled(false);

    QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", modelComponentInfo->id())
                   .replace("[Version]", modelComponentInfo->version())
                   .replace("[Url]", modelComponentInfo->url())
                   .replace("[Caption]", modelComponentInfo->caption())
                   .replace("[IconPath]", iconPath)
                   .replace("[Description]", modelComponentInfo->description())
                   .replace("[License]", modelComponentInfo->license())
                   .replace("[Vendor]", modelComponentInfo->vendor())
                   .replace("[Email]", modelComponentInfo->email())
                   .replace("[Copyright]", modelComponentInfo->copyright());


    parent->setToolTip(html);
    parent->setWhatsThis(html);

    QMap<QString,QVariant> data;
    data["ModelComponentInfo"] = modelComponentInfo->id();
    parent->setData(data, Qt::UserRole + 1);
    m_internalAdaptedOutputFactoriesStandardItem->appendRow(parent);
    treeViewAdaptedOutputFactoryComponents->expand(m_internalAdaptedOutputFactoriesStandardItem->index());
  }

  for(int i = 0; i < modelComponentInfo->adaptedOutputFactories().size() ; i++)
  {
    IAdaptedOutputFactory *adaptedOutputFactory = modelComponentInfo->adaptedOutputFactories()[i];
    QStandardItem *factoryStandardItem = findStandardItem(adaptedOutputFactory->id(),parent,"AdaptedOutputFactory");

    if(factoryStandardItem == nullptr)
    {
      QIcon factoryIcon(":/HydroCoupleComposer/adaptedoutputfactory");
      factoryStandardItem = new QStandardItem(factoryIcon,adaptedOutputFactory->caption());
      factoryStandardItem->setDragEnabled(false);

      QString desc =  QString(sc_standardInfoHtml).replace("[Component]", adaptedOutputFactory->id())
                      .replace("[Caption]", adaptedOutputFactory->caption())
                      .replace("[Description]", adaptedOutputFactory->description());

      factoryStandardItem->setToolTip(desc);
      factoryStandardItem->setWhatsThis(desc);
      factoryStandardItem->setStatusTip(adaptedOutputFactory->caption());
      parent->appendRow(factoryStandardItem);
    }

    QMap<QString,QVariant> data;
    data["AdaptedOutputFactory"] = adaptedOutputFactory->id();
    data["AdaptedOutputFactoryModelComponentInfo"] = modelComponentInfo->id();
    factoryStandardItem->setData(data, Qt::UserRole + 1);

    QObject* componentInfoAsObject = dynamic_cast<QObject*>(adaptedOutputFactory);

    if(componentInfoAsObject)
    {
      connect(componentInfoAsObject , SIGNAL(propertyChanged(const QString&)),
              this,SLOT(onComponentInfoPropertyChanged(const QString&)));
    }
  }

  treeViewAdaptedOutputFactoryComponents->expand(parent->index());

  resetAdaptedOutputFactoryModel(m_adaptedOutputFactoriesModel->invisibleRootItem());
}

void HydroCoupleComposer::unloadModelComponentInfoAdaptedOutputFactory(const IModelComponentInfo *modelComponentInfo)
{
  QStandardItem *modelComponentInfoStandardItem = findStandardItem(modelComponentInfo->id(), m_internalAdaptedOutputFactoriesStandardItem, "AdaptedOutputFactoryModelComponentInfo");

  if(modelComponentInfoStandardItem)
  {
    m_adaptedOutputFactoriesModel->removeRow(modelComponentInfoStandardItem->row(), modelComponentInfoStandardItem->parent()->index());
    m_propertyModel->setData(QVariant());
    resetAdaptedOutputFactoryModel(m_adaptedOutputFactoriesModel->invisibleRootItem());
  }
}

void HydroCoupleComposer::onAdaptedOutputFactoryComponentInfoLoaded(const QString &adaptedOutputFactoryComponentInfoId)
{

  IAdaptedOutputFactoryComponentInfo *adaptedOutputFactoryComponentInfo = m_project->componentManager()->findAdaptedOutputFactoryComponentInfo(adaptedOutputFactoryComponentInfoId);

  if(adaptedOutputFactoryComponentInfo)
  {

    QStringList categories = adaptedOutputFactoryComponentInfo->category().split('\\', QString::SplitBehavior::SkipEmptyParts);
    QStandardItem* parent = m_externalAdaptedOutputFactoriesStandardItem;

    while (categories.count() > 0)
    {
      QString category = categories[0];

      QStandardItem* childCategoryItem = findStandardItem(category, parent , "AdaptedOutputFactoryComponentInfoCategory" , true);

      if (childCategoryItem == nullptr)
      {
        QStandardItem* newCategoryItem = new QStandardItem(s_categoryIcon, category);
        newCategoryItem->setToolTip(category);
        newCategoryItem->setStatusTip(category);
        newCategoryItem->setWhatsThis(category);

        QMap<QString,QVariant> data;
        data["AdaptedOutputFactoryComponentInfoCategory"] = category;
        newCategoryItem->setData(data, Qt::UserRole + 1);
        newCategoryItem->setDragEnabled(false);
        parent->appendRow(newCategoryItem);
        treeViewAdaptedOutputFactoryComponents->expand(parent->index());
        parent = newCategoryItem;
      }
      else
      {
        parent = childCategoryItem;
      }

      categories.removeAt(0);
    }

    QString iconPath = ":/HydroCoupleComposer/adaptedoutputfactory";

    QFileInfo iconFile(QFileInfo(adaptedOutputFactoryComponentInfo->libraryFilePath()).dir(), adaptedOutputFactoryComponentInfo->iconFilePath());

    if (iconFile.isFile() && iconFile.exists())
    {
      iconPath = iconFile.absoluteFilePath();
    }

    bool found = true;
    QStandardItem* componentTreeViewItem = findStandardItem(adaptedOutputFactoryComponentInfoId, parent, "AdaptedOutputFactoryComponentInfo");

    if(componentTreeViewItem == nullptr)
    {
      QIcon cIcon = QIcon(iconPath);
      componentTreeViewItem = new QStandardItem(cIcon, adaptedOutputFactoryComponentInfo->caption());
      found = false;
    }
    else
    {
      QIcon cIcon = QIcon(iconPath);
      componentTreeViewItem->setIcon(cIcon);
      componentTreeViewItem->setText(adaptedOutputFactoryComponentInfo->caption());
    }

    componentTreeViewItem->setStatusTip(adaptedOutputFactoryComponentInfo->caption());
    componentTreeViewItem->setDragEnabled(false);

    QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", adaptedOutputFactoryComponentInfo->id())
                   .replace("[Version]", adaptedOutputFactoryComponentInfo->version())
                   .replace("[Url]", adaptedOutputFactoryComponentInfo->url())
                   .replace("[Caption]", adaptedOutputFactoryComponentInfo->caption())
                   .replace("[IconPath]", iconPath)
                   .replace("[Description]", adaptedOutputFactoryComponentInfo->description())
                   .replace("[License]", adaptedOutputFactoryComponentInfo->license())
                   .replace("[Vendor]", adaptedOutputFactoryComponentInfo->vendor())
                   .replace("[Email]", adaptedOutputFactoryComponentInfo->email())
                   .replace("[Copyright]", adaptedOutputFactoryComponentInfo->copyright());


    componentTreeViewItem->setToolTip(html);
    componentTreeViewItem->setWhatsThis(html);

    QMap<QString,QVariant> data;
    data["AdaptedOutputFactoryComponentInfo"] = adaptedOutputFactoryComponentInfo->id();
    componentTreeViewItem->setData(data, Qt::UserRole + 1);

    if(found == false)
    {
      parent->appendRow(componentTreeViewItem);
    }

    QObject* componentInfoAsObject = dynamic_cast<QObject*>(adaptedOutputFactoryComponentInfo);

    if(componentInfoAsObject)
    {
      connect(componentInfoAsObject , SIGNAL(propertyChanged(const QString&)),
              this,SLOT(onComponentInfoPropertyChanged(const QString&)));
    }

    treeViewAdaptedOutputFactoryComponents->expandToDepth(1);

    resetAdaptedOutputFactoryModel(m_adaptedOutputFactoriesModel->invisibleRootItem());
  }
}

void HydroCoupleComposer::onAdaptedOutputFactoryComponentInfoUnloaded(const QString &adaptedOutputFactoryComponentInfoId)
{
  QStandardItem* childCategoryItem = findStandardItem(adaptedOutputFactoryComponentInfoId, m_externalAdaptedOutputFactoriesStandardItem ,"AdaptedOutputFactoryComponentInfo", true);

  if(childCategoryItem)
  {
    m_adaptedOutputFactoriesModel->removeRow(childCategoryItem->index().row() , childCategoryItem->parent()->index()) ;
    m_propertyModel->setData(QVariant());
    resetAdaptedOutputFactoryModel(m_adaptedOutputFactoriesModel->invisibleRootItem());
  }
}

void HydroCoupleComposer::onWorkflowComponentInfoLoaded(const QString &workflowComponentInfoId)
{
  IWorkflowComponentInfo *workflowComponentInfo = m_project->componentManager()->findWorkflowComponentInfo(workflowComponentInfoId);

  if(workflowComponentInfo)
  {
    QStringList categories = workflowComponentInfo->category().split('\\', QString::SplitBehavior::SkipEmptyParts);
    QStandardItem* parent = m_workflowComponentModel->invisibleRootItem();

    while (categories.count() > 0)
    {
      QString category = categories[0];

      QStandardItem* childCategoryItem = findStandardItem(category, parent , "WorkflowComponentInfoCategory" , true);

      if (childCategoryItem == nullptr)
      {
        QStandardItem* newCategoryItem = new QStandardItem(s_categoryIcon, category);
        newCategoryItem->setToolTip(category);
        newCategoryItem->setStatusTip(category);
        newCategoryItem->setWhatsThis(category);

        QMap<QString,QVariant> data;
        data["WorkflowComponentInfoCategory"] = category;
        newCategoryItem->setData(data, Qt::UserRole);
        newCategoryItem->setDragEnabled(false);
        parent->appendRow(newCategoryItem);
        treeViewWorkflowComponents->expand(parent->index());
        parent = newCategoryItem;
      }
      else
      {
        parent = childCategoryItem;
      }

      categories.removeAt(0);
    }

    QString iconPath = ":/HydroCoupleComposer/workflow";
    QFileInfo iconFile(QFileInfo(workflowComponentInfo->libraryFilePath()).dir(), workflowComponentInfo->iconFilePath());

    if (iconFile.isFile() && iconFile.exists())
    {
      iconPath = iconFile.absoluteFilePath();
    }

    QStandardItem *standardItem = findStandardItem(workflowComponentInfoId, parent, "WorkflowComponentInfo");
    bool found = true;

    if(standardItem == nullptr)
    {
      QIcon wIcon = QIcon(iconPath);
      standardItem = new QStandardItem(wIcon, workflowComponentInfo->caption());
      found = false;
    }
    else
    {
      QIcon wIcon = QIcon(iconPath);
      standardItem->setIcon(wIcon);
      standardItem->setText(workflowComponentInfo->caption());
    }


    standardItem->setCheckable(true);
    standardItem->setDragEnabled(false);

    QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", workflowComponentInfo->id())
                   .replace("[Version]", workflowComponentInfo->version())
                   .replace("[Url]", workflowComponentInfo->url())
                   .replace("[Caption]", workflowComponentInfo->caption())
                   .replace("[IconPath]", iconPath)
                   .replace("[Description]", workflowComponentInfo->description())
                   .replace("[License]", workflowComponentInfo->license())
                   .replace("[Vendor]", workflowComponentInfo->vendor())
                   .replace("[Email]", workflowComponentInfo->email())
                   .replace("[Copyright]", workflowComponentInfo->copyright());


    standardItem->setToolTip(html);
    standardItem->setWhatsThis(html);
    standardItem->setStatusTip(workflowComponentInfo->caption());

    QMap<QString,QVariant> data;
    data["WorkflowComponentInfo"] = workflowComponentInfoId;
    standardItem->setData(data,Qt::UserRole + 1);

    if(found == false)
    {
      parent->appendRow(standardItem);
    }

    treeViewWorkflowComponents->expand(parent->index());

    QObject* componentInfoAsObject = dynamic_cast<QObject*>(workflowComponentInfo);

    if(componentInfoAsObject)
    {
      connect(componentInfoAsObject , SIGNAL(propertyChanged(const QString&)),
              this,SLOT(onComponentInfoPropertyChanged(const QString&)));
    }
  }
}

void HydroCoupleComposer::onWorkflowComponentInfoUnloadedLoaded(const QString &workflowComponentInfoId)
{
  IWorkflowComponentInfo *workflowComponentInfo = m_project->componentManager()->findWorkflowComponentInfo(workflowComponentInfoId);

  if(workflowComponentInfo)
  {
    QStandardItem *standardItem = findStandardItem(workflowComponentInfoId, m_workflowComponentModel->invisibleRootItem(), "WorkflowComponentInfo", true);

    if(standardItem)
    {
      m_workflowComponentModel->removeRow(standardItem->row(), standardItem->parent()->index());
    }

    IWorkflowComponent *workflowComponent = m_project->componentManager()->getWorkflowComponent(workflowComponentInfoId);

    if(m_project->workflowComponent() == workflowComponent)
    {
      m_project->setWorkflowComponent(nullptr);
    }
  }
}

void HydroCoupleComposer::onWorkflowComponentCheckStateChanged(QStandardItem *item)
{
  m_workflowComponentModel->blockSignals(true);

  bool checked = item->checkState() == Qt::Checked;

  QMap<QString,QVariant> data = item->data(Qt::UserRole + 1).toMap();
  QList<QString> identifiers = m_project->componentManager()->workflowComponentInfoIdentifiers();

  if(data.contains("WorkflowComponentInfo"))
  {
    QString workflowId = data["WorkflowComponentInfo"].toString();
    IWorkflowComponent *workflowComponent = m_project->componentManager()->getWorkflowComponent(workflowId);

    if(workflowComponent && checked)
    {
      m_project->setWorkflowComponent(workflowComponent);
    }
    else
    {
      m_project->setWorkflowComponent(nullptr);
    }

    for(int i = 0; i < identifiers.size(); i++)
    {
      QString identifier = identifiers[i];

      if(QString::compare(identifier, workflowId))
      {
        QStandardItem *rItem = findStandardItem(identifier, m_workflowComponentModel->invisibleRootItem(), "WorkflowComponentInfo", true);

        if(rItem != item)
        {
          rItem->setCheckState(Qt::Unchecked);
        }
      }
    }
  }

  m_workflowComponentModel->blockSignals(false);
}

void HydroCoupleComposer::onProjectWorkflowComponentChanged(IWorkflowComponent *workflowComponent)
{
  if(workflowComponent)
  {
    QStandardItem *childCategoryItem = findStandardItem(workflowComponent->componentInfo()->id(), m_workflowComponentModel->invisibleRootItem() , "WorkflowComponentInfo" , true);

    if(childCategoryItem)
    {
      QMap<QString,QVariant> data = childCategoryItem->data(Qt::UserRole + 1).toMap();

      if(data.contains("WorkflowComponentInfo") && !QString::compare(workflowComponent->componentInfo()->id(), data["WorkflowComponentInfo"].toString()))
      {
        childCategoryItem->setCheckState(Qt::Checked);
      }
    }
  }
}

void HydroCoupleComposer::onModelComponentInfoClicked(const QModelIndex& index)
{
  QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();
  IModelComponentInfo *modelComponentInfo = nullptr;

  if (index.isValid() && value.contains("ModelComponentInfo")
      && (modelComponentInfo = m_project->componentManager()->findModelComponentInfo(value["ModelComponentInfo"].toString())))
  {
    QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(modelComponentInfo));
    m_propertyModel->setData(variant);
    treeViewProperties->expandToDepth(0);
    treeViewProperties->resizeColumnToContents(0);
    actionAddComponent->setEnabled(true);
    actionValidateModelComponentInfo->setEnabled(true);
    actionBrowseModelComponentInfoFolder->setEnabled(true);
    actionUnloadModelComponentLibrary->setEnabled(true);
  }
  else
  {
    m_propertyModel->setData(QVariant());
    actionAddComponent->setEnabled(false);
    actionValidateModelComponentInfo->setEnabled(false);
    actionBrowseModelComponentInfoFolder->setEnabled(false);
    actionUnloadModelComponentLibrary->setEnabled(false);
  }
}

void HydroCoupleComposer::onModelComponentInfoDoubleClicked(const QModelIndex& index)
{
  QMap<QString,QVariant> data = index.data(Qt::UserRole + 1).toMap();

  if (data.contains("ModelComponentInfo"))
  {
    QString id = data["ModelComponentInfo"].toString();
    QString message;

    IModelComponent *component = m_project->componentManager()->createModelComponentInstance(id, message);

    if(component)
    {
      GModelComponent* gcomponent = new GModelComponent(component, m_project);

      QPointF f = graphicsViewHydroCoupleComposer->mapToScene(graphicsViewHydroCoupleComposer->frameRect().center()) - gcomponent->boundingRect().bottomRight()/2;

      gcomponent->setPos(f);

      if(m_project->modelComponents().count() == 0)
      {
        gcomponent->setTrigger(true);
      }

      m_project->addModelComponent(gcomponent);
    }
  }
}

void HydroCoupleComposer::onAdaptedOutputClicked(const QModelIndex& index)
{
  QMap<QString,QVariant> data = index.data(Qt::UserRole + 1).toMap();

  bool set = false;

  if(!data.contains("AdaptedOutput") && data.contains("AdaptedOutputFactoryComponentInfo"))
  {
    QString adaptedOutputFactoryComponentInfoId = data["AdaptedOutputFactoryComponentInfo"].toString();

    IAdaptedOutputFactoryComponentInfo *adaptedOutputFactoryComponentInfo = m_project->componentManager()->findAdaptedOutputFactoryComponentInfo(adaptedOutputFactoryComponentInfoId);

    if(adaptedOutputFactoryComponentInfo)
    {
      set = true;
      actionValidateAdaptedOutputFactoryComponentInfo->setEnabled(true);
      actionUnloadAdaptedOutputFactoryComponentLibrary->setEnabled(true);
      actionBrowseAdaptedOutputFactoryComponentInfoFolder->setEnabled(true);
      actionInsertAdaptedOutput->setEnabled(false);
      QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(adaptedOutputFactoryComponentInfo));
      m_propertyModel->setData(variant);
      treeViewProperties->expandToDepth(0);
      treeViewProperties->resizeColumnToContents(0);
    }
  }
  else if(!data.contains("AdaptedOutput") && data.contains("AdaptedOutputFactory"))
  {
    QString adaptedOutputFactoryId = data["AdaptedOutputFactory"].toString();
    QString modelComponentInfoId = data["ModelComponentInfo"].toString();

    IModelComponentInfo *modelComponentInfo = m_project->componentManager()->findModelComponentInfo(modelComponentInfoId);

    if(modelComponentInfo)
    {
      for(IAdaptedOutputFactory *adaptedOutputFactory : modelComponentInfo->adaptedOutputFactories())
      {
        if(!QString::compare(adaptedOutputFactory->id(), adaptedOutputFactoryId))
        {

          set = true;

          actionValidateAdaptedOutputFactoryComponentInfo->setEnabled(false);
          actionUnloadAdaptedOutputFactoryComponentLibrary->setEnabled(false);
          actionBrowseAdaptedOutputFactoryComponentInfoFolder->setEnabled(false);
          actionInsertAdaptedOutput->setEnabled(false);

          QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(adaptedOutputFactory));
          m_propertyModel->setData(variant);
          treeViewProperties->expandToDepth(0);
          treeViewProperties->resizeColumnToContents(0);

          break;
        }
      }
    }
  }
  else if(m_selectedConnections.length() == 1 && data.contains("AdaptedOutput"))
  {
    if(data.contains("AdaptedOutputFactory"))
    {
      GConnection *connection = m_selectedConnections[0];

      QString adaptedOutputId = data["AdaptedOutput"].toString();
      QString adaptedOutputFactoryId = data["AdaptedOutputFactory"].toString();
      QString modelComponentInfoId = data["ModelComponentInfo"].toString();

      IModelComponentInfo *modelComponentInfo = m_project->componentManager()->findModelComponentInfo(modelComponentInfoId);

      if(modelComponentInfo)
      {
        for(IAdaptedOutputFactory *adaptedOutputFactory : modelComponentInfo->adaptedOutputFactories())
        {
          if(!QString::compare(adaptedOutputFactory->id(), adaptedOutputFactoryId))
          {
            GOutput *output = dynamic_cast<GOutput*>(connection->producer());
            GInput *input = dynamic_cast<GInput*>(connection->consumer());

            QList<IIdentity*> identities = adaptedOutputFactory->getAvailableAdaptedOutputIds(output->output(), input->input());

            for(IIdentity* identity : identities)
            {
              if(!QString::compare(adaptedOutputId, identity->id()))
              {
                set = true;

                actionValidateAdaptedOutputFactoryComponentInfo->setEnabled(false);
                actionUnloadAdaptedOutputFactoryComponentLibrary->setEnabled(false);
                actionBrowseAdaptedOutputFactoryComponentInfoFolder->setEnabled(false);
                actionInsertAdaptedOutput->setEnabled(true);

                QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(identity));
                m_propertyModel->setData(variant);
                treeViewProperties->expandToDepth(0);
                treeViewProperties->resizeColumnToContents(0);

                break;
              }
            }
            break;
          }
        }
      }
    }
    else if(data.contains("AdaptedOutputFactoryComponentInfo"))
    {
      GConnection *connection = m_selectedConnections[0];

      QString adaptedOutputId = data["AdaptedOutput"].toString();
      QString adaptedOutputFactoryComponentInfoId = data["AdaptedOutputFactoryComponentInfo"].toString();

      IAdaptedOutputFactoryComponent *adaptedOutputFactoryComponent = m_project->componentManager()->getAdaptedOutputFactoryComponent(adaptedOutputFactoryComponentInfoId);

      if(adaptedOutputFactoryComponent)
      {
        GOutput *output = dynamic_cast<GOutput*>(connection->producer());
        GInput *input = dynamic_cast<GInput*>(connection->consumer());

        QList<IIdentity*> identities = adaptedOutputFactoryComponent->getAvailableAdaptedOutputIds(output->output(), input->input());

        for(IIdentity* identity : identities)
        {
          if(!QString::compare(adaptedOutputId, identity->id()))
          {
            set = true;

            actionValidateAdaptedOutputFactoryComponentInfo->setEnabled(false);
            actionUnloadAdaptedOutputFactoryComponentLibrary->setEnabled(false);
            actionBrowseAdaptedOutputFactoryComponentInfoFolder->setEnabled(false);
            actionInsertAdaptedOutput->setEnabled(true);

            QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(identity));
            m_propertyModel->setData(variant);
            treeViewProperties->expandToDepth(0);
            treeViewProperties->resizeColumnToContents(0);

            break;
          }
        }
      }
    }
  }


  if(!set)
  {
    m_propertyModel->setData(QVariant());
    actionValidateAdaptedOutputFactoryComponentInfo->setEnabled(false);
    actionBrowseAdaptedOutputFactoryComponentInfoFolder->setEnabled(false);
    actionInsertAdaptedOutput->setEnabled(false);
    actionUnloadAdaptedOutputFactoryComponentLibrary->setEnabled(false);
  }
}

void HydroCoupleComposer::onAdaptedOutputDoubleClicked(const QModelIndex& index)
{
  if(m_selectedConnections.length() == 1)
  {
    GConnection* connection = m_selectedConnections[0];

    if(connection->producer()->nodeType() == GNode::NodeType::Output ||
       connection->producer()->nodeType() == GNode::NodeType::AdaptedOutput)
    {
      QMap<QString,QVariant> mapData = index.data(Qt::UserRole + 1).toMap();

      if(mapData.contains("AdaptedOutput"))
      {
        QString adaptedOutputId = mapData["AdaptedOutput"].toString();

        if(mapData.contains("AdaptedOutputFactory"))
        {
          connection->insertAdaptedOutput(adaptedOutputId, mapData["AdaptedOutputFactory"].toString());
        }
        else if(mapData.contains("AdaptedOutputFactoryComponentInfo"))
        {
          connection->insertAdaptedOutput(adaptedOutputId, mapData["AdaptedOutputFactoryComponentInfo"].toString(), true);
        }
      }
    }
  }
}

void HydroCoupleComposer::onAdaptedOutputDoubleClicked(GAdaptedOutput* adaptedOutput)
{
  if(m_argumentDialog->isHidden())
  {
    m_argumentDialog->show();
    m_argumentDialog->setAdaptedOutput(adaptedOutput);
  }
}

void HydroCoupleComposer::onInsertAdaptedOutput()
{

}

void HydroCoupleComposer::onWorkflowComponentClicked(const QModelIndex &index)
{
  QMap<QString,QVariant> value = index.data(Qt::UserRole + 1).toMap();

  if(value.contains("WorkflowComponentInfo"))
  {
    QString workflowComponentInfoId = value["WorkflowComponentInfo"].toString();

    IWorkflowComponentInfo *workflowComponentInfo = m_project->componentManager()->findWorkflowComponentInfo(workflowComponentInfoId);

    if(workflowComponentInfo)
    {
      QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(workflowComponentInfo));
      m_propertyModel->setData(variant);
      treeViewProperties->expandToDepth(0);
      treeViewProperties->resizeColumnToContents(0);
      actionValidateWorkflowComponentInfo->setEnabled(true);
      actionBrowseWorkflowComponentInfoFolder->setEnabled(true);
      actionUnloadWorkflowComponentLibrary->setEnabled(true);

      return;
    }
  }

  m_propertyModel->setData(QVariant());
  actionValidateWorkflowComponentInfo->setEnabled(false);
  actionBrowseWorkflowComponentInfoFolder->setEnabled(false);
  actionUnloadWorkflowComponentLibrary->setEnabled(false);
}

void HydroCoupleComposer::onModelComponentStatusItemClicked(const QModelIndex &index)
{
  ModelStatusItem* item = nullptr;

  if((item = static_cast<ModelStatusItem*>(index.internalPointer())))
  {
    m_propertyModel->setData(QVariant::fromValue(dynamic_cast<QObject*>(item->component())));
    treeViewProperties->expandToDepth(1);
    return;
  }

  m_propertyModel->setData(QVariant());
}

void HydroCoupleComposer::onModelComponentStatusItemDoubleClicked(const QModelIndex &index)
{
  ModelStatusItem* item = nullptr;

  if((item = static_cast<ModelStatusItem*>(index.internalPointer())))
  {

    for(GModelComponent* modelComponent : m_project->modelComponents())
    {
      if(modelComponent->modelComponent() == item->component())
      {

        QRectF itemsBoundingRect = modelComponent->sceneBoundingRect();

        float height = itemsBoundingRect.height()/2.0;

        itemsBoundingRect.adjust(-height , -height,height, height);

        graphicsViewHydroCoupleComposer->fitInView(itemsBoundingRect, Qt::AspectRatioMode::KeepAspectRatio);

        return;
      }
    }
  }
}

void HydroCoupleComposer::onComponentInfoPropertyChanged(const QString& propertyName)
{

  IModelComponentInfo *modelComponentInfo = nullptr ;
  IAdaptedOutputFactoryComponentInfo *adaptedOutputFactoryComponentInfo = nullptr;
  IAdaptedOutputFactory *adaptedOutputFactory = nullptr;
  IWorkflowComponentInfo *workflowComponentInfo = nullptr;

  if((modelComponentInfo = dynamic_cast<IModelComponentInfo*>(sender())))
  {
    QStandardItem* componentTreeViewItem = findStandardItem(modelComponentInfo->id(), m_modelComponentInfoModel->invisibleRootItem(), "ModelComponentInfo", true);

    if(componentTreeViewItem && componentTreeViewItem != m_modelComponentInfoModel->invisibleRootItem())
    {

      QString iconPath = ":/HydroCoupleComposer/hydrocouplecomposer";
      QFileInfo iconFile(QFileInfo(modelComponentInfo->libraryFilePath()).dir(), modelComponentInfo->iconFilePath());

      if (iconFile.exists())
      {
        iconPath = iconFile.absoluteFilePath();
      }

      QIcon cIcon = QIcon(iconPath);
      componentTreeViewItem->setIcon(cIcon);
      componentTreeViewItem->setStatusTip(modelComponentInfo->id());

      QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", modelComponentInfo->id())
                     .replace("[Version]", modelComponentInfo->version())
                     .replace("[Url]", modelComponentInfo->url())
                     .replace("[Caption]", modelComponentInfo->caption())
                     .replace("[IconPath]", iconPath)
                     .replace("[Description]", modelComponentInfo->description())
                     .replace("[License]", modelComponentInfo->license())
                     .replace("[Vendor]", modelComponentInfo->vendor())
                     .replace("[Email]", modelComponentInfo->email())
                     .replace("[Copyright]", modelComponentInfo->copyright());


      componentTreeViewItem->setToolTip(html);
      componentTreeViewItem->setWhatsThis(html);
    }
  }
  else if((adaptedOutputFactoryComponentInfo = dynamic_cast<IAdaptedOutputFactoryComponentInfo*>(sender())))
  {
    QStandardItem* componentTreeViewItem = findStandardItem(adaptedOutputFactoryComponentInfo->id(), m_externalAdaptedOutputFactoriesStandardItem , "AdaptedOutputFactoryComponentInfo", true);

    if(componentTreeViewItem)
    {
      QString iconPath = ":/HydroCoupleComposer/adaptedoutputfactory";

      QFileInfo iconFile(QFileInfo(adaptedOutputFactoryComponentInfo->libraryFilePath()).dir(), adaptedOutputFactoryComponentInfo->iconFilePath());

      if (iconFile.exists())
      {
        iconPath = iconFile.absoluteFilePath();
      }

      QIcon cIcon = QIcon(iconPath);
      componentTreeViewItem->setIcon(cIcon);

      QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", adaptedOutputFactoryComponentInfo->id())
                     .replace("[Version]", adaptedOutputFactoryComponentInfo->version())
                     .replace("[Url]", adaptedOutputFactoryComponentInfo->url())
                     .replace("[Caption]", adaptedOutputFactoryComponentInfo->caption())
                     .replace("[IconPath]", iconPath)
                     .replace("[Description]", adaptedOutputFactoryComponentInfo->description())
                     .replace("[License]", adaptedOutputFactoryComponentInfo->license())
                     .replace("[Vendor]", adaptedOutputFactoryComponentInfo->vendor())
                     .replace("[Email]", adaptedOutputFactoryComponentInfo->email())
                     .replace("[Copyright]", adaptedOutputFactoryComponentInfo->copyright());


      componentTreeViewItem->setToolTip(html);
      componentTreeViewItem->setWhatsThis(html);
    }
  }
  else if((adaptedOutputFactory = dynamic_cast<IAdaptedOutputFactory*>(sender())))
  {
    QStandardItem* componentTreeViewItem = findStandardItem(adaptedOutputFactory->id(), m_internalAdaptedOutputFactoriesStandardItem , "AdaptedOutputFactory", true);



    QString desc =  QString(sc_standardInfoHtml).replace("[Component]", adaptedOutputFactory->id())
                    .replace("[Caption]", adaptedOutputFactory->caption())
                    .replace("[Description]", adaptedOutputFactory->description());

    componentTreeViewItem->setToolTip(desc);
    componentTreeViewItem->setWhatsThis(desc);
    componentTreeViewItem->setStatusTip(adaptedOutputFactory->caption());
  }
  else if((workflowComponentInfo = dynamic_cast<IWorkflowComponentInfo*>(sender())))
  {
    QStandardItem* standardItem = findStandardItem(workflowComponentInfo->id(), m_workflowComponentModel->invisibleRootItem(), "WorkflowComponentInfo", true);

    QString iconPath = ":/HydroCoupleComposer/workflow";
    QFileInfo iconFile(QFileInfo(workflowComponentInfo->libraryFilePath()).dir(), workflowComponentInfo->iconFilePath());

    if(iconFile.isFile() && iconFile.exists())
    {
      iconPath = iconFile.absoluteFilePath();
    }

    QIcon wIcon = QIcon(iconPath);
    standardItem->setIcon(wIcon);

    QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", workflowComponentInfo->id())
                   .replace("[Version]", workflowComponentInfo->version())
                   .replace("[Url]", workflowComponentInfo->url())
                   .replace("[Caption]", workflowComponentInfo->caption())
                   .replace("[IconPath]", iconPath)
                   .replace("[Description]", workflowComponentInfo->description())
                   .replace("[License]", workflowComponentInfo->license())
                   .replace("[Vendor]", workflowComponentInfo->vendor())
                   .replace("[Email]", workflowComponentInfo->email())
                   .replace("[Copyright]", workflowComponentInfo->copyright());


    standardItem->setToolTip(html);
    standardItem->setWhatsThis(html);
    standardItem->setStatusTip(workflowComponentInfo->caption());

  }
}

void HydroCoupleComposer::onModelComponentAdded(GModelComponent* modelComponent)
{
  connect(modelComponent , SIGNAL(doubleClicked(GModelComponent*)),
          this , SLOT(onModelComponentDoubleClicked(GModelComponent*)));

  connect(modelComponent , SIGNAL(doubleClicked(GAdaptedOutput*)),
          this , SLOT(onAdaptedOutputDoubleClicked(GAdaptedOutput*)));

  addRemoveNodeToGraphicsView(modelComponent);

  for(GInput* input : modelComponent->inputGraphicObjects().values())
  {
    addRemoveNodeToGraphicsView(input);
  }

  m_modelStatusItemModel->addNewModel(modelComponent->modelComponent());
  treeViewSimulationStatus->expandAll();
}

void HydroCoupleComposer::onModelComponentDeleting(GModelComponent *modelComponent)
{
  m_modelStatusItemModel->removeModel(modelComponent->modelComponent());
  HydroCoupleVis::getInstance()->removeModelComponent(modelComponent->modelComponent());
}

void HydroCoupleComposer::onModelComponentDoubleClicked(GModelComponent *modelComponent)
{
  if(!m_argumentDialog->isVisible() && (modelComponent->modelComponent()->status() == IModelComponent::Created
                                        || modelComponent->modelComponent()->status() == IModelComponent::Failed
                                        || modelComponent->modelComponent()->status() == IModelComponent::Initialized ))
  {
    m_argumentDialog->show();
    m_argumentDialog->setComponent(modelComponent);
  }
}

void HydroCoupleComposer::onItemDroppedInGraphicsView(const QPointF& scenePos, const QMap<QString,QVariant>& dropData)
{
  if(dropData.contains("ModelComponentInfo"))
  {
    QString modelComponentInfoId = dropData["ModelComponentInfo"].toString();

    QString message;
    IModelComponent* component = m_project->componentManager()->createModelComponentInstance(modelComponentInfoId,message);

    if(component)
    {
      GModelComponent* gcomponent = new GModelComponent(component, m_project);

      QPointF f(scenePos);
      f.setX(f.x() - gcomponent->boundingRect().width() / 2);
      f.setY(f.y() - gcomponent->boundingRect().height() / 2);

      gcomponent->setPos(f);

      if (m_project->modelComponents().count() == 0)
      {
        gcomponent->setTrigger(true);
      }

      m_project->addModelComponent(gcomponent);
    }

    if(!message.isEmpty())
    {
      onPostMessage(message);
    }
  }
  else if(dropData.contains("AdaptedOutput"))
  {
    if(m_selectedConnections.length() == 1)
    {
      GConnection* connection = m_selectedConnections[0];

      if(connection->sceneBoundingRect().contains(scenePos))
      {
        if(dropData.contains("AdaptedOutputFactory") && dropData.contains("AdaptedOutputFactoryModelComponentInfo"))
        {
          QString adaptedOutputId = dropData["AdaptedOutput"].toString();
          QString adaptedOutputFactoryId = dropData["AdaptedOutputFactory"].toString();
          connection->insertAdaptedOutput(adaptedOutputId, adaptedOutputFactoryId);
        }
        else if(dropData.contains("AdaptedOutputFactoryComponentInfo"))
        {

        }
      }
    }
  }
}

void HydroCoupleComposer::onProjectHasChanges(bool hasChanges)
{
  if(hasChanges)
  {
    setWindowTitle(m_project->projectFile().fileName() + "[*] - HydroCouple Composer");
  }
  else
  {
    setWindowTitle(m_project->projectFile().fileName() + " - HydroCouple Composer");
  }

  setWindowModified(hasChanges);
}

void HydroCoupleComposer::onSetCurrentTool(bool on)
{
  //QAction* action = static_cast<QAction*>(sender());

  if (actionDefaultSelect->isChecked())
  {
    m_currentTool = GraphicsView::Tool::Default;
  }
  else if (actionZoomIn->isChecked())
  {
    m_currentTool = GraphicsView::Tool::ZoomIn;
  }
  else if (actionZoomOut->isChecked())
  {
    m_currentTool = GraphicsView::Tool::ZoomOut;
  }
  else if (actionPan->isChecked())
  {
    m_currentTool = GraphicsView::Tool::Pan;
  }

  graphicsViewHydroCoupleComposer->onCurrentToolChanged(m_currentTool);

  emit currentToolChanged(m_currentTool);
}

void HydroCoupleComposer::onZoomExtent()
{
  QRectF itemsBoundingRect = graphicsViewHydroCoupleComposer->scene()->itemsBoundingRect();
  graphicsViewHydroCoupleComposer->fitInView(itemsBoundingRect, Qt::AspectRatioMode::KeepAspectRatio);
}

void HydroCoupleComposer::onSelectionChanged()
{
  QList<QGraphicsItem*> graphicsItems = graphicsViewHydroCoupleComposer->scene()->selectedItems();
  QList<QObject*> qobjectGraphicItems;
  m_selectedModelComponents.clear();
  m_selectedConnections.clear();
  m_selectedExchangeItems.clear();
  m_selectedNodes.clear();
  m_selectedAdaptedOutputs.clear();

  for(QGraphicsItem* graphicsItem : graphicsItems)
  {

    GModelComponent* component = nullptr ;
    GConnection* connection = nullptr;
    GExchangeItem* exchangeItem = nullptr;
    QObject* object = nullptr;

    if((object = dynamic_cast<QObject*>(graphicsItem)))
    {
      qobjectGraphicItems.append(object);
    }

    if( (component = dynamic_cast<GModelComponent*>(graphicsItem)))
    {
      m_selectedModelComponents.insert(0, component);
      m_selectedNodes.insert(0,component);
    }
    else if((connection = dynamic_cast<GConnection*> (graphicsItem)))
    {
      if(connection->producer()->nodeType() != GNode::Component &&
         connection->consumer()->nodeType() != GNode::Component)
      {
        m_selectedConnections.insert(0 , connection);
      }
    }
    else if((exchangeItem = dynamic_cast<GExchangeItem*> (graphicsItem)))
    {
      m_selectedExchangeItems.insert(0 , exchangeItem);
      m_selectedNodes.insert(0,exchangeItem);

      GAdaptedOutput* adaptedOutputExchangeItem = dynamic_cast<GAdaptedOutput*>(exchangeItem);

      if(adaptedOutputExchangeItem)
      {
        m_selectedAdaptedOutputs.append(adaptedOutputExchangeItem);
      }
    }
  }

  if(m_selectedNodes.length())
  {
    if(m_selectedModelComponents.length())
    {
      actionDeleteComponent->setEnabled(true);
      actionCloneModelComponent->setEnabled(true);
      actionSetTrigger->setEnabled(true);
      actionInitializeComponent->setEnabled(true);
      actionValidateComponent->setEnabled(true);
    }
    else
    {
      actionDeleteComponent->setEnabled(false);
      actionCloneModelComponent->setEnabled(false);
      actionSetTrigger->setEnabled(false);
      actionInitializeComponent->setEnabled(false);
      actionValidateComponent->setEnabled(false);
    }

    actionAlignBottom->setEnabled(true);
    actionAlignTop->setEnabled(true);
    actionAlignLeft->setEnabled(true);
    actionAlignRight->setEnabled(true);
    actionAlignHorizontalCenters->setEnabled(true);
    actionAlignVerticalCenters->setEnabled(true);
    actionDistributeTopEdges->setEnabled(true);
    actionDistributeBottomEdges->setEnabled(true);
    actionDistributeLeftEdges->setEnabled(true);
    actionDistributeRightEdges->setEnabled(true);
    actionDistributeHorizontalCenters->setEnabled(true);
    actionDistributeVerticalCenters->setEnabled(true);

    if(actionCreateConnection->isChecked())
    {
      if(m_connProd && m_selectedExchangeItems.length() == 1)
      {
        createConnection(m_connProd , m_selectedExchangeItems[0]);
      }
      else if( m_selectedExchangeItems.length() == 2 && m_selectedExchangeItems[0] == m_connProd)
      {
        createConnection(m_connProd , m_selectedExchangeItems[1]);
      }
      else if( m_selectedExchangeItems.length() == 2 && m_selectedExchangeItems[1] == m_connProd)
      {
        createConnection(m_connProd , m_selectedExchangeItems[0]);
      }
      else if(!m_connProd && ! m_connCons && m_selectedExchangeItems.length() == 2)
      {
        if( m_selectedExchangeItems[0] ->zValue() <  m_selectedExchangeItems[1]->zValue())
        {
          createConnection(m_selectedExchangeItems[0] , m_selectedExchangeItems[1]);
        }
        else
        {
          createConnection(m_selectedExchangeItems[1] , m_selectedExchangeItems[0]);
        }
      }
      else if(m_selectedExchangeItems.length() == 1 )
      {
        m_connProd = m_selectedExchangeItems[0];
      }
    }
  }
  else
  {
    actionDeleteComponent->setEnabled(false);
    actionCloneModelComponent->setEnabled(false);
    actionSetTrigger->setEnabled(false);
    actionInitializeComponent->setEnabled(false);
    actionValidateComponent->setEnabled(false);
    actionAlignBottom->setEnabled(false);
    actionAlignTop->setEnabled(false);
    actionAlignLeft->setEnabled(false);
    actionAlignRight->setEnabled(false);
    actionAlignHorizontalCenters->setEnabled(false);
    actionAlignVerticalCenters->setEnabled(false);
    actionDistributeTopEdges->setEnabled(false);
    actionDistributeBottomEdges->setEnabled(false);
    actionDistributeLeftEdges->setEnabled(false);
    actionDistributeRightEdges->setEnabled(false);
    actionDistributeHorizontalCenters->setEnabled(false);
    actionDistributeVerticalCenters->setEnabled(false);
  }

  if(m_selectedConnections.length())
  {
    actionDeleteConnection->setEnabled(true);
  }
  else
  {
    actionDeleteConnection->setEnabled(false);
  }

  resetAdaptedOutputFactoryModel(m_adaptedOutputFactoriesModel->invisibleRootItem());

  if ((m_selectedModelComponents.length() == 1 && m_selectedAdaptedOutputs.length() == 0) ||
      (m_selectedModelComponents.length() == 0 && m_selectedAdaptedOutputs.length() == 1))
  {
    actionEditSelected->setEnabled(true);
  }
  else
  {
    actionEditSelected->setEnabled(false);
  }

  // property grid
  if(qobjectGraphicItems.length() == 1)
  {
    QVariant propValue = QVariant::fromValue(qobjectGraphicItems[0])   ;
    m_propertyModel->setData(propValue);
    treeViewProperties->expandToDepth(0);
    treeViewProperties->resizeColumnToContents(0);
  }
  else
  {
    m_propertyModel->setData(QVariant());
  }
}

void HydroCoupleComposer::onAlignTop()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    double top = std::numeric_limits<double>::max();

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];

      if (model->pos().y() < top)
      {
        top = model->pos().y();
      }
    }

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      model->setY(top);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onAlignBottom()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    double bottom = std::numeric_limits<double>::min();

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];

      if (model->sceneBoundingRect().bottom() > bottom)
      {
        bottom = model->sceneBoundingRect().bottom();
      }
    }

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      setBottom(m_selectedNodes[i], bottom);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onAlignLeft()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    double left = std::numeric_limits<double>::max();

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];

      if (model->pos().x() < left)
      {
        left = model->pos().x();
      }
    }

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      model->setX(left);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onAlignRight()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    double right = std::numeric_limits<double>::min();

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];

      if (model->sceneBoundingRect().right() > right)
      {
        right = model->sceneBoundingRect().right();
      }
    }

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      setRight(m_selectedNodes[i], right);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onAlignVerticalCenters()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    double centerY = std::numeric_limits<double>::max();

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];

      if (model->sceneBoundingRect().center().y() < centerY)
      {
        centerY = model->sceneBoundingRect().center().y();
      }
    }

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      setVerticalCenter(m_selectedNodes[i], centerY);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onAlignHorizontalCenters()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    double centerX = std::numeric_limits<double>::max();

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];

      if (model->sceneBoundingRect().center().x() < centerX)
      {
        centerX = model->sceneBoundingRect().center().x();
      }
    }

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      setHorizontalCenter(m_selectedNodes[i], centerX);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onDistributeTopEdges()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    qSort(m_selectedNodes.begin(), m_selectedNodes.end(), &HydroCoupleComposer::compareTopEdges);

    double bottom = m_selectedNodes[0]->pos().y();
    double top = m_selectedNodes[m_selectedNodes.count() - 1]->pos().y();

    double dx = (top - bottom) / (m_selectedNodes.count() - 1.0);

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      model->setY(bottom + i*dx);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onDistributeBottomEdges()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    qSort(m_selectedNodes.begin(), m_selectedNodes.end(), &HydroCoupleComposer::compareBottomEdges);

    double bottom = m_selectedNodes[0]->sceneBoundingRect().bottom();
    double top = m_selectedNodes[m_selectedNodes.count() - 1]->sceneBoundingRect().bottom();

    double dx = (top - bottom) / (m_selectedNodes.count() - 1.0);

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      setBottom(model, bottom + i*dx);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onDistributeLeftEdges()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    qSort(m_selectedNodes.begin(), m_selectedNodes.end(), &HydroCoupleComposer::compareLeftEdges);

    double left = m_selectedNodes[0]->pos().x();
    double right = m_selectedNodes[m_selectedNodes.count() - 1]->pos().x();

    double dx = (right - left) / (m_selectedNodes.count() - 1.0);

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      model->setX(left + i*dx);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onDistributeRightEdges()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    qSort(m_selectedNodes.begin(), m_selectedNodes.end(), &HydroCoupleComposer::compareRightEdges);

    double left = m_selectedNodes[0]->sceneBoundingRect().right();
    double right = m_selectedNodes[m_selectedNodes.count() - 1]->sceneBoundingRect().right();

    double dx = (right - left) / (m_selectedNodes.count() - 1.0);

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      setRight(model, left + i*dx);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onDistributeVerticalCenters()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    qSort(m_selectedNodes.begin(), m_selectedNodes.end(), &HydroCoupleComposer::compareVerticalCenters);

    double bottom = m_selectedNodes[0]->sceneBoundingRect().center().y();
    double top = m_selectedNodes[m_selectedNodes.count() - 1]->sceneBoundingRect().center().y();

    double dx = (top - bottom) / (m_selectedNodes.count() - 1.0);

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      setVerticalCenter(model, bottom + i*dx);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onDistributeHorizontalCenters()
{
  if (m_selectedNodes.count() > 1)
  {
    graphicsViewHydroCoupleComposer->blockSignals(true);

    qSort(m_selectedNodes.begin(), m_selectedNodes.end(), &HydroCoupleComposer::compareHorizontalCenters);

    double left = m_selectedNodes[0]->sceneBoundingRect().center().x();
    double right = m_selectedNodes[m_selectedNodes.count() - 1]->sceneBoundingRect().center().x();

    double dx = (right - left) / (m_selectedNodes.count() - 1.0);

    for (int i = 0; i < m_selectedNodes.count(); i++)
    {
      GNode* model = m_selectedNodes[i];
      setHorizontalCenter(model, left + i*dx);
    }

    graphicsViewHydroCoupleComposer->blockSignals(false);
  }
}

void HydroCoupleComposer::onTreeViewModelComponentInfoContextMenuRequested(const QPoint& pos)
{
  m_treeViewModelComponentInfoContextMenu->exec(treeViewModelComponentInfos->viewport()->mapToGlobal(pos));
}

void HydroCoupleComposer::onTreeViewAdaptedOutputFactoryComponentContextMenuRequested(const QPoint &pos)
{
  m_treeViewAdaptedOutputFactoryComponentInfoContextMenu->exec(treeViewAdaptedOutputFactoryComponents->viewport()->mapToGlobal(pos));
}

void HydroCoupleComposer::onTreeViewWorkflowComponentContextMenuRequested(const QPoint &pos)
{
  m_treeViewWorkflowComponentInfoContextMenu->exec(treeViewWorkflowComponents->viewport()->mapToGlobal(pos));
}

void HydroCoupleComposer::onGraphicsViewHydroCoupleComposerContextMenuRequested(const QPoint& pos)
{
  if(m_project->modelComponents().size() > 1)
  {
    actionCreateConnection->setEnabled(true);
  }
  else
  {
    actionCreateConnection->setEnabled(false);
  }

  m_graphicsContextMenu->exec(graphicsViewHydroCoupleComposer->viewport()->mapToGlobal(pos));
}

void HydroCoupleComposer::onShowHydroCoupleVis()
{
  if(!HydroCoupleVis::getInstance()->isVisible())
  {
    HydroCoupleVis::getInstance()->show();

    for(GModelComponent* component : m_project->modelComponents())
    {
      HydroCoupleVis::getInstance()->addModelComponent(component->modelComponent());
    }

  }
}

void HydroCoupleComposer::onAbout()
{
  QMessageBox::about(this, "HydroCouple Composer",
                     "<html>"
                     "<head>"
                     "<title>Component Information</title>"
                     "</head>"
                     "<body>"
                     "<img alt=\"icon\" src=':/HydroCoupleComposer/hydrocouplecomposer' width=\"100\" align=\"left\" />"
                     "<h3 align=\"center\">HydroCouple Composer 1.0.0</h3>"
                     "<hr>"
                     "<p>Build Date: 11/30/2017</p>"
                     "<p align=\"center\">Copyright 2014-2017. The HydroCouple Organization. All rights reserved.</p>"
                     "<p align=\"center\">This program and its associated libraries is provided AS IS with NO WARRANTY OF ANY KIND, "
                     "INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.</p>"
                     "<p align=\"center\"><a href=\"mailto:caleb.buahin@gmail.com?Subject=HydroCouple Composer\">caleb.buahin@gmail.com</a></p>"
                     "<p align=\"center\"><a href=\"www.hydrocouple.org\">www.hydrocouple.org</a></p>"
                     "</body>"
                     "</html>");
}

void HydroCoupleComposer::onPreferences()
{
  m_preferencesDialog->show();
}
