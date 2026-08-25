#include "ui/composermainwindow.h"

#include "core/composerapplication.h"
#include "layers/dataitemlayer.h"
#include "layers/gdalrasterlayer.h"
#include "layers/meshlayer.h"
#include "layers/gdalvectorlayer.h"
#include "layers/networktilesource.h"
#include "layers/tilelayer.h"
#include "gis/spatialreference.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "scene/sceneview.h"
#include "map/maplayer.h"
#include "project/hcpimporter.h"
#include "ui/dialogs/layerstyledialog.h"
#include "ui/panels/layertreepanel.h"
#include "ui/theme/thememanager.h"
#include "ui/toolbars/ribbonbar.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QUndoStack>

namespace HydroCouple::Composer
{

  ComposerMainWindow::ComposerMainWindow(QWidget *parent)
    : QMainWindow(parent)
  {
    setWindowTitle(tr("HydroCouple Composer"));
    setObjectName(QStringLiteral("composerMainWindow"));
    resize(1400, 880);

    m_document = new CompositionDocument(this);
    m_registry = new ComponentRegistry(this);

    m_instances = std::make_unique<ComponentInstances>(m_document, m_registry);
    m_simulation = std::make_unique<SimulationManager>(m_registry);

    m_scene = new CompositionScene(m_document, m_instances.get(), this);
    m_canvas = new CompositionCanvas(m_scene, this);

    // Composition and map are two views of the same session rather than two
    // windows: a component's spatial extent and the wiring that feeds it are
    // read together, and a tab keeps both one click apart without either
    // stealing the screen from the other.
    m_workspace = new QTabWidget(this);
    m_workspace->setObjectName(QStringLiteral("workspaceTabs"));
    m_workspace->setDocumentMode(true);
    m_workspace->addTab(m_canvas, tr("Composition"));
    setCentralWidget(m_workspace);

    createMapView();

    createActions();
    createMenus();
    createToolBar();
    createDocks();
    createStatusBar();

    connect(m_scene, &QGraphicsScene::selectionChanged, this,
            &ComposerMainWindow::onSelectionChanged);

    connect(m_document, &CompositionDocument::modifiedChanged, this,
            [this](bool) { refreshTitle(); });
    connect(m_document, &CompositionDocument::filePathChanged, this,
            [this](const QString &) { refreshTitle(); });

    connect(m_simulation.get(), &SimulationManager::stateChanged, this,
            &ComposerMainWindow::onSimulationState);
    connect(m_simulation.get(), &SimulationManager::finished, this,
            [this](bool succeeded, const QString &summary)
            {
              log(succeeded ? tr("Run finished: %1").arg(summary)
                            : tr("Run failed: %1").arg(summary));

              for (const QString &error : m_simulation->errors())
              {
                log(QStringLiteral("  ") + error);
              }

              if (!m_simulation->runManifestPath().isEmpty())
              {
                log(tr("Run manifest: %1")
                      .arg(m_simulation->runManifestPath()));
              }
            });
    connect(m_simulation.get(), &SimulationManager::stepCompleted, this,
            [this](int step)
            {
              statusBar()->showMessage(tr("Step %1").arg(step));
            });

    refreshTitle();
    log(tr("HydroCouple Composer %1 — ready.")
          .arg(ComposerApplication::versionString()));
    log(tr("Load component libraries from Components ▸ Load Directory…, then "
           "drag a component onto the canvas."));
  }

  ComposerMainWindow::~ComposerMainWindow()
  {
    // The simulation holds component instances created from the registry's
    // libraries, so it must be torn down before the registry unloads them.
    m_simulation.reset();
    m_instances.reset();
  }

  CompositionDocument *ComposerMainWindow::document() const
  {
    return m_document;
  }

  ComponentRegistry *ComposerMainWindow::registry() const
  {
    return m_registry;
  }

  CompositionCanvas *ComposerMainWindow::canvas() const
  {
    return m_canvas;
  }

  ComponentConfigurator *ComposerMainWindow::configurator() const
  {
    return m_configurator;
  }

  MapCanvas *ComposerMainWindow::mapCanvas() const
  {
    return m_mapCanvas;
  }

  SceneView *ComposerMainWindow::sceneView() const
  {
    return m_sceneView;
  }

  LayerStackModel *ComposerMainWindow::layerStack() const
  {
    return m_layerStack;
  }

  LayerTreePanel *ComposerMainWindow::layerTree() const
  {
    return m_layerTree;
  }

  namespace
  {
    /*!
     * \brief Gives an action a platform-standard icon when it has none.
     *
     * The ribbon's whole point is icon-over-label faces, so a face with no
     * icon reads as a gap. These are placeholders with correct semantics —
     * a designed icon set can replace them without touching this code.
     */
    void ensureIcon(QAction *action, QStyle::StandardPixmap pixmap)
    {
      if (action && action->icon().isNull())
      {
        action->setIcon(QApplication::style()->standardIcon(pixmap));
      }
    }
  } // namespace

  void ComposerMainWindow::createMapView()
  {
    m_layerStack = new LayerStackModel(this);

    m_mapCanvas = new MapCanvas(this);
    m_mapCanvas->setModel(m_layerStack);

    // Web Mercator: it is what tiled basemaps are published in, and a map
    // whose CRS differs from its backdrop's has to resample every tile.
    m_mapCanvas->setCrs(SpatialReference::webMercator());

    m_workspace->addTab(m_mapCanvas, tr("Map"));

    // The same stack, seen a second way. Neither view knows about the other:
    // hiding or restyling a layer in the tree changes both because the stack
    // is the single owner of what is drawn.
    m_sceneView = new SceneView(this);
    m_sceneView->setModel(m_layerStack);

    m_workspace->addTab(m_sceneView, tr("3D"));

    connect(m_mapCanvas, &MapCanvas::cursorMoved, this,
            [this](const QPointF &world)
            {
              if (!m_coordinateLabel)
              {
                return;
              }

              // Six decimals resolves roughly a tenth of a metre in degrees
              // and a micrometre in metres — enough for either without
              // pretending to a precision the data does not have.
              m_coordinateLabel->setText(QStringLiteral("%1, %2")
                                           .arg(world.x(), 0, 'f', 6)
                                           .arg(world.y(), 0, 'f', 6));
            });
  }

  void ComposerMainWindow::createActions()
  {
    m_zoomFullAction = new QAction(tr("Zoom to &Full Extent"), this);
    m_zoomFullAction->setObjectName(QStringLiteral("zoomFullAction"));
    connect(m_zoomFullAction, &QAction::triggered, this,
            [this]
            {
              // Framing follows whichever view is in front. One action for
              // both, because "zoom to everything" means the same thing in
              // each and two would be two things to keep in step.
              if (m_workspace->currentWidget() == m_sceneView)
              {
                m_sceneView->zoomToFullExtent();
              }
              else
              {
                m_mapCanvas->zoomToFullExtent();
              }
            });

    m_zoomInAction = new QAction(tr("Zoom &In"), this);
    m_zoomInAction->setObjectName(QStringLiteral("zoomInAction"));
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this,
            [this] { m_mapCanvas->zoomBy(1.25); });

    m_addVectorAction = new QAction(tr("Add &Vector Data…"), this);
    m_addVectorAction->setObjectName(QStringLiteral("addVectorAction"));
    connect(m_addVectorAction, &QAction::triggered, this,
            &ComposerMainWindow::onAddVectorLayer);

    m_addRasterAction = new QAction(tr("Add &Raster Data…"), this);
    m_addRasterAction->setObjectName(QStringLiteral("addRasterAction"));
    connect(m_addRasterAction, &QAction::triggered, this,
            &ComposerMainWindow::onAddRasterLayer);

    m_addMeshAction = new QAction(tr("Add &Mesh…"), this);
    m_addMeshAction->setObjectName(QStringLiteral("addMeshAction"));

    // Offered only when the SDK this build links can read UGRID files —
    // NetCDF is optional there, and a command that always fails is worse
    // than one that is not shown.
    m_addMeshAction->setEnabled(MeshLayer::ugridSupported());
    connect(m_addMeshAction, &QAction::triggered, this,
            &ComposerMainWindow::onAddMeshLayer);

    m_addComponentLayersAction =
      new QAction(tr("Add Layers from &Components"), this);
    m_addComponentLayersAction->setObjectName(
      QStringLiteral("addComponentLayersAction"));
    connect(m_addComponentLayersAction, &QAction::triggered, this,
            &ComposerMainWindow::onAddComponentLayers);

    m_styleLayerAction = new QAction(tr("&Style Layer…"), this);
    m_styleLayerAction->setObjectName(QStringLiteral("styleLayerAction"));
    m_styleLayerAction->setEnabled(false);
    connect(m_styleLayerAction, &QAction::triggered, this,
            [this] { m_layerTree->styleCurrent(); });

    m_zoomOutAction = new QAction(tr("Zoom &Out"), this);
    m_zoomOutAction->setObjectName(QStringLiteral("zoomOutAction"));
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this,
            [this] { m_mapCanvas->zoomBy(1.0 / 1.25); });

    m_runAction = new QAction(tr("&Run"), this);
    m_runAction->setObjectName(QStringLiteral("runAction"));
    m_runAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(m_runAction, &QAction::triggered, this, &ComposerMainWindow::onRun);

    m_pauseAction = new QAction(tr("&Pause"), this);
    m_pauseAction->setObjectName(QStringLiteral("pauseAction"));
    m_pauseAction->setEnabled(false);
    connect(m_pauseAction, &QAction::triggered, this,
            &ComposerMainWindow::onPause);

    m_stopAction = new QAction(tr("&Stop"), this);
    m_stopAction->setObjectName(QStringLiteral("stopAction"));
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, this,
            &ComposerMainWindow::onStop);
  }

  void ComposerMainWindow::createMenus()
  {
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->setObjectName(QStringLiteral("fileMenu"));

    m_newAction = fileMenu->addAction(tr("&New"));
    m_newAction->setObjectName(QStringLiteral("newAction"));
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this,
            [this]
            {
              m_document->clear();
              log(tr("New composition."));
            });

    m_openAction = fileMenu->addAction(tr("&Open…"));
    m_openAction->setObjectName(QStringLiteral("openAction"));
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this,
            [this]
            {
              // Opened from the action's triggered() — a release, never a
              // press: a modal opened from a mouse press wedges input on macOS.
              const QString path = QFileDialog::getOpenFileName(
                this, tr("Open composition"), QString(),
                tr("Composition documents (*.json *.yaml *.yml)"));

              if (path.isEmpty())
              {
                return;
              }

              QString message;

              if (!openComposition(path, message))
              {
                QMessageBox::warning(this, tr("Cannot open"), message);
              }
            });

    m_saveAction = fileMenu->addAction(tr("&Save"));
    m_saveAction->setObjectName(QStringLiteral("saveAction"));
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this,
            [this]
            {
              QString path = m_document->filePath();

              if (path.isEmpty())
              {
                path = QFileDialog::getSaveFileName(
                  this, tr("Save composition"), QString(),
                  tr("Composition documents (*.json)"));
              }

              if (path.isEmpty())
              {
                return;
              }

              QString message;

              if (m_document->save(path, message))
              {
                log(tr("Saved %1").arg(path));
              }
              else
              {
                QMessageBox::warning(this, tr("Cannot save"), message);
              }
            });

    fileMenu->addSeparator();

    QAction *importAction = fileMenu->addAction(tr("&Import HydroCouple 1.x…"));
    connect(importAction, &QAction::triggered, this,
            [this]
            {
              const QString path = QFileDialog::getOpenFileName(
                this, tr("Import project"), QString(),
                tr("HydroCouple 1.x projects (*.hcp)"));

              if (path.isEmpty())
              {
                return;
              }

              const ImportResult result = HcpImporter::importFile(path);

              for (const ImportIssue &issue : result.issues)
              {
                log(issue.toString());
              }

              if (!result.succeeded)
              {
                QMessageBox::warning(
                  this, tr("Import failed"),
                  tr("The project could not be imported; see the log."));
                return;
              }

              QString message;
              const QByteArray json =
                QByteArray::fromStdString(result.spec.toJson().dump(2) + "\n");

              if (m_document->loadFromJson(json, message))
              {
                m_document->presentation() = result.presentation;
                log(tr("Imported %1 (%2 issue(s) — see above)")
                      .arg(path)
                      .arg(result.issues.size()));
              }
              else
              {
                QMessageBox::warning(this, tr("Import failed"), message);
              }
            });

    fileMenu->addSeparator();

    QAction *quit = fileMenu->addAction(tr("E&xit"));
    quit->setMenuRole(QAction::QuitRole);
    connect(quit, &QAction::triggered, qApp, &QApplication::closeAllWindows);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->setObjectName(QStringLiteral("editMenu"));

    m_undoAction = m_document->undoStack()->createUndoAction(this, tr("&Undo"));
    m_undoAction->setObjectName(QStringLiteral("undoAction"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(m_undoAction);

    m_redoAction = m_document->undoStack()->createRedoAction(this, tr("&Redo"));
    m_redoAction->setObjectName(QStringLiteral("redoAction"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(m_redoAction);

    QMenu *componentsMenu = menuBar()->addMenu(tr("&Components"));
    componentsMenu->setObjectName(QStringLiteral("componentsMenu"));

    m_loadComponentsAction = componentsMenu->addAction(tr("&Load Directory…"));
    m_loadComponentsAction->setObjectName(QStringLiteral("loadComponentsAction"));
    connect(m_loadComponentsAction, &QAction::triggered, this,
            [this]
            {
              const QString directory = QFileDialog::getExistingDirectory(
                this, tr("Component library directory"));

              if (directory.isEmpty())
              {
                return;
              }

              const int loaded = loadComponentDirectory(directory);
              log(tr("Loaded %1 component library(ies) from %2")
                    .arg(loaded)
                    .arg(directory));
            });

    QMenu *runMenu = menuBar()->addMenu(tr("&Run"));
    runMenu->setObjectName(QStringLiteral("runMenu"));
    runMenu->addAction(m_runAction);
    runMenu->addAction(m_pauseAction);
    runMenu->addAction(m_stopAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->setObjectName(QStringLiteral("viewMenu"));

    viewMenu->addAction(m_addVectorAction);
    viewMenu->addAction(m_addRasterAction);
    viewMenu->addAction(m_addMeshAction);
    viewMenu->addAction(m_addComponentLayersAction);

    QMenu *basemapMenu = viewMenu->addMenu(tr("&Basemap"));
    basemapMenu->setObjectName(QStringLiteral("basemapMenu"));

    m_basemapGroup = new QActionGroup(this);
    m_basemapGroup->setExclusive(true);

    QAction *noBasemap = basemapMenu->addAction(tr("&None"));
    noBasemap->setObjectName(QStringLiteral("basemapNoneAction"));
    noBasemap->setCheckable(true);
    noBasemap->setChecked(true);
    m_basemapGroup->addAction(noBasemap);
    connect(noBasemap, &QAction::triggered, this,
            [this] { setBasemap(QString()); });

    for (const BasemapProvider &provider :
         NetworkTileSource::builtinProviders())
    {
      QAction *action = basemapMenu->addAction(provider.name);
      action->setCheckable(true);
      m_basemapGroup->addAction(action);

      connect(action, &QAction::triggered, this,
              [this, name = provider.name] { setBasemap(name); });
    }

    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomFullAction);
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_styleLayerAction);
    viewMenu->addSeparator();

    QMenu *appearanceMenu = viewMenu->addMenu(tr("&Appearance"));
    appearanceMenu->setObjectName(QStringLiteral("appearanceMenu"));

    auto *appearanceGroup = new QActionGroup(this);
    appearanceGroup->setExclusive(true);

    const struct
    {
        ThemeManager::Mode mode;
        const char *label;
        const char *name;
    } appearances[] = {
      {ThemeManager::Mode::System, QT_TR_NOOP("&System"), "appearanceSystem"},
      {ThemeManager::Mode::Light, QT_TR_NOOP("&Light"), "appearanceLight"},
      {ThemeManager::Mode::Dark, QT_TR_NOOP("&Dark"), "appearanceDark"},
    };

    ThemeManager *theme = ThemeManager::instance();

    for (const auto &appearance : appearances)
    {
      QAction *action = appearanceMenu->addAction(tr(appearance.label));
      action->setObjectName(QString::fromLatin1(appearance.name));

      switch (appearance.mode)
      {
        case ThemeManager::Mode::Light:
          m_lightAction = action;
          break;
        case ThemeManager::Mode::Dark:
          m_darkAction = action;
          break;
        case ThemeManager::Mode::System:
          m_systemAction = action;
          break;
      }
      action->setCheckable(true);
      action->setChecked(theme->mode() == appearance.mode);
      appearanceGroup->addAction(action);

      connect(action, &QAction::triggered, this,
              [this, theme, mode = appearance.mode]
              {
                theme->setMode(mode);
                theme->apply();

                // Persisted so the choice survives a restart, matching how
                // openswmm.gui remembers its appearance preference.
                QSettings().setValue(QStringLiteral("appearance/mode"),
                                     ThemeManager::modeToString(mode));

                log(tr("Appearance: %1")
                      .arg(ThemeManager::modeToString(mode)));
              });
    }

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->setObjectName(QStringLiteral("helpMenu"));

    QAction *about = helpMenu->addAction(tr("&About"));
    connect(about, &QAction::triggered, this,
            [this]
            {
              QMessageBox::about(
                this, tr("About HydroCouple Composer"),
                tr("<b>HydroCouple Composer %1</b><br>"
                   "Composition editor, configurator and results viewer for "
                   "HydroCouple 2.0 model components.")
                  .arg(ComposerApplication::versionString()));
            });
  }

  void ComposerMainWindow::createToolBar()
  {
    // A ribbon rather than a strip of small buttons, matching openswmm.gui:
    // tabs of captioned groups with large icon-over-label faces.
    m_ribbon = new RibbonBar(this);

    // The ribbon is hosted in a fixed QToolBar so it docks above the menu
    // area like any other top toolbar and cannot be dragged out of place.
    auto *host = new QToolBar(tr("Ribbon"), this);
    host->setObjectName(QStringLiteral("ribbonHost"));
    host->setMovable(false);
    host->setFloatable(false);
    host->addWidget(m_ribbon);
    addToolBar(Qt::TopToolBarArea, host);

    // Declare the tab before its groups, or addGroup() would create it using
    // the id as its title.
    m_ribbon->addTab(QStringLiteral("home"), tr("Home"));

    RibbonGroup *composition = m_ribbon->addGroup(QStringLiteral("home"),
                                                  tr("Composition"));
    composition->addAction(m_newAction, tr("New"));
    composition->addAction(m_openAction, tr("Open"));
    composition->addAction(m_saveAction, tr("Save"));

    RibbonGroup *componentsGroup =
      m_ribbon->addGroup(QStringLiteral("home"), tr("Components"));
    componentsGroup->addAction(m_loadComponentsAction, tr("Load\nLibraries"));

    RibbonGroup *simulation =
      m_ribbon->addGroup(QStringLiteral("home"), tr("Simulation"));
    simulation->addAction(m_runAction, tr("Run"));
    simulation->addAction(m_pauseAction, tr("Pause"));
    simulation->addAction(m_stopAction, tr("Stop"));

    RibbonGroup *edit = m_ribbon->addGroup(QStringLiteral("home"), tr("Edit"));
    edit->addAction(m_undoAction, tr("Undo"));
    edit->addAction(m_redoAction, tr("Redo"));

    // The map's own tab. Classification, basemaps and import join it in C1c
    // and C1d; the results tools get a tab of their own in phase D.
    m_ribbon->addTab(QStringLiteral("map"), tr("Map"));

    RibbonGroup *navigate =
      m_ribbon->addGroup(QStringLiteral("map"), tr("Navigate"));
    navigate->addAction(m_zoomFullAction, tr("Full\nExtent"));
    navigate->addAction(m_zoomInAction, tr("Zoom\nIn"));
    navigate->addAction(m_zoomOutAction, tr("Zoom\nOut"));

    RibbonGroup *data = m_ribbon->addGroup(QStringLiteral("map"), tr("Data"));
    data->addAction(m_addVectorAction, tr("Add\nVector"));
    data->addAction(m_addRasterAction, tr("Add\nRaster"));
    data->addAction(m_addMeshAction, tr("Add\nMesh"));
    data->addAction(m_addComponentLayersAction, tr("From\nComponents"));

    RibbonGroup *symbology =
      m_ribbon->addGroup(QStringLiteral("map"), tr("Symbology"));
    symbology->addAction(m_styleLayerAction, tr("Style\nLayer"));

    m_ribbon->addTab(QStringLiteral("view"), tr("View"));

    RibbonGroup *appearance =
      m_ribbon->addGroup(QStringLiteral("view"), tr("Appearance"));
    appearance->addAction(m_lightAction, tr("Light"));
    appearance->addAction(m_darkAction, tr("Dark"));
    appearance->addAction(m_systemAction, tr("System"));

    ensureIcon(m_newAction, QStyle::SP_FileIcon);
    ensureIcon(m_openAction, QStyle::SP_DirOpenIcon);
    ensureIcon(m_saveAction, QStyle::SP_DialogSaveButton);
    ensureIcon(m_loadComponentsAction, QStyle::SP_DirLinkIcon);
    ensureIcon(m_runAction, QStyle::SP_MediaPlay);
    ensureIcon(m_pauseAction, QStyle::SP_MediaPause);
    ensureIcon(m_stopAction, QStyle::SP_MediaStop);
    ensureIcon(m_undoAction, QStyle::SP_ArrowBack);
    ensureIcon(m_redoAction, QStyle::SP_ArrowForward);
    ensureIcon(m_lightAction, QStyle::SP_DialogYesButton);
    ensureIcon(m_darkAction, QStyle::SP_DialogNoButton);
    ensureIcon(m_systemAction, QStyle::SP_ComputerIcon);
    ensureIcon(m_zoomFullAction, QStyle::SP_FileDialogListView);
    ensureIcon(m_zoomInAction, QStyle::SP_TitleBarMaxButton);
    ensureIcon(m_zoomOutAction, QStyle::SP_TitleBarMinButton);
    ensureIcon(m_styleLayerAction, QStyle::SP_DialogApplyButton);
    ensureIcon(m_addVectorAction, QStyle::SP_FileDialogNewFolder);
    ensureIcon(m_addRasterAction, QStyle::SP_FileDialogDetailedView);
    ensureIcon(m_addComponentLayersAction, QStyle::SP_DriveNetIcon);
    ensureIcon(m_addMeshAction, QStyle::SP_FileDialogInfoView);

    m_ribbon->setCurrentTab(QStringLiteral("home"));
  }

  void ComposerMainWindow::createDocks()
  {
    // ── Component palette ────────────────────────────────────────────────
    auto *paletteDock = new QDockWidget(tr("Components"), this);
    paletteDock->setObjectName(QStringLiteral("paletteDock"));

    m_paletteModel = new ComponentPaletteModel(m_registry, this);
    m_paletteView = new QListView(paletteDock);
    m_paletteView->setObjectName(QStringLiteral("paletteView"));
    m_paletteView->setModel(m_paletteModel);
    m_paletteView->setDragEnabled(true);
    m_paletteView->setDragDropMode(QAbstractItemView::DragOnly);
    paletteDock->setWidget(m_paletteView);

    addDockWidget(Qt::LeftDockWidgetArea, paletteDock);

    // ── Layers ───────────────────────────────────────────────────────────
    auto *layerDock = new QDockWidget(tr("Layers"), this);
    layerDock->setObjectName(QStringLiteral("layerDock"));

    m_layerTree = new LayerTreePanel(layerDock);
    m_layerTree->setModel(m_layerStack);
    layerDock->setWidget(m_layerTree);

    // Tabbed with the palette rather than stacked: both are lists of things
    // to place, and only one is in use at a time.
    addDockWidget(Qt::LeftDockWidgetArea, layerDock);
    tabifyDockWidget(paletteDock, layerDock);
    paletteDock->raise();

    connect(m_layerTree, &LayerTreePanel::styleLayerRequested, this,
            &ComposerMainWindow::onStyleLayer);

    connect(m_layerTree, &LayerTreePanel::currentLayerChanged, this,
            [this](MapLayer *layer)
            {
              m_styleLayerAction->setEnabled(
                LayerStyleDialog::canStyle(layer));
            });

    connect(m_layerTree, &LayerTreePanel::zoomToLayerRequested, this,
            [this](MapLayer *layer)
            {
              // Framing a layer is only meaningful on the map, so asking for
              // it brings the map forward rather than acting invisibly behind
              // whichever tab happens to be showing.
              m_workspace->setCurrentWidget(m_mapCanvas);
              m_mapCanvas->zoomToLayer(layer);
            });

    // ── Configurator ─────────────────────────────────────────────────────
    auto *configuratorDock = new QDockWidget(tr("Arguments"), this);
    configuratorDock->setObjectName(QStringLiteral("configuratorDock"));

    m_configurator = new ComponentConfigurator(m_document, m_instances.get(),
                                               configuratorDock);
    configuratorDock->setWidget(m_configurator);

    addDockWidget(Qt::RightDockWidgetArea, configuratorDock);

    // ── Log ──────────────────────────────────────────────────────────────
    auto *logDock = new QDockWidget(tr("Log"), this);
    logDock->setObjectName(QStringLiteral("logDock"));

    m_log = new QPlainTextEdit(logDock);
    m_log->setObjectName(QStringLiteral("logView"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    logDock->setWidget(m_log);

    addDockWidget(Qt::BottomDockWidgetArea, logDock);
  }

  void ComposerMainWindow::addMapLayer(MapLayer *layer)
  {
    if (!layer)
    {
      return;
    }

    m_layerStack->addLayer(layer);

    // Brought forward and framed: data added to a map the user cannot see
    // looks like nothing happened.
    m_workspace->setCurrentWidget(m_mapCanvas);
    m_mapCanvas->zoomToLayer(layer);

    log(tr("Added layer %1.").arg(layer->name()));
  }

  void ComposerMainWindow::onAddVectorLayer()
  {
    // Opened from triggered(), a release — a modal opened from a mouse press
    // wedges input on macOS.
    const QString path = QFileDialog::getOpenFileName(
      this, tr("Add vector data"), QString(),
      tr("Vector data (*.shp *.geojson *.json *.gpkg *.gml *.kml *.tab);;"
         "All files (*)"));

    if (path.isEmpty())
    {
      return;
    }

    // A GeoPackage routinely holds several layers, and opening only the
    // first would silently ignore the rest.
    const QStringList sublayers = GdalVectorLayer::sublayerNames(path);
    QString chosen;

    if (sublayers.size() > 1)
    {
      bool accepted = false;
      chosen = QInputDialog::getItem(this, tr("Choose a layer"),
                                     tr("This dataset holds several layers:"),
                                     sublayers, 0, false, &accepted);

      if (!accepted)
      {
        return;
      }
    }

    QString message;
    std::unique_ptr<GdalVectorLayer> layer =
      GdalVectorLayer::open(path, message, chosen);

    if (!layer)
    {
      QMessageBox::warning(this, tr("Cannot add data"), message);
      log(message);

      return;
    }

    addMapLayer(layer.release());
  }

  void ComposerMainWindow::onAddRasterLayer()
  {
    const QString path = QFileDialog::getOpenFileName(
      this, tr("Add raster data"), QString(),
      tr("Raster data (*.tif *.tiff *.img *.asc *.vrt *.nc);;All files (*)"));

    if (path.isEmpty())
    {
      return;
    }

    QString message;
    std::unique_ptr<GdalRasterLayer> layer =
      GdalRasterLayer::open(path, message);

    if (!layer)
    {
      QMessageBox::warning(this, tr("Cannot add data"), message);
      log(message);

      return;
    }

    addMapLayer(layer.release());
  }

  int ComposerMainWindow::addComponentLayers()
  {
    int added = 0;

    for (const QString &componentId : m_document->componentIds())
    {
      HydroCouple::IModelComponent *component =
        m_instances->instance(componentId);

      if (!component)
      {
        continue;
      }

      // Inputs as well as outputs: a component's forcing is as worth seeing
      // on a map as its results, and both are the same kind of item.
      std::vector<HydroCouple::IComponentDataItem *> items;

      for (HydroCouple::IInput *input : component->inputs())
      {
        items.push_back(input);
      }

      for (HydroCouple::IOutput *output : component->outputs())
      {
        items.push_back(output);
      }

      for (HydroCouple::IComponentDataItem *item : items)
      {
        if (!DataItemLayer::isSpatial(item))
        {
          continue;
        }

        QString message;
        std::unique_ptr<DataItemLayer> layer =
          DataItemLayer::create(item, message);

        if (!layer)
        {
          log(tr("%1: %2").arg(componentId, message));
          continue;
        }

        layer->setName(tr("%1 — %2").arg(componentId, layer->name()));

        m_layerStack->addLayer(layer.release());
        ++added;
      }
    }

    return added;
  }

  void ComposerMainWindow::onAddMeshLayer()
  {
    const QString path = QFileDialog::getOpenFileName(
      this, tr("Add UGRID mesh"), QString(),
      tr("UGRID meshes (*.nc *.nc4);;All files (*)"));

    if (path.isEmpty())
    {
      return;
    }

    const QStringList meshes = MeshLayer::ugridMeshNames(path);
    QString chosen;

    // A file may hold a 1-D network and a 2-D floodplain both; opening only
    // the first would silently ignore the rest.
    if (meshes.size() > 1)
    {
      bool accepted = false;
      chosen = QInputDialog::getItem(this, tr("Choose a mesh"),
                                     tr("This file holds several meshes:"),
                                     meshes, 0, false, &accepted);

      if (!accepted)
      {
        return;
      }
    }

    QString message;
    std::unique_ptr<MeshLayer> layer =
      MeshLayer::fromUGRIDFile(path, chosen, MeshEntity::Face, message);

    if (!layer)
    {
      QMessageBox::warning(this, tr("Cannot add mesh"), message);
      log(message);

      return;
    }

    addMapLayer(layer.release());
  }

  void ComposerMainWindow::onAddComponentLayers()
  {
    const int added = addComponentLayers();

    if (added == 0)
    {
      // Said plainly: a component with no spatial data items is the common
      // case, and a menu entry that silently does nothing reads as a bug.
      QMessageBox::information(
        this, tr("No spatial data"),
        tr("None of the components in this composition expose data items "
           "with geometry."));

      return;
    }

    m_workspace->setCurrentWidget(m_mapCanvas);
    m_mapCanvas->zoomToFullExtent();

    log(tr("Added %1 layer(s) from components.").arg(added));
  }

  void ComposerMainWindow::setBasemap(const QString &providerName)
  {
    // At most one basemap, always at the bottom of the stack: two backdrops
    // would fight for the same pixels and the upper one would simply win.
    for (MapLayer *layer : m_layerStack->layers())
    {
      if (layer->isBasemap())
      {
        m_layerStack->removeLayer(m_layerStack->rowOf(layer));
        break;
      }
    }

    if (providerName.isEmpty())
    {
      log(tr("Basemap removed."));
      return;
    }

    const BasemapProvider provider =
      NetworkTileSource::builtinProvider(providerName);

    auto source = std::make_unique<NetworkTileSource>(provider);
    NetworkTileSource *raw = source.get();

    auto *basemap = new TileLayer(provider.name, std::move(source));

    // The bottom of the stack, so every data layer draws over it.
    m_layerStack->insertLayer(m_layerStack->rowCount(), basemap);

    raw->setTileReadyCallback([basemap] { basemap->onTileReady(); });

    log(tr("Basemap: %1 — %2").arg(provider.name, provider.attribution));
  }

  void ComposerMainWindow::onStyleLayer(MapLayer *layer)
  {
    if (!LayerStyleDialog::canStyle(layer))
    {
      return;
    }

    // Shown from an action's triggered() or a button's clicked() — a release,
    // never a press: a modal opened from a mouse press wedges input on macOS.
    LayerStyleDialog dialog(layer, this);

    if (dialog.exec() == QDialog::Accepted)
    {
      log(tr("Restyled %1.").arg(layer->name()));
    }
  }

  void ComposerMainWindow::createStatusBar()
  {
    statusBar()->showMessage(
      tr("HydroCouple Composer %1").arg(ComposerApplication::versionString()));

    m_coordinateLabel = new QLabel(statusBar());
    m_coordinateLabel->setObjectName(QStringLiteral("coordinateLabel"));

    // A permanent widget, so a transient status message cannot cover the one
    // readout that has to stay put while the pointer moves.
    statusBar()->addPermanentWidget(m_coordinateLabel);
  }

  void ComposerMainWindow::refreshTitle()
  {
    const QString name = m_document->filePath().isEmpty()
                           ? tr("Untitled")
                           : QFileInfo(m_document->filePath()).fileName();

    setWindowTitle(tr("%1%2 — HydroCouple Composer")
                     .arg(name, m_document->isModified()
                                  ? QStringLiteral("*")
                                  : QString()));
  }

  int ComposerMainWindow::loadComponentDirectory(const QString &directoryPath)
  {
    const int loaded = m_registry->scanDirectory(directoryPath);

    for (const ComponentLoadFailure &failure : m_registry->failures())
    {
      log(tr("Skipped %1: %2").arg(failure.filePath, failure.message));
    }

    return loaded;
  }

  bool ComposerMainWindow::openComposition(const QString &filePath,
                                           QString &message)
  {
    if (!m_document->load(filePath, message))
    {
      return false;
    }

    log(tr("Opened %1").arg(filePath));

    return true;
  }

  void ComposerMainWindow::log(const QString &text)
  {
    m_log->appendPlainText(text);
  }

  void ComposerMainWindow::onSelectionChanged()
  {
    for (QGraphicsItem *item : m_scene->selectedItems())
    {
      if (auto *node = qgraphicsitem_cast<ComponentNodeItem *>(item))
      {
        m_configurator->setComponent(node->componentId());
        return;
      }
    }

    m_configurator->setComponent(QString());
  }

  void ComposerMainWindow::onRun()
  {
    QString message;

    if (!m_simulation->start(*m_document, message))
    {
      log(tr("Cannot run: %1").arg(message));
      QMessageBox::warning(this, tr("Cannot run"), message);
      return;
    }

    log(tr("Run started."));
  }

  void ComposerMainWindow::onPause()
  {
    if (m_simulation->state() == SimulationState::Paused)
    {
      m_simulation->requestResume();
      log(tr("Resumed."));
      return;
    }

    m_simulation->requestPause();
    log(tr("Pause requested."));
  }

  void ComposerMainWindow::onStop()
  {
    m_simulation->requestStop();
    log(tr("Stop requested."));
  }

  void ComposerMainWindow::onSimulationState(SimulationState state)
  {
    const bool running = m_simulation->isRunning();

    m_runAction->setEnabled(!running);
    m_pauseAction->setEnabled(running);
    m_stopAction->setEnabled(running);

    m_pauseAction->setText(state == SimulationState::Paused ? tr("Res&ume")
                                                            : tr("&Pause"));

    switch (state)
    {
      case SimulationState::Running:
        statusBar()->showMessage(tr("Running…"));
        break;
      case SimulationState::Paused:
        statusBar()->showMessage(tr("Paused"));
        break;
      case SimulationState::Stopping:
        statusBar()->showMessage(tr("Stopping…"));
        break;
      case SimulationState::Finished:
        statusBar()->showMessage(tr("Finished"));
        break;
      case SimulationState::Failed:
        statusBar()->showMessage(tr("Failed"));
        break;
      default:
        break;
    }
  }

} // namespace HydroCouple::Composer
