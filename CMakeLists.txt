# HydroCoupleComposer 2.0 — graphical composition editor, configurator, and
# model-agnostic results viewer for HydroCouple 2.0 model components.
#
# Author  Caleb Amoa Buahin
# License GNU Lesser General Public License
#
# Build scaffolding follows openswmm.gui: CMake + vcpkg manifest + per-OS
# presets. HydroCouple and HydroCoupleSDK are consumed as prebuilt IMPORTED
# packages (find_package) exactly as openswmm.gui consumes openswmm.engine —
# build and install those first (see README).

cmake_minimum_required(VERSION 3.21)

project(
    HydroCoupleComposer
    VERSION 2.0.0
    DESCRIPTION "Graphical composition editor and results viewer for HydroCouple 2.0"
    HOMEPAGE_URL "https://www.hydrocouple.org"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/forms")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(COMPOSER_BUILD_TESTS "Build the offscreen GUI test suite" ON)
option(COMPOSER_BUILD_DOCS  "Build Doxygen documentation" OFF)

include(GNUInstallDirs)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Qt root discovery from env ──────────────────────────────────────────────
# CMakePresets inject CMAKE_PREFIX_PATH from $env{QT_ROOT_DIR}. Replicate that
# for plain `cmake -B build` invocations so a developer with QT_ROOT_DIR (or
# Qt6_DIR / QTDIR) exported can configure without a preset. list(PREPEND)
# keeps command-line overrides winning when both mechanisms are active.
foreach(_qt_env_var IN ITEMS QT_ROOT_DIR Qt6_DIR QTDIR)
    if(DEFINED ENV{${_qt_env_var}} AND NOT "$ENV{${_qt_env_var}}" STREQUAL "")
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{${_qt_env_var}}")
        message(STATUS "Qt: picking up $ENV{${_qt_env_var}} from env var ${_qt_env_var}")
        break()
    endif()
endforeach()
unset(_qt_env_var)

# ── Qt ──────────────────────────────────────────────────────────────────────
# Qt6 only: the v2 rendering path (C3) needs the Qt6 scene graph and qsb-
# compiled materials, which have no Qt5 equivalent.
find_package(Qt6 REQUIRED
    COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network Concurrent
               Svg Charts Quick QuickWidgets Qml PrintSupport Xml
               ShaderTools Test)

qt_standard_project_setup()

# ── HydroCouple interfaces + SDK (prebuilt IMPORTED packages) ───────────────
# Sibling-checkout install trees are searched first, mirroring the
# openswmm.gui → openswmm.engine convention (../<repo>/install/<system>).
set(_hc_sibling_hints
    "${CMAKE_CURRENT_SOURCE_DIR}/../HydroCouple/install/${CMAKE_SYSTEM_NAME}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../HydroCouple/install")
set(_sdk_sibling_hints
    "${CMAKE_CURRENT_SOURCE_DIR}/../HydroCoupleSDK/install/${CMAKE_SYSTEM_NAME}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../HydroCoupleSDK/install")

find_package(HydroCouple CONFIG REQUIRED HINTS ${_hc_sibling_hints})
find_package(HydroCoupleSDK CONFIG REQUIRED HINTS ${_sdk_sibling_hints})

message(STATUS "HydroCouple    : ${HydroCouple_DIR}")
message(STATUS "HydroCoupleSDK : ${HydroCoupleSDK_DIR}")

# ── GDAL (GIS/CRS — Phase C) ────────────────────────────────────────────────
find_package(GDAL REQUIRED)

# ── nanoflann (header-only KD-tree for spatial picking — Phase C) ───────────
find_package(nanoflann CONFIG REQUIRED)

# ── nlohmann_json (composition documents — Phase A) ─────────────────────────
find_package(nlohmann_json CONFIG REQUIRED)

# ── Generated version header ────────────────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/core/version.h"
    @ONLY)

# ── Core library (everything except main(); linked by app and tests) ────────
# Q_OBJECT headers MUST be listed as target sources or AUTOMOC silently skips
# them and the link fails on missing vtables.
set(COMPOSER_SOURCES
    include/core/composerapplication.h
    src/core/composerapplication.cpp
    include/plugins/componentabi.h
    include/plugins/componentlibrary.h
    src/plugins/componentlibrary.cpp
    include/plugins/componentregistry.h
    src/plugins/componentregistry.cpp
    include/results/timecontroller.h
    src/results/timecontroller.cpp
    include/results/runbrowsermodel.h
    src/results/runbrowsermodel.cpp
    include/results/julianday.h
    src/results/julianday.cpp
    include/results/seriesexport.h
    src/results/seriesexport.cpp
    include/results/transect.h
    src/results/transect.cpp
    include/results/runsession.h
    src/results/runsession.cpp
    include/gis/crscatalog.h
    src/gis/crscatalog.cpp
    include/gis/spatialreference.h
    src/gis/spatialreference.cpp
    include/project/presentation.h
    src/project/presentation.cpp
    include/project/compositiondocument.h
    src/project/compositiondocument.cpp
    include/project/hcpimporter.h
    src/project/hcpimporter.cpp
    include/project/documentcommands.h
    src/project/documentcommands.cpp
    include/project/componentinstances.h
    src/project/componentinstances.cpp
    include/simulation/runrecording.h
    src/simulation/runrecording.cpp
    include/simulation/simulationmanager.h
    src/simulation/simulationmanager.cpp
    include/configurator/argumentdescriptor.h
    src/configurator/argumentdescriptor.cpp
    include/configurator/componentconfigurator.h
    src/configurator/componentconfigurator.cpp
    include/canvas/canvasitems.h
    src/canvas/canvasitems.cpp
    include/canvas/compositionscene.h
    src/canvas/compositionscene.cpp
    include/canvas/compositioncanvas.h
    src/canvas/compositioncanvas.cpp
    include/canvas/componentpalettemodel.h
    src/canvas/componentpalettemodel.cpp
    include/render/colorramp.h
    src/render/colorramp.cpp
    include/render/attributeprovider.h
    src/render/attributeprovider.cpp
    include/render/classification.h
    src/render/classification.cpp
    include/render/layerstyle.h
    src/render/layerstyle.cpp
    include/render/labelconfig.h
    src/render/labelconfig.cpp
    include/map/extentmath.h
    include/map/tilegrid.h
    src/map/tilegrid.cpp
    include/layers/tilelayer.h
    src/layers/tilelayer.cpp
    include/layers/networktilesource.h
    src/layers/networktilesource.cpp
    include/layers/featurelayer.h
    src/layers/featurelayer.cpp
    include/layers/ogrgeometryreader.h
    src/layers/ogrgeometryreader.cpp
    include/layers/dataitemlayer.h
    src/layers/dataitemlayer.cpp
    include/layers/timelayer.h
    include/mesh/meshdomain.h
    src/mesh/meshdomain.cpp
    include/mesh/meshdomainmodel.h
    src/mesh/meshdomainmodel.cpp
    include/layers/domainlayer.h
    src/layers/domainlayer.cpp
    include/layers/differencelayer.h
    src/layers/differencelayer.cpp
    include/layers/gdalvectorlayer.h
    src/layers/gdalvectorlayer.cpp
    include/layers/layeredmesh.h
    src/layers/layeredmesh.cpp
    include/layers/meshlayer.h
    src/layers/meshlayer.cpp
    include/layers/gdalrasterlayer.h
    src/layers/gdalrasterlayer.cpp
    include/map/maptool.h
    src/map/maptool.cpp
    include/map/maptransform.h
    src/map/maptransform.cpp
    include/map/maplayer.h
    src/map/maplayer.cpp
    include/map/layerstackmodel.h
    src/map/layerstackmodel.cpp
    include/map/mapcanvas.h
    src/map/mapcanvas.cpp
    include/scene/scenegeometry.h
    src/scene/scenegeometry.cpp
    include/scene/scenesource.h
    include/pick/centroidindex.h
    include/pick/terrainray.h
    src/pick/terrainray.cpp
    include/scene/camera.h
    src/scene/camera.cpp
    include/scene/scenerenderer.h
    src/scene/scenerenderer.cpp
    include/scene/groundplane.h
    src/scene/groundplane.cpp
    include/scene/sceneimage.h
    src/scene/sceneimage.cpp
    include/scene/sceneview.h
    src/scene/sceneview.cpp
    include/ui/dialogs/crsselectiondialog.h
    src/ui/dialogs/crsselectiondialog.cpp
    include/ui/dialogs/layerpropertiesdialog.h
    src/ui/dialogs/layerpropertiesdialog.cpp
    include/ui/panels/layertreepanel.h
    src/ui/panels/layertreepanel.cpp
    include/ui/panels/runbrowserpanel.h
    src/ui/panels/runbrowserpanel.cpp
    include/ui/panels/mapstatusbar.h
    src/ui/panels/mapstatusbar.cpp
    include/ui/panels/timecontrolpanel.h
    src/ui/panels/timecontrolpanel.cpp
    include/ui/panels/seriesplotpanel.h
    src/ui/panels/seriesplotpanel.cpp
    include/ui/panels/profileplotpanel.h
    src/ui/panels/profileplotpanel.cpp
    include/ui/panels/transectpanel.h
    src/ui/panels/transectpanel.cpp
    include/ui/panels/attributetablemodel.h
    src/ui/panels/attributetablemodel.cpp
    include/ui/panels/attributetablepanel.h
    src/ui/panels/attributetablepanel.cpp
    include/qtbridge/queuedslot.h
    include/qtbridge/componentstatusobserver.h
    src/qtbridge/componentstatusobserver.cpp
    include/ui/toolbars/ribbongroup.h
    src/ui/toolbars/ribbongroup.cpp
    include/ui/toolbars/ribbonbar.h
    src/ui/toolbars/ribbonbar.cpp
    include/ui/theme/thememanager.h
    src/ui/theme/thememanager.cpp
    include/ui/theme/iconfactory.h
    src/ui/theme/iconfactory.cpp
    include/ui/composermainwindow.h
    src/ui/composermainwindow.cpp
)

add_library(composer_core STATIC ${COMPOSER_SOURCES})

# ── Chrome icons ────────────────────────────────────────────────────────────
# The monochrome SVG set shared with openswmm.gui, recoloured per theme at
# render time by IconFactory.
#
# The FILES form, not a .qrc: on a static library the .qrc form's initialiser
# is never referenced by anything the linker keeps, so the resource silently
# does not exist at runtime and every icon renders as blank space. This form
# emits a separate init object that Qt propagates to consumers.
#
# Listed rather than globbed, so a new icon that was not registered fails
# CMake here instead of leaving one toolbar face empty.
qt_add_resources(composer_core "composer_icons"
    PREFIX "/icons"
    BASE "resources/icons"
    FILES
        resources/icons/add_component_layers.svg
        resources/icons/add_mesh.svg
        resources/icons/add_raster.svg
        resources/icons/add_vector.svg
        resources/icons/composition.svg
        resources/icons/dark.svg
        resources/icons/delete.svg
        resources/icons/dock_layers.svg
        resources/icons/extent.svg
        resources/icons/pan.svg
        resources/icons/select.svg
        resources/icons/zoom_rect_out.svg
        resources/icons/zoom_rect.svg
        resources/icons/globe.svg
        resources/icons/layer_styling.svg
        resources/icons/move_down.svg
        resources/icons/move_up.svg
        resources/icons/new.svg
        resources/icons/open.svg
        resources/icons/cycle.svg
        resources/icons/pause.svg
        resources/icons/play.svg
        resources/icons/plugins.svg
        resources/icons/redo.svg
        resources/icons/save.svg
        resources/icons/scene_3d.svg
        resources/icons/skip_first.svg
        resources/icons/skip_last.svg
        resources/icons/step_back.svg
        resources/icons/step_forward.svg
        resources/icons/stop.svg
        resources/icons/sun.svg
        resources/icons/transect.svg
        resources/icons/system.svg
        resources/icons/undo.svg
        resources/icons/zoomin.svg
        resources/icons/zoomout.svg)

# ── C3 — scene materials, compiled to .qsb ──────────────────────────────────
# Loaded from :/shaders at runtime by SceneRenderer. qsb cross-compiles one
# GLSL source to every backend Qt supports, which is what lets the same
# material serve Metal here and D3D or Vulkan on the other CI platforms.
# BASE, not just PREFIX: without it the resource path keeps the sources'
# directory too, and the shaders land at :/shaders/resources/shaders/... —
# present in the binary, and not where anything looks for them.
qt_add_shaders(composer_core "composer_scene_shaders"
    PREFIX "/shaders"
    BASE "resources/shaders"
    FILES
        resources/shaders/scene.vert
        resources/shaders/scene.frag
        resources/shaders/ground.vert
        resources/shaders/ground.frag)

target_include_directories(composer_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(composer_core
    PUBLIC
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
        Qt6::Network Qt6::Concurrent Qt6::Svg Qt6::Charts
        Qt6::Quick Qt6::QuickWidgets Qt6::Qml Qt6::PrintSupport Qt6::Xml
        # QRhi — the 3D scene's graphics abstraction (C3). Qt ships it under
        # the private include path, so the module is named explicitly; the
        # QRhiWidget class that hosts it is public QtWidgets API.
        Qt6::GuiPrivate
        HydroCouple::HydroCouple
        HydroCoupleSDK::HydroCoupleSDK
        GDAL::GDAL
        nanoflann::nanoflann
        nlohmann_json::nlohmann_json)

set_target_properties(composer_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ── QT_NO_KEYWORDS (mandatory, not stylistic) ───────────────────────────────
# HydroCouple's ISignal declares `virtual void emit(Args...)`. Qt defines
# `emit` as an empty macro, which rewrites that declaration to
# `virtual void (Args...)` — "field has incomplete type 'void'" — in every
# translation unit that includes both Qt and hydrocouple.h. Since Composer is
# by definition the place those two meet, the Qt keywords are disabled
# project-wide and their Q_-prefixed forms used instead:
#     signals: -> Q_SIGNALS:   slots: -> Q_SLOTS:   emit x -> Q_EMIT x
# PUBLIC so every consumer (app, tests) inherits the same rule; a target that
# missed it would fail only when it first included an interface header.
target_compile_definitions(composer_core PUBLIC QT_NO_KEYWORDS)

# ── Application ─────────────────────────────────────────────────────────────
qt_add_executable(HydroCoupleComposer MACOSX_BUNDLE src/app/main.cpp)

target_link_libraries(HydroCoupleComposer PRIVATE composer_core)

set_target_properties(HydroCoupleComposer PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "HydroCoupleComposer"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.hydrocouple.composer")

install(TARGETS HydroCoupleComposer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# ── Component libraries need the platform's module suffix ──────────────────
# CMake's MODULE default on macOS is .so; components are looked up by the
# platform suffix ComponentLibrary::librarySuffix() reports, so keep them equal.
if(APPLE)
    set(COMPOSER_MODULE_SUFFIX ".dylib")
elseif(WIN32)
    set(COMPOSER_MODULE_SUFFIX ".dll")
else()
    set(COMPOSER_MODULE_SUFFIX ".so")
endif()

# ── Screenshot tool (visual review, runs headless) ──────────────────────────
if(COMPOSER_BUILD_TESTS)
    qt_add_executable(layered_perf tools/layered_perf.cpp)
    target_link_libraries(layered_perf PRIVATE composer_core)
    set_target_properties(layered_perf PROPERTIES MACOSX_BUNDLE OFF)
    qt_add_executable(composer_screenshot tools/screenshot.cpp)
    target_link_libraries(composer_screenshot PRIVATE composer_core)
    set_target_properties(composer_screenshot PROPERTIES MACOSX_BUNDLE OFF)
endif()

# ── Documentation ───────────────────────────────────────────────────────────
if(COMPOSER_BUILD_DOCS)
    find_package(Doxygen REQUIRED OPTIONAL_COMPONENTS dot)
    add_subdirectory(docs)
endif()

# ── Locate the SDK's example composition (round-trip fixture for A4) ───────
find_file(COMPOSER_SDK_EXAMPLE_COMPOSITION
    NAMES composition.json
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../HydroCoupleSDK/examples/serial_coupling"
    NO_DEFAULT_PATH)
if(NOT COMPOSER_SDK_EXAMPLE_COMPOSITION)
    message(STATUS
        "SDK example composition not found — its round-trip test will skip.")
    set(COMPOSER_SDK_EXAMPLE_COMPOSITION "")
endif()

# ── Locate the SDK's reopen fixture (recorded run, four formats) ──────────
# The catalog D1 has to read faithfully, produced by the SDK's own tests. Kept
# optional the way the example composition is: a checkout without the SDK's
# test artifacts still builds, and the test that needs them says it skipped.
find_file(COMPOSER_SDK_REOPEN_MANIFEST
    NAMES run.json
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../HydroCoupleSDK/tests/artifacts/reopen/slabs"
    NO_DEFAULT_PATH)
if(NOT COMPOSER_SDK_REOPEN_MANIFEST)
    message(STATUS
        "SDK reopen fixture not found — the run browser's catalog test will "
        "fall back to its own fixture only.")
    set(COMPOSER_SDK_REOPEN_MANIFEST "")
endif()

# ── Tests ───────────────────────────────────────────────────────────────────
if(COMPOSER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/fixtures/testcomponent)
    add_subdirectory(tests/gui)
endif()
