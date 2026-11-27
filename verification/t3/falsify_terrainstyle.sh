#!/bin/bash
# Falsification: elevation as a theme, smooth shading as the default.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
LOG=verification/t3/falsify_terrainstyle.log
: > "$LOG"

FILES=(src/layers/meshlayer.cpp include/layers/meshlayer.h src/ui/composermainwindow.cpp)

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

  if ! cmake --build $BUILD --target test_scene3d test_terrainsampler test_layerproperties -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $BUILD/tests/gui/test_scene3d 2>&1; timeout 300 $BUILD/tests/gui/test_terrainsampler 2>&1; timeout 300 $BUILD/tests/gui/test_layerproperties 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The elevation field reads node z per corner-count wrong: sum without
#    dividing, so a face's "mean" scales with how many corners it has.
m_sum_not_mean() {
  perl -0pi -e 's/    return sum \/ double\(to - from\);/    return sum;/' src/layers/meshlayer.cpp
}

# 2. Smooth is the default the plan chose; flip it and the complaint
#    ("terrain not rendered nicely") ships onwards as an opt-in.
m_flat_by_default() {
  perl -0pi -e 's/      bool m_flatShading = false;/      bool m_flatShading = true;/' include/layers/meshlayer.h
}

# 3. Accumulate normals without orienting them up, so opposite windings
#    cancel to nothing.
m_no_orientation() {
  perl -0pi -e 's/        if \(normal\.z\(\) < 0\.0f\)\n        \{\n          normal = -normal;\n        \}\n\n        for \(int corner = 0; corner < corners; \+\+corner\)/        for (int corner = 0; corner < corners; ++corner)/' src/layers/meshlayer.cpp
}

# 4. Graduate a zero-span sheet anyway.
m_graduate_nothing() {
  perl -0pi -e 's/    if \(!\(\*high > \*low\)\)\n    \{\n      return;\n    \}//' src/layers/meshlayer.cpp
}

# 5. Stamp the default over whatever style is there.
m_stamp_over_styles() {
  perl -0pi -e 's/    if \(style\(\)->mode\(\) != StyleMode::Single\n        \|\| !style\(\)->attribute\(\)\.isEmpty\(\)\)\n    \{\n      return;\n    \}//' src/layers/meshlayer.cpp
}

# 6. Sample Elevations forgets to theme the surface it just made.
#    RECORDED AS UNFALSIFIABLE: onSampleTerrain asks for its mesh and raster
#    through static modal QInputDialogs, which no headless gate can drive --
#    the same boundary the action's original gates stopped at. The method it
#    calls is gated (mutations 4 and 5); the one line of wiring is not.
m_sampling_forgets_theme() {
  perl -0pi -e 's/    \/\/ A mesh that just became a surface wears its own ground, unless its\n    \/\/ style has been touched -- the same default a fresh DEM mesh gets, and\n    \/\/ the guard against overwriting someone.s classification lives inside\.\n    mesh->applyDefaultElevationStyle\(\);\n//' src/ui/composermainwindow.cpp
}

run_mutation 1-sum-not-mean          m_sum_not_mean
run_mutation 2-flat-by-default      m_flat_by_default
run_mutation 3-no-orientation       m_no_orientation
run_mutation 4-graduate-nothing     m_graduate_nothing
run_mutation 5-stamp-over-styles    m_stamp_over_styles
run_mutation 6-sampling-forgets     m_sampling_forgets_theme

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
