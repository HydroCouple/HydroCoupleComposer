/*!
 * \file   composermainwindow.h
 * \author Caleb Buahin
 * \brief  ComposerMainWindow — the application shell.
 *
 * Assembles the pieces built in phases A and B into one window: the component
 * palette and the composition canvas, the argument configurator, the run
 * controls and a diagnostics log. The window owns the session's document,
 * registry, live instances and simulation manager, and wires them together;
 * it holds no composition state of its own.
 */

#ifndef HYDROCOUPLECOMPOSER_UI_COMPOSERMAINWINDOW_H
#define HYDROCOUPLECOMPOSER_UI_COMPOSERMAINWINDOW_H

#include "canvas/componentpalettemodel.h"
#include "canvas/compositioncanvas.h"
#include "configurator/adapterinspector.h"
#include "configurator/componentconfigurator.h"
#include "plugins/componentregistry.h"
#include "project/componentinstances.h"
#include "project/compositiondocument.h"
#include "simulation/executionpanel.h"
#include "simulation/simulationmanager.h"
#include "results/runbrowsermodel.h"
#include "ui/panels/mapstatusbar.h"
#include "ui/recentcompositions.h"
#include "ui/welcomepage.h"
#include "ui/panels/runbrowserpanel.h"

#include <QMainWindow>

#include <memory>

class QAction;
class QActionGroup;
class QDoubleSpinBox;
class QLabel;
class QMenu;
class QListView;
class QPlainTextEdit;
class QTabWidget;

namespace HydroCouple::Composer
{
  class LayerRestorer;
  class LayerStackModel;
  class AttributeTablePanel;
  class LayerTreePanel;
  class MapCanvas;
  class SceneView;
  class MapLayer;
  class TileLayer;
  class RibbonBar;
  class ProfilePlotPanel;
  class TransectPanel;
  class MeshDomainModel;
  class SeriesPlotPanel;
  class TimeController;
  class TimeControlPanel;
}

namespace HydroCouple::Composer
{

  /*!
   * \brief The main application window.
   */
  class ComposerMainWindow : public QMainWindow
  {
      Q_OBJECT

    public:
      explicit ComposerMainWindow(QWidget *parent = nullptr);

      ~ComposerMainWindow() override;

      /*!
       * \brief The session's composition document.
       */
      [[nodiscard]] CompositionDocument *document() const;

      /*!
       * \brief The session's component registry.
       */
      [[nodiscard]] ComponentRegistry *registry() const;

      /*!
       * \brief The composition canvas.
       */
      [[nodiscard]] CompositionCanvas *canvas() const;

      /*!
       * \brief The argument configurator.
       */
      [[nodiscard]] ComponentConfigurator *configurator() const;

      /*!
       * \brief The adapter/connection inspector.
       */
      [[nodiscard]] AdapterInspector *adapterInspector() const;

      /*!
       * \brief The workflow/execution panel.
       */
      [[nodiscard]] ExecutionPanel *executionPanel() const;

      /*!
       * \brief The map view.
       */
      [[nodiscard]] MapCanvas *mapCanvas() const;

      /*!
       * \brief The session's layer stack.
       *
       * The single owner of the map's layers: the canvas and the layer tree
       * are both views of it.
       */
      [[nodiscard]] LayerStackModel *layerStack() const;

      /*!
       * \brief The layer tree panel.
       */
      [[nodiscard]] LayerTreePanel *layerTree() const;

      /*!
       * \brief The attribute table panel.
       */
      [[nodiscard]] AttributeTablePanel *attributeTable() const;

      /*!
       * \brief The run browser panel.
       */
      [[nodiscard]] RunBrowserPanel *runBrowser() const;

      /*!
       * \brief The open runs.
       */
      [[nodiscard]] RunBrowserModel *runs() const;

      //! \returns The plot of what the selection recorded.
      [[nodiscard]] SeriesPlotPanel *seriesPlot() const;

      //! \returns The vertical profile of the selected column.
      [[nodiscard]] ProfilePlotPanel *profilePlot() const;

      //! \returns The section cut by the line drawn on the map.
      [[nodiscard]] TransectPanel *transect() const;

      //! \returns The mesh domain being drawn; never null.
      [[nodiscard]] MeshDomainModel *meshDomain() const;

      /*!
       * \brief Draws one item as its difference between two runs.
       *
       * \param runRow The run to subtract from.
       * \param otherRunRow The run to subtract.
       * \param componentId The component that recorded it in both.
       * \param itemId The item's identifier.
       * \param[out] message Diagnostic on failure.
       * \returns True when the comparison was drawn.
       */
      bool compareRunItem(int runRow, int otherRunRow,
                          const QString &componentId, const QString &itemId,
                          QString &message);

      /*!
       * \brief Draws one recorded item of an open run on the map.
       *
       * The bridge between browsing a run and seeing it: the catalog row
       * names an item, and this is what turns that name into a layer.
       *
       * \param runRow Which open run it belongs to.
       * \param componentId The component that recorded it.
       * \param itemId The item's identifier.
       * \param[out] message Diagnostic on failure.
       * \returns True when a layer was added.
       */
      bool showRunItem(int runRow, const QString &componentId,
                       const QString &itemId, QString &message);

      /*!
       * \brief Opens a finished run's manifest.
       * \param manifestPath The run manifest to read.
       * \param[out] message Diagnostic on failure.
       * \returns True when the run was added.
       */
      bool openRun(const QString &manifestPath, QString &message);

      /*!
       * \brief The 3D view of the same layer stack.
       */
      [[nodiscard]] SceneView *sceneView() const;

      /*!
       * \brief Loads component libraries from a directory into the palette.
       * \param directoryPath Directory to scan.
       * \returns The number of libraries loaded.
       */
      int loadComponentDirectory(const QString &directoryPath);

      /*!
       * \brief Opens a composition document.
       * \param filePath Document to open.
       * \param[out] message Diagnostic on failure.
       */
      bool openComposition(const QString &filePath, QString &message);

      /*!
       * \brief Appends a line to the diagnostics log.
       * \param text The line to append.
       */
      void log(const QString &text);

    private Q_SLOTS:
      void onSelectionChanged();
      void onLayerProperties(HydroCouple::Composer::MapLayer *layer);
      void onSetMapCrs();
      void onPreferences();
      void onOpenRun();

      //! Draws the recorded item the run browser asked for.
      void onShowRunItem(int runRow, const QString &componentId,
                         const QString &itemId);

      //! Asks which other run to compare against, then draws the difference.
      void onCompareRunItem(int runRow, const QString &componentId,
                            const QString &itemId);

      //! Installs the drawing tool for whichever domain action is checked.
      void onDomainToolChosen();

      /*!
       * \brief Adds the current layer's selected features to the domain.
       *
       * Which part they become is carried on the action that sent this, so
       * the four menu entries are one handler rather than four.
       */
      void onImportSelectionAsDomainPart();

      /*!
       * \brief Adds the four layers that show the domain, once.
       *
       * Called when a domain first has something in it, or when the user
       * picks up a tool to draw one — not at startup, because four empty
       * rows in every composition that never meshes anything is clutter,
       * and empty layers still join the extent every view frames.
       */
      void ensureDomainLayers();
      void onMapToolChosen();
      void onProjectionChosen();
      void onSceneToolChosen();
      void onAddVectorLayer();
      void onAddRasterLayer();
      void onAddComponentLayers();
      void onAddMeshLayer();

      //! Reads a raster layer's values onto a mesh's vertices.
      void onSampleTerrain();
      void onRun();
      void onPause();
      void onStop();
      void onSimulationState(HydroCouple::Composer::SimulationState state);

    private:
      void createActions();
      void createMenus();
      void createToolBar();
      void createDocks();
      void createMapView();

      //! Hands the remembered library directories to the registry and,
      //! when the preference says so, scans them.
      void applyComponentSearchPaths();

      /*!
       * \brief Puts \a layer on the map and frames it.
       * \param layer The layer to add; ownership passes to the stack.
       */
      void addMapLayer(MapLayer *layer);

      //! Records every rebuildable layer into the sidecar, before saving.
      void captureLayers();

      //! Rebuilds what the sidecar recorded, after opening.
      void restoreLayers();

      //! Refills File ▸ Open Recent from the remembered documents.
      void rebuildRecentMenu();

      /*!
       * \brief Adds a layer for every spatial data item in the composition.
       * \returns How many layers were added.
       */
      int addComponentLayers();

      /*!
       * \brief Switches the basemap to \a providerName, or removes it.
       * \param providerName Provider name, or empty for none.
       */
      void setBasemap(const QString &providerName);

      /*!
       * \brief Asks for a web map service and makes a basemap of it.
       *
       * The dialog does the asking; this replaces whatever basemap is
       * there with what came back, exactly as choosing a built-in one
       * does.
       */
      void addServiceBasemap();

      /*!
       * \brief Puts \a basemap at the bottom of the stack, alone.
       *
       * At most one basemap: two backdrops would fight for the same pixels
       * and the upper one would simply win.
       *
       * \param basemap The layer, or nullptr to leave none.
       */
      void installBasemap(TileLayer *basemap);

      void createStatusBar();
      void refreshTitle();

      CompositionDocument *m_document = nullptr;
      ComponentRegistry *m_registry = nullptr;
      std::unique_ptr<ComponentInstances> m_instances;
      std::unique_ptr<SimulationManager> m_simulation;

      QTabWidget *m_workspace = nullptr;

      CompositionScene *m_scene = nullptr;
      CompositionCanvas *m_canvas = nullptr;
      ComponentConfigurator *m_configurator = nullptr;
      AdapterInspector *m_adapterInspector = nullptr;
      ExecutionPanel *m_executionPanel = nullptr;
      ComponentPaletteModel *m_paletteModel = nullptr;
      QListView *m_paletteView = nullptr;
      QPlainTextEdit *m_log = nullptr;

      LayerStackModel *m_layerStack = nullptr;

      LayerRestorer *m_layerRestorer = nullptr;
      MapCanvas *m_mapCanvas = nullptr;
      SceneView *m_sceneView = nullptr;
      LayerTreePanel *m_layerTree = nullptr;
      AttributeTablePanel *m_attributeTable = nullptr;
      RunBrowserPanel *m_runBrowser = nullptr;
      RunBrowserModel *m_runs = nullptr;
      MapStatusBar *m_mapStatus = nullptr;
      TimeController *m_clock = nullptr;
      TimeControlPanel *m_timeControls = nullptr;
      SeriesPlotPanel *m_seriesPlot = nullptr;
      ProfilePlotPanel *m_profilePlot = nullptr;
      TransectPanel *m_transect = nullptr;
      MeshDomainModel *m_meshDomain = nullptr;
      bool m_domainLayersShown = false;

      RibbonBar *m_ribbon = nullptr;

      QAction *m_newAction = nullptr;
      QAction *m_openAction = nullptr;
      QAction *m_saveAction = nullptr;
      QAction *m_undoAction = nullptr;
      QAction *m_redoAction = nullptr;
      QAction *m_layoutAction = nullptr;
      QMenu *m_recentMenu = nullptr;
      RecentCompositions *m_recent = nullptr;
      WelcomePage *m_welcome = nullptr;
      QAction *m_loadComponentsAction = nullptr;
      QAction *m_lightAction = nullptr;
      QAction *m_darkAction = nullptr;
      QAction *m_systemAction = nullptr;
      QAction *m_preferencesAction = nullptr;
      QAction *m_runAction = nullptr;
      QAction *m_pauseAction = nullptr;
      QAction *m_stopAction = nullptr;
      QAction *m_zoomFullAction = nullptr;
      QAction *m_zoomInAction = nullptr;
      QAction *m_zoomOutAction = nullptr;
      QAction *m_layerPropertiesAction = nullptr;
      QAction *m_mapCrsAction = nullptr;
      QAction *m_selectToolAction = nullptr;
      QAction *m_panToolAction = nullptr;
      QAction *m_zoomInToolAction = nullptr;
      QAction *m_zoomOutToolAction = nullptr;
      QAction *m_transectToolAction = nullptr;
      QAction *m_drawBoundaryAction = nullptr;
      QAction *m_drawHoleAction = nullptr;
      QAction *m_drawBreaklineAction = nullptr;
      QAction *m_drawPointAction = nullptr;
      QAction *m_editVerticesAction = nullptr;

      //! Hosts the menu of parts; never triggered itself.
      QAction *m_importSelectionAction = nullptr;
      QMenu *m_importSelectionMenu = nullptr;
      QAction *m_orbitToolAction = nullptr;
      QAction *m_sceneSelectToolAction = nullptr;
      QAction *m_sceneZoomInToolAction = nullptr;
      QAction *m_sceneZoomOutToolAction = nullptr;
      QAction *m_perspectiveAction = nullptr;
      QAction *m_orthographicAction = nullptr;
      QDoubleSpinBox *m_exaggerationSpin = nullptr;
      QAction *m_addVectorAction = nullptr;
      QAction *m_addRasterAction = nullptr;
      QAction *m_addComponentLayersAction = nullptr;
      QAction *m_addMeshAction = nullptr;
      QAction *m_sampleTerrainAction = nullptr;
      QActionGroup *m_basemapGroup = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_COMPOSERMAINWINDOW_H
