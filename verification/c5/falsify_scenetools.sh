#!/bin/bash
# 3D tools falsification run: Orbit, Select, Zoom In Box and Zoom Out Box in
# the scene view.
#
# The band in 3D is the interesting half: a screen rectangle covers a ground
# rectangle only after four rays have been cast through it, and every way that
# goes wrong still produces a rectangle.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_viewhandoff
LOG=verification/c5/falsify_scenetools.log
: > "$LOG"

FILES=(src/scene/sceneview.cpp src/ui/composermainwindow.cpp)

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
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_viewhandoff -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 180 ]; do
    sleep 1
    waited=$((waited + 1))
  done

  if kill -0 $pid 2>/dev/null; then
    kill -9 $pid 2>/dev/null
    wait $pid 2>/dev/null
    echo "MUTATION $name: CAUGHT (hung -- the suite never finished)" | tee -a "$LOG"
    return
  fi

  wait $pid
  local status=$?

  local out
  out=$(cat "$out_file")
  rm -f "$out_file"
  echo "=== $name ===" >> "$LOG"
  echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] ViewHandoff' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The left button always bands, so Orbit stops turning the scene.
#    (Setting m_orbiting unconditionally is *not* a behavioural change:
#    m_banding is read first and short-circuits, so that mutation was inert
#    and this replaced it.)
m_always_band() {
  perl -0pi -e 's/    m_banding = event->button\(\) == Qt::LeftButton\n                && m_toolKind != SceneToolKind::Orbit;/    m_banding = event->button() == Qt::LeftButton;/' src/scene/sceneview.cpp
}

# 2. The band never starts, so a drag under the zoom tools does nothing.
m_no_band() {
  perl -0pi -e 's/    m_banding = event->button\(\) == Qt::LeftButton\n                && m_toolKind != SceneToolKind::Orbit;/    m_banding = false;/' src/scene/sceneview.cpp
}

# 3. Only one corner of the band is unprojected, so its ground rectangle is a
#    point and every band covers nothing.
m_one_corner() {
  perl -0pi -e 's/    for \(int corner = 1; corner < 4; \+\+corner\)/    for (int corner = 1; corner < 1; ++corner)/' src/scene/sceneview.cpp
}

# 4. A corner that misses the terrain fails the whole gesture rather than
#    falling back to the ground plane, so a band near a model's edge is dead.
m_no_plane_fallback() {
  perl -0pi -e 's/    return rayGroundPoint\(origin, direction, nullptr, m_camera\.farPlane\(\),\n                          ground\);/    return false;/' src/scene/sceneview.cpp
}

# 5. Zooming out ignores how big a box was dragged.
m_zoomout_fixed() {
  perl -0pi -e 's/            \? std::min\(double\(rectangle\.width\(\)\) \/ double\(width\(\)\),\n                       double\(rectangle\.height\(\)\) \/ double\(height\(\)\)\)\n            : 1\.0 \/ kBandZoomFactor;/            ? 1.0 \/ kBandZoomFactor\n            : 1.0 \/ kBandZoomFactor;/' src/scene/sceneview.cpp
}

# 6. Zoom in frames nothing, so a band leaves the view where it was.
m_zoomin_dead() {
  perl -0pi -e 's/          showGroundExtent\(ground\);/          ;/' src/scene/sceneview.cpp
}

# 7. A band selects in every layer rather than the topmost that catches
#    anything, so the table shows a fraction of the highlight.
m_select_all_layers() {
  perl -0pi -e 's/        stack->selectOnly\(features, caught\);\n\n        Q_EMIT featurePicked\(features, \*caught\.constBegin\(\)\);\n\n        return;/        features->setSelection(caught);/' src/scene/sceneview.cpp
}

# 8. A band over empty ground leaves the previous selection standing.
m_no_clear() {
  perl -0pi -e 's/    stack->selectOnly\(nullptr, QSet<int>\{\}\);\n\n    Q_EMIT featurePicked\(nullptr, -1\);/    ;/' src/scene/sceneview.cpp
}

# 9. Switching tools leaves a band in progress, so one belonging to a tool
#    that is no longer active stays live.
m_switch_keeps_band() {
  perl -0pi -e 's/    m_banding = false;\n\n    if \(m_band\)\n    \{\n      m_band->hide\(\);\n    \}\n\n    setCursor/    if (m_band)\n    {\n      m_band->hide();\n    }\n\n    setCursor/' src/scene/sceneview.cpp
}

# 10. Every 3D tool button chooses Orbit, so the group is decorative.
m_buttons_dead() {
  perl -0pi -e 's/    m_sceneView->setToolKind\(kind\);/    m_sceneView->setToolKind(SceneToolKind::Orbit);\n    Q_UNUSED(kind)/' src/ui/composermainwindow.cpp
}

run_mutation "1-left-always-bands"        m_always_band
run_mutation "2-band-never-starts"        m_no_band
run_mutation "3-one-corner-only"          m_one_corner
run_mutation "4-no-ground-plane-fallback" m_no_plane_fallback
run_mutation "5-zoomout-ignores-the-box"  m_zoomout_fixed
run_mutation "6-zoomin-frames-nothing"    m_zoomin_dead
run_mutation "7-selects-every-layer"      m_select_all_layers
run_mutation "8-empty-band-keeps-it"      m_no_clear
run_mutation "9-switch-keeps-the-band"    m_switch_keeps_band
run_mutation "10-tool-buttons-are-dead"   m_buttons_dead

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_viewhandoff -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
