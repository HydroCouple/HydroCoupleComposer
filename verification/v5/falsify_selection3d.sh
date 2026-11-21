#!/bin/bash
# Falsification: selection is visible in the scene.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_scene3d
LOG=verification/v5/falsify_selection3d.log
: > "$LOG"

FILES=(src/layers/featurelayer.cpp src/layers/meshlayer.cpp)

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

  if ! cmake --build $BUILD --target test_scene3d test_drape -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $TEST 2>&1; timeout 300 $BUILD/tests/gui/test_drape 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The scene ignores the selection -- the pre-slice behavior.
m_scene_ignores_selection() {
  perl -0pi -e 's/    return m_selection\.contains\(feature\) \? kSelectionColor : color;/    return color;/' src/layers/featurelayer.cpp
}

# 2. Selection resurrects a legend-hidden class -- the override applied
#    before the validity check instead of after it.
m_selection_resurrects() {
  perl -0pi -e 's/    const QColor color =\n      style \? style->colorFor\(\*this, feature\) : QColor\(Qt::gray\);\n\n    if \(!color\.isValid\(\)\)\n    \{\n      return color;\n    \}\n\n    return m_selection\.contains\(feature\) \? kSelectionColor : color;/    if (m_selection.contains(feature))\n    \{\n      return kSelectionColor;\n    \}\n\n    return style ? style->colorFor(*this, feature) : QColor(Qt::gray);/' src/layers/featurelayer.cpp
}

# 3. The mesh surface bypasses the shared helper and keeps its own color,
#    so a selected catchment highlights as lines but never as a surface.
m_mesh_bypasses_helper() {
  perl -0pi -e 's/      const QColor color = sceneColorFor\(layerStyle, feature\);/      const QColor color =\n        layerStyle ? layerStyle->colorFor(*this, feature) : QColor(Qt::gray);/' src/layers/meshlayer.cpp
}

run_mutation 1-scene-ignores-selection  m_scene_ignores_selection
run_mutation 2-selection-resurrects     m_selection_resurrects
run_mutation 3-mesh-bypasses-helper     m_mesh_bypasses_helper

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
