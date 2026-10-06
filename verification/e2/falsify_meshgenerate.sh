#!/bin/bash
# E2a falsification: the domain is meshed by the SDK, not by us.
#
# Every mutation still returns a mesh, and most still return one with a
# plausible number of triangles in it. What they break is whether the mesh
# is the SDK's own answer, whether every part of the domain reached it, and
# whether the options were passed through or decided here.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_meshgenerate
LOG=verification/e2/falsify_meshgenerate.log
: > "$LOG"

FILES=(src/mesh/meshgenerator.cpp)

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

  if ! cmake --build $BUILD --target test_meshgenerate -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!; local waited=0
  while kill -0 $pid 2>/dev/null && [ $waited -lt 240 ]; do sleep 1; waited=$((waited+1)); done
  if kill -0 $pid 2>/dev/null; then
    kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
    echo "MUTATION $name: CAUGHT (hung)" | tee -a "$LOG"; return
  fi
  wait $pid; local status=$?
  local out; out=$(cat "$out_file"); rm -f "$out_file"
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] MeshGenerate' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The domain is not checked first, so a domain that is not one reaches
#    the triangulator and comes back in the triangulator's words.
m_no_validation() {
  perl -0pi -e 's/    if \(!domain\.isValid\(why\)\)/    if (false)/' src/mesh/meshgenerator.cpp
}

# 2. The refusal is rewritten here, so it drifts away from what the domain
#    itself says is wrong with it.
m_own_refusal_wording() {
  perl -0pi -e 's/      result\.message = why;/      result.message = QObject::tr("The domain cannot be meshed.");/' src/mesh/meshgenerator.cpp
}

# 3. The edge-length limit is dropped on the way through, so the refinement
#    control the user set does nothing.
m_edge_limit_dropped() {
  perl -0pi -e 's/    if \(!Tools::Triangulator::triangulate\(domain\.toTriangulationInput\(\),/    auto _in = domain.toTriangulationInput();\n    _in.maxEdgeLength = 0.0;\n    if (!Tools::Triangulator::triangulate(_in,/' src/mesh/meshgenerator.cpp
}

# 4. The forced interior points are dropped, so a point the user placed is
#    not a node.
m_points_dropped() {
  perl -0pi -e 's/    if \(!Tools::Triangulator::triangulate\(domain\.toTriangulationInput\(\),/    auto _in = domain.toTriangulationInput();\n    _in.interiorPoints.clear();\n    if (!Tools::Triangulator::triangulate(_in,/' src/mesh/meshgenerator.cpp
}

# 5. The holes are dropped, so the mesh covers ground the domain cut out.
m_holes_dropped() {
  perl -0pi -e 's/    if \(!Tools::Triangulator::triangulate\(domain\.toTriangulationInput\(\),/    auto _in = domain.toTriangulationInput();\n    _in.holes.clear();\n    if (!Tools::Triangulator::triangulate(_in,/' src/mesh/meshgenerator.cpp
}

# 6. The breaklines are dropped, so the mesh does not follow the lines it
#    was told to follow.
m_constraints_dropped() {
  perl -0pi -e 's/    if \(!Tools::Triangulator::triangulate\(domain\.toTriangulationInput\(\),/    auto _in = domain.toTriangulationInput();\n    _in.constraintSegments.clear();\n    if (!Tools::Triangulator::triangulate(_in,/' src/mesh/meshgenerator.cpp
}

# 7. The quad option is ignored, so asking for quads gives triangles.
m_quads_ignored() {
  perl -0pi -e 's/    if \(options\.quadDominant\)/    if (false)/' src/mesh/meshgenerator.cpp
}

# 8. Quads are merged whether or not they were asked for.
m_quads_always() {
  perl -0pi -e 's/    if \(options\.quadDominant\)/    if (true)/' src/mesh/meshgenerator.cpp
}

# 9. The quality threshold is decided here rather than passed through, so
#    the only control over the merge does nothing.
m_threshold_hardcoded() {
  perl -0pi -e 's/            result\.mesh, options\.minQuadQuality, merged, message\)\)/            result.mesh, 0.5, merged, message))/' src/mesh/meshgenerator.cpp
}

# 10. The merged mesh is computed and thrown away, so quad-dominant returns
#     the triangulation and says it merged.
m_merge_discarded() {
  perl -0pi -e 's/      result\.mesh = std::move\(merged\);//' src/mesh/meshgenerator.cpp
}

# 11. The faces are counted before the merge, so the counts describe a mesh
#     that is no longer the one being handed back.
m_counted_before_merge() {
  perl -0pi -e 's/    if \(options\.quadDominant\)\n    \{\n      IO::MeshDefinition merged;/    countFaces(result.mesh, result.triangles, result.quads);\n\n    if (options.quadDominant)\n    {\n      IO::MeshDefinition merged;/' src/mesh/meshgenerator.cpp
  perl -0pi -e 's/    countFaces\(result\.mesh, result\.triangles, result\.quads\);\n\n    result\.ok = true;/    result.ok = true;/' src/mesh/meshgenerator.cpp
}

# 12. Quads are counted as triangles too, so the counts do not add up to the
#     mesh they describe.
m_miscounted_faces() {
  perl -0pi -e 's/        if \(width == 3\)/        if (width >= 3)/' src/mesh/meshgenerator.cpp
}

# 13. A run the user stopped is reported as a run that failed, so they go
#     looking for the cause of a failure they caused on purpose.
m_cancel_called_a_failure() {
  perl -0pi -e 's/      if \(message == "cancelled"\)\n      \{\n        result\.cancelled = true;\n        result\.message = QObject::tr\("Meshing was cancelled\."\);\n        result\.mesh = \{\};\n\n        return result;\n      \}\n\n//' src/mesh/meshgenerator.cpp
}

# 14. The watcher is not passed to the triangulator, so nothing is reported
#     and a cancel during the long phase does nothing.
m_progress_not_forwarded() {
  perl -0pi -e 's/                                          result\.mesh, message, progress\)\)/                                          result.mesh, message))/' src/mesh/meshgenerator.cpp
}

# 15. The watcher is not passed to the quad merge, so the second half of a
#     quad-dominant run cannot be stopped.
m_progress_not_forwarded_to_merge() {
  perl -0pi -e 's/            result\.mesh, options\.minQuadQuality, merged, message, progress\)\)/            result.mesh, options.minQuadQuality, merged, message))/' src/mesh/meshgenerator.cpp
}

# 16. A merge the user stopped keeps the triangulation that preceded it, so
#     a cancelled run still hands back a mesh.
m_cancelled_merge_keeps_triangles() {
  perl -0pi -e 's/        if \(message == "cancelled"\)\n        \{\n          result\.cancelled = true;\n          result\.message = QObject::tr\("Meshing was cancelled\."\);\n          result\.mesh = \{\};/        if (message == "cancelled")\n        {\n          result.cancelled = true;\n          result.message = QObject::tr("Meshing was cancelled.");/' src/mesh/meshgenerator.cpp
}

run_mutation "1-domain-not-validated-first"   m_no_validation
run_mutation "2-refusal-reworded-here"        m_own_refusal_wording
run_mutation "3-edge-length-limit-dropped"    m_edge_limit_dropped
run_mutation "4-forced-points-dropped"        m_points_dropped
run_mutation "5-holes-dropped"                m_holes_dropped
run_mutation "6-breaklines-dropped"           m_constraints_dropped
run_mutation "7-quad-option-ignored"          m_quads_ignored
run_mutation "8-quads-merged-regardless"      m_quads_always
run_mutation "9-quality-threshold-hardcoded"  m_threshold_hardcoded
run_mutation "10-merged-mesh-discarded"       m_merge_discarded
run_mutation "11-faces-counted-before-merge"  m_counted_before_merge
run_mutation "12-quads-counted-as-triangles"  m_miscounted_faces
run_mutation "13-cancel-reported-as-failure"  m_cancel_called_a_failure
run_mutation "14-watcher-not-forwarded"       m_progress_not_forwarded
run_mutation "15-watcher-not-forwarded-to-merge" m_progress_not_forwarded_to_merge
run_mutation "16-cancelled-merge-keeps-triangles" m_cancelled_merge_keeps_triangles

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_meshgenerate -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
