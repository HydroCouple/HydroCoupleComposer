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

      //! ComponentManager manages all component types
      /*!
        */
      ComponentManager* componentManager() const;

      //! HydroCoupleProject project for this session
      /*!
        */
      HydroCoupleProject* project() const;

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

   private:
      /*!
       * \brief openFile
       * \param file
       */
      void openFile(const QFileInfo& file);

      void readSettings();

      void writeSettings();

      void initializeGUIComponents();

      void initializeSimulationStatusTreeView();

      void initializeComponentInfoTreeView();

      void initializePropertyGrid();

      void initializeActions();

      void initializeSignalSlotConnections();

      void initializeProjectSignalSlotConnections();

      void initializeContextMenus();

      void createConnection(GModelComponent* producer , GModelComponent* consumer);

      QStandardItem* findStandardItem(const QString& displayName, QStandardItem* parent,
                                      QVariant::Type userType = QVariant::Bool, Qt::ItemDataRole role = Qt::DisplayRole, bool recursive = false);

      //!Set right
      static void setRight(GModelComponent *component, double right);

      //!Set vertical center
      static void setHorizontalCenter(GModelComponent *component, double hcenter);

      //!Set bottom
      static void setBottom(GModelComponent *component, double bottom);

      //!Set vertical center
      static void setVerticalCenter(GModelComponent *component, double vcenter);

      //!compare left edge
      static bool compareLeftEdges(GModelComponent *a, GModelComponent *b);

      //!compare horizontal center
      static bool compareHorizontalCenters(GModelComponent *a, GModelComponent *b);

      //!compare right edges
      static bool compareRightEdges(GModelComponent *a, GModelComponent *b);

      //!compare top edges
      static bool compareTopEdges(GModelComponent *a, GModelComponent *b);

      //!compare verticel center
      static bool compareVerticalCenters(GModelComponent *a, GModelComponent *b);

      //!compare right edges
      static bool compareBottomEdges(GModelComponent *a, GModelComponent *b);

   public slots:
      //!Set process progress
      void onSetProgress(bool visible, const QString& message = QString(), int progress = 0 , int min = 0 , int max = 100);

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

      /*!
       * \brief onPrint
       */
      void onPrint();

      /*!
       * \brief onOpenRecentFile Slot connected to open recent file action
       */
      void onOpenRecentFile();

      //!Update recentFiles QActions
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
       * \brief onSetAsTrigger
       */
      void onSetAsTrigger();

      /*!
       * \brief onDeleteSelectedComponents
       */
      void onDeleteSelectedComponents();

      /*!
       * \brief onDeleteSelectedConnections
       */
      void onDeleteSelectedConnections();

      void onValidateModelComponentLibrary();

      void onBrowseToComponentLibraryPath();

      void onLoadComponentLibrary();

      void onUnloadComponentLibrary();

      /*!
         * \brief onModelComponentInfoLoaded
         * \param modelComponentInfo
         */
      void onModelComponentInfoLoaded(const HydroCouple::IModelComponentInfo* modelComponentInfo);

      void onModelComponentInfoUnloaded(const QString& id);

      void onModelComponentInfoClicked(const QModelIndex& index);

      void onModelComponentInfoDoubleClicked(const QModelIndex& index);

      void onModelComponentInfoDropped(const QPointF& scenePos, const QString& id);

      void onModelComponentInfoPropertyChanged(const QString& propertyName, const QVariant& value);

      void onModelComponentAdded(GModelComponent* modelComponent);

      void onModelComponentDeleting(GModelComponent* modelComponent);

      void onModelComponentConnectionAdded(GModelComponentConnection* modelComponentConnection);

      void onModelComponentConnectionDeleting(GModelComponentConnection* modelComponentConnection);

      void onModelComponentConnectionDoubleClicked(QGraphicsSceneMouseEvent * event);

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
      QList<GModelComponentConnection*> m_selectModelComponentConnections;
      int m_currentModelComponentzValue;
      int m_currentModelComponentConnectionzValue;
      QVariantHolderHelper* m_qVariantPropertyHolder;
      QString m_lastPath;
      GModelComponent *m_connProd , *m_connCons;
      bool m_createConnection;
      QList<QAction*> m_treeviewComponentInfoContextMenuActions;
      QList<QAction*> m_graphicsViewContextMenuActions;
      ModelStatusItemModel* m_modelStatusItemModel;
};

//Encapsulate options in one class and expose;

#endif // HYDROCOUPLECOMPOSER_H
