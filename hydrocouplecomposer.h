#ifndef HYDROCOUPLECOMPOSER_H
#define HYDROCOUPLECOMPOSER_H

#include <QtWidgets/QMainWindow>
#include "ui_hydrocouplecomposer.h"
#include "componentmanager.h"
#include "hydrocoupleproject.h"
#include "qpropertymodel.h"
#include "qvariantholderhelper.h"

class GModelComponent;



class HydroCoupleComposer : public QMainWindow, public Ui::HydroCoupleComposerClass
{
	Q_OBJECT;
	Q_PROPERTY(ComponentManager* Componentmanager READ componentManager);
	Q_PROPERTY(HydroCoupleProject* Project READ project);

public:

	HydroCoupleComposer(QWidget *parent = 0);

	~HydroCoupleComposer();

	//! ComponentManager manages all component types
	/*!
	*/
	ComponentManager* componentManager() const;

	//! HydroCoupleProject project for this session
	/*!
	*/
	HydroCoupleProject* project() const;

public slots:
	//!Set process progress 
	void onSetProgress(bool visible, const QString& message = QString(), int progress = 0);

	//!Post Output message
	void onPostMessage(const QString& message);

protected:
	//!dispose resources and save on close
	void closeEvent(QCloseEvent * event) override;

	//!Implement Drag Drop
	void dragMoveEvent(QDragMoveEvent * event) override;

	void dragEnterEvent(QDragEnterEvent * event) override;

	void dropEvent(QDropEvent * event) override;

	//!Implement create connection and delete connection
	void mousePressEvent(QMouseEvent * event) override;

	//!Implement delete and others
	void keyPressEvent(QKeyEvent * event)  override;

private slots:
	//!Slot connected to open recent file action
	void onOpenRecentFile();

	//!Update recentFiles QActions
	void onUpdateRecentFiles();

	//!Clear recenFiles list
	void onClearRecentFiles();

	//!Clears GUI and other  application settings
	void onClearSettings();

	void onModelComponentInfoLoaded(const HydroCouple::IModelComponentInfo* modelComponentInfo);

	void onModelComponentInfoClicked(const QModelIndex& index);

	void onModelComponentInfoDoubleClicked(const QModelIndex& index);

	void onModelComponentInfoDropped(const QPointF& scenePos, const QString& id);

	void onModelComponentAdded(GModelComponent* modelComponent);

	void onModelComponentBeingDeleted(GModelComponent* modelComponent);

	void onModelComponentConnectionDoubleClicked(GModelComponent* modelComponent);

	void onSetCurrentTool(bool toggled);

	void onZoomExtent();

	//!Align top
	void onAlignTop();

	//!Align bottom
	void onAlignBottom();

	//!Aign left
	void onAlignLeft();

	//!Align left
	void onAlignRight();

	//!Align vertical centers
	void onAlignVerticalCenters();

	//!Align horizontal centers
	void onAlignHorizontalCenters();

	//!Distribute top edges
	void onDistributeTopEdges();

	//!Distribute bottom edges
	void onDistributeBottomEdges();

	//!Distribute left edges
	void onDistributeLeftEdges();

	//!Distribute right edges
	void onDistributeRightEdges();

	//!Distribute vertical centers
	void onDistributeVerticalCenters();

	//!Distribute vertical centers
	void onDistributeHorizontalCenters();

	void onTreeViewModelComponentInfoContextMenuRequested(const QPoint& pos);

	void onGraphicsViewHydroCoupleComposerContextMenuRequested(const QPoint& pos);

	void onSelectionChanged();

private:
	
	void openFile(const QFileInfo& file);

	void readSettings();

	void writeSettings();

	void initializeGUIComponents();

	void setupComponentInfoTreeView();

	void setupPropertyGrid();

	void setupActions();

	void setupSignalSlotConnections();

	void setupContextMenus();

	QStandardItem* findCategoryForModelComponentInfo(const QString& category, QStandardItem* parent, bool recursive = false);

	//!Set right
	static void setRight(GModelComponent * const component, double right);

	//!Set vertical center
	static void setHorizontalCenter(GModelComponent * const component, double hcenter);

	//!Set bottom
	static void setBottom(GModelComponent * const component, double bottom);

	//!Set vertical center
	static void setVerticalCenter(GModelComponent * const component, double vcenter);

	//!compare left edge
	static bool compareLeftEdges(GModelComponent*   a, GModelComponent *  b);

	//!compare horizontal center
	static bool compareHorizontalCenters(GModelComponent*   a, GModelComponent *  b);

	//!compare right edges
	static bool compareRightEdges(GModelComponent*   a, GModelComponent *  b);

	//!compare top edges
	static bool compareTopEdges(GModelComponent*   a, GModelComponent *  b);

	//!compare verticel center
	static bool compareVerticalCenters(GModelComponent*   a, GModelComponent *  b);

	//!compare right edges
	static bool compareBottomEdges(GModelComponent*   a, GModelComponent *  b);

signals:
	void currentToolChanged(int currentTool);

private:
	ComponentManager* m_componentManager;
	HydroCoupleProject* m_project;
	QProgressBar* m_progressBar;
	QPropertyModel* m_propertyModel;
	QStandardItemModel* m_componentTreeViewModel;
	static QIcon s_categoryIcon;
	QStringList m_recentFiles;
	QSettings m_settings;
	QString m_lastOpenedFilePath;
	QAction* m_recentFilesActions[10];
	QMenu* m_recentFilesMenu;
	QAction* m_clearRecentFilesAction;
	static const QString sc_modelComponentInfoHtml;
	GraphicsView::Tool m_currentTool;
	QList<GModelComponent*> m_selectedModelComponents;
	int m_currentModelComponentzValue;
	int m_currentModelComponentConnectionzValue;
	QVariantHolderHelper* m_qVariantPropertyHolder;
};



#endif // HYDROCOUPLECOMPOSER_H
