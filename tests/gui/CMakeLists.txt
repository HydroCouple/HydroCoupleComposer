# Offscreen GUI test suite.
#
# Every test links composer_core and constructs real widgets under the
# offscreen QPA platform. Link-level-only tests are deliberately avoided: a
# suite that never instantiates a widget cannot observe the bugs this program
# cares about.

find_package(GTest CONFIG REQUIRED)

# One QApplication per process is a Qt hard requirement, so each test binary
# owns its own main() via gtest_main and constructs the app in a fixture.
function(composer_add_gui_test _name)
    add_executable(${_name} ${_name}.cpp)
    # Qt6::Test supplies QSignalSpy, which is how a test observes signals
    # without inventing its own capture harness.
    target_link_libraries(${_name}
        PRIVATE composer_core Qt6::Test GTest::gtest GTest::gtest_main)
    add_test(NAME ${_name} COMMAND ${_name})
    set_tests_properties(${_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120)
endfunction()

composer_add_gui_test(test_shell_smoke)
composer_add_gui_test(test_qtbridge)
composer_add_gui_test(test_spatialreference)

# C1b — the map substrate. The transform suite is pure arithmetic; the other
# two render for real, because draw order and opacity are not observable from
# call counts alone.
composer_add_gui_test(test_maptransform)
composer_add_gui_test(test_layerstack)
composer_add_gui_test(test_mapcanvas)
composer_add_gui_test(test_maptools)

# C1c — styling. The ramp and classification suites are value-type maths and
# need no widgets; the labelling suite renders, because a halo that is not
# drawn is only observable in pixels.
composer_add_gui_test(test_styling)
composer_add_gui_test(test_labelling)
composer_add_gui_test(test_crs)

composer_add_gui_test(test_runbrowser)
target_compile_definitions(test_runbrowser PRIVATE
    COMPOSER_RESULTS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/results"
    COMPOSER_SDK_REOPEN_MANIFEST="${COMPOSER_SDK_REOPEN_MANIFEST}")
composer_add_gui_test(test_mapstatusbar)
composer_add_gui_test(test_layerproperties)
target_compile_definitions(test_layerproperties PRIVATE
    COMPOSER_GIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/gis")

# C1d — basemaps and GDAL import. The tile-grid suite is arithmetic; the
# layer suites read real datasets. The raster fixture is generated at test
# time into the source tree, next to the hand-authored vector ones, so a
# failure can be inspected rather than guessed at.
composer_add_gui_test(test_tilegrid)
composer_add_gui_test(test_tilelayer)
composer_add_gui_test(test_ogctilesource)
target_compile_definitions(test_ogctilesource PRIVATE
    COMPOSER_OGC_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/ogc")
composer_add_gui_test(test_wfsfeaturelayer)
composer_add_gui_test(test_wcscoveragelayer)
composer_add_gui_test(test_layerpersistence)
composer_add_gui_test(test_terrainsampler)
target_compile_definitions(test_terrainsampler PRIVATE
    COMPOSER_OGC_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/ogc")
target_compile_definitions(test_layerpersistence PRIVATE
    COMPOSER_OGC_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/ogc")
target_compile_definitions(test_wcscoveragelayer PRIVATE
    COMPOSER_OGC_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/ogc")
composer_add_gui_test(test_ogcservicedialog)
target_compile_definitions(test_ogcservicedialog PRIVATE
    COMPOSER_OGC_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/ogc")
composer_add_gui_test(test_gdallayers)

# C2 — component data items and meshes. Built on the SDK's own data-item
# bases, exactly as a component would be.
composer_add_gui_test(test_dataitemlayers)
composer_add_gui_test(test_timecontroller)
composer_add_gui_test(test_timecontrols)
composer_add_gui_test(test_seriesplot)
# D4a — one variable, two runs, drawn as the difference between them.
composer_add_gui_test(test_difference)

# E1a — the ground a mesh will be generated over. Writes its round-tripped
# sidecar next to the other fixtures, so what it checked can be opened.
composer_add_gui_test(test_meshdomain)

# E1b-1 — the domain on the map: four homogeneous layers over one model.
composer_add_gui_test(test_domainlayer)

# E1b-2 — drawing the domain's parts, driven through real mouse events.
composer_add_gui_test(test_domaindraw)

# E1b-3 — the reach that decides what was clicked, and the drag, insert and
# remove gestures built on it.
composer_add_gui_test(test_domainedit)

# E1c — features already on the map become part of the domain. Reads the
# hand-authored GeoJSON beside the other mesh fixtures, so what a gate
# checked can be opened and looked at.
composer_add_gui_test(test_domainimport)
target_compile_definitions(test_domainimport PRIVATE
    COMPOSER_MESH_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/mesh")

# E2a — meshing the domain. Data in, data out: the gate is that what comes
# back is the SDK's own mesh, so the suite calls the SDK directly to compare
# against and needs no widgets.
composer_add_gui_test(test_meshgenerate)
target_compile_definitions(test_meshdomain PRIVATE
    COMPOSER_MESH_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/mesh")
# D3d — a section line cut through a layered mesh. The slice arithmetic, the
# layer's answer, the pixels the panel paints and the gesture that draws it.
composer_add_gui_test(test_transect)
# The export suite writes real CSV and .dat files, next to the run fixtures,
# so what it checked can be opened and read rather than taken on trust.
target_compile_definitions(test_seriesplot PRIVATE
    COMPOSER_RESULTS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/results")
target_compile_definitions(test_dataitemlayers PRIVATE
    COMPOSER_GIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/gis")
target_compile_definitions(test_gdallayers PRIVATE
    COMPOSER_GIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/gis")

# The shared SVG chrome set. Renders every icon for real, because a missing
# or untintable one is only visible in pixels.
composer_add_gui_test(test_icons)

# C3a — the 3D scene. The camera suite is arithmetic and needs no device; the
# scene suite renders for real, through a standalone QRhi rather than through
# QRhiWidget, because QRhiWidget cannot create a device under the offscreen
# platform plugin every test here runs under.
composer_add_gui_test(test_camera)
composer_add_gui_test(test_scene3d)
target_compile_definitions(test_scene3d PRIVATE
    COMPOSER_GIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/gis")

# C3b — sigma-layered meshes as prismatic cells, and peeling.
composer_add_gui_test(test_layeredmesh)
target_compile_definitions(test_layeredmesh PRIVATE
    COMPOSER_GIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/gis")

# C3c — vector layers composed against a terrain: draping and extrusion.
composer_add_gui_test(test_drape)
target_compile_definitions(test_drape PRIVATE
    COMPOSER_GIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/gis")

# C3c-2 — image layers as the ground they are a picture of. Renders for real:
# a texture mapping is only observable in pixels.
composer_add_gui_test(test_groundplane)
target_link_libraries(test_groundplane PRIVATE GDAL::GDAL)
target_compile_definitions(test_groundplane PRIVATE
    COMPOSER_GIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/gis")

# C3c-3 — switching between the map and the 3D view. Drives the real main
# window: what is under test is the tab change, not the camera arithmetic.
composer_add_gui_test(test_viewhandoff)

# C4 — identifying what is under a click. Drives the canvas with real mouse
# events: telling a click from a pan is not observable from the hit test.
composer_add_gui_test(test_picking)

# C4c — the attribute table. Driven through its real widgets: what is under
# test is the wiring, not the model.
composer_add_gui_test(test_attributetable)

# The document tests round-trip the SDK's own example composition, so they need
# to know where the installed SDK examples live.
composer_add_gui_test(test_compositiondocument)
target_compile_definitions(test_compositiondocument PRIVATE
    COMPOSER_SDK_EXAMPLE_COMPOSITION="${COMPOSER_SDK_EXAMPLE_COMPOSITION}")

composer_add_gui_test(test_compositioncanvas)
target_compile_definitions(test_compositioncanvas PRIVATE
    COMPOSER_FIXTURE_DIR="$<TARGET_FILE_DIR:testcomponent>")
add_dependencies(test_compositioncanvas testcomponent testadapterfactory)

composer_add_gui_test(test_httpuriresolver)
target_compile_definitions(test_httpuriresolver PRIVATE
    COMPOSER_CONFIGURATOR_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/configurator")

composer_add_gui_test(test_configurator)
target_compile_definitions(test_configurator PRIVATE
    COMPOSER_FIXTURE_DIR="$<TARGET_FILE_DIR:testcomponent>"
    COMPOSER_CONFIGURATOR_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/configurator")
add_dependencies(test_configurator testcomponent)

composer_add_gui_test(test_adapterinspector)
target_compile_definitions(test_adapterinspector PRIVATE
    COMPOSER_FIXTURE_DIR="$<TARGET_FILE_DIR:testcomponent>")
add_dependencies(test_adapterinspector testcomponent)

composer_add_gui_test(test_simulationmanager)
target_compile_definitions(test_simulationmanager PRIVATE
    COMPOSER_FIXTURE_DIR="$<TARGET_FILE_DIR:testcomponent>")
add_dependencies(test_simulationmanager testcomponent testadapterfactory)

composer_add_gui_test(test_hcpimporter)
target_compile_definitions(test_hcpimporter PRIVATE
    COMPOSER_LEGACY_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/legacy")

# The loader tests need the fixture libraries built first, and need to be told
# where they landed.
composer_add_gui_test(test_component_loader)
target_compile_definitions(test_component_loader PRIVATE
    COMPOSER_FIXTURE_DIR="$<TARGET_FILE_DIR:testcomponent>")
add_dependencies(test_component_loader
    testcomponent testadapterfactory notacomponent badabicomponent
    legacycomponent)

# ── Headless CLI (B4) ───────────────────────────────────────────────────────
# Drives the shipped binary as a subprocess, because the property under test is
# that the application itself runs a composition without a display — something
# a library-level test cannot demonstrate.
add_test(NAME test_headless_run
    COMMAND ${CMAKE_COMMAND}
        -DCOMPOSER_BINARY=$<TARGET_FILE:HydroCoupleComposer>
        -DFIXTURE_DIR=$<TARGET_FILE_DIR:testcomponent>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/headless
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_headless.cmake)
set_tests_properties(test_headless_run PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 180)
