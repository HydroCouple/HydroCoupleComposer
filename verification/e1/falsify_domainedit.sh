#!/bin/bash
# E1b-3 falsification: the reach, and the edit gestures built on it.
#
# Every mutation still edits the domain and still looks right on the map.
# What they break is which vertex was reached, where a corner lands, what a
# shape is left as when it can no longer be one, and whether the thing on
# screen mid-drag is the domain or a copy of it.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_domainedit
LOG=verification/e1/falsify_domainedit.log
: > "$LOG"

FILES=(src/mesh/domainsnap.cpp src/mesh/domainedittool.cpp
       src/mesh/meshdomainmodel.cpp src/mesh/meshdomain.cpp)

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

  if ! cmake --build $BUILD --target test_domainedit -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] DomainEdit' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── the reach ───────────────────────────────────────────────────────────────

# 1. The reach is a ground distance after all, so it is right at one zoom and
#    wrong at every other.
m_tolerance_not_converted() {
  perl -0pi -e 's/    return pixels \/ scale;/    return pixels;/' src/mesh/domainsnap.cpp
}

# 2. An unusable scale is not guarded, so a view with no transform yet snaps
#    the whole domain onto the first click.
m_tolerance_unguarded() {
  perl -0pi -e 's/    if \(scale <= 0\.0 \|\| !std::isfinite\(scale\)\)\n    \{\n      return 0\.0;\n    \}\n\n//' src/mesh/domainsnap.cpp
}

# 3. The first vertex within reach wins rather than the nearest, so a click
#    between two corners grabs whichever the walk reached first.
m_first_hit_wins() {
  perl -0pi -e 's/      if \(distance <= best\)\n      \{\n        best = distance;/      if (distance <= best \&\& !result.hit)\n      {\n        best = distance;/' src/mesh/domainsnap.cpp
}

# 4. The dragged vertex is within reach of itself, so it snaps back onto its
#    own starting point and small corrections cannot be made at all.
m_no_exclusion() {
  perl -0pi -e 's/      if \(addresses\.at\(index\) == exclude\)\n      \{\n        continue;\n      \}\n\n//' src/mesh/domainsnap.cpp
}

# 5. The projection onto an edge is not clamped to it, so a click a long way
#    past the end of a segment inserts a corner on the line it is not on.
m_unclamped_projection() {
  perl -0pi -e 's/      along = std::clamp\(along, 0\.0, 1\.0\);\n\n//' src/mesh/domainsnap.cpp
}

# 6. A ring is walked like a line, so its closing edge -- which is drawn --
#    cannot be clicked.
m_ring_not_closed() {
  perl -0pi -e 's/        const int edges = ring \? int\(vertices\.size\(\)\)\n                               : int\(vertices\.size\(\)\) - 1;/        const int edges = int(vertices.size()) - 1;/' src/mesh/domainsnap.cpp
}

# 7. A breakline is walked like a ring, so an edge is offered between its two
#    ends where nothing is drawn.
m_line_closed() {
  perl -0pi -e 's/      const bool ring = part != DomainPart::Breaklines;/      const bool ring = true;/' src/mesh/domainsnap.cpp
}

# 8. The address an edge hit reports is the edge's first vertex rather than
#    the corner the insert would become, so the corner lands one edge over.
m_edge_address_off_by_one() {
  perl -0pi -e 's/            result\.at = DomainVertex\{part, shape, edge \+ 1\};/            result.at = DomainVertex{part, shape, edge};/' src/mesh/domainsnap.cpp
}

# 9. The forced points are searched as though they were a ring, so an edge is
#    reported between two points nothing joins.
m_points_have_edges() {
  perl -0pi -e 's/        case DomainPart::ForcedPoints:\n          return \{\};/        case DomainPart::ForcedPoints:\n          return {QPolygonF(domain.points)};/' src/mesh/domainsnap.cpp
  perl -0pi -e 's/    for \(const DomainPart part :\n         \{DomainPart::Boundary, DomainPart::Holes, DomainPart::Breaklines\}\)/    for (const DomainPart part :\n         {DomainPart::Boundary, DomainPart::Holes, DomainPart::Breaklines,\n          DomainPart::ForcedPoints})/' src/mesh/domainsnap.cpp
}

# ── the model's vertex operations ───────────────────────────────────────────

# 10. A move to where the vertex already is is announced, so a slow drag
#     repaints the map once per mouse move for nothing.
m_move_announces_nothing() {
  perl -0pi -e 's/    if \(!ring \|\| at\.vertex >= ring->size\(\) \|\| ring->at\(at\.vertex\) == to\)/    if (!ring || at.vertex >= ring->size())/' src/mesh/meshdomainmodel.cpp
}

# 11. A forced point is addressed as though the point list were a ring, so
#     dragging one does nothing at all.
m_point_move_folded_into_rings() {
  perl -0pi -e 's/    if \(at\.part == DomainPart::ForcedPoints\)\n    \{\n      if \(at\.vertex != 0 \|\| at\.shape >= m_domain\.points\.size\(\)\n          \|\| m_domain\.points\.at\(at\.shape\) == to\)\n      \{\n        return false;\n      \}\n\n      m_domain\.points\[at\.shape\] = to;\n\n      Q_EMIT domainChanged\(\);\n\n      return true;\n    \}\n\n//' src/mesh/meshdomainmodel.cpp
}

# 12. One past the last is refused, so the corner a ring's closing edge asks
#     for can never be appended.
m_no_append() {
  perl -0pi -e 's/    if \(!ring \|\| at\.vertex > ring->size\(\)\)/    if (!ring || at.vertex >= ring->size())/' src/mesh/meshdomainmodel.cpp
}

# 13. A shape below the count its part needs is kept, so a two-cornered hole
#     stays in the domain looking like a hole.
m_broken_shape_kept() {
  perl -0pi -e 's/    if \(ring->size\(\) - 1 < minimumVertices\(at\.part\)\)/    if (false)/' src/mesh/meshdomainmodel.cpp
}

# 14. Every removal takes the whole shape, so correcting one corner of a
#     boundary deletes the boundary.
m_removal_always_takes_the_shape() {
  perl -0pi -e 's/    if \(ring->size\(\) - 1 < minimumVertices\(at\.part\)\)/    if (true)/' src/mesh/meshdomainmodel.cpp
}

# 15. The last hole goes rather than the one addressed, so editing an early
#     hole deletes a later one.
m_wrong_hole_removed() {
  perl -0pi -e 's/          m_domain\.holes\.removeAt\(at\.shape\);/          m_domain.holes.removeLast();/' src/mesh/meshdomainmodel.cpp
}

# 16. Every part demands three vertices, so a breakline is deleted the moment
#     it is straightened and a forced point cannot be the one thing it is.
m_uniform_minimum() {
  perl -0pi -e 's/      case DomainPart::Breaklines:\n        return 2;\n\n      case DomainPart::ForcedPoints:\n        return 1;/      case DomainPart::Breaklines:\n      case DomainPart::ForcedPoints:\n        return 3;/' src/mesh/meshdomain.cpp
}

# ── the gestures ────────────────────────────────────────────────────────────

# 17. The drag is held in the tool and written at the end, so what is on the
#     map during it is a copy that can disagree with the domain.
m_drag_committed_at_the_end() {
  perl -0pi -e 's/namespace HydroCouple::Composer\n\{\n/namespace HydroCouple::Composer\n{\n  static QPointF s_pendingDrag;\n\n/' src/mesh/domainedittool.cpp
  perl -0pi -e 's/    m_model->moveVertex\(m_grabbed, snap\.hit \? snap\.point : world\);/    s_pendingDrag = snap.hit ? snap.point : world;/' src/mesh/domainedittool.cpp
  perl -0pi -e 's/    m_dragging = false;\n    m_grabbed = DomainVertex\{\};\n\n    refreshHandles\(\);\n\n    return true;\n  \}\n\n  void DomainEditTool::cancel/    m_model->moveVertex(m_grabbed, s_pendingDrag);\n\n    m_dragging = false;\n    m_grabbed = DomainVertex{};\n\n    refreshHandles();\n\n    return true;\n  }\n\n  void DomainEditTool::cancel/' src/mesh/domainedittool.cpp
}

# 18. A right-click on a vertex removes nothing, so the gesture is inert.
m_right_click_inert() {
  perl -0pi -e 's/        m_hovered = DomainVertex\{\};\n        m_model->removeVertex\(hit\.at\);/        m_hovered = DomainVertex{};/' src/mesh/domainedittool.cpp
}

# 19. A right-click that missed is not taken, so it falls through to whatever
#     the canvas would otherwise make of it.
m_right_click_falls_through() {
  perl -0pi -e 's/      return true;\n    \}\n\n    if \(event->button\(\) != Qt::LeftButton\)/      return hit.hit;\n    }\n\n    if (event->button() != Qt::LeftButton)/' src/mesh/domainedittool.cpp
}

# 20. A press that reached neither a vertex nor an edge starts a drag anyway,
#     so the tool holds the mouse over empty ground.
m_press_on_nothing_grabs() {
  perl -0pi -e 's/      if \(!edge\.hit \|\| !m_model->insertVertex\(edge\.at, edge\.point\)\)\n      \{\n        return false;\n      \}\n\n//' src/mesh/domainedittool.cpp
}

# 21. An interrupted drag is left where the pointer happened to be, so
#     reaching for the ribbon is an edit.
m_cancel_does_not_restore() {
  perl -0pi -e 's/    if \(m_dragging && m_model\)\n    \{\n      m_model->moveVertex\(m_grabbed, m_original\);\n    \}\n\n//' src/mesh/domainedittool.cpp
}

# 22. The handles outlive the tool, so the map offers grab points nothing is
#     listening for.
m_handles_outlive_the_tool() {
  perl -0pi -e 's/    \/\/ The handles belong to the tool, not to the domain: leaving them behind\n    \/\/ would offer the user something to grab that nothing is listening for\.\n    m_canvas->setVertexHandles\(\{\}, -1\);//' src/mesh/domainedittool.cpp
}

# 23. Nothing is published until something changes, so picking the editor up
#     shows the user nothing to grab.
m_no_handles_at_pickup() {
  perl -0pi -e 's/    \/\/ Published straight away: picking up the editor is how the user asks to\n    \/\/ see what there is to grab\.\n    refreshHandles\(\);//' src/mesh/domainedittool.cpp
}

# 24. The handles are published once and never again, so they stay where the
#     vertices used to be.
m_handles_never_refreshed() {
  perl -0pi -e 's/      connect\(m_model, &MeshDomainModel::domainChanged, this,\n              \[this\] \{ refreshHandles\(\); \}\);/      \/\/ mutation: not connected/' src/mesh/domainedittool.cpp
}

# 25. The marked handle is indexed by the vertex's place in its own shape
#     rather than in the published list, so the mark sits on another part.
m_active_index_from_shape() {
  perl -0pi -e 's/      vertices, active\.isValid\(\) \? int\(m_handles\.indexOf\(active\)\) : -1\);/      vertices, active.isValid() ? active.vertex : -1);/' src/mesh/domainedittool.cpp
}

# 26. The hover is set but never cleared, so the mark stays on the last
#     vertex the pointer touched for the rest of the session.
m_hover_never_cleared() {
  perl -0pi -e 's/      m_hovered = hover\.hit \? hover\.at : DomainVertex\{\};/      if (hover.hit) { m_hovered = hover.at; }/' src/mesh/domainedittool.cpp
}

run_mutation "1-reach-not-converted-to-ground"   m_tolerance_not_converted
run_mutation "2-unusable-scale-unguarded"        m_tolerance_unguarded
run_mutation "3-first-hit-wins-not-nearest"      m_first_hit_wins
run_mutation "4-dragged-vertex-snaps-to-itself"  m_no_exclusion
run_mutation "5-projection-not-clamped-to-edge"  m_unclamped_projection
run_mutation "6-ring-closing-edge-unclickable"   m_ring_not_closed
run_mutation "7-breakline-closed-like-a-ring"    m_line_closed
run_mutation "8-edge-address-off-by-one"         m_edge_address_off_by_one
run_mutation "9-forced-points-given-edges"       m_points_have_edges
run_mutation "10-unchanged-move-announced"       m_move_announces_nothing
run_mutation "11-point-move-folded-into-rings"   m_point_move_folded_into_rings
run_mutation "12-one-past-the-last-refused"      m_no_append
run_mutation "13-broken-shape-kept"              m_broken_shape_kept
run_mutation "14-removal-always-takes-the-shape" m_removal_always_takes_the_shape
run_mutation "15-wrong-hole-removed"             m_wrong_hole_removed
run_mutation "16-every-part-demands-three"       m_uniform_minimum
run_mutation "17-drag-committed-at-the-end"      m_drag_committed_at_the_end
run_mutation "18-right-click-removes-nothing"    m_right_click_inert
run_mutation "19-missed-right-click-falls-through" m_right_click_falls_through
run_mutation "20-press-on-nothing-grabs"         m_press_on_nothing_grabs
run_mutation "21-cancel-does-not-restore"        m_cancel_does_not_restore
run_mutation "22-handles-outlive-the-tool"       m_handles_outlive_the_tool
run_mutation "23-no-handles-at-pickup"           m_no_handles_at_pickup
run_mutation "24-handles-never-refreshed"        m_handles_never_refreshed
run_mutation "25-mark-indexed-within-its-shape"  m_active_index_from_shape
run_mutation "26-hover-never-cleared"            m_hover_never_cleared

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_domainedit -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
