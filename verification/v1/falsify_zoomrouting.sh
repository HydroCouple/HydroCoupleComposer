#!/bin/bash
# Falsification: the zoom shortcuts act on the view in front.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_viewhandoff
LOG=verification/v1/falsify_zoomrouting.log
: > "$LOG"

FILES=(src/ui/composermainwindow.cpp src/scene/sceneview.cpp)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }

  local changed=0
  for f in "${FILES[@]}"; do cmp -s "$f" "$f.orig" || changed=1; done
  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"; return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_viewhandoff -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if [ "$status" -eq 124 ]; then
    echo "MUTATION $name: CAUGHT (hung)" | tee -a "$LOG"
  elif echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ViewHandoffTest' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The old behavior: zoom in always targets the map.
m_zoom_in_always_map() {
  perl -0pi -e 's/              if \(m_workspace->currentWidget\(\) == m_sceneView\)\n              \{\n                m_sceneView->zoomIn\(\);\n              \}\n              else\n              \{\n                m_mapCanvas->zoomBy\(1\.25\);\n              \}/              m_mapCanvas->zoomBy(1.25);/' src/ui/composermainwindow.cpp
}

# 2. The old behavior, mirrored for zoom out.
m_zoom_out_always_map() {
  perl -0pi -e 's/              if \(m_workspace->currentWidget\(\) == m_sceneView\)\n              \{\n                m_sceneView->zoomOut\(\);\n              \}\n              else\n              \{\n                m_mapCanvas->zoomBy\(1\.0 \/ 1\.25\);\n              \}/              m_mapCanvas->zoomBy(1.0 \/ 1.25);/' src/ui/composermainwindow.cpp
}

# 3. Reach both views at once, so a shortcut on the map tab also disturbs the
#    parked 3D camera.
m_zoom_reaches_both() {
  perl -0pi -e 's/              if \(m_workspace->currentWidget\(\) == m_sceneView\)\n              \{\n                m_sceneView->zoomIn\(\);\n              \}\n              else\n              \{\n                m_mapCanvas->zoomBy\(1\.25\);\n              \}/              m_sceneView->zoomIn();\n              m_mapCanvas->zoomBy(1.25);/' src/ui/composermainwindow.cpp
}

# 4. Zoom by moving the target instead of the distance -- a pan wearing a
#    zoom's name.
m_zoom_moves_target() {
  perl -0pi -e 's/    \/\/ The same step a wheel notch takes, so the shortcut and the wheel move\n    \/\/ by amounts a user can predict from one another\.\n    m_camera\.dolly\(1\.0 \/ kZoomPerNotch\);/    Camera moved = m_camera;\n    moved.setTarget(m_camera.target() + QVector3D(1, 0, 0));\n    m_camera = moved;/' src/scene/sceneview.cpp
}

# 5. Both directions dolly the same way, so Zoom Out is a second Zoom In.
m_zoom_out_zooms_in() {
  perl -0pi -e 's/  void SceneView::zoomOut\(\)\n  \{\n    m_camera\.dolly\(kZoomPerNotch\);/  void SceneView::zoomOut()\n  {\n    m_camera.dolly(1.0 \/ kZoomPerNotch);/' src/scene/sceneview.cpp
}

run_mutation 1-zoom-in-always-map   m_zoom_in_always_map
run_mutation 2-zoom-out-always-map  m_zoom_out_always_map
run_mutation 3-zoom-reaches-both    m_zoom_reaches_both
run_mutation 4-zoom-moves-target    m_zoom_moves_target
run_mutation 5-zoom-out-zooms-in    m_zoom_out_zooms_in

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
