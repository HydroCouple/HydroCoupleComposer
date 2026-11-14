#!/bin/bash
# Follow-up falsification: a mesh item is drawn on the entity its values are
# attached to.
#
# The mesh here has 4 nodes, 5 edges and 2 faces, so drawing the wrong entity
# is visible in the count. That is the whole reason the fixture was chosen: on
# a single triangle, nodes and edges are both three, and every mutation below
# would produce a map that counted correctly and meant nothing.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_dataitemlayers
LOG=verification/d2/falsify_meshentity.log
: > "$LOG"

FILES=(src/layers/dataitemlayer.cpp)

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

  if ! cmake --build $BUILD --target test_dataitemlayers -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 180 ]; do
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] DataItemLayer' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Edge-located values fall through to the patch branch -- the state before
#    this fix: five values drawn on two faces, three of them lost and the two
#    shown attached to the wrong shapes.
m_edges_drawn_as_faces() {
  perl -0pi -e 's/      else if \(attachedTo == Spatial::MeshDataObjectType::Edge\)/      else if (false)/' src/layers/dataitemlayer.cpp
}

# 2. Both ends of every edge come from its origin, so each edge is a zero
#    length line at the right place -- five features, right count, no lines.
m_edge_ends_collapse() {
  perl -0pi -e 's/        part\.append\(QPointF\(xs\[static_cast<size_t>\(destination\)\],\n                            ys\[static_cast<size_t>\(destination\)\]\)\);/        part.append(QPointF(xs[static_cast<size_t>(origin)],\n                            ys[static_cast<size_t>(origin)]));/' src/layers/dataitemlayer.cpp
}

# 3. An endpoint outside the node arrays is read anyway.
m_no_bounds_check() {
  perl -0pi -e 's/        if \(origin < 0 \|\| destination < 0\n            \|\| static_cast<size_t>\(origin\) >= xs\.size\(\)\n            \|\| static_cast<size_t>\(destination\) >= xs\.size\(\)\n            \|\| xs\.size\(\) != ys\.size\(\)\)\n        \{\n          continue;\n        \}//' src/layers/dataitemlayer.cpp
}

# 4. A surface with no edges is drawn as faces instead of being refused, so a
#    map appears that is confidently wrong about what its colours mean.
m_no_edges_falls_back() {
  perl -0pi -e 's/        if \(!appendMeshEdges\(collected, surface->meshView\(\)\)\)/        if (false)/' src/layers/dataitemlayer.cpp
}

# 5. The vertex case is folded in with the faces, so a node-located item
#    draws faces too.
m_vertices_drawn_as_faces() {
  perl -0pi -e 's/      if \(attachedTo == Spatial::MeshDataObjectType::Vertex\)/      if (false)/' src/layers/dataitemlayer.cpp
}

run_mutation "1-edges-drawn-as-faces"     m_edges_drawn_as_faces
run_mutation "2-edge-ends-collapse"       m_edge_ends_collapse
run_mutation "3-endpoint-bounds-unchecked" m_no_bounds_check
run_mutation "4-no-edges-falls-back"      m_no_edges_falls_back
run_mutation "5-vertices-drawn-as-faces"  m_vertices_drawn_as_faces

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_dataitemlayers -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
