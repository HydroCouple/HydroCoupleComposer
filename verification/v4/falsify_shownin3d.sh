#!/bin/bash
# Falsification: the 3D half of visibility.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
LOG=verification/v4/falsify_shownin3d.log

# The same platform ctest gives the suite. Without it a windowing test can
# fail for reasons that have nothing to do with a mutation -- which is
# exactly how mutation 5 first claimed a CAUGHT it had not earned: the
# mapcanvas suite fails under a real window server regardless, and the
# mutation was in truth surviving.
export QT_QPA_PLATFORM=offscreen
: > "$LOG"

FILES=(src/map/maplayer.cpp src/scene/scenerenderer.cpp
       src/map/layerstackmodel.cpp src/ui/panels/layertreepanel.cpp)

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

  if ! cmake --build $BUILD --target test_layerstack test_scene3d test_mapcanvas -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $BUILD/tests/gui/test_scene3d 2>&1; timeout 300 $BUILD/tests/gui/test_layerstack 2>&1; timeout 300 $BUILD/tests/gui/test_mapcanvas 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The renderer ignores the flag in rebuildBatches -- the layer stays drawn.
m_batches_ignore_flag() {
  perl -0pi -e '$c=0; s/      if \(!layer->isVisible\(\) \|\| !layer->isShownIn3D\(\)\)/$c++ == 2 ? "      if (!layer->isVisible())" : $&/ge' src/scene/scenerenderer.cpp
}

# 2. The flag leaks into 2D: the map also stops drawing a layer kept out of
#    the scene, so the "3D half" governs both views again.
m_flag_leaks_into_2d() {
  perl -0pi -e 's/  bool MapLayer::isVisible\(\) const\n  \{\n    return m_visible;\n  \}/  bool MapLayer::isVisible() const\n  \{\n    return m_visible \&\& m_shownIn3D;\n  \}/' src/map/maplayer.cpp
}

# 3. Bounds keep counting a layer kept out, so the camera frames geometry
#    that is not there.
m_bounds_ignore_flag() {
  perl -0pi -e '$c=0; s/      if \(!layer->isVisible\(\) \|\| !layer->isShownIn3D\(\)\)/$c++ == 0 ? "      if (!layer->isVisible())" : $&/ge' src/scene/scenerenderer.cpp
}

# 4. Terrain election keeps a surface kept out of the scene, so everything
#    drapes on an invisible ground.
m_terrain_ignores_flag() {
  perl -0pi -e '$c=0; s/      if \(!layer->isVisible\(\) \|\| !layer->isShownIn3D\(\)\)/$c++ == 1 ? "      if (!layer->isVisible())" : $&/ge' src/scene/scenerenderer.cpp
}

# 5. Setting the flag never announces itself, so the scene draws the old
#    stack until something else repaints.
m_silent_flag() {
  perl -0pi -e 's/    m_shownIn3D = shown;\n    Q_EMIT appearanceChanged\(\);/    m_shownIn3D = shown;/' src/map/maplayer.cpp
}

# 6. The model reports every layer as having a 3D form, so the tree offers a
#    live toggle on a point layer that nothing can honour.
m_everything_has_form() {
  perl -0pi -e 's/      case HasSceneFormRole:\n        return layer->sceneSource\(\) != nullptr;/      case HasSceneFormRole:\n        return true;/' src/map/layerstackmodel.cpp
}

# 7. The tooltip claims "Shown in 3D" for a layer with no form at all.
m_tooltip_lies() {
  perl -0pi -e 's/        const QString sceneLine =\n          layer->sceneSource\(\) == nullptr\n            \? tr\("No 3D form"\)\n            : layer->isShownIn3D\(\) \? tr\("Shown in 3D"\) : tr\("Kept out of 3D"\);/        const QString sceneLine =\n          layer->isShownIn3D() ? tr("Shown in 3D") : tr("Kept out of 3D");/' src/map/layerstackmodel.cpp
}

run_mutation 1-batches-ignore-flag   m_batches_ignore_flag
run_mutation 2-flag-leaks-into-2d    m_flag_leaks_into_2d
run_mutation 3-bounds-ignore-flag    m_bounds_ignore_flag
run_mutation 4-terrain-ignores-flag  m_terrain_ignores_flag
run_mutation 5-silent-flag           m_silent_flag
run_mutation 6-everything-has-form   m_everything_has_form
run_mutation 7-tooltip-lies          m_tooltip_lies

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
