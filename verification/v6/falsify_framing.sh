#!/bin/bash
# Falsification: framing works in both views and frames the same world.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
LOG=verification/v6/falsify_framing.log
: > "$LOG"

FILES=(src/ui/composermainwindow.cpp src/scene/scenerenderer.cpp
       src/scene/sceneview.cpp src/layers/featurelayer.cpp)

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

  if ! cmake --build $BUILD --target test_viewhandoff test_scene3d -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $BUILD/tests/gui/test_viewhandoff 2>&1; timeout 300 $BUILD/tests/gui/test_scene3d 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The old behavior: zoom-to-layer always forces the Map tab.
m_always_map() {
  perl -0pi -e 's/              if \(m_workspace->currentWidget\(\) == m_sceneView\)\n              \{\n                const auto \*source = layer->sceneSource\(\);\n\n                if \(!source\)\n                \{\n                  source = dynamic_cast<const ISceneSource \*>\(layer\);\n                \}\n\n                if \(source && source->sceneBounds\(\)\.isValid\(\)\)\n                \{\n                  m_sceneView->frameBounds\(source->sceneBounds\(\)\);\n\n                  return;\n                \}\n              \}\n\n//' src/ui/composermainwindow.cpp
}

# 2. Frame the whole scene instead of the layer asked for.
m_frames_everything() {
  perl -0pi -e 's/                  m_sceneView->frameBounds\(source->sceneBounds\(\)\);/                  m_sceneView->zoomToFullExtent();/' src/ui/composermainwindow.cpp
}

# 3. Bounds go back to counting only what draws.
m_bounds_only_drawn() {
  perl -0pi -e 's/      const auto \*source = layer->sceneSource\(\);\n\n      if \(!source\)\n      \{\n        source = dynamic_cast<const ISceneSource \*>\(layer\);\n      \}\n\n      if \(source\)/      if (const ISceneSource *source = layer->sceneSource())/' src/scene/scenerenderer.cpp
}

# 4. Point layers decline bounds again.
m_points_decline_bounds() {
  perl -0pi -e 's/    \/\/ Point layers answer too, though sceneGeometry\(\) declines them: bounds\n    \/\/ are about where the data IS, not what can be drawn\. Leaving points out\n    \/\/ made "Zoom to Full Extent" frame a different world in each view, and\n    \/\/ left the basemap.s ground plane stopping short of the gauges standing\n    \/\/ on it\.\n    const double rise =/    if (m_kind == GeometryKind::Point)\n    \{\n      return bounds;\n    \}\n\n    const double rise =/' src/layers/featurelayer.cpp
}

# 5. frameBounds frames invalid bounds, pointing the camera at the origin's
#    couple of units when a layer has nothing to say.
m_frames_nothing() {
  perl -0pi -e 's/    \/\/ Not for fitTo.s sake -- the camera refuses invalid bounds itself --\n    \/\/ but for m_framed.s: marking an empty framing as deliberate would stop\n    \/\/ the first-geometry auto-frame, and a model loaded afterwards would\n    \/\/ never be framed at all\.\n    if \(!bounds\.isValid\(\)\)\n    \{\n      return;\n    \}\n//' src/scene/sceneview.cpp
}

run_mutation 1-always-map           m_always_map
run_mutation 2-frames-everything    m_frames_everything
run_mutation 3-bounds-only-drawn    m_bounds_only_drawn
run_mutation 4-points-decline       m_points_decline_bounds
run_mutation 5-frames-nothing       m_frames_nothing

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
