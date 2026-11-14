#!/bin/bash
# E1b-1 falsification: a mesh domain drawn on the map.
#
# Every mutation still puts features on a layer. The gates are on which part
# went where, what kind each layer reports, whether the views actually follow
# the model, and whether an edit is announced at all.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_domainlayer
LOG=verification/e1/falsify_domainlayer.log
: > "$LOG"

FILES=(src/layers/domainlayer.cpp src/mesh/meshdomainmodel.cpp)

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

  if ! cmake --build $BUILD --target test_domainlayer -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] DomainLayer' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The layer never follows the model, so a domain edited after the layer
#    was built is drawn as it stood when the layer was built.
m_no_follow() {
  perl -0pi -e 's/      connect\(model, &MeshDomainModel::domainChanged, layer\.get\(\),\n              \[raw = layer\.get\(\)\] \{ raw->rebuild\(\); \}\);//' src/layers/domainlayer.cpp
}

# 2. Features are added without clearing, so every edit appends another copy
#    of the whole domain on top of the last.
m_no_clear() {
  perl -0pi -e 's/  void DomainLayer::rebuild\(\)\n  \{\n    clearFeatures\(\);/  void DomainLayer::rebuild()\n  {/' src/layers/domainlayer.cpp
}

# 3. Holes are drawn as lines, so the layer picks as a line layer and a hole
#    stops reading as an absence.
m_holes_as_lines() {
  perl -0pi -e 's/            addRing\(domain\.holes\.at\(index\), GeometryKind::Polygon, index\);/            addRing(domain.holes.at(index), GeometryKind::Line, index);/' src/layers/domainlayer.cpp
}

# 4. Forced points are drawn as polygons, which is the mixed-kind bug the
#    four-layer split exists to prevent.
m_points_as_polygons() {
  perl -0pi -e 's/            addRing\(QPolygonF\(\{domain\.points\.at\(index\)\}\),\n                    GeometryKind::Point, index\);/            addRing(QPolygonF({domain.points.at(index)}), GeometryKind::Polygon, index);/' src/layers/domainlayer.cpp
}

# 5. Every hole feature is built from the first hole: the right count of
#    rings, all of them the same ring.
m_first_hole_only() {
  perl -0pi -e 's/            addRing\(domain\.holes\.at\(index\), GeometryKind::Polygon, index\);/            addRing(domain.holes.first(), GeometryKind::Polygon, index);/' src/layers/domainlayer.cpp
}

# 6. The boundary layer draws the holes too -- a plausible layer with one
#    feature too many.
m_boundary_draws_holes() {
  perl -0pi -e 's/        case DomainPart::Boundary:\n          addRing\(domain\.boundary, GeometryKind::Polygon, 0\);\n          break;/        case DomainPart::Boundary:\n          addRing(domain.boundary, GeometryKind::Polygon, 0);\n          for (const QPolygonF \&h : domain.holes) { addRing(h, GeometryKind::Polygon, 0); }\n          break;/' src/layers/domainlayer.cpp
}

# 7. The layer never asks to be redrawn, so the features are right and the
#    canvas keeps showing the previous domain.
m_no_repaint() {
  perl -0pi -e 's/    notifyAppearanceChanged\(\);//' src/layers/domainlayer.cpp
}

# 8. The extent change is not announced, so a view framing this layer keeps
#    framing the domain as it was several edits ago.
m_no_extent_signal() {
  perl -0pi -e 's/    notifyExtentChanged\(\);//' src/layers/domainlayer.cpp
}

# 9. A model that has gone away is still read from.
m_dangling_model() {
  perl -0pi -e 's/      connect\(model, &QObject::destroyed, layer\.get\(\),\n              \[raw = layer\.get\(\)\]\n              \{\n                raw->m_model = nullptr;\n                raw->rebuild\(\);\n              \}\);//' src/layers/domainlayer.cpp
}

# 10. Every change is announced, including one that changed nothing, so every
#     save of an unchanged domain repaints every view.
m_always_announce() {
  perl -0pi -e 's/    if \(sameDomain\(m_domain, domain\)\)\n    \{\n      return;\n    \}//' src/mesh/meshdomainmodel.cpp
}

# 11. Sameness is judged on counts, so moving a point without adding one is
#     not a change and the map never redraws.
m_sameness_by_counts() {
  perl -0pi -e 's/      return left\.boundary == right\.boundary && left\.holes == right\.holes\n             && left\.constraintLines == right\.constraintLines\n             && left\.points == right\.points\n             && qFuzzyCompare\(left\.maxEdgeLength \+ 1\.0,\n                              right\.maxEdgeLength \+ 1\.0\);/      return left.boundary.size() == right.boundary.size() \&\& left.holes.size() == right.holes.size() \&\& left.constraintLines.size() == right.constraintLines.size() \&\& left.points.size() == right.points.size();/' src/mesh/meshdomainmodel.cpp
}

# 12. The boundary is filled like any polygon, hiding every layer the domain
#     was drawn over.
m_boundary_filled() {
  perl -0pi -e 's/        symbol\.fill = QColor\(0, 0, 0, 0\);/        symbol.fill = QColor(30, 90, 200, 120);/' src/layers/domainlayer.cpp
}

run_mutation "1-layer-never-follows-the-model"  m_no_follow
run_mutation "2-features-appended-not-replaced" m_no_clear
run_mutation "3-holes-drawn-as-lines"           m_holes_as_lines
run_mutation "4-points-drawn-as-polygons"       m_points_as_polygons
run_mutation "5-every-hole-is-the-first-hole"   m_first_hole_only
run_mutation "6-boundary-layer-draws-holes"     m_boundary_draws_holes
run_mutation "7-no-repaint-requested"           m_no_repaint
run_mutation "8-extent-change-not-announced"    m_no_extent_signal
run_mutation "9-dead-model-still-read"          m_dangling_model
run_mutation "10-no-op-edits-announced"         m_always_announce
run_mutation "11-sameness-judged-on-counts"     m_sameness_by_counts
run_mutation "12-boundary-filled-in"            m_boundary_filled

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_domainlayer -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
