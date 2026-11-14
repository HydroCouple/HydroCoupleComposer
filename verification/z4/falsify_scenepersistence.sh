#!/bin/bash
# Falsification: the scene settings survive a save and an open.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_layerpersistence
LOG=verification/z4/falsify_scenepersistence.log
: > "$LOG"

FILES=(src/layers/layerrestorer.cpp src/ui/composermainwindow.cpp)

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

  if ! cmake --build $BUILD --target test_layerpersistence -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The restore ignores the scene block -- reopening flattens the 3D
#    arrangement, as before this slice.
m_restore_ignores_scene() {
  perl -0pi -e 's/      const QJsonObject sceneState =\n        entry\.value\(QStringLiteral\("scene"\)\)\.toObject\(\);/      const QJsonObject sceneState;/' src/layers/layerrestorer.cpp
}

# 2. The policy comes back but the mode is dropped: everything reopens
#    OnTerrain whatever it was placed as.
m_mode_dropped() {
  perl -0pi -e 's/        policy\.mode = static_cast<ZMode>\(\n          sceneState\.value\(QStringLiteral\("zMode"\)\)\n            \.toInt\(static_cast<int>\(policy\.mode\)\)\);/        policy.mode = ZMode::OnTerrain;/' src/layers/layerrestorer.cpp
}

# 3. The terrain-consent fallback hard-codes "offered", so an old
#    composition's raster reopens as a terrain candidate nobody offered.
m_consent_fallback_flipped() {
  perl -0pi -e 's/        scene->setTerrainEnabled\(\n          sceneState\.value\(QStringLiteral\("terrainEnabled"\)\)\n            \.toBool\(scene->terrainEnabled\(\)\)\);/        scene->setTerrainEnabled(\n          sceneState.value(QStringLiteral("terrainEnabled")).toBool(true));/' src/layers/layerrestorer.cpp
}

# 4. The raster's shading is captured but never restored.
m_shading_not_restored() {
  perl -0pi -e 's/        raster->setRampName\(shading\.value\(QStringLiteral\("ramp"\)\)\n                              \.toString\(raster->rampName\(\)\)\);/        (void)shading;/' src/layers/layerrestorer.cpp
}

# 5. Opacity is dropped on restore.
m_opacity_dropped() {
  perl -0pi -e 's/      if \(entry\.contains\(QStringLiteral\("opacity"\)\)\)\n      \{\n        layer->setOpacity\(\n          entry\.value\(QStringLiteral\("opacity"\)\)\.toDouble\(1\.0\)\);\n      \}//' src/layers/layerrestorer.cpp
}

run_mutation 1-restore-ignores-scene   m_restore_ignores_scene
run_mutation 2-mode-dropped            m_mode_dropped
run_mutation 3-consent-fallback        m_consent_fallback_flipped
run_mutation 4-shading-not-restored    m_shading_not_restored
run_mutation 5-opacity-dropped         m_opacity_dropped

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'

# The build left behind is the last mutation's; put the real one back.
restore
cmake --build $BUILD --target test_layerpersistence -j 8 >> "$LOG" 2>&1
