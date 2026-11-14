#!/bin/bash
# C3c-1 falsification run.
#
# Each mutation breaks one property the suite claims to hold. A mutation that
# survives means the property is not actually being checked, however many
# assertions mention it. Output lands in verification/c3c/ so it can be read
# rather than trusted.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_drape
LOG=verification/c3c/falsify.log
: > "$LOG"

FILES=(src/layers/meshlayer.cpp src/layers/featurelayer.cpp src/scene/scenerenderer.cpp)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() {
  for f in "${FILES[@]}"; do
    cp "$f.orig" "$f"
    # A restore that only rewrites bytes can leave the mtime inside ninja's
    # resolution, and the next build then links the mutated object.
    touch "$f"
  done
}
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

  if ! cmake --build $BUILD --target test_drape -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT by $(echo "$out" | grep -c '^\[  FAILED  \] Drape') test(s) -- $(echo "$out" | grep '^\[  FAILED  \] Drape' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The drape stops densifying: it samples only the vertices it was given.
m_no_densify() {
  perl -0pi -e 's/      if \(step <= 0\.0 \|\| part\.size\(\) < 2\)\n      \{\n        return QVector<QPointF>\(part\.begin\(\), part\.end\(\)\);\n      \}/      if (true)\n      {\n        return QVector<QPointF>(part.begin(), part.end());\n      }/' src/layers/featurelayer.cpp
}

# 2. The sampler fans from a different corner than the renderer draws.
m_wrong_fan() {
  perl -0pi -e 's/              point, ring\.at\(0\), ring\.at\(corner\), ring\.at\(corner \+ 1\),/              point, ring.at(1), ring.at(corner), ring.at(corner + 1),/' src/layers/meshlayer.cpp
}

# 3. The widened search is dropped: only the nearest centroid is consulted.
m_no_widen() {
  perl -0pi -e 's/    \(void\)m_terrainIndex->tree->radiusSearch\(query, reach, candidates\);/    (void)reach;/' src/layers/meshlayer.cpp
}

# 4. A point exactly on a shared edge belongs to neither face.
m_strict_edges() {
  perl -0pi -e 's/      constexpr double kEdgeTolerance = -1\.0e-9;/      constexpr double kEdgeTolerance = 1.0e-9;/' src/layers/meshlayer.cpp
}

# 5. A sample the terrain refuses falls to zero instead of holding.
m_no_hold() {
  perl -0pi -e 's/        double elevation = held;/        double elevation = 0.0;/' src/layers/featurelayer.cpp
}

# 6. The leading run before the terrain starts is left at zero.
m_no_backfill() {
  perl -0pi -e 's/      for \(int i = 0; i < firstKnown; \+\+i\)\n      \{\n        ground\[i\] = ground\.at\(firstKnown\);\n      \}/      for (int i = 0; i < 0; ++i)\n      {\n        ground[i] = ground.at(firstKnown);\n      }/' src/layers/featurelayer.cpp
}

# 7. The lowest terrain in the stack is chosen instead of the uppermost.
m_first_terrain() {
  perl -0pi -e 's/        if \(const ITerrainSource \*candidate = source->terrain\(\)\)\n        \{\n          terrain = candidate;\n        \}/        if (const ITerrainSource *candidate = source->terrain())\n        {\n          if (!terrain) terrain = candidate;\n        }/' src/scene/scenerenderer.cpp
}

# 8. A hidden layer still counts as terrain. Anchored on resolveTerrain's own
#    local, because sceneBounds() walks the stack with the same three lines and
#    a pattern matching both mutates the wrong function.
m_hidden_terrain() {
  perl -0pi -e 's/    const ITerrainSource \*terrain = nullptr;\n\n    for \(const MapLayer \*layer : m_model->renderOrder\(\)\)\n    \{\n      if \(!layer->isVisible\(\)\)/    const ITerrainSource *terrain = nullptr;\n\n    for (const MapLayer *layer : m_model->renderOrder())\n    {\n      if (false)/' src/scene/scenerenderer.cpp
}

# 9. Lines written at their literal depth, coplanar with the surface.
m_no_nudge() {
  perl -0pi -e 's/    constexpr float kCoplanarNudge = 1\.0e-4f;/    constexpr float kCoplanarNudge = 0.0f;/' src/scene/scenerenderer.cpp
}

# 9b. The nudge inflated until it lifts lines out of the terrain entirely.
m_huge_nudge() {
  perl -0pi -e 's/    constexpr float kCoplanarNudge = 1\.0e-4f;/    constexpr float kCoplanarNudge = 1.0e-1f;/' src/scene/scenerenderer.cpp
}

# 10. The curtain faces along its own line instead of across it.
m_normal_along() {
  perl -0pi -e 's/            const QVector3D normal\(float\(along\.y\(\) \/ length\),\n                                   float\(-along\.x\(\) \/ length\), 0\.0f\);/            const QVector3D normal(float(along.x() \/ length),\n                                   float(along.y() \/ length), 0.0f);/' src/layers/featurelayer.cpp
}

# 11. A mesh with no elevations offers itself as terrain anyway.
m_flat_is_terrain() {
  perl -0pi -e 's/    if \(m_entity != MeshEntity::Face \|\| m_mesh\.faceCount\(\) == 0 \|\|\n        m_mesh\.nodeZ\.empty\(\)\)/    if (m_entity != MeshEntity::Face || m_mesh.faceCount() == 0)/' src/layers/meshlayer.cpp
}

# 12. Flat placement samples the terrain anyway.
m_flat_samples() {
  perl -0pi -e 's/    const ITerrainSource \*terrain =\n      m_drape == SceneDrape::Flat \? nullptr : context\.terrain;/    const ITerrainSource *terrain = context.terrain;/' src/layers/featurelayer.cpp
}

# 13. Resolution reported as the cell's width across rather than its edges.
m_resolution_diameter() {
  perl -0pi -e 's/        edgeSum \+= perimeter \/ double\(corners\);/        edgeSum += 2.0 * radius;/' src/layers/meshlayer.cpp
}

# 14. The curtain's top is not raised: a wall of no height.
m_no_rise() {
  perl -0pi -e 's/                        float\(ground\.at\(i\) \+ m_extrusionHeight\)\),/                        float(ground.at(i))),/' src/layers/featurelayer.cpp
}

run_mutation "1-no-densification"       m_no_densify
run_mutation "2-wrong-fan"              m_wrong_fan
run_mutation "3-no-widened-search"      m_no_widen
run_mutation "4-strict-edge-tolerance"  m_strict_edges
run_mutation "5-no-held-elevation"      m_no_hold
run_mutation "6-no-leading-backfill"    m_no_backfill
run_mutation "7-lowest-terrain-wins"    m_first_terrain
run_mutation "8-hidden-terrain-used"    m_hidden_terrain
run_mutation "9-no-coplanar-nudge"      m_no_nudge
run_mutation "9b-inflated-nudge"        m_huge_nudge
run_mutation "10-curtain-normal-along"  m_normal_along
run_mutation "11-flat-mesh-is-terrain"  m_flat_is_terrain
run_mutation "12-flat-samples-terrain"  m_flat_samples
run_mutation "13-resolution-diameter"   m_resolution_diameter
run_mutation "14-curtain-does-not-rise" m_no_rise

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_drape -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -4
