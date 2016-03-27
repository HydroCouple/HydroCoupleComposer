#include "stdafx.h"
#include "hydrocouplecomposer.h"
#include "qpropertyitemdelegate.h"
#include "gmodelcomponent.h"
#include "gmodelcomponentconnection.h"
#include "custompropertyitems.h"
#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>
#include <iostream>
#include <string>

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
   : QMainWindow(parent), m_currentModelComponentzValue(10000), m_currentModelComponentConnectionzValue(-10000)
{
   setupUi(this);


   m_componentManager = new ComponentManager(this);
   m_project = new HydroCoupleProject(this);
   m_componentTreeViewModel = new QStandardItemModel(this);
   m_propertyModel = new QPropertyModel(this);
   m_qVariantPropertyHolder = new QVariantHolderHelper(QVariant() , this);

   initializeGUIComponents();
   setupComponentInfoTreeView();
   setupPropertyGrid();
   setupActions();
   setupContextMenus();
   setupSignalSlotConnections();
   setupProjectSignalSlotConnections();
   readSettings();
}

HydroCoupleComposer::~HydroCoupleComposer()
{

}

ComponentManager* HydroCoupleComposer::componentManager() const
{
   return m_componentManager;
}

HydroCoupleProject* HydroCoupleComposer::project() const
{
   return m_project;
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

   //Must be removed so that can be deleted manually else graphicsView will delete
   for(GModelComponent* comp : m_project->modelComponents())
   {
      for(GModelComponentConnection* conn : comp->modelComponentConnections())
      {
         graphicsViewHydroCoupleComposer->scene()->removeItem(conn);
      }

      graphicsViewHydroCoupleComposer->scene()->removeItem(comp);
   }

   graphicsViewHydroCoupleComposer->scene()->blockSignals(false);

   //graphicsViewHydroCoupleComposer->scene()->clear();
   QMainWindow::closeEvent(event);
}

void HydroCoupleComposer::dragMoveEvent(QDragMoveEvent* event)
{

}

void HydroCoupleComposer::dragEnterEvent(QDragEnterEvent * event)
{

}

void HydroCoupleComposer::dropEvent(QDropEvent * event)
{

}

void HydroCoupleComposer::mousePressEvent(QMouseEvent * event)
{

}

void HydroCoupleComposer::keyPressEvent(QKeyEvent * event)
{
   if (event->key() == Qt::Key::Key_Delete)
   {
      graphicsViewHydroCoupleComposer->scene()->blockSignals(true);

      if (QMessageBox::question(this, "Delete ?", "Are you sure you want to delete selected items ?",
                                QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
      {

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

         if(m_selectModelComponentConnections.length())
         {
            for(GModelComponentConnection* connection : m_selectModelComponentConnections)
            {
               QString id;

               if (connection->producerComponent()->deleteComponentConnection(connection))
               {
                  setStatusTip(id + " has been removed");
               }
            }
         }

         m_selectModelComponentConnections.clear();
      }

      m_propertyModel->setData(QVariant());
      graphicsViewHydroCoupleComposer->scene()->blockSignals(false);
   }
}

void HydroCoupleComposer::openFile(const QFileInfo& file)
{
   onPostMessage("Opening file '/" + file.absoluteFilePath() + "'/");
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
      m_componentManager->addComponent(file);
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

      HydroCoupleProject* project;

      if ((project = HydroCoupleProject::readProjectFile(file)))
      {
         delete m_project;
         m_project = project;
      }

      setupProjectSignalSlotConnections();
   }
   else if(!extension.compare("hcc" , Qt::CaseInsensitive))
   {
      GModelComponent* modelComponent = GModelComponent::readComponentFile(file);
      if(modelComponent)
      {
         m_project->addComponent(modelComponent);
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

   m_settings.setValue("LastPath", m_lastOpenedFilePath);

   onPostMessage("Finished writing settings");
}

void HydroCoupleComposer::initializeGUIComponents()
{
   //Progressbar
   m_progressBar = new QProgressBar(this);
   m_progressBar->setToolTip("Progress bar");
   m_progressBar->setWhatsThis("Sets the progress of a process");
   m_progressBar->setStatusTip("Progress bar");
   m_progressBar->setMaximumWidth(250);
   m_progressBar->setMaximumHeight(18);
   m_progressBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
   statusBarMain->setContentsMargins(0, 0, 0, 0);
   statusBarMain->addPermanentWidget(m_progressBar);
   m_progressBar->setVisible(false);

}

void HydroCoupleComposer::setupComponentInfoTreeView()
{
   QStandardItem* rootItem = m_componentTreeViewModel->invisibleRootItem();

   s_categoryIcon.addFile(":/HydroCoupleComposer/close", QSize(), QIcon::Mode::Normal, QIcon::State::Off);
   s_categoryIcon.addFile(":/HydroCoupleComposer/open", QSize(), QIcon::Mode::Normal, QIcon::State::On);

   rootItem->setIcon(s_categoryIcon);
   rootItem->setText("Model Components");
   rootItem->setToolTip("Model Components");
   rootItem->setStatusTip("Model Components");
   rootItem->setWhatsThis("Model Components");

   QStringList temp; temp << "";
   m_componentTreeViewModel->setHorizontalHeaderLabels(temp);
   m_componentTreeViewModel->setSortRole(Qt::DisplayRole);

   treeViewModelComponentInfos->setModel(m_componentTreeViewModel);
   treeViewModelComponentInfos->expand(rootItem->index());

}

void HydroCoupleComposer::setupPropertyGrid()
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

void HydroCoupleComposer::setupActions()
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
}

void HydroCoupleComposer::setupSignalSlotConnections()
{
   //component manager
   connect(m_componentManager, &ComponentManager::modelComponentInfoLoaded, this, &HydroCoupleComposer::onModelComponentInfoLoaded);
   connect(m_componentManager, &ComponentManager::modelComponentInfoRemoved, this, &HydroCoupleComposer::onModelComponentInfoRemoved);

   connect(graphicsViewHydroCoupleComposer->scene(), SIGNAL(selectionChanged()), this, SLOT(onGraphicsItemsSelected()));


   //ComponentInfoTreeview
   connect(treeViewModelComponentInfos, SIGNAL(clicked(const QModelIndex&)), this, SLOT(onModelComponentInfoClicked(const QModelIndex&)));
   connect(treeViewModelComponentInfos, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(onModelComponentInfoDoubleClicked(const QModelIndex&)));
   //connect(treeViewComponents->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(onCurrentChanged(const QModelIndex &, const QModelIndex &)));

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
   connect(actionPrint, SIGNAL(triggered()), this, SLOT(onPrint()));
   connect(actionClearAllSettings, SIGNAL(triggered()), this, SLOT(onClearSettings()));
   connect(actionAddComponent, SIGNAL(triggered()), this, SLOT(onAddModelComponent()));

   connect(actionValidateComponentLibrary , SIGNAL ( triggered()) , this , SLOT( onValidateModelComponentLibrary()));
   connect(actionBrowseComponentInfoFolder , SIGNAL(triggered()) , this , SLOT(onBrowseToComponentLibraryPath()));
   connect(actionActionAddModelComponentLibrary , SIGNAL(triggered()) , this , SLOT(onOpenComponentLibrary()));
   connect(actionDeleteModelComponentLibrary , SIGNAL(triggered()) , this , SLOT(onRemoveComponentLibrary()));

   connect(actionDefaultSelect, SIGNAL(toggled(bool)), actionCreateConnection, SLOT(setEnabled(bool)));
   connect(actionCreateConnection, SIGNAL(toggled(bool)), this, SLOT(onCreateConnection(bool)));
   connect(actionSetTrigger, SIGNAL(triggered()), this, SLOT(onSetAsTrigger()));
   connect(actionCloneComponent, SIGNAL(triggered()), this, SLOT(onCloneModel()));
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
   connect(graphicsViewHydroCoupleComposer, SIGNAL(statusChanged(const QString&)), this->statusBarMain, SLOT(showMessage(const QString&)));
   connect(graphicsViewHydroCoupleComposer, SIGNAL(modelComponentInfoDropped(const QPointF&, const QString&)), this, SLOT(onModelComponentInfoDropped(const QPointF& , const QString& )));

   //graphicsScene
   connect(graphicsViewHydroCoupleComposer->scene(), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
}

void HydroCoupleComposer::setupProjectSignalSlotConnections()
{
   //HydroCoupleProject
   setWindowModified(m_project->hasChanges());

   connect(m_project, SIGNAL(componentAdded(GModelComponent* )), this, SLOT(onModelComponentAdded(GModelComponent* )));
   connect(m_project, SIGNAL(componentDeleting(GModelComponent* )), this, SLOT(onModelComponentDeleting(GModelComponent* )));
   connect(m_project, SIGNAL(stateModified(bool)), this, SLOT(onProjectHasChanges(bool)));
}

void HydroCoupleComposer::setupContextMenus()
{
   //Component Info
   m_treeviewComponentInfoContextMenuActions.append(actionAddComponent);
   m_treeviewComponentInfoContextMenuActions.append(actionValidateComponentLibrary);
   m_treeviewComponentInfoContextMenuActions.append(actionBrowseComponentInfoFolder);
   m_treeviewComponentInfoContextMenuActions.append(actionActionAddModelComponentLibrary);
   m_treeviewComponentInfoContextMenuActions.append(actionDeleteModelComponentLibrary);
}

void HydroCoupleComposer::createConnection(GModelComponent *producer, GModelComponent *consumer)
{
   if(producer != consumer)
   {
      producer->createComponentModelConnection(consumer);
   }
   actionCreateConnection->setChecked(false);
}

QStandardItem* HydroCoupleComposer::findStandardItem(const QString& displayName, QStandardItem* parent,  QVariant::Type userType, Qt::ItemDataRole role, bool recursive)
{
   if (!displayName.isNull() && !displayName.isEmpty())
   {
      for (int i = 0; i < parent->rowCount(); i++)
      {
         QStandardItem* child = parent->child(i);

         QString cname = child->data(role).toString();
         QVariant userRole = child->data(Qt::UserRole);

         if (userRole.userType() == userType && !cname.compare(displayName, Qt::CaseInsensitive))
         {
            return child;
         }
         else if (recursive)
         {
            QStandardItem* c;

            if (child->hasChildren())
            {
               c = findStandardItem(displayName, child , userType , role , recursive);

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

void HydroCoupleComposer::setRight(GModelComponent * const component, double right)
{
   double diff = right - component->sceneBoundingRect().right();
   component->setX(component->pos().x() + diff);
}

void HydroCoupleComposer::setHorizontalCenter(GModelComponent * const component, double hcenter)
{
   double diff = hcenter - component->sceneBoundingRect().center().x();
   component->setX(component->pos().x() + diff);
}

void HydroCoupleComposer::setBottom(GModelComponent * const component, double bottom)
{
   double diff = bottom - component->sceneBoundingRect().bottom();
   component->setY(component->pos().y() + diff);
}

void HydroCoupleComposer::setVerticalCenter(GModelComponent * const component, double vcenter)
{
   double diff = vcenter - component->sceneBoundingRect().center().y();
   component->setY(component->pos().y() + diff);
}

bool HydroCoupleComposer::compareLeftEdges(GModelComponent*   a, GModelComponent *  b)
{
   return a->pos().x() < b->pos().x();
}

bool HydroCoupleComposer::compareHorizontalCenters(GModelComponent*   a, GModelComponent *  b)
{
   return a->sceneBoundingRect().center().x() < b->sceneBoundingRect().center().x();
}

bool HydroCoupleComposer::compareRightEdges(GModelComponent*   a, GModelComponent *  b)
{
   return a->sceneBoundingRect().right() < b->sceneBoundingRect().right();
}

bool HydroCoupleComposer::compareTopEdges(GModelComponent*   a, GModelComponent *  b)
{
   return a->pos().y() < b->pos().y();
}

bool HydroCoupleComposer::compareVerticalCenters(GModelComponent*   a, GModelComponent *  b)
{

   return a->sceneBoundingRect().center().y() < b->sceneBoundingRect().center().y();
}

bool HydroCoupleComposer::compareBottomEdges(GModelComponent*   a, GModelComponent *  b)
{
   return a->sceneBoundingRect().bottom() < b->sceneBoundingRect().bottom();
}

void HydroCoupleComposer::onSetProgress(bool visible, const QString& message , int progress)
{
   m_progressBar->setVisible(visible);

   if (visible)
   {
      onPostMessage(message);
      m_progressBar->setValue(progress);
   }
}

void HydroCoupleComposer::onPostMessage(const QString& message)
{
   this->statusBarMain->showMessage(QDateTime::currentDateTime().toString() + " | " + message ,10000);
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

   delete m_project;

   m_project = new HydroCoupleProject(this);
   setupProjectSignalSlotConnections();
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
                                                  "HydroCouple Composer Project (*hcp)")).isEmpty())
   {
      m_project->onSaveProjectAs(QFileInfo(file));
   }
}

void HydroCoupleComposer::onSaveAs()
{
   QString file;

   if (!(file = QFileDialog::getSaveFileName(this, "Save", m_lastPath,
                                             "HydroCouple Composer Project (*hcp)")).isEmpty())
   {
      m_project->onSaveProjectAs(QFileInfo(file));
   }
}

void HydroCoupleComposer::onPrint()
{

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

   IModelComponentInfo* m_comp = m_componentManager->modelComponentInfoList()[0];
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

   comp2->createComponentModelConnection(comp1);
   comp1->createComponentModelConnection(comp2);
   comp3->createComponentModelConnection(comp1);
   comp4->createComponentModelConnection(comp1);
   comp5->createComponentModelConnection(comp2);
   comp6->createComponentModelConnection(comp2);
   comp7->createComponentModelConnection(comp2);
   comp8->createComponentModelConnection(comp3);
   comp9->createComponentModelConnection(comp3);
   comp10->createComponentModelConnection(comp3);
   comp11->createComponentModelConnection(comp6);
   comp11->createComponentModelConnection(comp4);
   comp12->createComponentModelConnection(comp13);
   comp13->createComponentModelConnection(comp12);
   comp15->createComponentModelConnection(comp14);
   comp16->createComponentModelConnection(comp15);
   comp13->createComponentModelConnection(comp4);
   comp14->createComponentModelConnection(comp11);

   onZoomExtent();
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

            IModelComponentInfo* foundModelComponentInfo = nullptr;

            if ((foundModelComponentInfo = m_componentManager->findModelComponentInfoById(id)) != nullptr)
            {
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
      if(m_selectedModelComponents.count() > 2)
      {
         actionCreateConnection->setChecked(false);
      }
      else if (m_selectedModelComponents.count() == 2)
      {
         if( m_selectedModelComponents[0] ->zValue() <  m_selectedModelComponents[1]->zValue())
         {
            createConnection(m_selectedModelComponents[0] , m_selectedModelComponents[1]);
         }
         else
         {
            createConnection(m_selectedModelComponents[1] , m_selectedModelComponents[0]);
         }
      }
      else if (m_selectedModelComponents.length() ==1 )
      {
         m_connProd = m_selectedModelComponents[0];
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

   Agraph_t* G = agopen("ComponentLayout" , Agdirected , 0);
   GVC_t* gvc = gvContext();

   if(m_project->modelComponents().length())
   {
      QRectF ff =  graphicsViewHydroCoupleComposer->scene()->itemsBoundingRect() ;

      string g_size  = to_string(ff.width()) + "," + to_string(ff.height());
      char* g_size_c = new char[g_size.length() + 1];

      strcpy(g_size_c, g_size.c_str());

      agsafeset(G , "size" , g_size_c,"");
      agsafeset(G , "nodesep" , "4.0","");
      agsafeset(G , "ranksep" , "2.0","");
      agsafeset(G , "rankdir" , "RL","");

      delete[] g_size_c ;
   }

   //Create nodes

   for(GModelComponent* component : m_project->modelComponents())
   {
      char* name = new char[component->modelComponent()->id().length() +1]();
      strcpy(name, component->modelComponent()->id().toStdString().c_str());
      Agnode_t* node = agnode(G,name,1);

      //agsafeset(node , "fixedsize" ,"true","");

      string height_s = std::to_string(component->boundingRect().height()).c_str();
      char* height = new char[height_s.length() + 1]();
      strcpy(height, height_s.c_str());
      //agsafeset(node , "height" , height,"");


      string width_s = std::to_string(component->boundingRect().width()).c_str();
      char* width = new char[width_s.length() + 1]();
      strcpy(width, width_s.c_str());
      //agsafeset(node , "width" , width,"");

      delete[] name;
      delete[] width;
      delete[] height;
   }
   
   //Create edges
   for(GModelComponent* component : m_project->modelComponents())
   {
      for(GModelComponentConnection* connection : component->modelComponentConnections())
      {
         char* from_n = new char[connection->producerComponent()->modelComponent()->id().length() +1]();
         strcpy(from_n, connection->producerComponent()->modelComponent()->id().toStdString().c_str());
         Agnode_t* from = agnode( G , from_n, 0 );

         char* to_n = new char[connection->consumerComponent()->modelComponent()->id().length() +1]();
         strcpy(to_n, connection->consumerComponent()->modelComponent()->id().toStdString().c_str());
         Agnode_t* to = agnode( G , to_n , 0 );

         Agedge_t* edge =  agedge(G , from , to , "" , 1);

         // agsafeset(edge , "weight" , "1.2","");
         // agsafeset(edge , "len" , "100.0","");

         delete[] from_n;
         delete[] to_n;
      }
   }

   gvLayout(gvc,G,"dot");
   
   for(GModelComponent* component : m_project->modelComponents())
   {

      char* name = new char[component->modelComponent()->id().length() +1]();
      strcpy(name, component->modelComponent()->id().toStdString().c_str());
      Agnode_t* node = agnode( G , name , 0 );
      QPointF pos;
      pos.setX( ND_coord(node).x  - component->boundingRect().width() /2);
      pos.setY( ND_coord(node).y  - component->boundingRect().height() /2);
      component->setPos(pos);

      delete[] name;
   }

   agclose(G);
   gvFreeContext(gvc);

   onZoomExtent();

}

void HydroCoupleComposer::onSetAsTrigger()
{
   if(m_selectedModelComponents.length() == 1 && !m_selectedModelComponents[0]->trigger())
   {
      m_selectedModelComponents[0]->setTrigger(true);
   }
}

void HydroCoupleComposer::onDeleteSelectedComponents()
{
   if (QMessageBox::question(this, "Delete ?", "Are you sure you want to delete selected Model Components ?",
                             QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
   {

      if(m_selectedModelComponents.length())
      {
         graphicsViewHydroCoupleComposer->scene()->blockSignals(true);

         for (GModelComponent* model : m_selectedModelComponents)
         {
            QString id = model->modelComponent()->id();

            if (m_project->deleteComponent(model))
            {
               setStatusTip(id + " has been removed");
            }
         }

         m_selectedModelComponents.clear();

         m_propertyModel->setData(QVariant());
         graphicsViewHydroCoupleComposer->scene()->blockSignals(false);
      }
   }
}

void HydroCoupleComposer::onDeleteSelectedConnections()
{
   if (QMessageBox::question(this, "Delete ?", "Are you sure you want to delete selected connections ?",
                             QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No , QMessageBox::StandardButton::Yes) == QMessageBox::Yes)
   {

      if(m_selectModelComponentConnections.length())
      {
         graphicsViewHydroCoupleComposer->scene()->blockSignals(true);

         for(GModelComponentConnection* connection : m_selectModelComponentConnections)
         {
            QString id;

            if (connection->producerComponent()->deleteComponentConnection(connection))
            {
               setStatusTip(id + " has been removed");
            }
         }
         m_propertyModel->setData(QVariant());
         graphicsViewHydroCoupleComposer->scene()->blockSignals(false);
      }

      m_selectModelComponentConnections.clear();
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
         IModelComponentInfo* foundComponentInfo = NULL;
         if ((foundComponentInfo = m_componentManager->findModelComponentInfoById(id)))
         {
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
         IModelComponentInfo* foundComponentInfo = NULL;
         if ((foundComponentInfo = m_componentManager->findModelComponentInfoById(id)))
         {
            QFileInfo file (foundComponentInfo->libraryFilePath());

            if (file.exists())
            {
               QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
            }
         }
      }
   }
}

void HydroCoupleComposer::onOpenComponentLibrary()
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

void HydroCoupleComposer::onRemoveComponentLibrary()
{
   QModelIndex index = treeViewModelComponentInfos->currentIndex();

   if (index.isValid())
   {
      QVariant value = index.data(Qt::UserRole);

      if (value.type() != QVariant::Bool)
      {
         QString id = value.toString();
         m_componentManager->removeModelComponentInfoById(id);
         treeViewModelComponentInfos->setCurrentIndex(QModelIndex());
         onModelComponentInfoClicked(QModelIndex());
      }
   }
}

void HydroCoupleComposer::onModelComponentInfoLoaded(const IModelComponentInfo* modelComponentInfo)
{

   QStringList categories = modelComponentInfo->category().split('\\', QString::SplitBehavior::SkipEmptyParts);

   QStandardItem* parent = m_componentTreeViewModel->invisibleRootItem();


   while (categories.count() > 0)
   {
      QString category = categories[0];

      QStandardItem* childCategoryItem = findStandardItem(category, parent , QVariant::Bool , Qt::UserRole , true);

      if (childCategoryItem == parent)
      {
         QStandardItem* newCategoryItem = new QStandardItem(s_categoryIcon, category);
         newCategoryItem->setToolTip(category);
         newCategoryItem->setStatusTip(category);
         newCategoryItem->setWhatsThis(category);
         newCategoryItem->setData(true, Qt::UserRole);
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

   QStandardItem* componentTreeViewItem = new QStandardItem(cIcon, modelComponentInfo->name());
   componentTreeViewItem->setStatusTip(modelComponentInfo->name());
   componentTreeViewItem->setDragEnabled(true);

   QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", modelComponentInfo->name())
         .replace("[Version]", modelComponentInfo->version())
         .replace("[Url]", modelComponentInfo->url())
         .replace("[Caption]", modelComponentInfo->getCaption())
         .replace("[IconPath]", iconPath)
         .replace("[Description]", modelComponentInfo->getDescription())
         .replace("[License]", modelComponentInfo->license())
         .replace("[Vendor]", modelComponentInfo->vendor())
         .replace("[Email]", modelComponentInfo->email())
         .replace("[Copyright]", modelComponentInfo->copyright());


   componentTreeViewItem->setToolTip(html);
   componentTreeViewItem->setWhatsThis(html);
   componentTreeViewItem->setData(modelComponentInfo->id(), Qt::UserRole);




   onPostMessage("Component Library Loaded: " + modelComponentInfo->name());

   parent->appendRow(componentTreeViewItem);

   const QObject* modelComponentInfoObject = dynamic_cast<const QObject*>(modelComponentInfo);

   if(modelComponentInfoObject)
   {
      connect( modelComponentInfoObject , SIGNAL(propertyChanged(const QString& , const QVariant& )   )
               , this , SLOT(onModelComponentInfoPropertyChanged(const QString& , const QVariant& ) ));
   }

   treeViewModelComponentInfos->expandToDepth(0);
}

void HydroCoupleComposer::onModelComponentInfoRemoved(const IModelComponentInfo* modelComponentInfo)
{
   QStandardItem* childCategoryItem = findStandardItem(modelComponentInfo->id(), m_componentTreeViewModel->invisibleRootItem() , QVariant::String , Qt::UserRole , true);
   if(childCategoryItem)
   {
      m_componentTreeViewModel->removeRow(childCategoryItem->index().row()) ;
      m_propertyModel->setData(QVariant());
   }
}

void HydroCoupleComposer::onModelComponentInfoClicked(const QModelIndex& index)
{
   QVariant value = index.data(Qt::UserRole);
   IModelComponentInfo* foundModelComponentInfo = nullptr;

   if (index.isValid() && (value.type() != QVariant::Bool)  && (foundModelComponentInfo = m_componentManager->findModelComponentInfoById(value.toString())) != nullptr)
   {
      QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(foundModelComponentInfo));
      m_propertyModel->setData(variant);
      treeViewProperties->expandToDepth(0);
      treeViewProperties->resizeColumnToContents(0);
      actionAddComponent->setEnabled(true);
      actionValidateComponentLibrary->setEnabled(true);
      actionBrowseComponentInfoFolder->setEnabled(true);
      actionDeleteModelComponentLibrary->setEnabled(true);
   }
   else
   {
      m_propertyModel->setData(QVariant());
      actionAddComponent->setEnabled(false);
      actionValidateComponentLibrary->setEnabled(false);
      actionBrowseComponentInfoFolder->setEnabled(false);
      actionDeleteModelComponentLibrary->setEnabled(false);
   }
}

void HydroCoupleComposer::onModelComponentInfoDoubleClicked(const QModelIndex& index)
{
   QVariant value = index.data(Qt::UserRole);

   if (value.type() != QVariant::Bool)
   {
      QString id = value.toString();

      IModelComponentInfo* foundModelComponentInfo = nullptr;

      if ((foundModelComponentInfo = m_componentManager->findModelComponentInfoById(id)) != nullptr)
      {
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

void HydroCoupleComposer::onModelComponentInfoDropped(const QPointF& scenePos, const QString& id)
{
   IModelComponentInfo* foundModelComponentInfo = nullptr;

   if ((foundModelComponentInfo = m_componentManager->findModelComponentInfoById(id)) != nullptr)
   {
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

void HydroCoupleComposer::onModelComponentInfoPropertyChanged(const QString& propertyName, const QVariant& value)
{

   IModelComponentInfo* modelComponentInfo = dynamic_cast<IModelComponentInfo*>(sender());

   if(modelComponentInfo)
   {
      QStandardItem* root = m_componentTreeViewModel->invisibleRootItem();
      QStandardItem* componentTreeViewItem = findStandardItem( modelComponentInfo->id(), root, QVariant::String , Qt::UserRole , true);

      if(componentTreeViewItem && componentTreeViewItem != root)
      {

         QString iconPath = ":/HydroCoupleComposer/hydrocouplecomposer";

         QFileInfo iconFile(QFileInfo(modelComponentInfo->libraryFilePath()).dir(), modelComponentInfo->iconFilePath());

         if (iconFile.exists())
         {
            iconPath = iconFile.absoluteFilePath();
         }

         QIcon cIcon = QIcon(iconPath);
         componentTreeViewItem->setIcon(cIcon);
         componentTreeViewItem->setStatusTip(modelComponentInfo->name());
         componentTreeViewItem->setDragEnabled(true);

         QString html = QString(sc_modelComponentInfoHtml).replace("[Component]", modelComponentInfo->name())
               .replace("[Version]", modelComponentInfo->version())
               .replace("[Url]", modelComponentInfo->url())
               .replace("[Caption]", modelComponentInfo->getCaption())
               .replace("[IconPath]", iconPath)
               .replace("[Description]", modelComponentInfo->getDescription())
               .replace("[License]", modelComponentInfo->license())
               .replace("[Vendor]", modelComponentInfo->vendor())
               .replace("[Email]", modelComponentInfo->email())
               .replace("[Copyright]", modelComponentInfo->copyright());


         componentTreeViewItem->setToolTip(html);
         componentTreeViewItem->setWhatsThis(html);
         componentTreeViewItem->setData(modelComponentInfo->id(), Qt::UserRole);
      }
   }

}

void HydroCoupleComposer::onModelComponentAdded(GModelComponent* modelComponent)
{
   graphicsViewHydroCoupleComposer->scene()->addItem(modelComponent);
   modelComponent->setZValue(m_currentModelComponentzValue);
   m_currentModelComponentzValue++;
   
   connect(modelComponent, SIGNAL(componentConnectionAdded(GModelComponentConnection* )), this, SLOT(onModelComponentConnectionAdded(GModelComponentConnection* )));
   connect(modelComponent, SIGNAL(componentConnectionDeleting(GModelComponentConnection* )), this, SLOT(onModelComponentConnectionDeleting(GModelComponentConnection* )));
}

void HydroCoupleComposer::onModelComponentDeleting(GModelComponent* modelComponent)
{
   graphicsViewHydroCoupleComposer->scene()->removeItem(modelComponent);
}

void HydroCoupleComposer::onModelComponentConnectionAdded(GModelComponentConnection* modelComponentConnection)
{
   graphicsViewHydroCoupleComposer->scene()->addItem(modelComponentConnection);
   modelComponentConnection->setZValue(m_currentModelComponentConnectionzValue);
   connect(modelComponentConnection , SIGNAL(doubleClicked(QGraphicsSceneMouseEvent* )) , this , SLOT( onModelComponentConnectionDoubleClicked(QGraphicsSceneMouseEvent*)));
   m_currentModelComponentConnectionzValue--;
}

void HydroCoupleComposer::onModelComponentConnectionDeleting(GModelComponentConnection* modelComponentConnection)
{
   graphicsViewHydroCoupleComposer->scene()->removeItem(modelComponentConnection);
}

void HydroCoupleComposer::onModelComponentConnectionDoubleClicked(QGraphicsSceneMouseEvent * event)
{
   //Show editor dialog;
}

void HydroCoupleComposer::onProjectHasChanges(bool hasChanges)
{
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
   graphicsViewHydroCoupleComposer->fitInView(graphicsViewHydroCoupleComposer->scene()->itemsBoundingRect(), Qt::AspectRatioMode::KeepAspectRatio);
}

void HydroCoupleComposer::onSelectionChanged()
{
   QList<QGraphicsItem*> graphicsItems = graphicsViewHydroCoupleComposer->scene()->selectedItems();
   QList<QObject*> qobjectGraphicItems ;
   m_selectedModelComponents.clear();
   m_selectModelComponentConnections.clear();

   for(QGraphicsItem* graphicsItem : graphicsItems)
   {
      QObject* test = dynamic_cast<QObject*>(graphicsItem);

      if(test)
      {
         qobjectGraphicItems.append(test);
      }

      GModelComponent* component = nullptr ;
      GModelComponentConnection* connection = nullptr;

      if( (component = dynamic_cast<GModelComponent*>(graphicsItem)))
      {
         m_selectedModelComponents.insert(0, component);
      }
      else if((connection = dynamic_cast<GModelComponentConnection*> (graphicsItem)))
      {
         m_selectModelComponentConnections.insert(0 , connection);
      }
   }

   if(m_selectedModelComponents.length())
   {
      actionDeleteComponent->setEnabled(true);
      actionCloneComponent->setEnabled(true);
      actionSetTrigger->setEnabled(true);
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
         if(m_connProd && m_selectedModelComponents.length() == 1)
         {
            createConnection(m_connProd , m_selectedModelComponents[0]);
         }
         else if( m_selectedModelComponents.length() == 2 && m_selectedModelComponents[0] == m_connProd)
         {
            createConnection(m_connProd , m_selectedModelComponents[1]);
         }
         else if( m_selectedModelComponents.length() == 2 && m_selectedModelComponents[1] == m_connProd)
         {
            createConnection(m_connProd , m_selectedModelComponents[0]);
         }
         else if(!m_connProd && ! m_connCons && m_selectedModelComponents.length() == 2)
         {
            if( m_selectedModelComponents[0] ->zValue() <  m_selectedModelComponents[1]->zValue())
            {
               createConnection(m_selectedModelComponents[0] , m_selectedModelComponents[1]);
            }
            else
            {
               createConnection(m_selectedModelComponents[1] , m_selectedModelComponents[0]);
            }
         }
         else if(m_selectedModelComponents.length() == 1 )
         {
            m_connProd = m_selectedModelComponents[0];
         }
      }
   }
   else
   {
      actionDeleteComponent->setEnabled(false);
      actionCloneComponent->setEnabled(false);
      actionSetTrigger->setEnabled(false);
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

   if(m_selectModelComponentConnections.length())
   {
      actionDeleteConnection->setEnabled(true);
   }
   else
   {
      actionDeleteConnection->setEnabled(false);
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
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      double top = std::numeric_limits<double>::max();

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];

         if (model->pos().y() < top)
         {
            top = model->pos().y();
         }
      }

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         model->setY(top);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onAlignBottom()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      double bottom = std::numeric_limits<double>::min();

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];

         if (model->sceneBoundingRect().bottom() > bottom)
         {
            bottom = model->sceneBoundingRect().bottom();
         }
      }

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         setBottom(m_selectedModelComponents[i], bottom);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onAlignLeft()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      double left = std::numeric_limits<double>::max();

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];

         if (model->pos().x() < left)
         {
            left = model->pos().x();
         }
      }

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         model->setX(left);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onAlignRight()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      double right = std::numeric_limits<double>::min();

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];

         if (model->sceneBoundingRect().right() > right)
         {
            right = model->sceneBoundingRect().right();
         }
      }

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         setRight(m_selectedModelComponents[i], right);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onAlignVerticalCenters()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      double centerY = std::numeric_limits<double>::max();

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];

         if (model->sceneBoundingRect().center().y() < centerY)
         {
            centerY = model->sceneBoundingRect().center().y();
         }
      }

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         setVerticalCenter(m_selectedModelComponents[i], centerY);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onAlignHorizontalCenters()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      double centerX = std::numeric_limits<double>::max();

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];

         if (model->sceneBoundingRect().center().x() < centerX)
         {
            centerX = model->sceneBoundingRect().center().x();
         }
      }

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         setHorizontalCenter(m_selectedModelComponents[i], centerX);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onDistributeTopEdges()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      qSort(m_selectedModelComponents.begin(), m_selectedModelComponents.end(), &HydroCoupleComposer::compareTopEdges);

      double bottom = m_selectedModelComponents[0]->pos().y();
      double top = m_selectedModelComponents[m_selectedModelComponents.count() - 1]->pos().y();

      double dx = (top - bottom) / (m_selectedModelComponents.count() - 1.0);

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         model->setY(bottom + i*dx);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onDistributeBottomEdges()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      qSort(m_selectedModelComponents.begin(), m_selectedModelComponents.end(), &HydroCoupleComposer::compareBottomEdges);

      double bottom = m_selectedModelComponents[0]->sceneBoundingRect().bottom();
      double top = m_selectedModelComponents[m_selectedModelComponents.count() - 1]->sceneBoundingRect().bottom();

      double dx = (top - bottom) / (m_selectedModelComponents.count() - 1.0);

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         setBottom(model, bottom + i*dx);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onDistributeLeftEdges()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      qSort(m_selectedModelComponents.begin(), m_selectedModelComponents.end(), &HydroCoupleComposer::compareLeftEdges);

      double left = m_selectedModelComponents[0]->pos().x();
      double right = m_selectedModelComponents[m_selectedModelComponents.count() - 1]->pos().x();

      double dx = (right - left) / (m_selectedModelComponents.count() - 1.0);

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         model->setX(left + i*dx);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onDistributeRightEdges()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      qSort(m_selectedModelComponents.begin(), m_selectedModelComponents.end(), &HydroCoupleComposer::compareRightEdges);

      double left = m_selectedModelComponents[0]->sceneBoundingRect().right();
      double right = m_selectedModelComponents[m_selectedModelComponents.count() - 1]->sceneBoundingRect().right();

      double dx = (right - left) / (m_selectedModelComponents.count() - 1.0);

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         setRight(model, left + i*dx);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onDistributeVerticalCenters()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      qSort(m_selectedModelComponents.begin(), m_selectedModelComponents.end(), &HydroCoupleComposer::compareVerticalCenters);

      double bottom = m_selectedModelComponents[0]->sceneBoundingRect().center().y();
      double top = m_selectedModelComponents[m_selectedModelComponents.count() - 1]->sceneBoundingRect().center().y();

      double dx = (top - bottom) / (m_selectedModelComponents.count() - 1.0);

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         setVerticalCenter(model, bottom + i*dx);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onDistributeHorizontalCenters()
{
   if (m_selectedModelComponents.count() > 1)
   {
      graphicsViewHydroCoupleComposer->blockSignals(true);

      qSort(m_selectedModelComponents.begin(), m_selectedModelComponents.end(), &HydroCoupleComposer::compareHorizontalCenters);

      double left = m_selectedModelComponents[0]->sceneBoundingRect().center().x();
      double right = m_selectedModelComponents[m_selectedModelComponents.count() - 1]->sceneBoundingRect().center().x();

      double dx = (right - left) / (m_selectedModelComponents.count() - 1.0);

      for (int i = 0; i < m_selectedModelComponents.count(); i++)
      {
         GModelComponent* model = m_selectedModelComponents[i];
         setHorizontalCenter(model, left + i*dx);
      }

      graphicsViewHydroCoupleComposer->blockSignals(false);
   }
}

void HydroCoupleComposer::onTreeViewModelComponentInfoContextMenuRequested(const QPoint& pos)
{
   QMenu contextMenu;
   contextMenu.addActions(m_treeviewComponentInfoContextMenuActions);
   contextMenu.exec(treeViewModelComponentInfos->viewport()->mapToGlobal(pos));
}

void HydroCoupleComposer::onGraphicsViewHydroCoupleComposerContextMenuRequested(const QPoint& pos)
{

}
