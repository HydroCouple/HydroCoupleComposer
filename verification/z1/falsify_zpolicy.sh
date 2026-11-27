#!/bin/bash
# Falsification: placement is orthogonal to extrusion, and every mode lands.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
LOG=verification/z1/falsify_zpolicy.log
: > "$LOG"

FILES=(src/layers/featurelayer.cpp src/layers/gdalrasterlayer.cpp
       include/scene/scenesource.h)

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

  if ! cmake --build $BUILD --target test_drape test_groundplane test_scene3d -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $BUILD/tests/gui/test_drape 2>&1; timeout 300 $BUILD/tests/gui/test_groundplane 2>&1; timeout 300 $BUILD/tests/gui/test_scene3d 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Constant reads the terrain anyway.
m_constant_drapes() {
  perl -0pi -e 's/        return m_zPolicy\.mode == ZMode::OnTerrain \? context\.terrain : nullptr;/        return context.terrain;/' include/scene/scenesource.h
}

# 2. FromAttribute reads the offset and forgets the field.
m_attribute_ignored() {
  perl -0pi -e 's/        flatBase = \(numeric \? value : 0\.0\) \+ policy\.offset;/        flatBase = policy.offset;/' src/layers/featurelayer.cpp
}

# 3. The drape offset is dropped.
m_offset_dropped() {
  perl -0pi -e 's/        if \(policy\.mode == ZMode::OnTerrain\)\n        \{\n          if \(!qFuzzyIsNull\(policy\.offset\)\)\n          \{\n            for \(double &z : ground\)\n            \{\n              z \+= policy\.offset;\n            \}\n          \}\n        \}/        if (policy.mode == ZMode::OnTerrain)\n        \{\n        \}/' src/layers/featurelayer.cpp
}

# 4. Extrusion still requires the terrain placement -- the old conflation.
m_extrusion_needs_terrain() {
  perl -0pi -e 's/    const bool extruding =\n      supportsExtrusion\(\) && !qFuzzyIsNull\(m_extrusionHeight\);/    const bool extruding = zPolicy().mode == ZMode::OnTerrain \&\&\n      supportsExtrusion() \&\& !qFuzzyIsNull(m_extrusionHeight);/' src/layers/featurelayer.cpp
}

# 5. A surface's constant datum is ignored: the plane stays wherever the
#    terrain put it.
m_surface_ignores_constant() {
  perl -0pi -e 's/    const double lift = zPolicy\(\)\.mode == ZMode::Constant\n                          \? zPolicy\(\)\.constant\n                          : zPolicy\(\)\.offset;/    const double lift = zPolicy().mode == ZMode::Constant\n                          ? 0.0\n                          : zPolicy().offset;/' src/layers/gdalrasterlayer.cpp
}

# 6. Setting the same policy announces anyway, so opening and closing the
#    dialog repaints the world.
m_same_policy_announces() {
  perl -0pi -e 's/  void FeatureLayer::setZPolicy\(const ZPolicy &policy\)\n  \{\n    if \(policy == zPolicy\(\)\)\n    \{\n      return;\n    \}\n/  void FeatureLayer::setZPolicy(const ZPolicy \&policy)\n  \{\n/' src/layers/featurelayer.cpp
}

run_mutation 1-constant-drapes        m_constant_drapes
run_mutation 2-attribute-ignored      m_attribute_ignored
run_mutation 3-offset-dropped         m_offset_dropped
run_mutation 4-extrusion-needs-terrain m_extrusion_needs_terrain
run_mutation 5-surface-ignores-constant m_surface_ignores_constant
run_mutation 6-same-policy-announces  m_same_policy_announces

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
