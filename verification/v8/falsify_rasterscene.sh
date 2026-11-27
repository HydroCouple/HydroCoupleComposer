#!/bin/bash
# Falsification: raster run results join the scene.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_dataitemlayers
LOG=verification/v8/falsify_rasterscene.log
: > "$LOG"

FILES=(src/layers/rasterdataitemlayer.cpp)

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

  if ! cmake --build $BUILD --target test_dataitemlayers -j 8 >> "$LOG" 2>&1; then
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

# 1. Decline the scene again -- the pre-slice behavior.
m_no_scene_source() {
  perl -0pi -e 's/  const ISceneSource \*RasterDataItemLayer::sceneSource\(\) const\n  \{\n    return this;\n  \}/  const ISceneSource *RasterDataItemLayer::sceneSource() const\n  \{\n    return nullptr;\n  \}/' src/layers/rasterdataitemlayer.cpp
}

# 2. The texture cache never invalidates on a band change.
m_stale_band_texture() {
  perl -0pi -e 's/    if \(readBand\(message\)\)\n    \{\n      m_groundValid = false;\n      notifyAppearanceChanged\(\);\n    \}/    if (readBand(message))\n    \{\n      notifyAppearanceChanged();\n    \}/' src/layers/rasterdataitemlayer.cpp
}

# 3. Drape over the raster's own focus rather than its footprint: cover only
#    context.focus like a basemap, so a result raster shows only the part of
#    itself the rest of the scene happens to cover.
m_bounds_empty() {
  perl -0pi -e 's/    \/\/ At ground level and flat, like a GeoTIFF.s: the terrain carries the\n    \/\/ relief, and framing must stay answerable without a drape\.\n    bounds\.expandTo\(QVector3D\(float\(box\.left\(\)\), float\(box\.top\(\)\), 0\.0f\)\);\n    bounds\.expandTo\(QVector3D\(float\(box\.right\(\)\), float\(box\.bottom\(\)\), 0\.0f\)\);\n\n    return bounds;/    return bounds;/' src/layers/rasterdataitemlayer.cpp
}

run_mutation 1-no-scene-source     m_no_scene_source
run_mutation 2-stale-band-texture  m_stale_band_texture
run_mutation 3-bounds-empty        m_bounds_empty

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
