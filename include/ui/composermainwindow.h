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
#include "configurator/componentconfigurator.h"
#include "plugins/componentregistry.h"
#include "project/componentinstances.h"
#include "project/compositiondocument.h"
#include "simulation/simulationmanager.h"

#include <QMainWindow>

#include <memory>

class QAction;
class QActionGroup;
class QDoubleSpinBox;
class QLabel;
class QListView;
class QPlainTextEdit;
class QTabWidget;

namespace HydroCouple::Composer
{
  class LayerStackModel;
  class AttributeTablePanel;
  class LayerTreePanel;
  class MapCanvas;
  class SceneView;
  class MapLayer;
  class RibbonBar;
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
      void onProjectionChosen();
      void onAddVectorLayer();
      void onAddRasterLayer();
      void onAddComponentLayers();
      void onAddMeshLayer();
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

      /*!
       * \brief Puts \a layer on the map and frames it.
       * \param layer The layer to add; ownership passes to the stack.
       */
      void addMapLayer(MapLayer *layer);

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
      ComponentPaletteModel *m_paletteModel = nullptr;
      QListView *m_paletteView = nullptr;
      QPlainTextEdit *m_log = nullptr;

      LayerStackModel *m_layerStack = nullptr;
      MapCanvas *m_mapCanvas = nullptr;
      SceneView *m_sceneView = nullptr;
      LayerTreePanel *m_layerTree = nullptr;
      AttributeTablePanel *m_attributeTable = nullptr;
      QLabel *m_coordinateLabel = nullptr;

      RibbonBar *m_ribbon = nullptr;

      QAction *m_newAction = nullptr;
      QAction *m_openAction = nullptr;
      QAction *m_saveAction = nullptr;
      QAction *m_undoAction = nullptr;
      QAction *m_redoAction = nullptr;
      QAction *m_loadComponentsAction = nullptr;
      QAction *m_lightAction = nullptr;
      QAction *m_darkAction = nullptr;
      QAction *m_systemAction = nullptr;
      QAction *m_runAction = nullptr;
      QAction *m_pauseAction = nullptr;
      QAction *m_stopAction = nullptr;
      QAction *m_zoomFullAction = nullptr;
      QAction *m_zoomInAction = nullptr;
      QAction *m_zoomOutAction = nullptr;
      QAction *m_layerPropertiesAction = nullptr;
      QAction *m_mapCrsAction = nullptr;
      QAction *m_perspectiveAction = nullptr;
      QAction *m_orthographicAction = nullptr;
      QDoubleSpinBox *m_exaggerationSpin = nullptr;
      QAction *m_addVectorAction = nullptr;
      QAction *m_addRasterAction = nullptr;
      QAction *m_addComponentLayersAction = nullptr;
      QAction *m_addMeshAction = nullptr;
      QActionGroup *m_basemapGroup = nullptr;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_UI_COMPOSERMAINWINDOW_H
