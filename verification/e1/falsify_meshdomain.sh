#!/bin/bash
# E1a falsification: the ground a mesh will be generated over.
#
# Every mutation below still produces a domain that looks like a domain --
# right vertex counts, right bounding box, right file on disk. The gates are
# on signed area, orientation, segment counts and what survives a save.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_meshdomain
LOG=verification/e1/falsify_meshdomain.log
: > "$LOG"

FILES=(src/mesh/meshdomain.cpp src/project/presentation.cpp)

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
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_meshdomain -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 240 ]; do
    sleep 1
    waited=$((waited + 1))
  done

  if kill -0 $pid 2>/dev/null; then
    kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
    echo "MUTATION $name: CAUGHT (hung)" | tee -a "$LOG"; return
  fi

  wait $pid
  local status=$?
  local out; out=$(cat "$out_file"); rm -f "$out_file"
  echo "=== $name ===" >> "$LOG"
  echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] MeshDomain' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── area and orientation ────────────────────────────────────────────────────

# 1. Degeneracy judged on the bounding box rather than the area, so three
#    collinear points -- which have a perfectly good box -- pass.
m_bbox_not_area() {
  perl -0pi -e 's/    if \(std::abs\(signedDoubleArea\(boundary\)\) < kAreaEpsilon\)/    if (boundary.boundingRect().isEmpty())/' src/mesh/meshdomain.cpp
}

# 2. The boundary is handed over in whatever orientation it was drawn, so a
#    clockwise one triangulates to nothing from a domain that validated.
m_no_reorientation() {
  perl -0pi -e 's/    if \(signedDoubleArea\(oriented\) < 0\.0\)\n    \{\n      std::reverse\(oriented\.begin\(\), oriented\.end\(\)\);\n    \}//' src/mesh/meshdomain.cpp
}

# 3. The boundary is reversed unconditionally, so a correctly-drawn
#    counter-clockwise ring is turned into a clockwise one.
m_always_reverse() {
  perl -0pi -e 's/    if \(signedDoubleArea\(oriented\) < 0\.0\)\n    \{\n      std::reverse\(oriented\.begin\(\), oriented\.end\(\)\);\n    \}/    std::reverse(oriented.begin(), oriented.end());/' src/mesh/meshdomain.cpp
}

# 4. The shoelace sum does not close the ring, so every area is short by one
#    edge's contribution -- wrong, and still plausibly non-zero.
m_ring_not_closed() {
  perl -0pi -e 's/      const QPointF &next = ring\.at\(\(index \+ 1\) % ring\.size\(\)\);/      const QPointF \&next = ring.at(index + 1 < ring.size() ? index + 1 : index);/' src/mesh/meshdomain.cpp
}

# ── what is refused ─────────────────────────────────────────────────────────

# 5. A hole outside the boundary is accepted; it cuts nothing and the mesh
#    comes out whole, looking entirely correct.
m_hole_containment_unchecked() {
  perl -0pi -e 's/        if \(!boundary\.containsPoint\(corner, Qt::OddEvenFill\)\)/        if (false)/' src/mesh/meshdomain.cpp
}

# 6. A hole with no area is accepted.
m_flat_hole_accepted() {
  perl -0pi -e 's/      if \(hole\.size\(\) < 3 \|\| !allFinite\(hole\)\n          \|\| std::abs\(signedDoubleArea\(hole\)\) < kAreaEpsilon\)/      if (hole.size() < 3)/' src/mesh/meshdomain.cpp
}

# 7. A breakline whose two points coincide is accepted -- the point count a
#    line needs, and no direction to constrain anything along.
m_zero_length_breakline() {
  perl -0pi -e 's/      if \(length <= 0\.0 && line\.size\(\) == 2\)/      if (false)/' src/mesh/meshdomain.cpp
}

# 8. Coordinates are not checked for being numbers, so a NaN from a failed
#    edit reaches the triangulator.
m_nonfinite_accepted() {
  perl -0pi -e 's/    return std::all_of\(points\.begin\(\), points\.end\(\),/    return true; return std::all_of(points.begin(), points.end(),/' src/mesh/meshdomain.cpp
}

# ── conversion ──────────────────────────────────────────────────────────────

# 9. A breakline is handed over as one segment from first point to last, so
#    everything between the ends stops constraining anything.
m_polyline_as_one_segment() {
  perl -0pi -e 's/      for \(int index = 0; index \+ 1 < line\.size\(\); \+\+index\)\n      \{\n        input\.constraintSegments\.push_back\(\n          \{Spatial::Point\(line\.at\(index\)\.x\(\), line\.at\(index\)\.y\(\)\),\n           Spatial::Point\(line\.at\(index \+ 1\)\.x\(\), line\.at\(index \+ 1\)\.y\(\)\)\}\);\n      \}/      if (line.size() >= 2) { input.constraintSegments.push_back({Spatial::Point(line.first().x(), line.first().y()), Spatial::Point(line.last().x(), line.last().y())}); }/' src/mesh/meshdomain.cpp
}

# 10. Every segment starts at the polyline's first point, a fan rather than a
#     chain: the right segment count over the wrong ground.
m_segments_fan_from_first() {
  perl -0pi -e 's/          \{Spatial::Point\(line\.at\(index\)\.x\(\), line\.at\(index\)\.y\(\)\),/          {Spatial::Point(line.first().x(), line.first().y()),/' src/mesh/meshdomain.cpp
}

# 11. The refinement control is dropped on the way to the triangulator, so a
#     domain meshes at its drawn resolution however finely it was specified.
m_max_edge_dropped() {
  perl -0pi -e 's/    input\.maxEdgeLength = maxEdgeLength;/    input.maxEdgeLength = 0.0;/' src/mesh/meshdomain.cpp
}

# ── persistence ─────────────────────────────────────────────────────────────

# 12. maxEdgeLength is not written, so a refinement setting is lost on save.
m_max_edge_not_saved() {
  perl -0pi -e 's/    root\.insert\(QStringLiteral\("max_edge_length"\), maxEdgeLength\);//' src/mesh/meshdomain.cpp
}

# 13. A domain that failed to parse keeps whatever was read before the
#     failure, so a truncated file loads as a smaller domain.
m_half_read_kept() {
  perl -0pi -e 's/      if \(!ringFromJson\(entry, hole, message\)\)\n      \{\n        domain = MeshDomain\{\};\n        return false;\n      \}/      if (!ringFromJson(entry, hole, message)) { return false; }/' src/mesh/meshdomain.cpp
}

# 14. The sidecar reports itself empty when it holds only a domain, so a
#     domain drawn before any component is added is written nowhere.
m_isempty_ignores_domain() {
  perl -0pi -e 's/    return m_components\.isEmpty\(\) && m_meshDomain\.isEmpty\(\);/    return m_components.isEmpty();/' src/project/presentation.cpp
}

# 15. The mesh section is written even when empty, so a sidecar that never
#     had a domain reads as one somebody drew and cleared.
m_section_always_written() {
  perl -0pi -e 's/    if \(hasMeshDomain\(\)\)\n    \{\n      root\.insert\(QStringLiteral\("mesh_domain"\), m_meshDomain\.toJson\(\)\);\n    \}/    root.insert(QStringLiteral("mesh_domain"), m_meshDomain.toJson());/' src/project/presentation.cpp
}

# 16. A sidecar whose mesh section will not parse loses its canvas positions
#     too -- throwing away good state over a bad section.
m_bad_domain_fails_sidecar() {
  perl -0pi -e 's/      if \(!MeshDomain::fromJson\(domain\.toObject\(\), m_meshDomain, message\)\)\n      \{\n        m_meshDomain = MeshDomain\{\};\n      \}/      if (!MeshDomain::fromJson(domain.toObject(), m_meshDomain, message)) { clear(); return false; }/' src/project/presentation.cpp
}

run_mutation "1-degeneracy-from-the-bounding-box" m_bbox_not_area
run_mutation "2-boundary-not-reoriented"          m_no_reorientation
run_mutation "3-boundary-always-reversed"         m_always_reverse
run_mutation "4-shoelace-does-not-close"          m_ring_not_closed
run_mutation "5-hole-containment-unchecked"       m_hole_containment_unchecked
run_mutation "6-flat-hole-accepted"               m_flat_hole_accepted
run_mutation "7-zero-length-breakline-accepted"   m_zero_length_breakline
run_mutation "8-non-numbers-accepted"             m_nonfinite_accepted
run_mutation "9-polyline-as-one-segment"          m_polyline_as_one_segment
run_mutation "10-segments-fan-from-first-point"   m_segments_fan_from_first
run_mutation "11-refinement-dropped"              m_max_edge_dropped
run_mutation "12-refinement-not-saved"            m_max_edge_not_saved
run_mutation "13-half-read-domain-kept"           m_half_read_kept
run_mutation "14-isempty-ignores-the-domain"      m_isempty_ignores_domain
run_mutation "15-mesh-section-always-written"     m_section_always_written
run_mutation "16-bad-domain-loses-positions"      m_bad_domain_fails_sidecar

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_meshdomain -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
