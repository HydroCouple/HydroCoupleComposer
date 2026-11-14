#!/bin/bash
# Falsification: elevations from a coverage onto a mesh's vertices.
#
# Every mutation still produces a mesh with numbers in nodeZ. What they
# break is whether those numbers are the ground under each vertex, whether
# a hole in the survey is admitted, and whether a mesh in another system is
# lined up before it is read.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_terrainsampler
LOG=verification/e6/falsify_terrainsampler.log
: > "$LOG"

FILES=(src/mesh/terrainsampler.cpp src/layers/gdalrasterlayer.cpp src/layers/meshlayer.cpp)

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

  if ! cmake --build $BUILD --target test_terrainsampler -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$($TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] TerrainSampler' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── is it the ground under the vertex ───────────────────────────────────────

# 1. Sample nearest-neighbour, so a continuous surface comes back in
#    half-cell terraces.
m_nearest_not_bilinear() {
  perl -0pi -e 's/      const double top_ = v00 \+ \(v10 - v00\) \* fx;\n      const double bottom_ = v01 \+ \(v11 - v01\) \* fx;\n\n      values\[i\] = top_ \+ \(bottom_ - top_\) \* fy;/      values[i] = v00;/' src/layers/gdalrasterlayer.cpp
}

# 2. Drop the half-cell offset, so every elevation is read from the cell up
#    and to the left of the one the vertex is actually in.
m_no_cell_centre() {
  perl -0pi -e 's/      return \(point\.x\(\) - geotransform\[0\]\) \/ geotransform\[1\] - 0\.5;/      return (point.x() - geotransform[0]) \/ geotransform[1];/' src/layers/gdalrasterlayer.cpp
  perl -0pi -e 's/      return \(point\.y\(\) - geotransform\[3\]\) \/ geotransform\[5\] - 0\.5;/      return (point.y() - geotransform[3]) \/ geotransform[5];/' src/layers/gdalrasterlayer.cpp
}

# 3. Do not reproject, so a mesh in degrees lands nowhere near a raster in
#    metres and quietly misses every vertex.
m_no_reprojection() {
  perl -0pi -e 's/    if \(meshCrs && raster\.crs\(\) && !meshCrs->isSameAs\(\*raster\.crs\(\)\)\)/    if (false)/' src/mesh/terrainsampler.cpp
}

# ── is a hole admitted ──────────────────────────────────────────────────────

# 4. Ignore the no-data value, so a hole comes back as the largest float
#    there is rather than as nothing.
m_ignore_nodata() {
  perl -0pi -e 's/      if \(hasNoData && qFuzzyCompare\(value, noData\)\)\n      \{\n        return std::numeric_limits<double>::quiet_NaN\(\);\n      \}//' src/layers/gdalrasterlayer.cpp
}

# 5. Read a no-data cell as zero, which in the Netherlands is an ordinary
#    elevation -- so a hole in the survey becomes a plausible hole in the
#    ground.
m_nodata_is_zero() {
  perl -0pi -e 's/      if \(hasNoData && qFuzzyCompare\(value, noData\)\)\n      \{\n        return std::numeric_limits<double>::quiet_NaN\(\);\n      \}/      if (hasNoData \&\& qFuzzyCompare(value, noData))\n      {\n        return 0.0;\n      }/' src/layers/gdalrasterlayer.cpp
}

# 6. Fill a missed vertex with zero, which in the Netherlands is a
#    perfectly ordinary elevation and so cannot be told from a reading.
m_zero_the_misses() {
  perl -0pi -e 's/    mesh\.nodeZ\.assign\(count, std::numeric_limits<double>::quiet_NaN\(\)\);/    mesh.nodeZ.assign(count, 0.0);/' src/mesh/terrainsampler.cpp
}

# 7. Count a miss as a sample, so the report says the mesh is fully
#    covered when it is not.
m_count_misses_as_samples() {
  perl -0pi -e 's/        \+\+result\.missed;\n\n        continue;/        ++result.sampled;\n\n        continue;/' src/mesh/terrainsampler.cpp
}

# ── refusing rather than pretending ─────────────────────────────────────────

# 8. Report success when nothing at all was sampled, leaving a mesh of NaNs
#    and no explanation.
m_empty_is_success() {
  perl -0pi -e 's/    if \(result\.sampled == 0\)/    if (false)/' src/mesh/terrainsampler.cpp
}

# 9. Accept a mesh with no vertices as a job done.
m_empty_mesh_ok() {
  perl -0pi -e 's/    if \(count == 0 \|\| mesh\.nodeY\.size\(\) != count\)/    if (false)/' src/mesh/terrainsampler.cpp
}

# 10. Read only the cell block that happens to be first, ignoring the
#     margin bilinear needs, so vertices near the block edge lose their
#     neighbours.
m_no_margin() {
  perl -0pi -e 's/    int left = static_cast<int>\(std::floor\(minimumColumn\)\) - 1;/    int left = static_cast<int>(std::floor(minimumColumn)) + 1;/' src/layers/gdalrasterlayer.cpp
}

# ── what it does to the mesh ────────────────────────────────────────────────

# 11. Take elevations of any length, so a mismatched array pairs each vertex
#     with a stranger's height -- every one of them plausible.
m_accept_any_length() {
  perl -0pi -e 's/    if \(elevations\.size\(\) != m_mesh\.nodeX\.size\(\)\)\n    \{\n      return false;\n    \}/    if (false)\n    {\n      return false;\n    }\n    m_mesh.nodeZ.resize(m_mesh.nodeX.size());/' src/layers/meshlayer.cpp
}

# 12. Set the elevations without telling anyone, so the map goes on drawing
#     the mesh it had until something else happens to repaint it.
m_silent_change() {
  perl -0pi -e 's/    notifyAppearanceChanged\(\);\n\n    return true;\n  \}\n\n  const ITerrainSource \*MeshLayer::terrain/    return true;\n  }\n\n  const ITerrainSource *MeshLayer::terrain/' src/layers/meshlayer.cpp
}

run_mutation 1-nearest-not-bilinear      m_nearest_not_bilinear
run_mutation 2-no-cell-centre            m_no_cell_centre
run_mutation 3-no-reprojection           m_no_reprojection
run_mutation 4-ignore-nodata             m_ignore_nodata
run_mutation 5-nodata-is-zero            m_nodata_is_zero
run_mutation 6-zero-the-misses           m_zero_the_misses
run_mutation 7-count-misses-as-samples   m_count_misses_as_samples
run_mutation 8-empty-is-success          m_empty_is_success
run_mutation 9-empty-mesh-ok             m_empty_mesh_ok
run_mutation 10-no-margin                m_no_margin
run_mutation 11-accept-any-length        m_accept_any_length
run_mutation 12-silent-change            m_silent_change

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
