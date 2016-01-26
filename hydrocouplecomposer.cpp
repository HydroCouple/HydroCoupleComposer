#include "stdafx.h"
#include "hydrocouplecomposer.h"
#include "qpropertyitemdelegate.h"
#include "gmodelcomponent.h"

using namespace HydroCouple;



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
	: QMainWindow(parent), m_currentModelComponentzValue(0), m_currentModelComponentConnectionzValue(-1)
{
	setupUi(this);
	setAcceptDrops(true);
	initializeGUIComponents();

	m_componentManager = new ComponentManager(this);
	m_project = new HydroCoupleProject(QFileInfo("Untitled Project.xml"), this);
	m_componentTreeViewModel = new QStandardItemModel(this);
	m_propertyModel = new QPropertyModel(this);
	//m_qVariantPropertyHolder = new QVariantHolderHelper(QVariant(), this);

	setupComponentInfoTreeView();
	setupPropertyGrid();
	setupActions();
	setupContextMenus();
	setupSignalSlotConnections();
	readSettings();
}

HydroCoupleComposer::~HydroCoupleComposer()
{
	QList<GModelComponent*> components = m_project->modelComponents();

	for (QList<GModelComponent*>::iterator it = components.begin();
		it != components.end(); it++)
	{
		graphicsViewHydroCoupleComposer->scene()->removeItem(*it);
	}
}

ComponentManager* HydroCoupleComposer::componentManager() const
{
	return m_componentManager;
}

HydroCoupleProject* HydroCoupleComposer::project() const
{
	return m_project;
}

void HydroCoupleComposer::onSetProgress(bool visible, const QString& message , int progress)
{
	m_progressBar->setVisible(visible);

	if (visible)
	{
		statusBarMain->showMessage(message);
		m_progressBar->setValue(progress);
	}
}

void HydroCoupleComposer::onPostMessage(const QString& message)
{
         this->statusBarMain->showMessage(QDateTime::currentDateTime().toString() + " | " + message);
}

void HydroCoupleComposer::closeEvent(QCloseEvent* event)
{
	writeSettings();
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
				QString text = tr("&%1 %2").arg(i + 1).arg(file.fileName());
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

}

void HydroCoupleComposer::onClearSettings()
{

}

void HydroCoupleComposer::onModelComponentInfoLoaded(const IModelComponentInfo* modelComponentInfo)
{

	QStringList categories = modelComponentInfo->category().split('\\', QString::SplitBehavior::SkipEmptyParts);

	QStandardItem* parent = m_componentTreeViewModel->invisibleRootItem();

	while (categories.count() > 0)
	{
		QString category = categories[0];

		QStandardItem* childCategoryItem = findCategoryForModelComponentInfo(category, parent);

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
}

void HydroCoupleComposer::onModelComponentInfoClicked(const QModelIndex& index)
{
	labelComponentInfoDescription->setText(index.data(Qt::ToolTipRole).toString());
	QVariant value = index.data(Qt::UserRole);

	IModelComponentInfo* foundModelComponentInfo = nullptr;

	if ((value.type() != QVariant::Bool)
		&& (foundModelComponentInfo = m_componentManager->findModelComponentInfoById(value.toString())) != nullptr)
	{
		QVariant variant = QVariant::fromValue(dynamic_cast<QObject*>(foundModelComponentInfo));
		m_propertyModel->setData(variant);
		treeViewProperties->expandToDepth(0);
		treeViewProperties->resizeColumnToContents(0);
		treeViewProperties->resizeColumnToContents(1);
	}
	else
	{
		m_propertyModel->setData(QVariant());
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

void HydroCoupleComposer::onModelComponentAdded(GModelComponent* modelComponent)
{
	graphicsViewHydroCoupleComposer->scene()->addItem(modelComponent);
	modelComponent->setZValue(m_currentModelComponentzValue);
	m_currentModelComponentzValue++;
}

void HydroCoupleComposer::onModelComponentBeingDeleted(GModelComponent* modelComponent)
{
	graphicsViewHydroCoupleComposer->scene()->removeItem(modelComponent);
}

void HydroCoupleComposer::onModelComponentConnectionDoubleClicked(GModelComponent* modelComponent)
{

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
	
}

void HydroCoupleComposer::onGraphicsViewHydroCoupleComposerContextMenuRequested(const QPoint& pos)
{

}

void HydroCoupleComposer::onSelectionChanged()
{
	QList<QGraphicsItem*> items = graphicsViewHydroCoupleComposer->scene()->selectedItems();

	if (items.count() == 1)
	{
		QGraphicsObject* gobject = (QGraphicsObject*)items[0];

		if (gobject)
		{
			m_propertyModel->setData(QVariant::fromValue(gobject));
		}

		treeViewProperties->expandToDepth(0);
		treeViewProperties->resizeColumnToContents(0);
		treeViewProperties->resizeColumnToContents(1);
	}
	else
	{
		m_propertyModel->setData(QVariant());
	}

}

void HydroCoupleComposer::openFile(const QFileInfo& file)
{
	onPostMessage("Opening file '/" + file.absolutePath() + "'/");

	QString extension = file.completeSuffix();
	m_lastOpenedFilePath = file.absolutePath();

	for (int i = 0; i < m_recentFiles.count(); i++)
	{
		if (!m_recentFiles[i].compare(file.absoluteFilePath(), Qt::CaseInsensitive))
		{
			m_recentFiles.removeAt(i);
		}
	}

	m_recentFiles.prepend(file.absoluteFilePath());

	while (m_recentFiles.size() > 10)
	{
		m_recentFiles.removeLast();
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
	treeViewProperties->setEditTriggers(QAbstractItemView::AllEditTriggers);
	treeViewProperties->setItemDelegate(modelDelegate);
	treeViewProperties->setAlternatingRowColors(true);
	treeViewProperties->expandToDepth(1);
	treeViewProperties->resizeColumnToContents(0);
	treeViewProperties->resizeColumnToContents(1);
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

	//HydroCoupleProject
	connect(m_project, SIGNAL(componentAdded(GModelComponent* )), this, SLOT(onModelComponentAdded(GModelComponent* )));
	connect(m_project, SIGNAL(componentBeingDeleted(GModelComponent* )), this, SLOT(onModelComponentBeingDeleted(GModelComponent* )));

	//connect(m_project, SIGNAL(connectionAdded(GComponentConnection* const)), this, SLOT(onConnectionAdded(GComponentConnection* const)));
	//connect(m_project, SIGNAL(connectionRemoved(GComponentConnection* const)), this, SLOT(onConnectionRemoved(GComponentConnection* const)));


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

	connect(actionDefaultSelect, SIGNAL(toggled(bool)), actionCreateConnection, SLOT(setEnabled(bool)));
	connect(actionCreateConnection, SIGNAL(toggled(bool)), this, SLOT(onCreateConnection(bool)));
	connect(actionSetTrigger, SIGNAL(triggered()), this, SLOT(onSetAsTrigger()));
	connect(actionCloneModel, SIGNAL(triggered()), this, SLOT(onCloneModel()));
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
	connect(actionLayoutComponents, SIGNAL(triggered()), this, SLOT(onLayoutComponents()));

	//graphicsView
	connect(graphicsViewHydroCoupleComposer, SIGNAL(statusChanged(const QString&)), this->statusBarMain, SLOT(showMessage(const QString&)));
	connect(graphicsViewHydroCoupleComposer, SIGNAL(modelComponentInfoDropped(const QPointF&, const QString&)), this, SLOT(onModelComponentInfoDropped(const QPointF& , const QString& )));

	//graphicsScene
	connect(graphicsViewHydroCoupleComposer->scene(), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));

}

void HydroCoupleComposer::setupContextMenus()
{

}

QStandardItem* HydroCoupleComposer::findCategoryForModelComponentInfo(const QString& category, QStandardItem* parent, bool recursive)
{
	if (category != "")
	{
		for (int i = 0; i < parent->rowCount(); i++)
		{
			QStandardItem* child = parent->child(i);

			QString cname = child->data(Qt::DisplayRole).toString();
			QVariant userRole = child->data(Qt::UserRole);
			if (userRole.type() == QVariant::Type::Bool && !cname.compare(category, Qt::CaseInsensitive))
			{
				return child;
			}
			else if (recursive)
			{
				QStandardItem* c;

				if (child->hasChildren())
				{
					c = findCategoryForModelComponentInfo(category, child);

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