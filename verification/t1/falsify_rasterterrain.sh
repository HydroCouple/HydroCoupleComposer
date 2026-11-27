#!/bin/bash
# Falsification: a DEM raster as the scene's terrain, by consent.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
LOG=verification/t1/falsify_rasterterrain.log
: > "$LOG"

FILES=(src/layers/gdalrasterlayer.cpp src/map/layerstackmodel.cpp
       src/scene/scenerenderer.cpp include/scene/scenesource.h)

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

  if ! cmake --build $BUILD --target test_groundplane test_layerproperties test_drape -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $BUILD/tests/gui/test_groundplane 2>&1; timeout 300 $BUILD/tests/gui/test_layerproperties 2>&1; timeout 300 $BUILD/tests/gui/test_drape 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Drop the half-cell center offset, shifting the whole terrain by half a
#    cell.
m_no_cell_centre() {
  perl -0pi -e 's/    const double column =\n      \(point\.x\(\) - m_terrain\.originX\) \/ m_terrain\.stepX - 0\.5;/    const double column =\n      (point.x() - m_terrain.originX) \/ m_terrain.stepX;/' src/layers/gdalrasterlayer.cpp
}

# 2. Nearest neighbor instead of bilinear: a continuous surface in
#    half-cell terraces.
m_nearest_not_bilinear() {
  perl -0pi -e 's/    const double top_ = v00 \+ \(v10 - v00\) \* fractionX;\n    const double bottom_ = v01 \+ \(v11 - v01\) \* fractionX;\n\n    elevation = top_ \+ \(bottom_ - top_\) \* fractionY;/    elevation = v00;/' src/layers/gdalrasterlayer.cpp
}

# 3. A hole answers zero -- sea level in most of the world's DEMs.
m_hole_is_sea_level() {
  perl -0pi -e 's/    if \(std::isnan\(v00\) \|\| std::isnan\(v10\) \|\| std::isnan\(v01\)\n        \|\| std::isnan\(v11\)\)\n    \{\n      return false;\n    \}/    if (std::isnan(v00) || std::isnan(v10) || std::isnan(v01)\n        || std::isnan(v11))\n    \{\n      elevation = 0.0;\n      return true;\n    \}/' src/layers/gdalrasterlayer.cpp
}

# 4. Every raster is terrain, color images included.
m_photos_are_ground() {
  perl -0pi -e 's/    \/\/ A colour image is a picture of the ground, not the ground\.\n    return m_colorImage \? nullptr : this;/    return this;/' src/layers/gdalrasterlayer.cpp
}

# 5. Consent is ignored: capability alone wins the election, and a rainfall
#    grid above a mesh steals the ground.
m_consent_ignored() {
  perl -0pi -e 's/      if \(source && source->terrain\(\) && source->terrainEnabled\(\)\)/      if (source \&\& source->terrain())/' src/map/layerstackmodel.cpp
}

# 6. The renderer runs its own election instead of the model's, and the two
#    can disagree.
m_renderer_own_election() {
  perl -0pi -e 's/    const MapLayer \*elected = m_model->electedTerrainLayer\(\);\n\n    if \(!elected\)\n    \{\n      return nullptr;\n    \}\n\n    const ISceneSource \*source = elected->sceneSource\(\);\n\n    return source \? source->terrain\(\) : nullptr;/    const ITerrainSource *terrain = nullptr;\n    for (const MapLayer *layer : m_model->renderOrder())\n    \{\n      if (!layer->isVisible()) continue;\n      if (const ISceneSource *source = layer->sceneSource())\n        if (const ITerrainSource *candidate = source->terrain())\n          terrain = candidate;\n    \}\n    return terrain;/' src/scene/scenerenderer.cpp
}

# 7. The consent default flips: rasters offer themselves out of the box.
m_rasters_offer_by_default() {
  perl -0pi -e 's/    \/\/ Terrain-capable, not terrain-offered: whether this band is heights is\n    \/\/ unknowable from here, and a rainfall grid stealing the election from a\n    \/\/ surveyed mesh would be worse than one extra click on a real DEM\.\n    setTerrainEnabled\(false\);\n//' src/layers/gdalrasterlayer.cpp
}

run_mutation 1-no-cell-centre          m_no_cell_centre
run_mutation 2-nearest-not-bilinear    m_nearest_not_bilinear
run_mutation 3-hole-is-sea-level       m_hole_is_sea_level
run_mutation 4-photos-are-ground       m_photos_are_ground
run_mutation 5-consent-ignored         m_consent_ignored
run_mutation 6-renderer-own-election   m_renderer_own_election
run_mutation 7-rasters-offer-default   m_rasters_offer_by_default

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
