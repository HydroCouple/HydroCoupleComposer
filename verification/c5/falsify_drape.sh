#!/bin/bash
# C5d falsification run: the drape as a choice, and the 3D view controls.
#
# Mutation 1 restores the state this slice found rather than one invented for
# the harness: the surface layers passed the context's terrain straight to
# buildGroundPlane, so a basemap draped whenever a DEM was in the stack and
# could not be told not to.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
LOG=verification/c5/falsify_drape.log
: > "$LOG"

FILES=(include/scene/scenesource.h src/layers/tilelayer.cpp
       src/layers/gdalrasterlayer.cpp src/scene/sceneview.cpp
       src/ui/composermainwindow.cpp)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

# Two suites: the drape lives in the ground-plane tests, the view controls in
# the hand-off ones. A mutation is caught if either bites.
run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }

  local changed=0
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_groundplane --target test_viewhandoff -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  local caught=0
  local died=0

  for suite in test_groundplane test_viewhandoff; do
    local out_file="$LOG.run"
    QT_QPA_PLATFORM=offscreen "$BUILD/tests/gui/$suite" > "$out_file" 2>&1 &
    local pid=$!
    local waited=0

    while kill -0 $pid 2>/dev/null && [ $waited -lt 120 ]; do
      sleep 1
      waited=$((waited + 1))
    done

    if kill -0 $pid 2>/dev/null; then
      kill -9 $pid 2>/dev/null
      wait $pid 2>/dev/null
      echo "MUTATION $name: CAUGHT ($suite hung)" | tee -a "$LOG"
      rm -f "$out_file"
      return
    fi

    wait $pid
    local status=$?

    echo "=== $name / $suite ===" >> "$LOG"
    grep '^\[  FAILED  \]' "$out_file" >> "$LOG"

    if grep -q "\[  FAILED  \]" "$out_file"; then
      caught=1
    elif [ "$status" -ne 0 ]; then
      died=1
    fi

    rm -f "$out_file"
  done

  if [ "$caught" -eq 1 ]; then
    echo "MUTATION $name: CAUGHT" | tee -a "$LOG"
  elif [ "$died" -eq 1 ]; then
    echo "MUTATION $name: CAUGHT (a suite died)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The state before C5d: a surface drapes whether or not it was asked to.
m_always_drape() {
  perl -0pi -e 's/    return m_drape == SceneDrape::Flat \? nullptr : context\.terrain;/    return context.terrain;/' include/scene/scenesource.h
}

# 2. The opposite: a surface never drapes, whatever it was set to.
m_never_drape() {
  perl -0pi -e 's/    return m_drape == SceneDrape::Flat \? nullptr : context\.terrain;/    return nullptr;/' include/scene/scenesource.h
}

# 3. A raster ignores its own setting while a basemap honours its.
m_raster_ignores() {
  perl -0pi -e 's/    SceneGeometry ground = buildGroundPlane\(m_ground, drapeTarget\(context\)\);/    SceneGeometry ground = buildGroundPlane(m_ground, context.terrain);/' src/layers/gdalrasterlayer.cpp
}

# 4. A basemap ignores its own setting.
m_basemap_ignores() {
  perl -0pi -e 's/    SceneGeometry ground = buildGroundPlane\(m_ground, drapeTarget\(context\)\);/    SceneGeometry ground = buildGroundPlane(m_ground, context.terrain);/' src/layers/tilelayer.cpp
}

# 5. Surfaces claim they can be extruded, so an editor offers a placement that
#    silently behaves as another.
m_surfaces_extrude() {
  perl -0pi -e 's/      \[\[nodiscard\]\] virtual bool supportsExtrusion\(\) const \{ return false; \}/      [[nodiscard]] virtual bool supportsExtrusion() const { return true; }/' include/scene/scenesource.h
}

# 6. The drape is recorded but the layer never redraws, so the 3D view keeps
#    showing the old placement until something else disturbs it. This is the
#    state this slice shipped in until the harness found it.
m_no_redraw() {
  perl -0pi -e 's/    ISceneSource::setDrape\(drape\);\n\n    notifyAppearanceChanged\(\);/    ISceneSource::setDrape(drape);/' src/layers/gdalrasterlayer.cpp
}

# 7. Switching projection reframes instead of matching the two through the
#    ground extent.
m_projection_reframes() {
  perl -0pi -e 's/    m_camera\.setProjection\(projection, aspect\);\n\n    update\(\);/    m_camera.setProjection(projection, aspect);\n    m_camera.dolly(2.0);\n\n    update();/' src/scene/sceneview.cpp
}

# 8. The toggle is wired to the wrong action, so both faces do the same thing.
m_toggle_backwards() {
  perl -0pi -e 's/    m_sceneView->setProjection\(m_orthographicAction->isChecked\(\)/    m_sceneView->setProjection(m_perspectiveAction->isChecked()/' src/ui/composermainwindow.cpp
}

# 9. The exaggeration control changes nothing.
m_exaggeration_dead() {
  perl -0pi -e 's/              m_sceneView->setVerticalExaggeration\(factor\);/              Q_UNUSED(factor)/' src/ui/composermainwindow.cpp
}

run_mutation "1-surfaces-always-drape"      m_always_drape
run_mutation "2-surfaces-never-drape"       m_never_drape
run_mutation "3-raster-ignores-its-drape"   m_raster_ignores
run_mutation "4-basemap-ignores-its-drape"  m_basemap_ignores
run_mutation "5-surfaces-claim-extrusion"   m_surfaces_extrude
run_mutation "6-drape-without-redraw"       m_no_redraw
run_mutation "7-projection-reframes"        m_projection_reframes
run_mutation "8-toggle-wired-backwards"     m_toggle_backwards
run_mutation "9-exaggeration-does-nothing"  m_exaggeration_dead

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_groundplane --target test_viewhandoff -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $BUILD/tests/gui/test_groundplane 2>&1 | tail -2
QT_QPA_PLATFORM=offscreen $BUILD/tests/gui/test_viewhandoff 2>&1 | tail -2
