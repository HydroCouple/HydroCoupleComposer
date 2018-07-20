/*!
 * \file   hydrocouple.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
 * This header file contains the core interface definitions for the
 * HydroCouple component-based modeling definitions.
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

#ifndef HYDROCOUPLECOMPOSER_H
#define HYDROCOUPLECOMPOSER_H

#include <QtWidgets>
#include <QtWidgets/QMainWindow>
#include "ui_hydrocouplecomposer.h"
#include "hydrocoupleproject.h"
#include "qpropertymodel.h"
#include "qvariantholderhelper.h"
#include <QStandardItem>
#include <QProgressBar>
#include <QSettings>
#include "modelstatusitemmodel.h"
#include "argumentdialog.h"

#ifdef GRAPHVIZ_LIBRARY
  #ifdef _WIN32
    #include "cgraph.h"
    #include "gvc.h"
  #else
    #include <graphviz/cgraph.h>
    #include <graphviz/gvc.h>
  #endif
#endif



class GModelComponent;
class SimulationManager;
class PreferencesDialog;

class HydroCoupleComposer : public QMainWindow, public Ui::HydroCoupleComposerClass
{
      Q_OBJECT
      Q_PROPERTY(HydroCoupleProject* Project READ project)

   public:

      /*!
       * \brief HydroCoupleComposer
       * \param parent
       */
      HydroCoupleComposer(QWidget *parent = 0);

      virtual ~HydroCoupleComposer();

      /*!
       * \brief HydroCoupleProject project for this session
       */
      HydroCoupleProject* project() const;

      /*!
       * \brief setProject
       * \param project
       */
      void setProject(HydroCoupleProject* project);

      /*!
       * \brief openFile
       * \param file
       */
      bool openFile(const QFileInfo& file);

   protected:

      /*!
       * \brief dispose resources and save on close.
       */
      void closeEvent(QCloseEvent * event) override;

      /*!
       * \brief Implement Drag Drop.
       */
      void dragMoveEvent(QDragMoveEvent * event) override;

      /*!
       * \brief dragEnterEvent
       * \param event
       */
      void dragEnterEvent(QDragEnterEvent * event) override;

      /*!
       * \brief dropEvent
       * \param event
       */
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
       * \brief initializeComponentInfoTreeView
       */
      void initializeModelComponentInfoTreeView();

      void initializeAdaptedOutputFactoryComponentInfoTreeView();

      void initializeWorkflowComponentInfoTreeView();

      /*!
       * \brief initializeSimulationStatusTreeView
       */
      void initializeSimulationStatusTreeView();

      /*!
       * \brief initializePropertyGrid
       */
      void initializePropertyGrid();

      /*!
       * \brief initializeActions
       */
      void initializeActions();

      /*!
       * \brief initializeSignalSlotConnections
       */
      void initializeSignalSlotConnections();

      /*!
       * \brief initializeProjectSignalSlotConnections
       */
      void initializeProjectSignalSlotConnections();

      /*!
       * \brief initializeContextMenus
       */
      void initializeContextMenus();

      void resetAdaptedOutputFactoryModel(QStandardItem *standardItem);

      /*!
       * \brief createConnection
       * \param producer
       * \param consumer
       */
      void createConnection(GExchangeItem* producer , GExchangeItem* consumer);

      /*!
       * \brief findStandardItem
       * \param id
       * \param parent
       * \param userType
       * \param role
       * \param recursive
       * \return
       */
      QStandardItem* findStandardItem(const QString& id, QStandardItem* parent, const QString & key = "ModelComponentInfo", bool recursive = false);

      QStandardItem* findStandardItem(const QList<QString> &identifiers, QStandardItem* parent, const QList<QString> &keys, bool recursive = false);

      /*!
       * \brief addRemoveNodeToGraphicsView
       * \param node
       * \param add
       */
      void addRemoveNodeToGraphicsView(GNode* node, bool add = true);

#ifdef GRAPHVIZ_LIBRARY
      /*!
       * \brief layoutNode
       * \param graph
       * \param identifiers
       * \param node
       * \param currentIndex
       * \param addToGraph
       */
      void layoutNode(Agraph_t* graph, QHash<GNode*,QString> & identifiers, GNode* node, int& currentIndex, bool addToGraph = true);

      /*!
       * \brief layoutEdges
       * \param graph
       * \param identifiers
       * \param node
       */
      void layoutEdges(Agraph_t* graph, const QHash<GNode*,QString> & identifiers, GNode* node);
#endif
      /*!
       * \brief stringToCharP
       * \param text
       * \param output
       */
      void stringToCharP(const QString& text, char * & output);

      /*!
       * \brief setRight
       * \param component
       * \param right
       */
      static void setRight(QGraphicsItem *graphicsItem, double right);

      /*!
       * \brief setHorizontalCenter
       * \param graphicsItem
       * \param hcenter
       */
      static void setHorizontalCenter(QGraphicsItem *graphicsItem, double hcenter);

      /*!
       * \brief setBottom
       * \param graphicsItem
       * \param bottom
       */
      static void setBottom(QGraphicsItem *graphicsItem, double bottom);

      /*!
       * \brief setVerticalCenter
       * \param graphicsItem
       * \param vcenter
       */
      static void setVerticalCenter(QGraphicsItem *graphicsItem, double vcenter);

      /*!
       * \brief compareLeftEdges
       * \param a
       * \param b
       * \return
       */
      static bool compareLeftEdges(QGraphicsItem *a, QGraphicsItem *b);

      /*!
       * \brief compareHorizontalCenters
       * \param a
       * \param b
       * \return
       */
      static bool compareHorizontalCenters(QGraphicsItem *a, QGraphicsItem *b);

      /*!
       * \brief compareRightEdges
       * \param a
       * \param b
       * \return
       */
      static bool compareRightEdges(QGraphicsItem *a, QGraphicsItem *b);

      /*!
       * \brief compareTopEdges
       * \param a
       * \param b
       * \return
       */
      static bool compareTopEdges(QGraphicsItem *a, QGraphicsItem *b);

      /*!
       * \brief compareVerticalCenters
       * \param a
       * \param b
       * \return
       */
      static bool compareVerticalCenters(QGraphicsItem *a, QGraphicsItem *b);

      /*!
       * \brief compareBottomEdges
       * \param a
       * \param b
       * \return
       */
      static bool compareBottomEdges(QGraphicsItem *a, QGraphicsItem *b);

   public slots:

      /*!
       * \brief onSetProgress
       * \param visible
       * \param progress
       * \param min
       * \param max
       */
      void onSetProgress(bool visible, int progress = 0 , int min = 0 , int max = 100);

      /*!
       * \brief onPostMessage
       * \param message
       */
      void onPostMessage(const QString& message);

      /*!
       * \brief onPostToStatusBar
       * \param message
       */
      void onPostToStatusBar(const QString &message);

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
       * \brief onExport
       */
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
       * \brief onRunSimulation
       */
      void onRunSimulation();

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

      /*!
       * \brief onInitializeComponent
       */
      void onInitializeComponent();

      /*!
       * \brief onValidateComponent
       */
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
       * \brief onValidateComponentLibrary
       */
      void onValidateComponentLibrary();

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
      void onModelComponentInfoLoaded(const QString &modelComponentInfoId);

      void onModelComponentInfoUnloaded(const QString &modelComponentInfoId);

      void loadModelComponentInfoAdaptedOutputFactory(const HydroCouple::IModelComponentInfo *modelComponentInfo);

      void unloadModelComponentInfoAdaptedOutputFactory(const HydroCouple::IModelComponentInfo *modelComponentInfo);

      void onAdaptedOutputFactoryComponentInfoLoaded(const QString &adaptedOutputFactoryComponentInfoId);

      void onAdaptedOutputFactoryComponentInfoUnloaded(const QString &adaptedOutputFactoryComponentInfoId);

      void onWorkflowComponentInfoLoaded(const QString &workflowComponentInfoId);

      void onWorkflowComponentInfoUnloadedLoaded(const QString &workflowComponentInfoId);

      void onWorkflowComponentCheckStateChanged(QStandardItem *item);

      void onProjectWorkflowComponentChanged(HydroCouple::IWorkflowComponent *workflowComponent);

      void onModelComponentInfoClicked(const QModelIndex& index);

      void onModelComponentInfoDoubleClicked(const QModelIndex& index);

      void onAdaptedOutputClicked(const QModelIndex& index);

      void onAdaptedOutputDoubleClicked(const QModelIndex& index);

      void onAdaptedOutputDoubleClicked(GAdaptedOutput* adaptedOutput);

      void onInsertAdaptedOutput();

      void onWorkflowComponentClicked(const QModelIndex &index);

      void onModelComponentStatusItemClicked(const QModelIndex& index);

      void onModelComponentStatusItemDoubleClicked(const QModelIndex& index);

      void onComponentInfoPropertyChanged(const QString& propertyName);

      void onModelComponentAdded(GModelComponent* modelComponent);

      void onModelComponentDeleting(GModelComponent* modelComponent);

      void onModelComponentDoubleClicked(GModelComponent* modelComponent);

      void onItemDroppedInGraphicsView(const QPointF& scenePos, const QMap<QString,QVariant>& dropData);

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

      void onTreeViewAdaptedOutputFactoryComponentContextMenuRequested(const QPoint& pos);

      void onTreeViewWorkflowComponentContextMenuRequested(const QPoint& pos);

      void onGraphicsViewHydroCoupleComposerContextMenuRequested(const QPoint& pos);

      void onShowHydroCoupleVis();

      void onAbout();
      
      void onPreferences();

   signals:

      void currentToolChanged(int currentTool);

   private:

      HydroCoupleProject *m_project;
      QPropertyModel *m_propertyModel;
      QStandardItemModel *m_modelComponentInfoModel, *m_adaptedOutputFactoriesModel, *m_workflowComponentModel;
      QStandardItem *m_externalAdaptedOutputFactoriesStandardItem, *m_internalAdaptedOutputFactoriesStandardItem;
      QProgressBar* m_progressBar;
      static QIcon s_categoryIcon, s_adaptedOutputFactoryIcon, s_adaptedOutput, s_adaptedOutputComponentIcon, s_workflowIcon, s_modelComponentIcon;
      QStringList m_recentFiles;
      QSettings m_settings;
      QString m_lastOpenedFilePath;
      QAction* m_recentFilesActions[20];
      QMenu* m_recentFilesMenu;
      QAction* m_clearRecentFilesAction;
      static const QString sc_modelComponentInfoHtml, sc_standardInfoHtml;
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
      ModelStatusItemModel* m_modelStatusItemModel;
      ArgumentDialog* m_argumentDialog;
      QMenu *m_graphicsContextMenu, *m_treeViewModelComponentInfoContextMenu, *m_treeViewAdaptedOutputFactoryComponentInfoContextMenu,
      *m_treeViewWorkflowComponentInfoContextMenu;
      SimulationManager *m_simulationManager;
      PreferencesDialog *m_preferencesDialog;

  public:

      static HydroCoupleComposer *hydroCoupleComposerWindow;
};

#endif // HYDROCOUPLECOMPOSER_H
