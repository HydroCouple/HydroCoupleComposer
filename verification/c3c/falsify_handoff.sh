#!/bin/bash
# C3c-3 falsification run.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_viewhandoff
LOG=verification/c3c/falsify_handoff.log
: > "$LOG"

FILES=(src/ui/composermainwindow.cpp src/scene/sceneview.cpp
       src/map/mapcanvas.cpp include/map/mapcanvas.h)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }
  # A pattern that no longer matches applies nothing and reports nothing, so
  # the run reads as a surviving mutation when it is really a stale script.
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

  # Bounded, and judged by the exit status as well as the output. A mutation
  # that makes the suite *die* -- a stack overflow from an echo between two
  # signals, say -- prints no "[  FAILED  ]" line at all, and a harness that
  # only greps for one reads the crash as a mutation that survived.
  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 120 ]; do
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] View' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Arriving at the 3D view carries nothing.
m_no_to_scene() {
  perl -0pi -e 's/                  m_sceneView->showGroundExtent\(\n                    m_mapCanvas->transform\(\)\.visibleExtent\(\)\);/                  ;/' src/ui/composermainwindow.cpp
}

# 2. Arriving at the map carries nothing.
m_no_to_map() {
  perl -0pi -e 's/                  m_mapCanvas->setVisibleExtent\(ground\);/                  ;/' src/ui/composermainwindow.cpp
}

# 3. The 3D view frames everything instead of what the map showed.
m_scene_full_extent() {
  perl -0pi -e 's/                  m_sceneView->showGroundExtent\(\n                    m_mapCanvas->transform\(\)\.visibleExtent\(\)\);/                  m_sceneView->showGroundExtent(m_mapCanvas->fullExtent());/' src/ui/composermainwindow.cpp
}

# 4. Framing on geometry runs every time rather than only the first, so a
#    layer arriving after the user chose a view throws that view away.
m_reframes_always() {
  perl -0pi -e 's/  void SceneView::frameOnFirstGeometry\(\)\n  \{\n    if \(m_framed\)\n    \{\n      return;\n    \}/  void SceneView::frameOnFirstGeometry()\n  {\n    if (false)\n    {\n      return;\n    }/' src/scene/sceneview.cpp
}

# 5. An empty map hands its default view over anyway, so a model loaded
#    afterwards is never framed.
m_hand_over_empty_map() {
  perl -0pi -e 's/                if \(!m_mapCanvas->fullExtent\(\)\.isEmpty\(\)\)\n                \{/                if (true)\n                {/' src/ui/composermainwindow.cpp
}

# 6. Showing a rectangle pads it, as it used to.
m_margin_returns() {
  perl -0pi -e 's/      void setVisibleExtent\(const QRectF &extent,\n                            double marginFraction = 0\.0\);/      void setVisibleExtent(const QRectF \&extent,\n                            double marginFraction = 0.05);/' include/map/mapcanvas.h
  perl -0pi -e 's/    setVisibleExtent\(fullExtent\(\), kFramingMargin\);/    setVisibleExtent(fullExtent());/' src/map/mapcanvas.cpp
  perl -0pi -e 's/    setVisibleExtent\(layerExtentInMapCrs\(layer\), kFramingMargin\);/    setVisibleExtent(layerExtentInMapCrs(layer));/' src/map/mapcanvas.cpp
}

# 7. Framing the data no longer leaves air around it.
m_no_framing_margin() {
  perl -0pi -e 's/    constexpr double kFramingMargin = 0\.05;/    constexpr double kFramingMargin = 0.0;/' src/map/mapcanvas.cpp
}

# 8. The ground extent is read at a square aspect regardless of the widget.
m_square_aspect() {
  perl -0pi -e 's/    return m_camera\.groundExtent\(double\(width\(\)\) \/ double\(height\(\)\)\);/    return m_camera.groundExtent(1.0);/' src/scene/sceneview.cpp
}

# 9. Framing the ground resets the camera's orientation.
m_reset_orientation() {
  perl -0pi -e 's/    m_camera\.setGroundExtent\(extent\.normalized\(\),\n                             double\(width\(\)\) \/ double\(height\(\)\)\);/    m_camera.setElevation(45.0);\n    m_camera.setAzimuth(0.0);\n    m_camera.setGroundExtent(extent.normalized(),\n                             double(width()) \/ double(height()));/' src/scene/sceneview.cpp
}

# 10. A scene showing nothing reports the default camera's view anyway.
m_empty_scene_hands_over() {
  perl -0pi -e 's/    if \(height\(\) <= 0 \|\| width\(\) <= 0 \|\| !m_renderer\.sceneBounds\(\)\.isValid\(\)\)/    if (height() <= 0 || width() <= 0)/' src/scene/sceneview.cpp
}

run_mutation "1-nothing-carried-to-scene" m_no_to_scene
run_mutation "2-nothing-carried-to-map"   m_no_to_map
run_mutation "3-scene-frames-everything"  m_scene_full_extent
run_mutation "4-reframes-on-every-layer"  m_reframes_always
run_mutation "5-empty-map-hands-over"     m_hand_over_empty_map
run_mutation "6-margin-back-in-primitive" m_margin_returns
run_mutation "7-framing-loses-its-air"    m_no_framing_margin
run_mutation "8-square-aspect"            m_square_aspect
run_mutation "9-orientation-reset"        m_reset_orientation
run_mutation "10-empty-scene-hands-over" m_empty_scene_hands_over

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_viewhandoff -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -4
