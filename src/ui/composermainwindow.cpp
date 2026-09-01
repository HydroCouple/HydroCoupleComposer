#include "ui/composermainwindow.h"

#include "core/composerapplication.h"
#include "layers/dataitemlayer.h"
#include "layers/differencelayer.h"
#include "layers/domainlayer.h"
#include "layers/featurelayer.h"
#include "mesh/domaindrawtool.h"
#include "mesh/domainedittool.h"
#include "mesh/domainimport.h"
#include "mesh/meshdomainmodel.h"
#include "layers/gdalrasterlayer.h"
#include "layers/meshlayer.h"
#include "layers/gdalvectorlayer.h"
#include "layers/networktilesource.h"
#include "layers/tilelayer.h"
#include "gis/spatialreference.h"
#include "map/layerstackmodel.h"
#include "map/mapcanvas.h"
#include "scene/camera.h"
#include "scene/sceneview.h"
#include "map/maplayer.h"
#include "project/hcpimporter.h"
#include "results/runbrowsermodel.h"
#include "results/runsession.h"
#include "results/timecontroller.h"
#include "ui/dialogs/crsselectiondialog.h"
#include "ui/dialogs/layerpropertiesdialog.h"
#include "mesh/terrainsampler.h"
#include "layers/layerrestorer.h"
#include "layers/wcscoveragelayer.h"
#include "layers/wfsfeaturelayer.h"
#include "ui/dialogs/ogcservicedialog.h"
#include "ui/panels/attributetablepanel.h"
#include "ui/panels/layertreepanel.h"
#include "ui/panels/runbrowserpanel.h"
#include "ui/panels/profileplotpanel.h"
#include "ui/panels/transectpanel.h"
#include "ui/panels/seriesplotpanel.h"
#include "ui/panels/timecontrolpanel.h"
#include "ui/theme/iconfactory.h"
#include "ui/theme/thememanager.h"
#include "ui/toolbars/ribbonbar.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QToolButton>
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
    m_workspace->addTab(m_canvas,
                        IconFactory::icon(QStringLiteral("composition")),
                        tr("Composition"));

    // The transport sits under the views rather than in a dock beside them.
    // The bottom docks are tabbed, and a clock that could be tabbed behind
    // the attribute table would be a clock you cannot see while reading the
    // values it is stepping through.
    m_clock = new TimeController(this);
    m_timeControls = new TimeControlPanel(this);
    m_timeControls->setController(m_clock);

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_workspace, 1);
    centralLayout->addWidget(m_timeControls);

    setCentralWidget(central);

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

  AttributeTablePanel *ComposerMainWindow::attributeTable() const
  {
    return m_attributeTable;
  }

  namespace
  {
    /*!
     * \brief Gives an action its icon from the shared chrome set.
     *
     * The ribbon's whole point is icon-over-label faces, so a face with no
     * icon reads as a gap. The set is openswmm.gui's, so the two
     * applications read as one family, and IconFactory recolours each glyph
     * to the current theme — which the platform's own standardIcon artwork,
     * being full-colour bitmaps, cannot do.
     */
    void ensureIcon(QAction *action, const QString &alias)
    {
      if (action && action->icon().isNull())
      {
        action->setIcon(IconFactory::icon(alias));
      }
    }
  } // namespace

  void ComposerMainWindow::createMapView()
  {
    m_layerStack = new LayerStackModel(this);

    m_meshDomain = new MeshDomainModel(this);

    // The layers that show the domain appear when there is a domain to
    // show, or when the user picks up a tool to draw one. Four permanent
    // rows in every composition that never meshes anything is clutter, and
    // it would put four empty layers into the extent every view frames.
    connect(m_meshDomain, &MeshDomainModel::domainChanged, this,
            [this]
            {
              if (!m_meshDomain->domain().isEmpty())
              {
                ensureDomainLayers();
              }
            });

    m_clock->setModel(m_layerStack);

    m_mapCanvas = new MapCanvas(this);
    m_mapCanvas->setModel(m_layerStack);

    // Web Mercator: it is what tiled basemaps are published in, and a map
    // whose CRS differs from its backdrop's has to resample every tile.
    m_mapCanvas->setCrs(SpatialReference::webMercator());

    m_workspace->addTab(m_mapCanvas,
                        IconFactory::icon(QStringLiteral("globe")),
                        tr("Map"));

    // The same stack, seen a second way. Neither view knows about the other:
    // hiding or restyling a layer in the tree changes both because the stack
    // is the single owner of what is drawn.
    m_sceneView = new SceneView(this);
    m_sceneView->setModel(m_layerStack);

    m_workspace->addTab(m_sceneView,
                        IconFactory::icon(QStringLiteral("scene_3d")),
                        tr("3D"));

    // The hand-off. The two views share a layer stack already; what they do
    // not share is where they are looking, and a user who frames a catchment
    // in one and finds the other showing a continent has been given two
    // applications rather than two views of one.
    //
    // Carried on the tab change rather than continuously: keeping them in
    // step live would mean the hidden view reframing on every pan of the
    // visible one, and a tilted camera's ground extent is a bounding box, so
    // each such exchange widens what is shown. Once, on arrival, does not
    // drift.
    connect(m_workspace, &QTabWidget::currentChanged, this,
            [this](int index)
            {
              QWidget *arriving = m_workspace->widget(index);

              if (arriving == m_sceneView)
              {
                // Only when the map is showing data. An empty map shows its
                // default view around the origin, and handing *that* over
                // would count as having framed the scene deliberately — so a
                // model loaded afterwards would never be framed at all, and
                // the 3D tab would sit looking at nothing near the origin.
                if (!m_mapCanvas->fullExtent().isEmpty())
                {
                  m_sceneView->showGroundExtent(
                    m_mapCanvas->transform().visibleExtent());
                }
              }
              else if (arriving == m_mapCanvas)
              {
                // Empty before the 3D view has had a size, which is exactly
                // the case where it has nothing to hand over anyway.
                const QRectF ground = m_sceneView->groundExtent();

                if (!ground.isEmpty())
                {
                  m_mapCanvas->setVisibleExtent(ground);
                }
              }
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

    m_sampleTerrainAction = new QAction(tr("Sample &Elevations…"), this);
    m_sampleTerrainAction->setToolTip(
      tr("Read a raster layer's values onto a mesh's vertices"));
    connect(m_sampleTerrainAction, &QAction::triggered, this,
            &ComposerMainWindow::onSampleTerrain);
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

    m_layerPropertiesAction = new QAction(tr("Layer &Properties…"), this);
    m_layerPropertiesAction->setObjectName(
      QStringLiteral("layerPropertiesAction"));
    m_layerPropertiesAction->setEnabled(false);
    connect(m_layerPropertiesAction, &QAction::triggered, this,
            [this] { m_layerTree->openProperties(); });

    m_mapCrsAction = new QAction(tr("Map &Coordinate System…"), this);
    m_mapCrsAction->setObjectName(QStringLiteral("mapCrsAction"));
    connect(m_mapCrsAction, &QAction::triggered, this,
            &ComposerMainWindow::onSetMapCrs);

    m_selectToolAction = new QAction(tr("&Select"), this);
    m_selectToolAction->setObjectName(QStringLiteral("selectToolAction"));
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setToolTip(
      tr("Click to select one feature; drag a box to select every feature "
         "it crosses."));

    m_panToolAction = new QAction(tr("Pa&n"), this);
    m_panToolAction->setObjectName(QStringLiteral("panToolAction"));
    m_panToolAction->setCheckable(true);
    m_panToolAction->setChecked(true);
    m_panToolAction->setToolTip(
      tr("Drag to pan; click to identify what is underneath."));

    m_zoomInToolAction = new QAction(tr("Zoom &In Box"), this);
    m_zoomInToolAction->setObjectName(QStringLiteral("zoomInToolAction"));
    m_zoomInToolAction->setCheckable(true);
    m_zoomInToolAction->setToolTip(
      tr("Drag a box to zoom into it; click to zoom in."));

    m_zoomOutToolAction = new QAction(tr("Zoom &Out Box"), this);
    m_zoomOutToolAction->setObjectName(QStringLiteral("zoomOutToolAction"));
    m_zoomOutToolAction->setCheckable(true);
    m_zoomOutToolAction->setToolTip(
      tr("Drag a box to fit the current view into it; click to zoom out."));

    m_transectToolAction = new QAction(tr("Sec&tion Line"), this);
    m_transectToolAction->setObjectName(QStringLiteral("transectToolAction"));
    m_transectToolAction->setCheckable(true);
    m_transectToolAction->setToolTip(
      tr("Drag a line across a layered mesh to cut a section through the "
         "water column."));

    m_drawBoundaryAction = new QAction(tr("&Boundary"), this);
    m_drawBoundaryAction->setObjectName(QStringLiteral("drawBoundaryAction"));
    m_drawBoundaryAction->setCheckable(true);
    m_drawBoundaryAction->setToolTip(
      tr("Click to place the boundary's corners; right-click to finish."));

    m_drawHoleAction = new QAction(tr("&Hole"), this);
    m_drawHoleAction->setObjectName(QStringLiteral("drawHoleAction"));
    m_drawHoleAction->setCheckable(true);
    m_drawHoleAction->setToolTip(
      tr("Click to cut a hole out of the domain; right-click to finish."));

    m_drawBreaklineAction = new QAction(tr("Break&line"), this);
    m_drawBreaklineAction->setObjectName(
      QStringLiteral("drawBreaklineAction"));
    m_drawBreaklineAction->setCheckable(true);
    m_drawBreaklineAction->setToolTip(
      tr("Click to draw a line the mesh must follow; right-click to "
         "finish."));

    m_drawPointAction = new QAction(tr("&Point"), this);
    m_drawPointAction->setObjectName(QStringLiteral("drawPointAction"));
    m_drawPointAction->setCheckable(true);
    m_drawPointAction->setToolTip(
      tr("Click to force a mesh vertex where you click."));

    m_editVerticesAction = new QAction(tr("&Edit Vertices"), this);
    m_editVerticesAction->setObjectName(QStringLiteral("editVerticesAction"));
    m_editVerticesAction->setCheckable(true);
    m_editVerticesAction->setToolTip(
      tr("Drag a domain vertex to move it, click an edge to add one, "
         "right-click a vertex to remove it."));

    // A command, not a gesture set: it acts once on what is already
    // selected and leaves the map under whatever tool it was under, so it
    // stays out of the exclusive group below and is never checkable.
    m_importSelectionAction = new QAction(tr("From &Selection"), this);
    m_importSelectionAction->setObjectName(
      QStringLiteral("importSelectionAction"));
    m_importSelectionAction->setToolTip(
      tr("Add the selected features of the current layer to the domain."));

    m_importSelectionMenu = new QMenu(this);

    // One handler for four entries: which part the features become is
    // carried on the action rather than written out four times.
    const struct
    {
        const char *name;
        QString text;
        DomainPart part;
    } importParts[] = {
      {"importAsBoundaryAction", tr("As &Boundary"), DomainPart::Boundary},
      {"importAsHoleAction", tr("As &Holes"), DomainPart::Holes},
      {"importAsBreaklineAction", tr("As Break&lines"),
       DomainPart::Breaklines},
      {"importAsPointAction", tr("As Forced &Points"),
       DomainPart::ForcedPoints}};

    for (const auto &entry : importParts)
    {
      QAction *action = m_importSelectionMenu->addAction(entry.text);
      action->setObjectName(QString::fromLatin1(entry.name));
      action->setData(int(entry.part));

      connect(action, &QAction::triggered, this,
              &ComposerMainWindow::onImportSelectionAsDomainPart);
    }

    // Exclusive: the map is under one gesture set at a time, and four
    // independent checkboxes would let the UI show a state it cannot be in.
    auto *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    toolGroup->addAction(m_selectToolAction);
    toolGroup->addAction(m_panToolAction);
    toolGroup->addAction(m_zoomInToolAction);
    toolGroup->addAction(m_zoomOutToolAction);
    toolGroup->addAction(m_transectToolAction);

    // The domain tools join the same group: they are gesture sets over the
    // same map, and one of them being on means panning is off.
    toolGroup->addAction(m_drawBoundaryAction);
    toolGroup->addAction(m_drawHoleAction);
    toolGroup->addAction(m_drawBreaklineAction);
    toolGroup->addAction(m_drawPointAction);
    toolGroup->addAction(m_editVerticesAction);

    for (QAction *tool : {m_drawBoundaryAction, m_drawHoleAction,
                          m_drawBreaklineAction, m_drawPointAction,
                          m_editVerticesAction})
    {
      connect(tool, &QAction::triggered, this,
              &ComposerMainWindow::onDomainToolChosen);
    }

    for (QAction *tool : {m_selectToolAction, m_panToolAction,
                          m_zoomInToolAction, m_zoomOutToolAction,
                          m_transectToolAction})
    {
      connect(tool, &QAction::triggered, this,
              &ComposerMainWindow::onMapToolChosen);
    }

    m_orbitToolAction = new QAction(tr("Or&bit"), this);
    m_orbitToolAction->setObjectName(QStringLiteral("orbitToolAction"));
    m_orbitToolAction->setCheckable(true);
    m_orbitToolAction->setChecked(true);
    m_orbitToolAction->setToolTip(
      tr("Drag to turn the scene; click to identify what is underneath."));

    m_sceneSelectToolAction = new QAction(tr("Se&lect in 3D"), this);
    m_sceneSelectToolAction->setObjectName(
      QStringLiteral("sceneSelectToolAction"));
    m_sceneSelectToolAction->setCheckable(true);
    m_sceneSelectToolAction->setToolTip(
      tr("Click to select one feature; drag a box to select what it covers."));

    m_sceneZoomInToolAction = new QAction(tr("Zoom In Box (3D)"), this);
    m_sceneZoomInToolAction->setObjectName(
      QStringLiteral("sceneZoomInToolAction"));
    m_sceneZoomInToolAction->setCheckable(true);
    m_sceneZoomInToolAction->setToolTip(
      tr("Drag a box to frame the ground under it; click to move closer."));

    m_sceneZoomOutToolAction = new QAction(tr("Zoom Out Box (3D)"), this);
    m_sceneZoomOutToolAction->setObjectName(
      QStringLiteral("sceneZoomOutToolAction"));
    m_sceneZoomOutToolAction->setCheckable(true);
    m_sceneZoomOutToolAction->setToolTip(
      tr("Drag a box to fit the view into it; click to move away."));

    auto *sceneToolGroup = new QActionGroup(this);
    sceneToolGroup->setExclusive(true);

    for (QAction *tool : {m_orbitToolAction, m_sceneSelectToolAction,
                          m_sceneZoomInToolAction, m_sceneZoomOutToolAction})
    {
      sceneToolGroup->addAction(tool);

      connect(tool, &QAction::triggered, this,
              &ComposerMainWindow::onSceneToolChosen);
    }

    m_perspectiveAction = new QAction(tr("&Perspective"), this);
    m_perspectiveAction->setObjectName(QStringLiteral("perspectiveAction"));
    m_perspectiveAction->setCheckable(true);
    m_perspectiveAction->setChecked(true);

    m_orthographicAction = new QAction(tr("&Orthographic"), this);
    m_orthographicAction->setObjectName(
      QStringLiteral("orthographicAction"));
    m_orthographicAction->setCheckable(true);

    // Exclusive, because a view is drawn one way or the other and two
    // independent checkboxes would let the UI show a state the camera has no
    // way to be in.
    auto *projectionGroup = new QActionGroup(this);
    projectionGroup->setExclusive(true);
    projectionGroup->addAction(m_perspectiveAction);
    projectionGroup->addAction(m_orthographicAction);

    connect(m_perspectiveAction, &QAction::triggered, this,
            &ComposerMainWindow::onProjectionChosen);
    connect(m_orthographicAction, &QAction::triggered, this,
            &ComposerMainWindow::onProjectionChosen);

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

              captureLayers();

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
    viewMenu->addAction(m_sampleTerrainAction);
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

    basemapMenu->addSeparator();

    QAction *addService = basemapMenu->addAction(tr("Add &Web Service…"));
    addService->setObjectName(QStringLiteral("basemapAddServiceAction"));
    connect(addService, &QAction::triggered, this,
            &ComposerMainWindow::addServiceBasemap);

    viewMenu->addAction(m_selectToolAction);
    viewMenu->addAction(m_panToolAction);
    viewMenu->addAction(m_zoomInToolAction);
    viewMenu->addAction(m_zoomOutToolAction);
    viewMenu->addAction(m_transectToolAction);
    viewMenu->addAction(m_mapCrsAction);

    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomFullAction);
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_layerPropertiesAction);
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

    RibbonGroup *tools =
      m_ribbon->addGroup(QStringLiteral("map"), tr("Tools"));
    tools->addAction(m_selectToolAction, tr("Select"));
    tools->addAction(m_panToolAction, tr("Pan"));
    tools->addAction(m_zoomInToolAction, tr("Zoom In\nBox"));
    tools->addAction(m_zoomOutToolAction, tr("Zoom Out\nBox"));
    tools->addAction(m_transectToolAction, tr("Section\nLine"));

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
    symbology->addAction(m_layerPropertiesAction, tr("Layer\nProperties"));

    RibbonGroup *reference =
      m_ribbon->addGroup(QStringLiteral("map"), tr("Reference"));
    reference->addAction(m_mapCrsAction, tr("Coordinate\nSystem"));

    // Meshing gets a tab rather than a corner of the Map tab: it is its own
    // workflow, and phase E adds generation, vertical grids, terrain
    // sampling and boundary conditions to it.
    m_ribbon->addTab(QStringLiteral("mesh"), tr("Mesh"));

    RibbonGroup *domain =
      m_ribbon->addGroup(QStringLiteral("mesh"), tr("Domain"));
    domain->addAction(m_drawBoundaryAction, tr("Boundary"));
    domain->addAction(m_drawHoleAction, tr("Hole"));
    domain->addAction(m_drawBreaklineAction, tr("Break\nline"));
    domain->addAction(m_drawPointAction, tr("Point"));
    domain->addAction(m_editVerticesAction, tr("Edit"));

    RibbonGroup *elevations =
      m_ribbon->addGroup(QStringLiteral("mesh"), tr("Elevations"));
    elevations->addAction(m_sampleTerrainAction, tr("Sample"));

    // A menu on the button rather than four more faces: the Mesh tab has
    // generation, vertical grids and boundary conditions still to come, and
    // E1b-2 measured that crowding a tab changes the canvas it sits above.
    if (QToolButton *button =
          domain->addAction(m_importSelectionAction, tr("From\nSelection")))
    {
      button->setMenu(m_importSelectionMenu);
      button->setPopupMode(QToolButton::InstantPopup);
    }

    m_ribbon->addTab(QStringLiteral("scene"), tr("3D"));

    RibbonGroup *sceneTools =
      m_ribbon->addGroup(QStringLiteral("scene"), tr("Tools"));
    sceneTools->addAction(m_orbitToolAction, tr("Orbit"));
    sceneTools->addAction(m_sceneSelectToolAction, tr("Select"));
    sceneTools->addAction(m_sceneZoomInToolAction, tr("Zoom In\nBox"));
    sceneTools->addAction(m_sceneZoomOutToolAction, tr("Zoom Out\nBox"));

    RibbonGroup *projection =
      m_ribbon->addGroup(QStringLiteral("scene"), tr("Projection"));
    projection->addAction(m_perspectiveAction, tr("Perspective"));
    projection->addAction(m_orthographicAction, tr("Ortho\ngraphic"));

    RibbonGroup *relief =
      m_ribbon->addGroup(QStringLiteral("scene"), tr("Relief"));

    m_exaggerationSpin = new QDoubleSpinBox(relief);
    m_exaggerationSpin->setObjectName(QStringLiteral("exaggerationSpin"));
    m_exaggerationSpin->setPrefix(tr("Vertical ×"));
    m_exaggerationSpin->setRange(0.1, 100.0);
    m_exaggerationSpin->setSingleStep(0.5);
    m_exaggerationSpin->setValue(m_sceneView->verticalExaggeration());
    m_exaggerationSpin->setToolTip(
      tr("How much to stretch elevation, for reading low relief."));

    connect(m_exaggerationSpin, &QDoubleSpinBox::valueChanged, this,
            [this](double factor)
            {
              m_sceneView->setVerticalExaggeration(factor);
            });

    relief->addWidget(m_exaggerationSpin);

    m_ribbon->addTab(QStringLiteral("view"), tr("View"));

    RibbonGroup *appearance =
      m_ribbon->addGroup(QStringLiteral("view"), tr("Appearance"));
    appearance->addAction(m_lightAction, tr("Light"));
    appearance->addAction(m_darkAction, tr("Dark"));
    appearance->addAction(m_systemAction, tr("System"));

    ensureIcon(m_newAction, QStringLiteral("new"));
    ensureIcon(m_openAction, QStringLiteral("open"));
    ensureIcon(m_saveAction, QStringLiteral("save"));
    ensureIcon(m_loadComponentsAction, QStringLiteral("plugins"));
    ensureIcon(m_runAction, QStringLiteral("play"));
    ensureIcon(m_pauseAction, QStringLiteral("pause"));
    ensureIcon(m_stopAction, QStringLiteral("stop"));
    ensureIcon(m_undoAction, QStringLiteral("undo"));
    ensureIcon(m_redoAction, QStringLiteral("redo"));
    ensureIcon(m_lightAction, QStringLiteral("sun"));
    ensureIcon(m_darkAction, QStringLiteral("dark"));
    ensureIcon(m_systemAction, QStringLiteral("system"));
    ensureIcon(m_zoomFullAction, QStringLiteral("extent"));
    ensureIcon(m_zoomInAction, QStringLiteral("zoomin"));
    ensureIcon(m_zoomOutAction, QStringLiteral("zoomout"));
    ensureIcon(m_layerPropertiesAction, QStringLiteral("layer_styling"));

    // The globe, the same glyph the Map tab carries: a coordinate system is
    // the one thing on the ribbon that is about the earth rather than about
    // the data drawn on it.
    ensureIcon(m_mapCrsAction, QStringLiteral("globe"));
    ensureIcon(m_selectToolAction, QStringLiteral("select"));
    ensureIcon(m_panToolAction, QStringLiteral("pan"));
    ensureIcon(m_zoomInToolAction, QStringLiteral("zoom_rect"));
    ensureIcon(m_zoomOutToolAction, QStringLiteral("zoom_rect_out"));
    ensureIcon(m_transectToolAction, QStringLiteral("transect"));

    // The domain glyphs: the ring, the ring with a bite out of it, the
    // breakline with its vertices, the crosshair that forces one, and the
    // ring whose corners are drawn as the handles the editor offers.
    ensureIcon(m_drawBoundaryAction, QStringLiteral("domain_boundary"));
    ensureIcon(m_drawHoleAction, QStringLiteral("domain_hole"));
    ensureIcon(m_drawBreaklineAction, QStringLiteral("domain_breakline"));
    ensureIcon(m_drawPointAction, QStringLiteral("domain_point"));
    ensureIcon(m_editVerticesAction, QStringLiteral("domain_edit"));
    ensureIcon(m_importSelectionAction, QStringLiteral("domain_import"));

    // The 3D glyph for the projection a 3D view is normally read in, and the
    // extent rectangle for the parallel one, which is what a plan view is.
    ensureIcon(m_orbitToolAction, QStringLiteral("scene_3d"));
    ensureIcon(m_sceneSelectToolAction, QStringLiteral("select"));
    ensureIcon(m_sceneZoomInToolAction, QStringLiteral("zoom_rect"));
    ensureIcon(m_sceneZoomOutToolAction, QStringLiteral("zoom_rect_out"));
    ensureIcon(m_perspectiveAction, QStringLiteral("scene_3d"));
    ensureIcon(m_orthographicAction, QStringLiteral("extent"));
    ensureIcon(m_addVectorAction, QStringLiteral("add_vector"));
    ensureIcon(m_addRasterAction, QStringLiteral("add_raster"));
    ensureIcon(m_addComponentLayersAction, QStringLiteral("add_component_layers"));
    ensureIcon(m_addMeshAction, QStringLiteral("add_mesh"));

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

    connect(m_layerTree, &LayerTreePanel::layerPropertiesRequested, this,
            &ComposerMainWindow::onLayerProperties);

    connect(m_layerTree, &LayerTreePanel::currentLayerChanged, this,
            [this](MapLayer *layer)
            {
              m_layerPropertiesAction->setEnabled(
                LayerPropertiesDialog::canEdit(layer));
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

    // Which layers a model argument may be read from. Asked for each time the
    // button is pressed rather than snapshotted, because the layer stack
    // changes while the panel is open.
    m_configurator->setLayerSources(
      [this]
      {
        QVector<ComponentConfigurator::LayerSource> sources;

        for (int row = 0; row < m_layerStack->rowCount(QModelIndex()); ++row)
        {
          MapLayer *layer = m_layerStack->layerAt(row);

          if (!layer || layer->sourceUri().isEmpty())
          {
            continue;
          }

          sources.append({layer->name(), layer->sourceUri()});
        }

        return sources;
      });

    addDockWidget(Qt::RightDockWidgetArea, configuratorDock);

    // ── Attributes ───────────────────────────────────────────────────────
    auto *attributeDock = new QDockWidget(tr("Attributes"), this);
    attributeDock->setObjectName(QStringLiteral("attributeDock"));

    m_attributeTable = new AttributeTablePanel(attributeDock);
    m_attributeTable->setModel(m_layerStack);
    attributeDock->setWidget(m_attributeTable);

    addDockWidget(Qt::BottomDockWidgetArea, attributeDock);

    // ── Runs ─────────────────────────────────────────────────────────────
    auto *runDock = new QDockWidget(tr("Runs"), this);
    runDock->setObjectName(QStringLiteral("runDock"));

    m_runs = new RunBrowserModel(this);

    m_runBrowser = new RunBrowserPanel(runDock);
    m_runBrowser->setModel(m_runs);
    runDock->setWidget(m_runBrowser);

    connect(m_runBrowser, &RunBrowserPanel::openRunRequested, this,
            &ComposerMainWindow::onOpenRun);
    connect(m_runBrowser, &RunBrowserPanel::showItemRequested, this,
            &ComposerMainWindow::onShowRunItem);
    connect(m_runBrowser, &RunBrowserPanel::compareItemRequested, this,
            &ComposerMainWindow::onCompareRunItem);

    addDockWidget(Qt::BottomDockWidgetArea, runDock);

    // ── Plot ─────────────────────────────────────────────────────────────
    auto *plotDock = new QDockWidget(tr("Plot"), this);
    plotDock->setObjectName(QStringLiteral("plotDock"));

    m_seriesPlot = new SeriesPlotPanel(plotDock);
    m_seriesPlot->setModel(m_layerStack);
    plotDock->setWidget(m_seriesPlot);

    addDockWidget(Qt::BottomDockWidgetArea, plotDock);

    // ── Profile ──────────────────────────────────────────────────────────
    auto *profileDock = new QDockWidget(tr("Profile"), this);
    profileDock->setObjectName(QStringLiteral("profileDock"));

    m_profilePlot = new ProfilePlotPanel(profileDock);
    m_profilePlot->setModel(m_layerStack);
    profileDock->setWidget(m_profilePlot);

    addDockWidget(Qt::BottomDockWidgetArea, profileDock);

    // ── Section ──────────────────────────────────────────────────────────
    auto *transectDock = new QDockWidget(tr("Section"), this);
    transectDock->setObjectName(QStringLiteral("transectDock"));

    m_transect = new TransectPanel(transectDock);
    m_transect->setModel(m_layerStack);
    transectDock->setWidget(m_transect);

    // The map owns the line; the panel is a second view of it, so the panel
    // never reaches back to the canvas.
    connect(m_mapCanvas, &MapCanvas::transectDrawn, m_transect,
            &TransectPanel::setLine);

    addDockWidget(Qt::BottomDockWidgetArea, transectDock);

    // ── Log ──────────────────────────────────────────────────────────────
    auto *logDock = new QDockWidget(tr("Log"), this);
    logDock->setObjectName(QStringLiteral("logDock"));

    m_log = new QPlainTextEdit(logDock);
    m_log->setObjectName(QStringLiteral("logView"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    logDock->setWidget(m_log);

    addDockWidget(Qt::BottomDockWidgetArea, logDock);
    // Tabbed with the attribute table: both are things you look *down* at
    // while working on the map above, and only one at a time.
    tabifyDockWidget(attributeDock, runDock);
    tabifyDockWidget(attributeDock, plotDock);
    tabifyDockWidget(attributeDock, profileDock);
    tabifyDockWidget(attributeDock, transectDock);
    tabifyDockWidget(attributeDock, logDock);
    attributeDock->raise();
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

  void ComposerMainWindow::onSampleTerrain()
  {
    // Every mesh on the map, and every raster that could supply heights for
    // one. Both are asked for by name rather than by selection, because the
    // pair matters and choosing them one at a time in a layer tree makes the
    // pairing invisible.
    QList<MeshLayer *> meshes;
    QList<GdalRasterLayer *> rasters;

    for (int row = 0; row < m_layerStack->rowCount(QModelIndex()); ++row)
    {
      MapLayer *layer = m_layerStack->layerAt(row);

      if (auto *mesh = dynamic_cast<MeshLayer *>(layer))
      {
        meshes.append(mesh);
      }
      else if (auto *raster = dynamic_cast<GdalRasterLayer *>(layer))
      {
        rasters.append(raster);
      }
    }

    if (meshes.isEmpty() || rasters.isEmpty())
    {
      log(tr("Sampling elevations needs a mesh and a raster of heights on "
             "the map. A coverage fetched from a WCS is one; so is a GeoTIFF "
             "opened from disk."));

      return;
    }

    QStringList meshNames;

    for (const MeshLayer *mesh : meshes)
    {
      meshNames.append(mesh->name());
    }

    QStringList rasterNames;

    for (const GdalRasterLayer *raster : rasters)
    {
      rasterNames.append(raster->name());
    }

    bool accepted = false;

    const QString meshName = QInputDialog::getItem(
      this, tr("Sample Elevations"), tr("Onto which mesh?"), meshNames, 0,
      false, &accepted);

    if (!accepted)
    {
      return;
    }

    const QString rasterName = QInputDialog::getItem(
      this, tr("Sample Elevations"), tr("From which heights?"), rasterNames, 0,
      false, &accepted);

    if (!accepted)
    {
      return;
    }

    MeshLayer *mesh = meshes.at(meshNames.indexOf(meshName));
    GdalRasterLayer *raster = rasters.at(rasterNames.indexOf(rasterName));

    HydroCouple::SDK::IO::MeshDefinition sampled = mesh->mesh();

    const TerrainSampleResult result =
      sampleTerrain(*raster, mesh->crs(), sampled);

    if (!result.ok)
    {
      log(result.message);

      return;
    }

    if (!mesh->setNodeElevations(sampled.nodeZ))
    {
      log(tr("%1 changed while its elevations were being read.")
            .arg(mesh->name()));

      return;
    }

    // The count of misses is the part worth reading. A mesh usually reaches
    // a little past the ground that was fetched, and a survey usually has
    // holes in it, so "all of them" is the unusual answer rather than the
    // expected one.
    log(result.missed == 0
          ? tr("%1: all %2 vertices took elevations from %3.")
              .arg(mesh->name())
              .arg(result.sampled)
              .arg(raster->name())
          : tr("%1: %2 vertices took elevations from %3, %4 found no "
               "reading.")
              .arg(mesh->name())
              .arg(result.sampled)
              .arg(raster->name())
              .arg(result.missed));
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

  void ComposerMainWindow::installBasemap(TileLayer *basemap)
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

    if (basemap)
    {
      // The bottom of the stack, so every data layer draws over it.
      m_layerStack->insertLayer(m_layerStack->rowCount(), basemap);
    }
  }

  void ComposerMainWindow::setBasemap(const QString &providerName)
  {
    if (providerName.isEmpty())
    {
      installBasemap(nullptr);
      log(tr("Basemap removed."));
      return;
    }

    const BasemapProvider provider =
      NetworkTileSource::builtinProvider(providerName);

    auto source = std::make_unique<NetworkTileSource>(provider);
    NetworkTileSource *raw = source.get();

    auto *basemap = new TileLayer(provider.name, std::move(source));

    installBasemap(basemap);

    raw->setTileReadyCallback([basemap] { basemap->onTileReady(); });

    log(tr("Basemap: %1 — %2").arg(provider.name, provider.attribution));
  }

  void ComposerMainWindow::addServiceBasemap()
  {
    OgcServiceDialog dialog(this);

    // What the map is looking at, in longitude and latitude, so a feature
    // service is asked about that ground rather than about the whole
    // country it holds. Left null when the map is in a system that cannot
    // be converted, in which case the collection is fetched whole up to
    // its feature limit.
    if (m_mapCanvas && m_mapCanvas->crs())
    {
      QString message;
      const std::unique_ptr<SpatialReference> wgs84 =
        SpatialReference::fromAuthority(QStringLiteral("EPSG"), 4326,
                                        message);

      if (wgs84)
      {
        const std::unique_ptr<CoordinateTransform> toGeographic =
          CoordinateTransform::between(*m_mapCanvas->crs(), *wgs84, message);

        if (toGeographic)
        {
          const QRectF visible = m_mapCanvas->transform().visibleExtent();
          bool ok = true;

          const QPointF lower =
            toGeographic->transform(visible.topLeft(), &ok);
          const QPointF upper =
            toGeographic->transform(visible.bottomRight(), &ok);

          if (ok)
          {
            dialog.setPreferredExtent(QRectF(lower, upper).normalized());
          }
        }
      }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
      return;
    }

    // A feature collection is data, not a backdrop: it joins the stack as
    // a layer of its own, above whatever basemap is there, and can then be
    // selected from and imported as a mesh domain.
    if (std::unique_ptr<WfsFeatureLayer> features = dialog.takeFeatureLayer())
    {
      const QString name = features->name();
      const int count = features->featureCount();

      addMapLayer(features.release());

      log(tr("%1: %2 features from %3")
            .arg(name)
            .arg(count)
            .arg(dialog.serviceTitle()));

      return;
    }

    // A coverage is data too, and the one kind of it a model can be built
    // on: elevations rather than a picture of elevations.
    if (std::unique_ptr<WcsCoverageLayer> coverage = dialog.takeCoverageLayer())
    {
      const QString name = coverage->name();
      const QSize size = coverage->rasterSize();

      double minimum = 0.0;
      double maximum = 0.0;
      coverage->valueRange(minimum, maximum);

      addMapLayer(coverage.release());

      log(tr("%1: %2 by %3 cells, %4 to %5, from %6")
            .arg(name)
            .arg(size.width())
            .arg(size.height())
            .arg(minimum, 0, 'f', 2)
            .arg(maximum, 0, 'f', 2)
            .arg(dialog.serviceTitle()));

      return;
    }

    std::unique_ptr<OgcTileSource> source = dialog.createSource();

    if (!source)
    {
      log(tr("No layer was chosen from that service."));
      return;
    }

    const QString name = dialog.layerName();
    OgcTileSource *raw = source.get();

    auto *basemap = new TileLayer(name, std::move(source));

    // The dialog is the only place that knows the address this came from:
    // a tile source is built from a capabilities document and cannot say
    // afterwards where that document was fetched.
    basemap->setPersistentState(dialog.persistentStateForChoice());

    installBasemap(basemap);

    raw->setTileReadyCallback([basemap] { basemap->onTileReady(); });

    // A service basemap is none of the built-in providers, so none of their
    // menu entries should go on looking chosen.
    if (QAction *checked = m_basemapGroup->checkedAction())
    {
      checked->setChecked(false);
    }

    log(tr("Basemap: %1 — %2").arg(name, dialog.serviceTitle()));
  }

  void ComposerMainWindow::onLayerProperties(MapLayer *layer)
  {
    if (!LayerPropertiesDialog::canEdit(layer))
    {
      return;
    }

    // Shown from an action's triggered() or a button's clicked() — a release,
    // never a press: a modal opened from a mouse press wedges input on macOS.
    LayerPropertiesDialog dialog(layer, this);

    if (dialog.exec() == QDialog::Accepted)
    {
      log(tr("Updated %1.").arg(layer->name()));
    }
  }

  void ComposerMainWindow::onSetMapCrs()
  {
    // Shown from an action's triggered() — a release, never a press: a modal
    // opened from a mouse press wedges input on macOS.
    CrsSelectionDialog chooser(this);
    chooser.setCurrentCrs(m_mapCanvas->crs());

    if (chooser.exec() != QDialog::Accepted)
    {
      return;
    }

    QString message;
    std::shared_ptr<SpatialReference> chosen = chooser.selectedCrs(message);

    if (!chosen)
    {
      QMessageBox::warning(this, tr("Cannot use that system"), message);

      return;
    }

    const QString described =
      QStringLiteral("%1 — %2")
        .arg(chooser.selectedAuthCode(), chosen->description());

    // Nothing is converted: every layer keeps its own coordinates and is
    // reprojected as it is drawn, so changing the map's system re-draws the
    // stack rather than rewriting it.
    m_mapCanvas->setCrs(std::move(chosen));

    log(tr("Map coordinate system: %1").arg(described));
  }

  MeshDomainModel *ComposerMainWindow::meshDomain() const
  {
    return m_meshDomain;
  }

  void ComposerMainWindow::ensureDomainLayers()
  {
    if (m_domainLayersShown)
    {
      return;
    }

    m_domainLayersShown = true;

    for (const DomainPart part :
         {DomainPart::Boundary, DomainPart::Holes, DomainPart::Breaklines,
          DomainPart::ForcedPoints})
    {
      m_layerStack->addLayer(DomainLayer::create(m_meshDomain, part).release());
    }
  }

  void ComposerMainWindow::onDomainToolChosen()
  {
    // Picking up a drawing tool is the moment the user has said they want a
    // domain, so the rows to toggle appear then rather than after the first
    // shape lands.
    ensureDomainLayers();

    if (m_editVerticesAction->isChecked())
    {
      m_mapCanvas->setTool(
        std::make_unique<DomainEditTool>(m_mapCanvas, m_meshDomain));
      m_workspace->setCurrentWidget(m_mapCanvas);

      return;
    }

    DomainPart part = DomainPart::Boundary;

    if (m_drawHoleAction->isChecked())
    {
      part = DomainPart::Holes;
    }
    else if (m_drawBreaklineAction->isChecked())
    {
      part = DomainPart::Breaklines;
    }
    else if (m_drawPointAction->isChecked())
    {
      part = DomainPart::ForcedPoints;
    }

    m_mapCanvas->setTool(std::make_unique<DomainDrawTool>(
      m_mapCanvas, m_meshDomain, part));

    // Brought forward, because a gesture set is a property of a view nobody
    // can use from another tab.
    m_workspace->setCurrentWidget(m_mapCanvas);
  }

  void ComposerMainWindow::onImportSelectionAsDomainPart()
  {
    QAction *action = qobject_cast<QAction *>(sender());

    if (!action)
    {
      return;
    }

    // The layer the user is pointing at in the tree, which is the one whose
    // selection they can see. A raster or a mesh has no features to take.
    auto *layer = dynamic_cast<FeatureLayer *>(m_layerTree->currentLayer());

    if (!layer)
    {
      const QString message =
        tr("Choose a vector layer in the Layers panel, and select the "
           "features to import on the map.");

      log(message);
      QMessageBox::information(this, tr("Nothing to import"), message);

      return;
    }

    // Created before the import rather than after it: the layers are made on
    // first use, and a domain that arrives with nothing drawing it looks
    // like an import that did nothing.
    ensureDomainLayers();

    const DomainImportResult result = importSelectionAsDomainPart(
      *layer, DomainPart(action->data().toInt()), *m_meshDomain);

    log(result.message);

    if (!result.ok)
    {
      QMessageBox::information(this, tr("Nothing to import"), result.message);

      return;
    }

    m_workspace->setCurrentWidget(m_mapCanvas);
  }

  void ComposerMainWindow::onMapToolChosen()
  {
    MapToolKind kind = MapToolKind::Pan;

    if (m_selectToolAction->isChecked())
    {
      kind = MapToolKind::Select;
    }
    else if (m_zoomInToolAction->isChecked())
    {
      kind = MapToolKind::ZoomIn;
    }
    else if (m_zoomOutToolAction->isChecked())
    {
      kind = MapToolKind::ZoomOut;
    }
    else if (m_transectToolAction->isChecked())
    {
      kind = MapToolKind::Transect;
    }

    m_mapCanvas->setToolKind(kind);

    // Brought forward, because a gesture set is a property of a view nobody
    // can use from another tab.
    m_workspace->setCurrentWidget(m_mapCanvas);
  }

  void ComposerMainWindow::onSceneToolChosen()
  {
    SceneToolKind kind = SceneToolKind::Orbit;

    if (m_sceneSelectToolAction->isChecked())
    {
      kind = SceneToolKind::Select;
    }
    else if (m_sceneZoomInToolAction->isChecked())
    {
      kind = SceneToolKind::ZoomIn;
    }
    else if (m_sceneZoomOutToolAction->isChecked())
    {
      kind = SceneToolKind::ZoomOut;
    }

    m_sceneView->setToolKind(kind);

    // Brought forward, because a gesture set is a property of a view nobody
    // can use from another tab.
    m_workspace->setCurrentWidget(m_sceneView);
  }

  void ComposerMainWindow::onProjectionChosen()
  {
    m_sceneView->setProjection(m_orthographicAction->isChecked()
                                 ? CameraProjection::Orthographic
                                 : CameraProjection::Perspective);

    // Brought forward, because a projection is a property of a view nobody
    // can see the effect of from another tab.
    m_workspace->setCurrentWidget(m_sceneView);
  }

  RunBrowserPanel *ComposerMainWindow::runBrowser() const
  {
    return m_runBrowser;
  }

  RunBrowserModel *ComposerMainWindow::runs() const
  {
    return m_runs;
  }

  SeriesPlotPanel *ComposerMainWindow::seriesPlot() const
  {
    return m_seriesPlot;
  }

  TransectPanel *ComposerMainWindow::transect() const
  {
    return m_transect;
  }

  ProfilePlotPanel *ComposerMainWindow::profilePlot() const
  {
    return m_profilePlot;
  }

  bool ComposerMainWindow::openRun(const QString &manifestPath,
                                   QString &message)
  {
    const RunSession *session = m_runs->addRun(manifestPath, message);

    if (!session)
    {
      return false;
    }

    log(tr("Opened run %1 — %2 recorded item(s) from %3 component(s).")
          .arg(session->title())
          .arg(session->manifest().results.size())
          .arg(session->componentIds().size()));

    return true;
  }

  bool ComposerMainWindow::showRunItem(int runRow, const QString &componentId,
                                      const QString &itemId, QString &message)
  {
    RunSession *session = m_runs->run(runRow);

    if (!session)
    {
      message = tr("That run is no longer open.");
      return false;
    }

    HydroCouple::IComponentDataItem *item =
      session->item(componentId, itemId, message);

    if (!item)
    {
      return false;
    }

    if (!DataItemLayer::isSpatial(item))
    {
      // Said plainly rather than as a failure to draw. A recorded run may
      // hold values with no geometry at all — a CSV of a gauge, a scalar
      // series — and that is a complete recording, not a broken one.
      message = tr("\"%1\" was recorded without geometry, so there is "
                   "nothing to draw. It can still be plotted.")
                  .arg(itemId);
      return false;
    }

    std::unique_ptr<DataItemLayer> layer = DataItemLayer::create(item, message);

    if (!layer)
    {
      return false;
    }

    // The run is in the name because two runs of the same model produce two
    // layers of the same component and item, and comparing them is the whole
    // point of holding two runs open.
    layer->setName(tr("%1 — %2 — %3")
                     .arg(session->title(), componentId, layer->name()));

    DataItemLayer *added = layer.release();
    m_layerStack->addLayer(added);

    log(tr("Added %1 (%2 feature(s), %3 recorded time(s)).")
          .arg(added->name())
          .arg(added->featureCount())
          .arg(added->timeCount()));

    // Brought forward, because a layer added from a dock at the bottom of the
    // window is otherwise drawn behind whichever tab happens to be showing.
    m_workspace->setCurrentWidget(m_mapCanvas);
    m_mapCanvas->zoomToLayer(added);

    return true;
  }

  bool ComposerMainWindow::compareRunItem(int runRow, int otherRunRow,
                                          const QString &componentId,
                                          const QString &itemId,
                                          QString &message)
  {
    RunSession *base = m_runs->run(runRow);
    RunSession *other = m_runs->run(otherRunRow);

    if (!base || !other)
    {
      message = tr("That run is no longer open.");
      return false;
    }

    if (base == other)
    {
      message = tr("A run compared against itself is zero everywhere. Pick "
                   "a different run.");
      return false;
    }

    HydroCouple::IComponentDataItem *baseItem =
      base->item(componentId, itemId, message);

    if (!baseItem)
    {
      return false;
    }

    HydroCouple::IComponentDataItem *otherItem =
      other->item(componentId, itemId, message);

    if (!otherItem)
    {
      return false;
    }

    std::unique_ptr<DifferenceLayer> layer =
      DifferenceLayer::create(baseItem, otherItem, message);

    if (!layer)
    {
      return false;
    }

    // Both runs in the name, in the order they are subtracted: a difference
    // map whose title does not say which way round it was taken is a map
    // whose sign nobody can read.
    layer->setName(tr("%1: %2 − %3")
                     .arg(itemId, base->title(), other->title()));

    DifferenceLayer *added = layer.release();
    m_layerStack->addLayer(added);

    log(tr("Comparing %1 (%2 feature(s), %3 recorded time(s)).")
          .arg(added->name())
          .arg(added->featureCount())
          .arg(added->timeCount()));

    m_workspace->setCurrentWidget(m_mapCanvas);
    m_mapCanvas->zoomToLayer(added);

    return true;
  }

  void ComposerMainWindow::onCompareRunItem(int runRow,
                                            const QString &componentId,
                                            const QString &itemId)
  {
    QVector<int> candidates = m_runs->runsCarrying(componentId, itemId);
    candidates.removeAll(runRow);

    if (candidates.isEmpty())
    {
      QMessageBox::information(
        this, tr("Nothing to compare against"),
        tr("No other open run recorded “%1”. Open a run that did, and try "
           "again.")
          .arg(itemId));
      return;
    }

    int chosen = candidates.first();

    if (candidates.size() > 1)
    {
      QStringList titles;

      for (const int row : candidates)
      {
        titles.append(m_runs->run(row)->title());
      }

      bool accepted = false;

      // From a button's clicked() — a release, never a press: a modal
      // opened from a mouse press wedges input on macOS.
      const QString picked = QInputDialog::getItem(
        this, tr("Compare against"),
        tr("Subtract which run from “%1”?").arg(m_runs->run(runRow)->title()),
        titles, 0, false, &accepted);

      if (!accepted)
      {
        return;
      }

      chosen = candidates.at(titles.indexOf(picked));
    }

    QString message;

    if (!compareRunItem(runRow, chosen, componentId, itemId, message))
    {
      QMessageBox::warning(this, tr("Cannot compare that item"), message);
    }
  }

  void ComposerMainWindow::onShowRunItem(int runRow, const QString &componentId,
                                         const QString &itemId)
  {
    QString message;

    if (!showRunItem(runRow, componentId, itemId, message))
    {
      QMessageBox::warning(this, tr("Cannot show that item"), message);
    }
  }

  void ComposerMainWindow::onOpenRun()
  {
    // From a button's clicked() — a release, never a press: a modal opened
    // from a mouse press wedges input on macOS.
    const QString path = QFileDialog::getOpenFileName(
      this, tr("Open run manifest"), QString(),
      tr("Run manifests (*.json);;All files (*)"));

    if (path.isEmpty())
    {
      return;
    }

    QString message;

    if (!openRun(path, message))
    {
      QMessageBox::warning(this, tr("Cannot open that run"), message);
    }
  }

  void ComposerMainWindow::createStatusBar()
  {
    statusBar()->showMessage(
      tr("HydroCouple Composer %1").arg(ComposerApplication::versionString()));

    m_mapStatus = new MapStatusBar(statusBar());
    m_mapStatus->setCanvas(m_mapCanvas);

    connect(m_mapStatus, &MapStatusBar::crsRequested, this,
            &ComposerMainWindow::onSetMapCrs);

    // A permanent widget, so a transient status message cannot cover the
    // readouts that have to stay put while the pointer moves.
    statusBar()->addPermanentWidget(m_mapStatus);

    // Live only where they mean something. A perspective camera has no
    // single scale, so on the other tabs the controls go dead rather than
    // show a number that quietly means nothing.
    const auto followTab = [this]
    { m_mapStatus->setLive(m_workspace->currentWidget() == m_mapCanvas); };

    connect(m_workspace, &QTabWidget::currentChanged, this,
            [followTab](int) { followTab(); });

    followTab();
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

    restoreLayers();

    return true;
  }

  void ComposerMainWindow::captureLayers()
  {
    QJsonArray layers;

    for (int row = 0; row < m_layerStack->rowCount(QModelIndex()); ++row)
    {
      const MapLayer *layer = m_layerStack->layerAt(row);

      if (!layer)
      {
        continue;
      }

      QJsonObject state = layer->persistentState();

      // An empty recipe means the layer is not one a reopened composition
      // rebuilds -- a mesh domain, a component's data item. Those have
      // their own homes in the sidecar or come back with the components.
      if (state.isEmpty())
      {
        continue;
      }

      // Recorded at save rather than at creation, because both can be
      // changed after the layer is added.
      state.insert(QStringLiteral("name"), layer->name());
      state.insert(QStringLiteral("visible"), layer->isVisible());

      layers.append(state);
    }

    m_document->presentation().setLayers(layers);
  }

  void ComposerMainWindow::restoreLayers()
  {
    const QJsonArray layers = m_document->presentation().layers();

    if (layers.isEmpty())
    {
      return;
    }

    if (!m_layerRestorer)
    {
      m_layerRestorer = new LayerRestorer(this);
    }

    m_layerRestorer->restore(
      layers, [this](MapLayer *layer) { m_layerStack->addLayer(layer); },
      [this](const QString &message) {
        // Reported rather than fatal. A composition whose basemap service is
        // down is still the composition, and refusing to open it would cost
        // far more than the basemap did.
        log(message);
      });
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
