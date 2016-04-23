#ifndef HYDROCOUPLECOMPOSER_H
#define HYDROCOUPLECOMPOSER_H

#include <QtWidgets/QMainWindow>
#include "ui_hydrocouplecomposer.h"
#include "componentmanager.h"
#include "hydrocoupleproject.h"
#include "qpropertymodel.h"
#include "qvariantholderhelper.h"
#include <QStandardItem>
#include <QProgressBar>
#include <QSettings>
#include "modelstatusitemmodel.h"
#include "argumentdialog.h"


#ifdef _WIN32

#include "cgraph.h"
#include "gvc.h"

#else

#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>

#endif



class GModelComponent;

class HydroCoupleComposer : public QMainWindow, public Ui::HydroCoupleComposerClass
{
      Q_OBJECT
      Q_PROPERTY(ComponentManager* Componentmanager READ componentManager)
      Q_PROPERTY(HydroCoupleProject* Project READ project)

   public:

      /*!
       * \brief HydroCoupleComposer
       * \param parent
       */
      HydroCoupleComposer(QWidget *parent = 0);

      virtual ~HydroCoupleComposer();

      /*!
       * \brief ComponentManager manages all component types.
       */
      ComponentManager* componentManager() const;

      /*!
       * \brief HydroCoupleProject project for this session
       */
      HydroCoupleProject* project() const;

   protected:

      /*!
       * \brief dispose resources and save on close.
       */
      void closeEvent(QCloseEvent * event) override;

      /*!
       * \brief Implement Drag Drop.
       */
      void dragMoveEvent(QDragMoveEvent * event) override;

      void dragEnterEvent(QDragEnterEvent * event) override;

      void dropEvent(QDropEvent * event) override;

      /*!
       * \brief Implement create connection and delete connection.
       */
      void mousePressEvent(QMouseEvent * event) override;

      /*!
       * \brief Implement delete and others.
       */
      void keyPressEvent(QKeyEvent * event)  override;

   private:

      /*!
       * \brief openFile
       * \param file
       */
      void openFile(const QFileInfo& file);

      /*!
       * \brief readSettings
       */
      void readSettings();

      /*!
       * \brief writeSettings
       */
      void writeSettings();

      /*!
       * \brief initializeGUIComponents
       */
      void initializeGUIComponents();

      /*!
       * \brief initializeSimulationStatusTreeView
       */
      void initializeSimulationStatusTreeView();

      /*!
       * \brief initializeComponentInfoTreeView
       */
      void initializeComponentInfoTreeView();

      void initializePropertyGrid();

      void initializeActions();

      void initializeSignalSlotConnections();

      void initializeProjectSignalSlotConnections();

      void initializeContextMenus();

      void createConnection(GExchangeItem* producer , GExchangeItem* consumer);

      QStandardItem* findStandardItem(const QString& displayName, QStandardItem* parent,
                                      QVariant::Type userType = QVariant::Bool, Qt::ItemDataRole role = Qt::DisplayRole, bool recursive = false);


      void addRemoveNodeToGraphicsView(GNode* node, bool add = true);

      void layoutNode(Agraph_t* graph, QHash<GNode*,QString> & identifiers, GNode* node, int& currentIndex, bool addToGraph = true);

      void layoutEdges(Agraph_t* graph, const QHash<GNode*,QString> & identifiers, GNode* node);

      void stringToCharP(const QString& text, char * & output);

      /*!
       * \brief setRight
       * \param component
       * \param right
       */
      static void setRight(QGraphicsItem *graphicsItem, double right);

      //!Set vertical center
      static void setHorizontalCenter(QGraphicsItem *graphicsItem, double hcenter);

      //!Set bottom
      static void setBottom(QGraphicsItem *graphicsItem, double bottom);

      //!Set vertical center
      static void setVerticalCenter(QGraphicsItem *graphicsItem, double vcenter);

      //!compare left edge
      static bool compareLeftEdges(QGraphicsItem *a, QGraphicsItem *b);

      //!compare horizontal center
      static bool compareHorizontalCenters(QGraphicsItem *a, QGraphicsItem *b);

      //!compare right edges
      static bool compareRightEdges(QGraphicsItem *a, QGraphicsItem *b);

      //!compare top edges
      static bool compareTopEdges(QGraphicsItem *a, QGraphicsItem *b);

      //!compare verticel center
      static bool compareVerticalCenters(QGraphicsItem *a, QGraphicsItem *b);

      //!compare right edges
      static bool compareBottomEdges(QGraphicsItem *a, QGraphicsItem *b);

   public slots:
      void onSetProgress(bool visible, int progress = 0 , int min = 0 , int max = 100);

      //!Post Output message
      void onPostMessage(const QString& message);

   private slots:

      /*!
       * \brief onNewProject
       */
      void onNewProject();

      /*!
       * \brief onOpenFiles
       */
      void onOpenFiles();

      /*!
       * \brief onSave
       */
      void onSave();

      /*!
       * \brief onSaveAs
       */
      void onSaveAs();


      void onExport();

      /*!
       * \brief onPrint
       */
      void onPrint();

      /*!
       * \brief onOpenRecentFile Slot connected to open recent file action
       */
      void onOpenRecentFile();

      /*!
       * \brief onAddComponentLibraryDirectory
       */
      void onAddComponentLibraryDirectory();

      /*!
       * \brief onUpdateRecentFiles
       */
      void onUpdateRecentFiles();

      /*!
       * \brief Clear recenFiles list
       */
      void onClearRecentFiles();

      /*!
       * \brief Clears GUI and other  application settings
       */
      void onClearSettings();

      /*!
       * \brief onEditSelectedItem
       */
      void onEditSelectedItem();

      /*!
       * \brief onAddModelComponent
       */
      void onAddModelComponent();

      /*!
       * \brief onCreateConnection
       */
      void onCreateConnection(bool create);

      /*!
       * \brief onLayoutComponents
       */
      void onLayoutComponents();

      /*!
       * \brief onCloneModelComponents
       */
      void onCloneModelComponents();

      /*!
       * \brief onSetAsTrigger
       */
      void onSetAsTrigger();

      void onInitializeComponent();

      void onValidateComponent();

      /*!
       * \brief onDeleteSelectedComponents
       */
      void onDeleteSelectedComponents();

      /*!
       * \brief onDeleteSelectedConnections
       */
      void onDeleteSelectedConnections();

      /*!
       * \brief onValidateModelComponentLibrary
       */
      void onValidateModelComponentLibrary();

      /*!
       * \brief onBrowseToComponentLibraryPath
       */
      void onBrowseToComponentLibraryPath();

      /*!
       * \brief onLoadComponentLibrary
       */
      void onLoadComponentLibrary();

      /*!
       * \brief onUnloadComponentLibrary
       */
      void onUnloadComponentLibrary();

      /*!
      * \brief onModelComponentInfoLoaded
      * \param modelComponentInfo
      */
      void onModelComponentInfoLoaded(const HydroCouple::IModelComponentInfo* modelComponentInfo);

      void onModelComponentInfoUnloaded(const QString& id);

      void onAdaptedOutputFactoryComponentInfoLoaded(const HydroCouple::IAdaptedOutputFactoryComponentInfo* adaptedOutputFactoryComponentInfo);

      void onAdaptedOutputFactoryComponentInfoUnloaded(const QString& id);

      void onComponentInfoClicked(const QModelIndex& index);

      void onComponentInfoDoubleClicked(const QModelIndex& index);

      void onModelComponentStatusItemClicked(const QModelIndex& index);

      void onModelComponentStatusItemDoubleClicked(const QModelIndex& index);

      void onModelComponentInfoDropped(const QPointF& scenePos, const QString& id);

      void onComponentInfoPropertyChanged(const QString& propertyName, const QVariant& value);

      void onModelComponentAdded(GModelComponent* modelComponent);

      void onModelComponentDeleting(GModelComponent* modelComponent);

      void onModelComponentDoubleClicked(GModelComponent* modelComponent);

      void onProjectHasChanges(bool hasChanges);

      /*!
       * \brief onSetCurrentTool
       * \param toggled
       */
      void onSetCurrentTool(bool toggled);

      /*!
       * \brief onZoomExtent
       */
      void onZoomExtent();

      /*!
       * \brief onSelectionChanged
       */
      void onSelectionChanged();

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

      void onAbout();
      
      void onPreferences();

   signals:

      void currentToolChanged(int currentTool);

   private:
      ComponentManager* m_componentManager;
      HydroCoupleProject* m_project;
      QPropertyModel* m_propertyModel;
      QStandardItemModel* m_componentTreeViewModel;
      QProgressBar* m_progressBar;
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
      QList<GExchangeItem*> m_selectedExchangeItems;
      QList<GAdaptedOutput*> m_selectedAdaptedOutputs;
      QList<GNode*> m_selectedNodes;
      QList<GConnection*> m_selectedConnections;
      QVariantHolderHelper* m_qVariantPropertyHolder;
      QString m_lastPath;
      GExchangeItem *m_connProd , *m_connCons;
      bool m_createConnection;
      QList<QAction*> m_treeviewComponentInfoContextMenuActions;
      QList<QAction*> m_graphicsViewContextMenuActions;
      ModelStatusItemModel* m_modelStatusItemModel;
      ArgumentDialog* m_argumentDialog;
      QStandardItem *m_modelComponentInfoStandardItem, *m_adaptedOutputComponentInfoStandardItem;
};

#endif // HYDROCOUPLECOMPOSER_H
