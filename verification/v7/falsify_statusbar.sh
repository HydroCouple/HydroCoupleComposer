#!/bin/bash
# Falsification: the status bar is honest on the 3D tab.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
LOG=verification/v7/falsify_statusbar.log
: > "$LOG"

FILES=(src/ui/panels/mapstatusbar.cpp src/ui/composermainwindow.cpp)

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

  if ! cmake --build $BUILD --target test_mapstatusbar test_viewhandoff -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $BUILD/tests/gui/test_mapstatusbar 2>&1; timeout 300 $BUILD/tests/gui/test_viewhandoff 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Keep the stale coordinate on the 3D tab -- the pre-slice behaviour.
m_keep_stale_coordinate() {
  perl -0pi -e 's/    if \(!live\)\n    \{\n      m_coordinateLabel->setText\(tr\("3D view"\)\);\n    \}//' src/ui/panels/mapstatusbar.cpp
}

# 2. The camera readout never reaches the bar.
m_camera_reading_lost() {
  perl -0pi -e 's/              const Camera &camera = m_sceneView->camera\(\);\n              m_mapStatus->showCameraReading\(camera\.azimuth\(\),\n                                             camera\.elevation\(\),\n                                             camera\.distance\(\)\);/              (void)this;/' src/ui/composermainwindow.cpp
}

# 3. The camera writes its reading whichever tab is front, so the map's live
#    coordinate is overwritten by a hidden camera.
m_camera_writes_always() {
  perl -0pi -e 's/              if \(m_workspace->currentWidget\(\) != m_sceneView\)\n              \{\n                return;\n              \}\n\n              const Camera &camera = m_sceneView->camera\(\);/              const Camera \&camera = m_sceneView->camera();/' src/ui/composermainwindow.cpp
}

run_mutation 1-keep-stale-coordinate  m_keep_stale_coordinate
run_mutation 2-camera-reading-lost    m_camera_reading_lost
run_mutation 3-camera-writes-always   m_camera_writes_always

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
