#include "stdafx.h"
#include "hydrocouplecomposer.h"
#include "qpropertyitemdelegate.h"
#include "gmodelcomponent.h"
#include "custompropertyitems.h"
#include <QPolygonF>
#include <iostream>
#include <string>
#include <random>
#include <QPrinter>
#include <QPrintDialog>
#include <QPointF>
#include "qxmlsyntaxhighlighter.h"
#include "simulationmanager.h"

using namespace HydroCouple;
using namespace std;

const QString HydroCoupleComposer::sc_modelComponentInfoHtml =
    "<html>"
    "<head>"
    "<title>Component Information</title>"
    "</head>"
    "<body>"
    "<img alt=\"icon\" src='[IconPath]' width=\"50\" align=\"left\" /><h3 align=\"center\">[Component]</h3>"
    "<h3 align=\"center\">[Version]</h3>"
    "<h4 align=\"center\">[Caption]</h4>"
    "<hr>"
    "<p>[Description]</p>"
    "<p align=\"center\">[License]</p>"
    "<p align=\"center\"><a href=\"[Url]\">[Vendor]</a></p>"
    "<p align=\"center\"><a href=\"mailto:[Email]?Subject=[Component] | [Version] | [License]\">[Email]</a></p>"
    "<p align=\"center\">[Copyright]</p>"
    "</body>"
    "</html>";

QIcon HydroCoupleComposer::s_categoryIcon = QIcon();

HydroCoupleComposer::HydroCoupleComposer(QWidget *parent)
  : QMainWindow(parent),
    m_simulationManager(nullptr)
{
  setupUi(this);

  qRegisterMetaType<QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>>();
  qRegisterMetaType<QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>>("QSharedPointer<HydroCouple::IComponentStatusChangeEventArgs>");

  m_argumentDialog = new ArgumentDialog();
  new QXMLSyntaxHighlighter(m_argumentDialog->textEditInput);
  m_modelStatusItemModel = new ModelStatusItemModel(this);
  m_project = new HydroCoupleProject(this);
  m_componentInfoModel = new QStandardItemModel(this);
  m_adaptedOutputFactoriesModel = new QStandardItemModel(this);
  m_propertyModel = new QPropertyModel(this);
  m_qVariantPropertyHolder = new QVariantHolderHelper(QVariant() , this);
  m_graphicsContextMenu = new QMenu(this);
  m_treeviewComponentInfoContextMenu = new QMenu(this);

  initializeGUIComponents();
  initializeComponentInfoTreeView();
  initializeAdaptedOutputTreeView();
  initializePropertyGrid();
  initializeActions();
  initializeContextMenus();
  initializeSignalSlotConnections();
  initializeProjectSignalSlotConnections();
  initializeSimulationStatusTreeView();
  readSettings();
}

HydroCoupleComposer::~HydroCoupleComposer()
{
  m_argumentDialog->close();
  delete m_argumentDialog;
}

HydroCoupleProject* HydroCoupleComposer::project() const
{
  return m_project;
}

void HydroCoupleComposer::setProject(HydroCoupleProject *project)
{
  if(m_project)
  {
    delete m_project;
  }

  m_project = project;
  setWindowTitle(m_project->projectFile().fileName() + "[*] - HydroCouple Composer");
  setWindowModified(m_project->hasChanges());

  initializeProjectSignalSlotConnections();

  for(GModelComponent* modelComponent : m_project->modelComponents())
  {
    onModelComponentAdded(modelComponent);
  }

  for(IModelComponentInfo* cinfo : m_project->componentManager()->modelComponentInfoById().values())
  {
    onModelComponentInfoLoaded(cinfo);
  }

  for(IAdaptedOutputFactoryComponentInfo* cinfo : m_project->componentManager()->adaptedOutputFactoryComponentInfoById().values())
  {
    onAdaptedOutputFactoryComponentInfoLoaded(cinfo);
  }


  m_simulationManager = new SimulationManager(m_project);

  connect(m_simulationManager, &SimulationManager::postMessage , this , &HydroCoupleComposer::onPostMessage);
  connect(m_simulationManager, &SimulationManager::setProgress , this , &HydroCoupleComposer::onSetProgress);

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

          if (m_project->deleteComponent(model))
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

void HydroCoupleComposer::openFile(const QFileInfo& file)
{

  onPostMessage("Opening file \"" + file.absoluteFilePath() + "\"");
  m_lastOpenedFilePath = file.absoluteFilePath();
  m_lastPath = file.absolutePath();

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
    m_project->componentManager()->loadComponent(file);
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

    if ((project = HydroCoupleProject::readProjectFile(file, errorMessages)))
    {
      setProject(project);
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
      m_project->addComponent(modelComponent);
    }

    for(QString message : errorMessages)
    {
      onPostMessage(message);
    }
  }

  onUpdateRecentFiles();
}

void HydroCoupleComposer::readSettings()
{
  onPostMessage("Reading settings");

  m_settings.beginGroup("MainWindow");

  //!HydroCoupleComposer GUI
  this->restoreState(m_settings.value("WindowState", saveState()).toByteArray());
  this->setWindowState((Qt::WindowState)m_settings.value("WindowStateEnum", (int)this->windowState()).toInt());
  this->setGeometry(m_settings.value("Geometry", geometry()).toRect());

  //!recent files
  m_recentFiles = m_settings.value("Recent Files", m_recentFiles).toStringList();

  while (m_recentFiles.size() > 10)
  {
    m_recentFiles.removeLast();
  }

  onUpdateRecentFiles();

  m_settings.endGroup();


  //   m_settings.beginGroup("ConnectionDialog");
  //   m_connectionDialog->restoreGeometry(m_settings.value("WindowState", m_connectionDialog->saveGeometry()).toByteArray());
  //   m_connectionDialog->setWindowState((Qt::WindowState)m_settings.value("WindowStateEnum", (int)m_connectionDialog->windowState()).toInt());
  //   m_connectionDialog->setGeometry(m_settings.value("Geometry", m_connectionDialog->geometry()).toRect());
  //   m_settings.endGroup();

  m_settings.beginGroup("ArgumentDialog");
  m_argumentDialog->restoreGeometry(m_settings.value("WindowState", m_argumentDialog->saveGeometry()).toByteArray());
  m_argumentDialog->setWindowState((Qt::WindowState)m_settings.value("WindowStateEnum", (int)m_argumentDialog->windowState()).toInt());
  m_argumentDialog->setGeometry(m_settings.value("Geometry", m_argumentDialog->geometry()).toRect());
  m_settings.endGroup();

  //!last path opened
  m_lastOpenedFilePath = m_settings.value("LastPath", m_lastOpenedFilePath).toString();

  onPostMessage("Finished reading settings");
}

void HydroCoupleComposer::writeSettings()
{
  onPostMessage("Writing settings");

  m_settings.beginGroup("MainWindow");
  m_settings.setValue("WindowState", saveState());
  m_settings.setValue("WindowStateEnum", (int)this->windowState());
  m_settings.setValue("Geometry", geometry());
  m_settings.setValue("Recent Files", m_recentFiles);
  m_settings.endGroup();

  //   m_settings.beginGroup("ConnectionDialog");
  //   m_settings.setValue("WindowState", m_connectionDialog->saveGeometry());
  //   m_settings.setValue("WindowStateEnum", (int)m_connectionDialog->windowState());
  //   m_settings.setValue("Geometry", m_connectionDialog->geometry());
  //   m_settings.endGroup();

  m_settings.beginGroup("ArgumentDialog");
  m_settings.setValue("WindowState", m_argumentDialog->saveGeometry());
  m_settings.setValue("WindowStateEnum", (int)m_argumentDialog->windowState());
  m_settings.setValue("Geometry", m_argumentDialog->geometry());
  m_settings.endGroup();

  m_settings.setValue("LastPath", m_lastOpenedFilePath);

  onPostMessage("Finished writing settings");
}

void HydroCoupleComposer::initializeGUIComponents()
{
  //Progressbar
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

void HydroCoupleComposer::initializeComponentInfoTreeView()
{
  m_componentInfoModel->clear();

  QStandardItem* rootItem = m_componentInfoModel->invisibleRootItem();

  s_categoryIcon.addFile(":/HydroCoupleComposer/close", QSize(), QIcon::Mode::Normal, QIcon::State::Off);
  s_categoryIcon.addFile(":/HydroCoupleComposer/open", QSize(), QIcon::Mode::Normal, QIcon::State::On);
  rootItem->setIcon(s_categoryIcon);

  rootItem->setText("Model Components");
  rootItem->setToolTip("Model Components");
  rootItem->setStatusTip("Model Components");
  rootItem->setWhatsThis("Model Components");

  QStringList temp; temp << "";
  m_componentInfoModel->setHorizontalHeaderLabels(temp);
  m_componentInfoModel->setSortRole(Qt::DisplayRole);

  treeViewModelComponentInfos->setModel(m_componentInfoModel);
  treeViewModelComponentInfos->expand(rootItem->index());

  m_modelComponentInfoStandardItem = new QStandardItem("Models");
  QIcon modelIcon = QIcon(":/HydroCoupleComposer/hydrocouplecomposer");
  m_modelComponentInfoStandardItem->setIcon(modelIcon);
  QMap<QString,QVariant> mdata ;
  mdata["ModelComponentInfo"] = "Models";
  m_modelComponentInfoStandardItem->setData(mdata, Qt::UserRole);
  m_componentInfoModel->appendRow(m_modelComponentInfoStandardItem);


  m_adaptedOutputComponentInfoStandardItem = new QStandardItem("AdaptedOutput Factories");
  QIcon adaptedOutputFactoryIcon = QIcon(":/HydroCoupleComposer/adaptercomponent");
  m_adaptedOutputComponentInfoStandardItem->setIcon(adaptedOutputFactoryIcon);

  QMap<QString,QVariant> adata;
  adata["AdaptedOutputFactoryComponentInfo"] = "AdaptedOutputFactories";
  m_adaptedOutputComponentInfoStandardItem->setData(true, Qt::UserRole);
  m_componentInfoModel->appendRow(m_adaptedOutputComponentInfoStandardItem);

}

void HydroCoupleComposer::initializeSimulationStatusTreeView()
{
  treeViewSimulationStatus->setModel(m_modelStatusItemModel);
  ModelStatusItemStyledItemDelegate* statusProgressStyledItemDelegate = new ModelStatusItemStyledItemDelegate(treeViewSimulationStatus);
  treeViewSimulationStatus->setItemDelegate(statusProgressStyledItemDelegate);
  treeViewSimulationStatus->setColumnWidth(3,400);
  //treeViewSimulationStatus->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
}

void HydroCoupleComposer::initializeAdaptedOutputTreeView()
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

  treeViewAdaptedOutputs->setModel(m_adaptedOutputFactoriesModel);
  treeViewAdaptedOutputs->expand(rootItem->index());
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

  for (int i = 0; i < 10; i++)
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

  //ComponentInfoTreeview
  connect(treeViewModelComponentInfos, SIGNAL(clicked(const QModelIndex&)), this, SLOT(onComponentInfoClicked(const QModelIndex&)));
  connect(treeViewModelComponentInfos, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(onComponentInfoDoubleClicked(const QModelIndex&)));


  //ModelComponentStatus
  connect(treeViewSimulationStatus, SIGNAL(clicked(const QModelIndex&)) , this , SLOT(onModelComponentStatusItemClicked(const QModelIndex&)));
  connect(treeViewSimulationStatus, SIGNAL(doubleClicked(const QModelIndex&)) , this , SLOT(onModelComponentStatusItemDoubleClicked(const QModelIndex&)));


  //Adaptedout factory
  connect(treeViewAdaptedOutputs , SIGNAL(clicked(const QModelIndex&)) , SLOT(onAdaptedOutputClicked(const QModelIndex&)));
  connect(treeViewAdaptedOutputs , SIGNAL(doubleClicked(const QModelIndex&)) , SLOT(onAdaptedOutputDoubleClicked(const QModelIndex&)));

  //Toolbar actions
  connect(actionDefaultSelect, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionZoomIn, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionZoomOut, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionPan, SIGNAL(toggled(bool)), this, SLOT(onSetCurrentTool(bool)));
  connect(actionZoomExtent, SIGNAL(triggered()), this, SLOT(onZoomExtent()));

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


  connect(actionValidateComponentLibrary , SIGNAL ( triggered()) , this , SLOT( onValidateModelComponentLibrary()));
  connect(actionBrowseComponentInfoFolder , SIGNAL(triggered()) , this , SLOT(onBrowseToComponentLibraryPath()));
  connect(actionLoadModelComponentLibrary , SIGNAL(triggered()) , this , SLOT(onLoadComponentLibrary()));
  connect(actionUnloadModelComponentLibrary , SIGNAL(triggered()) , this , SLOT(onUnloadComponentLibrary()));

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
  connect(m_project->componentManager(), &ComponentManager::adaptedOutputFactoryComponentUnloaded, this, &HydroCoupleComposer::onAdaptedOutputFactoryComponentInfoUnloaded);
  connect(m_project->componentManager(), &ComponentManager::postMessage, this, &HydroCoupleComposer::onPostMessage);
  connect(m_project->componentManager(), SIGNAL(setProgress(bool,int,int,int)), this, SLOT(onSetProgress(bool,int,int,int)));
}

void HydroCoupleComposer::initializeContextMenus()
{
  //Component Info
  m_treeviewComponentInfoContextMenu->addAction(actionAddComponent);
  m_treeviewComponentInfoContextMenu->addAction(actionValidateComponentLibrary);
  m_treeviewComponentInfoContextMenu->addAction(actionBrowseComponentInfoFolder);
  m_treeviewComponentInfoContextMenu->addAction(actionLoadModelComponentLibrary);


  m_graphicsContextMenu->addAction(actionSetTrigger);
  m_graphicsContextMenu->addAction(actionCloneModelComponent);
  m_graphicsContextMenu->addAction(actionDeleteComponent);
  m_graphicsContextMenu->addAction(actionInitializeComponent);
  m_graphicsContextMenu->addAction(actionValidateComponent);

  m_graphicsContextMenu->addSeparator();

  m_graphicsContextMenu->addAction(actionCreateConnection);
  m_graphicsContextMenu->addAction(actionDeleteConnection);

  m_graphicsContextMenu->addSeparator();

  QMenu* layout = new QMenu("Layout Components",m_graphicsContextMenu);
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

void HydroCoupleComposer::resetAdaptedOutputFactoryModel()
{
  m_adaptedOutputFactoriesModel->clear();

  QStringList temp; temp << "";
  m_adaptedOutputFactoriesModel->setHorizontalHeaderLabels(temp);
  m_adaptedOutputFactoriesModel->setSortRole(Qt::DisplayRole);


  QIcon factoryIcon(":/HydroCoupleComposer/adaptedoutputfactory");
  QIcon adaptedOutputIcon(":/HydroCoupleComposer/adaptedoutput");

  if(m_selectedConnections.length() == 1 &&
     dynamic_cast<GOutput*>(m_selectedConnections[0]->producer()) &&
     dynamic_cast<GInput*>(m_selectedConnections[0]->consumer()) &&
     m_selectedConnections[0]->adaptedOutputFactories().count())
  {
    GConnection* connection  = m_selectedConnections[0];

    for(IAdaptedOutputFactory* factory : connection->adaptedOutputFactories())
    {
      QStandardItem* factoryItem = new QStandardItem(factoryIcon, factory->caption());
      factoryItem->setStatusTip(factory->id());

      IAdaptedOutputFactoryComponent* factoryComponent = dynamic_cast<IAdaptedOutputFactoryComponent*>(factory);

      if(factoryComponent)
      {
        QString iconPath("");
        QFileInfo iconFile(QFileInfo(factoryComponent->componentInfo()->libraryFilePath()).dir(),
                           factoryComponent->componentInfo()->iconFilePath());

        if (iconFile.exists())
        {
          iconPath = iconFile.absoluteFilePath();
        }

        QIcon cIcon = QIcon(iconPath);
        factoryItem->setIcon(cIcon);
      }

      QMap<QString,QVariant> data;
      data["AdaptedOutputFactory"] = factory->id();
      factoryItem->setData(data, Qt::UserRole);

      QString desc ="<html>"
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

      QString html = QString(desc).replace("[Component]", factory->id())
                     .replace("[Caption]", factory->caption())
                     .replace("[Description]", factory->description());

      factoryItem->setToolTip(html);
      factoryItem->setWhatsThis(html);
      m_adaptedOutputFactoriesModel->appendRow(factoryItem);

      QList<IIdentity*> identities = connection->adaptedOutputs()[factory];

      for(IIdentity* identity : identities)
      {
        QStandardItem* adapterId = new QStandardItem(adaptedOutputIcon,identity->caption());

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
        idData["AdaptedOutputFactory"] = factory->id();

        adapterId->setData(idData, Qt::UserRole);
        adapterId->setDragEnabled(true);

        factoryItem->appendRow(adapterId);
      }
    }
  }

  treeViewAdaptedOutputs->expandAll();
}

void HydroCoupleComposer::createConnection(GExchangeItem *producer, GExchangeItem *consumer)
{
  if(producer != consumer &&
     (producer->nodeType() == GNode::Output || producer->nodeType() == GNode::AdaptedOutput) &&
     ( consumer->nodeType() == GNode::Input || consumer->nodeType() == GNode::MultiInput  || consumer->nodeType() == GNode::AdaptedOutput))
  {
    producer->createConnection(consumer);
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

      QMap<QString,QVariant> data = child->data(Qt::UserRole).toMap();

      if (data.contains(key) && !data[key].toString().compare(id, Qt::CaseInsensitive))
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

  return parent;
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
      qDebug() << "";
    }

    delete[] toIdP;

    if(connection->consumer()->connections().length() && connection->consumer()->nodeType() != GNode::Component)
    {
      layoutEdges(graph,identifiers,connection->consumer());
    }
  }

  delete[] fromIdP;
}

void HydroCoupleComposer::stringToCharP(const QString& text, char * & output)
{
  output = new char[text.length() + 1];
  stpcpy(output , text.toStdString().c_str());
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
  }
  else
  {
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
  }
}

void HydroCoupleComposer::onPostMessage(const QString& message)
{
  this->statusBarMain->showMessage(message ,10000);
  textEdit->append(QDateTime::currentDateTime().toString() + " : " + message);
}

void HydroCoupleComposer::onPostToStatusBar(const QString& message)
{
  this->statusBarMain->showMessage(message ,10000);
}

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

void HydroCoupleComposer::onSave()
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

  //   writeSettings();
}

void HydroCoupleComposer::onSaveAs()
{
  QString file;

  if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,
                                            "HydroCouple Composer Project (*.hcp)")).isEmpty())
  {
    QFileInfo inputFile(file);
    m_project->onSaveProjectAs(inputFile);
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
  if(!m_lastOpenedFilePath.isEmpty() && !m_lastOpenedFilePath.isNull())
  {
    for (int i = 0; i < m_recentFiles.count(); i++)
    {
      if (!m_recentFiles[i].compare(m_lastOpenedFilePath, Qt::CaseInsensitive))
      {
        m_recentFiles.removeAt(i);
      }
    }

    m_recentFiles.prepend(m_lastOpenedFilePath);
  }

  while (m_recentFiles.size() > 10)
  {
    m_recentFiles.removeLast();
  }


  if (m_recentFiles.count())
  {
    m_clearRecentFilesAction->setVisible(true);
    m_recentFilesMenu->setEnabled(true);

    for (int i = 0; i < 10; i++)
    {
      QAction* action = m_recentFilesActions[i];

      if (i < m_recentFiles.count())
      {
        QFileInfo file(m_recentFiles[i]);
        QString fullPath(file.absoluteFilePath());
        QString text = tr("&%1. %2").arg(i + 1).arg(file.fileName());
        action->setText(text);
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
    m_clearRecentFilesAction->setVisible(false);
    m_recentFilesMenu->setEnabled(false);
  }
}

void HydroCoupleComposer::onClearRecentFiles()
{
  m_recentFiles.clear();
  m_lastOpenedFilePath = "";
  onUpdateRecentFiles();
}

void HydroCoupleComposer::onClearSettings()
{

  IModelComponentInfo* m_comp = m_project->componentManager()->modelComponentInfoById().values()[0];
  GModelComponent* comp1 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp1->modelComponent()->setCaption("Comp1");
  GModelComponent* comp2 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp2->modelComponent()->setCaption("Comp2");
  GModelComponent* comp3 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp3->modelComponent()->setCaption("Comp3");
  GModelComponent* comp4 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp4->modelComponent()->setCaption("Comp4");
  GModelComponent* comp5 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp5->modelComponent()->setCaption("Comp5");
  GModelComponent* comp6 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp6->modelComponent()->setCaption("Comp6");
  GModelComponent* comp7 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp7->modelComponent()->setCaption("Comp7");
  GModelComponent* comp8 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp8->modelComponent()->setCaption("Comp8");
  GModelComponent* comp9 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp9->modelComponent()->setCaption("Comp9");
  GModelComponent* comp10 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp10->modelComponent()->setCaption("Comp10");
  GModelComponent* comp11 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp11->modelComponent()->setCaption("Comp11");
  GModelComponent* comp12 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp12->modelComponent()->setCaption("Comp12");
  GModelComponent* comp13 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp13->modelComponent()->setCaption("Comp13");
  GModelComponent* comp14 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp14->modelComponent()->setCaption("Comp14");
  GModelComponent* comp15 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp15->modelComponent()->setCaption("Comp15");
  GModelComponent* comp16 = new GModelComponent(m_comp->createComponentInstance(), m_project);
  comp16->modelComponent()->setCaption("Comp16");


  m_project->addComponent(comp1);
  m_project->addComponent(comp2);
  m_project->addComponent(comp3);
  m_project->addComponent(comp4);
  m_project->addComponent(comp5);
  m_project->addComponent(comp6);
  m_project->addComponent(comp7);
  m_project->addComponent(comp8);
  m_project->addComponent(comp9);
  m_project->addComponent(comp10);
  m_project->addComponent(comp11);
  m_project->addComponent(comp12);
  m_project->addComponent(comp13);
  m_project->addComponent(comp14);
  m_project->addComponent(comp15);
  m_project->addComponent(comp16);

  QRectF viewRect = QRectF(graphicsViewHydroCoupleComposer->viewport()->rect());
  QPolygonF rect = graphicsViewHydroCoupleComposer->mapToScene(viewRect.toRect());

  uniform_real_distribution<qreal> xdist (rect.boundingRect().left() , rect.boundingRect().right());
  uniform_real_distribution<qreal> ydist (rect.boundingRect().top() , rect.boundingRect().bottom());
  std::default_random_engine generator;

  for( GModelComponent * p : m_project->modelComponents())
  {
    p->setPos( xdist(generator) , ydist(generator));
  }

  onZoomExtent();
}

void HydroCoupleComposer::onEditSelectedItem()
{
  if(m_selectedModelComponents.length() == 1 && m_selectedAdaptedOutputs.length() == 0)
  {
    if( m_argumentDialog->isHidden())
    {
      GModelComponent *modelComponent = m_selectedModelComponents[0];
      if(modelComponent->modelComponent()->status() == HydroCouple::Created || modelComponent->modelComponent()->status() == HydroCouple::Failed)
      {
        m_argumentDialog->show();
        m_argumentDialog->setComponent(modelComponent);
      }
    }
  }
  else if (m_selectedModelComponents.length() == 0 && m_selectedAdaptedOutputs.length() == 1)
  {
    if(m_argumentDialog->isHidden())
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

        if(m_project->componentManager()->modelComponentInfoById().contains(id))
        {
          IModelComponentInfo* foundModelComponentInfo = m_project->componentManager()->modelComponentInfoById()[id];

          IModelComponent* component = foundModelComponentInfo->createComponentInstance();

          GModelComponent* gcomponent = new GModelComponent(component, m_project);

          QPointF f = graphicsViewHydroCoupleComposer->mapToScene(graphicsViewHydroCoupleComposer->frameRect().center()) - gcomponent->boundingRect().bottomRight()/2;

          gcomponent->setPos(f);

          if (m_project->modelComponents().count() == 0)
          {
            gcomponent->setTrigger(true);
          }

          m_project->addComponent(gcomponent);
          return;
        }
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
      if( m_selectedExchangeItems[0] ->zValue() <  m_selectedExchangeItems[1]->zValue())
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

  onZoomExtent();

}

void HydroCoupleComposer::onCloneModelComponents()
{
  if(m_selectedModelComponents.length())
  {
    for(GModelComponent* model : m_selectedModelComponents)
    {
      model->modelComponent()->clone();
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
      if(gModelComponent->modelComponent()->status() == ComponentStatus::Created ||
         gModelComponent->modelComponent()->status() == ComponentStatus::Initialized ||
         gModelComponent->modelComponent()->status() == ComponentStatus::Failed)
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

      if(modelComponent->status() == ComponentStatus::Initialized)
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

      if (m_project->deleteComponent(model))
      {
        setStatusTip(id + " has been removed");
      }
    }

    graphicsViewHydroCoupleComposer->scene()->blockSignals(false);

    m_selectedModelComponents.clear();

    m_propertyModel->setData(QVariant());
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

    m_selectedConnections.clear();
  }

}

void HydroCoupleComposer::onValidateModelComponentLibrary()
{
  QModelIndex index = treeViewModelComponentInfos->currentIndex();

  if (index.isValid())
  {
    QVariant value = index.data(Qt::UserRole);

    if (value.type() != QVariant::Bool)
    {
      QString id = value.toString();

      if ( m_project->componentManager()->modelComponentInfoById().contains(id))
      {
        IModelComponentInfo* foundComponentInfo = m_project->componentManager()->modelComponentInfoById()[id];

        QString validationMessage;

        if(foundComponentInfo->validateLicense(validationMessage))
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

void HydroCoupleComposer::onBrowseToComponentLibraryPath()
{
  QModelIndex index = treeViewModelComponentInfos->currentIndex();

  if (index.isValid())
  {
    QVariant value = index.data(Qt::UserRole);

    if (value.type() != QVariant::Bool)
    {
      QString id = value.toString();
      IComponentInfo* foundComponentInfo = NULL;
      if ((foundComponentInfo = m_project->componentManager()->findComponentInfoById(id)))
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
  if (QMessageBox::question(this, "Unload ?", "All model instances associated with the component library being unloaded will be deleted. Are you sure you want to unload selected component library ? ",
                            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
  {
    QModelIndex index = treeViewModelComponentInfos->currentIndex();

    if (index.isValid())
    {
      QVariant value = index.data(Qt::UserRole);

      if (value.type() != QVariant::Bool)
      {
        QString id = value.toString();
        m_project->componentManager()->unloadModelComponentInfoById(id);
        treeViewModelComponentInfos->setCurrentIndex(QModelIndex());
        onComponentInfoClicked(QModelIndex());
      }
    }
  }
}

void HydroCoupleComposer::onModelComponentInfoLoaded(const IModelComponentInfo* modelComponentInfo)
{
  QStringList categories = modelComponentInfo->category().split('\\', QString::SplitBehavior::SkipEmptyParts);

  QStandardItem* parent = m_modelComponentInfoStandardItem;

  while (categories.count() > 0)
  {
    QString category = categories[0];

    QStandardItem* childCategoryItem = findStandardItem(category, parent , "ModelComponentInfo" , true);

    if (childCategoryItem == parent)
    {
      QStandardItem* newCategoryItem = new QStandardItem(s_categoryIcon, category);
      newCategoryItem->setToolTip(category);
      newCategoryItem->setStatusTip(category);
      newCategoryItem->setWhatsThis(category);

      QMap<QString,QVariant> data;
      data["ModelComponentInfo"] = category;
      newCategoryItem->setData(data, Qt::UserRole);

      parent->appendRow(newCategoryItem);
      parent = newCategoryItem;
    }
    else
    {
      parent = childCategoryItem;
    }

    categories.removeAt(0);
  }

  QString iconPath = ":/HydroCoupleComposer/hydrocouplecomposer";

  QFileInfo iconFile(QFileInfo(modelComponentInfo->libraryFilePath()).dir(), modelComponentInfo->iconFilePath());

  if (iconFile.exists())
  {
    iconPath = iconFile.absoluteFilePath();
  }

  QIcon cIcon = QIcon(iconPath);

  QStandardItem* componentTreeViewItem = new QStandardItem(cIcon, modelComponentInfo->caption());
  componentTreeViewItem->setStatusTip(modelComponentInfo->caption());
  componentTreeViewItem->setDragEnabled(true);

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
  QMap<QString,QVariant> data;
  data["ModelComponentInfo"] = modelComponentInfo->id();
  componentTreeViewItem->setData(data, Qt::UserRole);

  onPostMessage("Component Library Loaded: " + modelComponentInfo->id());

  parent->appendRow(componentTreeViewItem);

  const QObject* modelComponentInfoObject = dynamic_cast<const QObject*>(modelComponentInfo);

  if(modelComponentInfoObject)
  {
    connect( modelComponentInfoObject , SIGNAL(propertyChanged(const QString &))
             , this , SLOT(onComponentInfoPropertyChanged(const QString &)));
  }

  treeViewModelComponentInfos->expandToDepth(2);
}

void HydroCoupleComposer::onModelComponentInfoUnloaded(const QString& id)
{
  QStandardItem* childCategoryItem = findStandardItem(id, m_modelComponentInfoStandardItem, "ModelComponentInfo" , true);

  if(childCategoryItem)
  {
    m_componentInfoModel->removeRow(childCategoryItem->index().row() , m_modelComponentInfoStandardItem->index()) ;
    m_propertyModel->setData(QVariant());
  }

  for(GModelComponent* modelComponent :  m_project->modelComponents())
  {
    if(!modelComponent->modelComponent()->componentInfo()->id().compare(id))
    {
      m_project->deleteComponent(modelComponent);
    }
  }
}

void HydroCoupleComposer::onAdaptedOutputFactoryComponentInfoLoaded(const IAdaptedOutputFactoryComponentInfo *adaptedOutputFactoryComponentInfo)
{

  QStringList categories = adaptedOutputFactoryComponentInfo->category().split('\\', QString::SplitBehavior::SkipEmptyParts);

  QStandardItem* parent = m_adaptedOutputComponentInfoStandardItem;


  while (categories.count() > 0)
  {
    QString category = categories[0];

    QStandardItem* childCategoryItem = findStandardItem(category, parent , "AdaptedOutputFactoryComponentInfo" , true);

    if (childCategoryItem == parent)
    {
      QStandardItem* newCategoryItem = new QStandardItem(s_categoryIcon, category);
      newCategoryItem->setToolTip(category);
      newCategoryItem->setStatusTip(category);
      newCategoryItem->setWhatsThis(category);

      QMap<QString,QVariant> data;
      data["AdaptedOutputFactoryComponentInfo"] = category;
      newCategoryItem->setData(data, Qt::UserRole);
      parent->appendRow(newCategoryItem);
      parent = newCategoryItem;
    }
    else
    {
      parent = childCategoryItem;
    }

    categories.removeAt(0);
  }

  QString iconPath = ":/HydroCoupleComposer/adaptercomponent";

  QFileInfo iconFile(QFileInfo(adaptedOutputFactoryComponentInfo->libraryFilePath()).dir(), adaptedOutputFactoryComponentInfo->iconFilePath());

  if (iconFile.exists())
  {
    iconPath = iconFile.absoluteFilePath();
  }

  QIcon cIcon = QIcon(iconPath);

  QStandardItem* componentTreeViewItem = new QStandardItem(cIcon, adaptedOutputFactoryComponentInfo->caption());
  componentTreeViewItem->setStatusTip(adaptedOutputFactoryComponentInfo->caption());
  componentTreeViewItem->setDragEnabled(true);

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
  componentTreeViewItem->setData(data, Qt::UserRole);




  onPostMessage("Component Library Loaded: " + adaptedOutputFactoryComponentInfo->id());

  parent->appendRow(componentTreeViewItem);

  const QObject* modelComponentInfoObject = dynamic_cast<const QObject*>(adaptedOutputFactoryComponentInfo);

  if(modelComponentInfoObject)
  {
    connect(modelComponentInfoObject , SIGNAL(propertyChanged(const QString&)),
            this,SLOT(onComponentInfoPropertyChanged(const QString&)));
  }

  treeViewModelComponentInfos->expandToDepth(1);

  for(GModelComponent* component : m_project->modelComponents())
  {
    component->retrieveAdaptedOutputsForConnections();
  }

  resetAdaptedOutputFactoryModel();
}

void HydroCoupleComposer::onAdaptedOutputFactoryComponentInfoUnloaded(const QString &id)
{
  QStandardItem* childCategoryItem = findStandardItem(id, m_adaptedOutputComponentInfoStandardItem ,"AdaptedOutputFactoryComponentInfo" , true);

  if(childCategoryItem)
  {
    m_componentInfoModel->removeRow(childCategoryItem->index().row() , m_adaptedOutputComponentInfoStandardItem->index()) ;
    m_propertyModel->setData(QVariant());
    resetAdaptedOutputFactoryModel();
  }
}

void HydroCoupleComposer::onComponentInfoClicked(const QModelIndex& index)
{
  QMap<QString,QVariant> value = index.data(Qt::UserRole).toMap();


  if (index.isValid() && value.contains("ModelComponentInfo")
      && m_project->componentManager()->modelComponentInfoById().contains(value["ModelComponentInfo"].toString()))
  {
    IModelComponentInfo* foundModelComponentInfo = m_project->componentManager()->modelComponentInfoById()[value["ModelComponentInfo"].toString()];

    QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(foundModelComponentInfo));
    m_propertyModel->setData(variant);
    treeViewProperties->expandToDepth(0);
    treeViewProperties->resizeColumnToContents(0);
    actionAddComponent->setEnabled(true);
    actionValidateComponentLibrary->setEnabled(true);
    actionBrowseComponentInfoFolder->setEnabled(true);
    actionUnloadModelComponentLibrary->setEnabled(true);
  }
  else
  {
    m_propertyModel->setData(QVariant());
    actionAddComponent->setEnabled(false);
    actionValidateComponentLibrary->setEnabled(false);
    actionBrowseComponentInfoFolder->setEnabled(false);
    actionUnloadModelComponentLibrary->setEnabled(false);
  }
}

void HydroCoupleComposer::onComponentInfoDoubleClicked(const QModelIndex& index)
{
  QMap<QString,QVariant> data = index.data(Qt::UserRole).toMap();

  if (data.contains("ModelComponentInfo"))
  {
    QString id = data["ModelComponentInfo"].toString();

    if (m_project->componentManager()->modelComponentInfoById().contains(id))
    {
      IModelComponentInfo* foundModelComponentInfo = m_project->componentManager()->modelComponentInfoById()[id];

      IModelComponent* component = foundModelComponentInfo->createComponentInstance();

      GModelComponent* gcomponent = new GModelComponent(component, m_project);

      QPointF f = graphicsViewHydroCoupleComposer->mapToScene(graphicsViewHydroCoupleComposer->frameRect().center()) - gcomponent->boundingRect().bottomRight()/2;

      gcomponent->setPos(f);

      if (m_project->modelComponents().count() == 0)
      {
        gcomponent->setTrigger(true);
      }

      m_project->addComponent(gcomponent);
    }
  }
}

void HydroCoupleComposer::onAdaptedOutputClicked(const QModelIndex& index)
{
  QMap<QString,QVariant> value = index.data(Qt::UserRole).toMap();

  if(m_selectedConnections.length() == 1 &&
     index.isValid() && value.contains("AdaptedOutput") &&
     value.contains("AdaptedOutputFactory"))
  {

    GConnection* connection = m_selectedConnections[0];
    QString factoryId = value["AdaptedOutputFactory"].toString();

    if(connection->adaptedOutputFactories().contains(factoryId))
    {
      QString adaptedOutputId = value["AdaptedOutput"].toString();

      QList<IIdentity*> identities = connection->adaptedOutputs()[connection->adaptedOutputFactories()[factoryId]];

      for(IIdentity* identity : identities)
      {
        if(identity->id().compare(adaptedOutputId, Qt::CaseInsensitive))
        {
          QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(identity));
          m_propertyModel->setData(variant);
          treeViewProperties->expandToDepth(0);
          treeViewProperties->resizeColumnToContents(0);
          actionAddComponent->setEnabled(true);
          actionValidateComponentLibrary->setEnabled(true);
          actionBrowseComponentInfoFolder->setEnabled(true);
          actionUnloadModelComponentLibrary->setEnabled(true);
          return ;
        }
      }
    }
  }

  m_propertyModel->setData(QVariant());
  actionAddComponent->setEnabled(false);
  actionValidateComponentLibrary->setEnabled(false);
  actionBrowseComponentInfoFolder->setEnabled(false);
  actionUnloadModelComponentLibrary->setEnabled(false);

}

void HydroCoupleComposer::onAdaptedOutputDoubleClicked(const QModelIndex& index)
{
  if(m_selectedConnections.length() == 1)
  {
    GConnection* connection = m_selectedConnections[0];

    QMap<QString,QVariant> mapData = index.data(Qt::UserRole).toMap();

    if(mapData.contains("AdaptedOutput") && mapData.contains("AdaptedOutputFactory"))
    {
      QString factoryId = mapData["AdaptedOutputFactory"].toString();

      if(connection->adaptedOutputFactories().contains(factoryId))
      {
        IAdaptedOutputFactory* factory = connection->adaptedOutputFactories()[factoryId];

        QString adaptedOutputId = mapData["AdaptedOutput"].toString();

        if(connection->adaptedOutputs().contains(factory))
        {
          QList<IIdentity*> identities = connection->adaptedOutputs()[factory];

          for(IIdentity* identity : identities)
          {
            if(!identity->id().compare(adaptedOutputId))
            {
              connection->insertAdaptedOutput(identity,factory);
              break;
            }
          }
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

  IModelComponentInfo* modelComponentInfo = nullptr ;
  IAdaptedOutputFactoryComponentInfo* adapted = nullptr;

  if((modelComponentInfo = dynamic_cast<IModelComponentInfo*>(sender())))
  {
    QStandardItem* componentTreeViewItem = findStandardItem( modelComponentInfo->id(), m_componentInfoModel->invisibleRootItem(), "ModelComponentInfo", true);

    if(componentTreeViewItem && componentTreeViewItem != m_componentInfoModel->invisibleRootItem())
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
      componentTreeViewItem->setDragEnabled(true);

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
      componentTreeViewItem->setData(modelComponentInfo->id(), Qt::UserRole);
    }
  }
  else if((adapted = dynamic_cast<IAdaptedOutputFactoryComponentInfo*>(sender())))
  {

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
}

void HydroCoupleComposer::onModelComponentDoubleClicked(GModelComponent *modelComponent)
{
  if(!m_argumentDialog->isVisible() && (modelComponent->modelComponent()->status() == HydroCouple::Created
                                        || modelComponent->modelComponent()->status() == HydroCouple::Failed
                                        || modelComponent->modelComponent()->status() == HydroCouple::Initialized ))
  {
    m_argumentDialog->show();
    m_argumentDialog->setComponent(modelComponent);
  }
}

void HydroCoupleComposer::onItemDroppedInGraphicsView(const QPointF& scenePos, const QMap<QString,QVariant>& dropData)
{
  if(dropData.contains("ModelComponentInfo"))
  {
    if (m_project->componentManager()->modelComponentInfoById().contains(dropData["ModelComponentInfo"].toString()))
    {
      IModelComponentInfo* foundModelComponentInfo = m_project->componentManager()->modelComponentInfoById()[dropData["ModelComponentInfo"].toString()];

      IModelComponent* component = foundModelComponentInfo->createComponentInstance();

      GModelComponent* gcomponent = new GModelComponent(component, m_project);

      QPointF f(scenePos);
      f.setX(f.x() - gcomponent->boundingRect().width() / 2);
      f.setY(f.y() - gcomponent->boundingRect().height() / 2);

      gcomponent->setPos(f);

      if (m_project->modelComponents().count() == 0)
      {
        gcomponent->setTrigger(true);
      }

      m_project->addComponent(gcomponent);
    }
  }
  else if(dropData.contains("AdaptedOutput"))
  {
    if(m_selectedConnections.length() == 1)
    {
      GConnection* connection = m_selectedConnections[0];

      if(connection->sceneBoundingRect().contains(scenePos))
      {
        QMap<QString,QVariant> mapData = dropData;

        if(mapData.contains("AdaptedOutput") && mapData.contains("AdaptedOutputFactory"))
        {
          QString factoryId = mapData["AdaptedOutputFactory"].toString();

          if(connection->adaptedOutputFactories().contains(factoryId))
          {
            IAdaptedOutputFactory* factory = connection->adaptedOutputFactories()[factoryId];

            QString adaptedOutputId = mapData["AdaptedOutput"].toString();

            if(connection->adaptedOutputs().contains(factory))
            {
              QList<IIdentity*> identities = connection->adaptedOutputs()[factory];

              for(IIdentity* identity : identities)
              {
                if(!identity->id().compare(adaptedOutputId))
                {
                  connection->insertAdaptedOutput(identity,factory);
                  break;
                }
              }
            }
          }
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

  resetAdaptedOutputFactoryModel();

  if ((m_selectedModelComponents.length() == 1 && m_selectedAdaptedOutputs.length() == 0)
      || (m_selectedModelComponents.length() == 0 && m_selectedAdaptedOutputs.length() == 1))
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
  m_treeviewComponentInfoContextMenu->exec(treeViewModelComponentInfos->viewport()->mapToGlobal(pos));
}

void HydroCoupleComposer::onGraphicsViewHydroCoupleComposerContextMenuRequested(const QPoint& pos)
{
  m_graphicsContextMenu->exec(graphicsViewHydroCoupleComposer->viewport()->mapToGlobal(pos));
}

void HydroCoupleComposer::onAbout()
{
  QMessageBox::about(this, "HydroCouple Composer",
                     "<html>"
                     "<head>"
                     "<title>Component Information</title>"
                     "</head>"
                     "<body>"
                     "<img alt=\"icon\" src=':/HydroCoupleComposer/hydrocouplecomposer' width=\"100\" align=\"left\" /><h3 align=\"center\">HydroCouple Composer 1.0.0</h3>"
                     "<hr>"
                     "<p>Built on</p>"
                     "<p align=\"left\">Copyright 2014-2017. The HydroCouple Organization. All rights reserved.</p>"
                     "<p align=\"left\">This program and its associated libraries is provided AS IS with NO WARRANTY OF ANY KIND, "
                     "INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.</p>"
                     "<p align=\"center\"><a href=\"mailto:caleb.buahin@gmail.com?Subject=HydroCouple Composer\">caleb.buahin@gmail.com</a></p>"
                     "<p align=\"center\"><a href=\"www.hydrocouple.org\">www.hydrocouple.org</a></p>"
                     "</body>"
                     "</html>");
}

void HydroCoupleComposer::onPreferences()
{

}
