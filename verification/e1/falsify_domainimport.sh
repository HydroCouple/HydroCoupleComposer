#!/bin/bash
# E1c falsification: features already on the map become part of the domain.
#
# Every mutation still imports something, and most still import the right
# NUMBER of rings. What they break is which ring is the ground, which is a
# hole, whether a ring that encloses nothing is counted, and whether the
# coordinates are the map's or the file's.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_domainimport
LOG=verification/e1/falsify_domainimport.log
: > "$LOG"

FILES=(src/mesh/domainimport.cpp src/mesh/meshdomain.cpp)

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

  if ! cmake --build $BUILD --target test_domainimport -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] DomainImport' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── what a ring is ──────────────────────────────────────────────────────────

# 1. The ring keeps the closing point OGR repeats, so every imported shape
#    has a corner sitting on another and a zero-length edge in it.
m_ring_not_opened() {
  perl -0pi -e 's/          const QPolygonF ring = openedRing\(piece\);/          const QPolygonF ring = piece;/' src/mesh/domainimport.cpp
}

# 2. Rings are placed by the order they arrived in rather than by what
#    contains them -- the rule a MultiPolygon breaks.
m_depth_from_part_order() {
  perl -0pi -e 's/          const int depth = nestingDepth\(rings, ring\);/          const int depth = ring;/' src/mesh/domainimport.cpp
}

# 3. Anything inside anything is a hole, so ground standing in a hole is cut
#    away as another hole.
m_islands_are_holes() {
  perl -0pi -e 's/          else if \(depth == 1\)/          else if (depth >= 1)/' src/mesh/domainimport.cpp
}

# 4. Rings that enclose no ground are imported, so three collinear points
#    become a hole that cuts nothing.
m_degenerate_kept() {
  perl -0pi -e 's/          if \(ringEnclosesArea\(ring\)\)/          if (true)/' src/mesh/domainimport.cpp
}

# 5. "Encloses ground" is answered with the bounding box, which three
#    collinear points have a perfectly good one of.
m_area_by_bounding_box() {
  perl -0pi -e 's/    return ring\.size\(\) >= 3 && allFinite\(ring\)\n           && std::abs\(signedDoubleArea\(ring\)\) >= kAreaEpsilon;/    return ring.size() >= 3 \&\& allFinite(ring)\n           \&\& !ring.boundingRect().isEmpty();/' src/mesh/meshdomain.cpp
}

# ── which ring is the ground ────────────────────────────────────────────────

# 6. The first outer ring becomes the boundary rather than the largest.
m_first_ring_wins() {
  perl -0pi -e 's/        domain\.boundary = \*widest;/        domain.boundary = outers.first();/' src/mesh/domainimport.cpp
  perl -0pi -e 's/        outers\.erase\(widest\);/        outers.erase(outers.begin());/' src/mesh/domainimport.cpp
}

# 7. Rings are compared by signed area, so a ring wound the other way round
#    -- which is how a shapefile winds one -- is the smallest of any set.
m_signed_area_comparison() {
  perl -0pi -e 's/            return std::abs\(signedDoubleArea\(left\)\)\n                   < std::abs\(signedDoubleArea\(right\)\);/            return signedDoubleArea(left) < signedDoubleArea(right);/' src/mesh/domainimport.cpp
}

# 8. The ring that became the boundary is left among the outers, so the
#    domain also has a hole in exactly its own shape.
m_boundary_also_a_hole() {
  perl -0pi -e 's/        outers\.erase\(widest\);//' src/mesh/domainimport.cpp
}

# 9. A polygon's own holes are dropped on a boundary import, so a catchment
#    with a lake in it is meshed straight over the lake.
m_inner_rings_dropped() {
  perl -0pi -e 's/        for \(const QPolygonF &ring : std::as_const\(inners\)\)\n        \{\n          domain\.holes\.append\(ring\);\n          \+\+result\.holes;\n        \}//' src/mesh/domainimport.cpp
}

# 10. On a hole import the interior rings are cut out too, taking away the
#     ground they stand on instead of being reported.
m_hole_import_cuts_islands() {
  perl -0pi -e 's/        result\.islands \+= int\(inners\.size\(\)\);/        for (const QPolygonF \&ring : std::as_const(inners)) { domain.holes.append(ring); ++result.holes; }/' src/mesh/domainimport.cpp
}

# ── what is read, and from where ────────────────────────────────────────────

# 11. The whole layer is imported rather than the selection, so a click on
#     one catchment brings in every catchment in the file.
m_selection_ignored() {
  perl -0pi -e 's/      QList<int> ordered\(layer\.selection\(\)\.begin\(\), layer\.selection\(\)\.end\(\)\);/      QList<int> ordered;\n      for (int i = 0; i < layer.features().size(); ++i) { ordered.append(i); }/' src/mesh/domainimport.cpp
}

# 12. The selection is walked in whatever order its hashes fall in.
m_selection_unsorted() {
  perl -0pi -e 's/      std::sort\(ordered\.begin\(\), ordered\.end\(\)\);//' src/mesh/domainimport.cpp
}

# 13. Any geometry will do for any part, so a stream network is imported as
#     a boundary and silently encloses nothing.
m_kind_unchecked() {
  perl -0pi -e 's/    if \(layer\.geometryKind\(\) != wanted\)/    if (false)/' src/mesh/domainimport.cpp
}

# 14. The layer's own coordinates are taken instead of the map's, so the
#     domain lands somewhere else entirely.
m_unprojected_geometry() {
  perl -0pi -e 's/    const QVector<QVector<QPolygonF>> &geometry = layer\.projectedFeatures\(\);/    QVector<QVector<QPolygonF>> geometry;\n    for (const VectorFeature \&f : layer.features()) { geometry.append(f.parts); }/' src/mesh/domainimport.cpp
}

# 15. A breakline is opened like a ring, so a constraint line that closes on
#     itself loses its last vertex and stops closing.
m_breakline_opened() {
  perl -0pi -e 's/            domain\.constraintLines\.append\(line\);/            domain.constraintLines.append(openedRing(line));/' src/mesh/domainimport.cpp
}

# 16. Only the first piece of a feature is read, so a multipoint forces one
#     point and a multi-line feature contributes one line.
m_first_piece_only() {
  perl -0pi -e 's/      for \(int piece = 0; piece < parts\.size\(\); \+\+piece\)\n      \{\n        switch \(part\)/      for (int piece = 0; piece < std::min(1, int(parts.size())); ++piece)\n      {\n        switch (part)/' src/mesh/domainimport.cpp
}

# 17. The domain is handed over once per ring, so the map redraws through
#     every half-finished state of the import.
m_written_per_ring() {
  perl -0pi -e 's/        domain\.holes\.append\(ring\);\n        \+\+result\.holes;\n      \}\n\n      if \(part == DomainPart::Boundary\)/        domain.holes.append(ring);\n        ++result.holes;\n        model.setDomain(domain);\n      }\n\n      if (part == DomainPart::Boundary)/' src/mesh/domainimport.cpp
}

# 18. What was left out is not said, so an import that dropped nine of
#     twelve rings reads exactly like a file that held three.
m_losses_unreported() {
  perl -0pi -e 's/    if \(result\.skipped > 0\)/    if (false)/' src/mesh/domainimport.cpp
  perl -0pi -e 's/    if \(result\.islands > 0\)/    if (false)/' src/mesh/domainimport.cpp
}

run_mutation "1-closing-point-kept"              m_ring_not_opened
run_mutation "2-rings-placed-by-arrival-order"   m_depth_from_part_order
run_mutation "3-islands-cut-out-as-holes"        m_islands_are_holes
run_mutation "4-groundless-rings-imported"       m_degenerate_kept
run_mutation "5-area-answered-by-bounding-box"   m_area_by_bounding_box
run_mutation "6-first-ring-wins-not-largest"     m_first_ring_wins
run_mutation "7-compared-by-signed-area"         m_signed_area_comparison
run_mutation "8-boundary-also-left-as-a-hole"    m_boundary_also_a_hole
run_mutation "9-polygon-holes-dropped"           m_inner_rings_dropped
run_mutation "10-hole-import-cuts-its-islands"   m_hole_import_cuts_islands
run_mutation "11-selection-ignored"              m_selection_ignored
run_mutation "12-selection-unsorted"             m_selection_unsorted
run_mutation "13-geometry-kind-unchecked"        m_kind_unchecked
run_mutation "14-unprojected-geometry"           m_unprojected_geometry
run_mutation "15-breakline-opened-like-a-ring"   m_breakline_opened
run_mutation "16-only-the-first-piece-read"      m_first_piece_only
run_mutation "17-model-written-once-per-ring"    m_written_per_ring
run_mutation "18-losses-not-reported"            m_losses_unreported

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_domainimport -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
