#!/bin/bash
# E1b-2 falsification: drawing a domain's parts on the map.
#
# Every mutation still puts a shape into the domain. The gates are on world
# coordinates, on which click ends the gesture, and on what happens to a
# shape the user never finished.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_domaindraw
LOG=verification/e1/falsify_domaindraw.log
: > "$LOG"

FILES=(src/mesh/domaindrawtool.cpp src/mesh/meshdomainmodel.cpp
       src/map/mapcanvas.cpp)

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

  if ! cmake --build $BUILD --target test_domaindraw -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] DomainDraw' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Vertices are stored where they were clicked on screen rather than on the
#    ground, so a shape slides off the map the moment the view pans.
m_pixels_not_world() {
  perl -0pi -e 's/    m_pending\.append\(m_canvas->transform\(\)\.toWorld\(event->pos\(\)\)\);/    m_pending.append(QPointF(event->pos()));/' src/mesh/domaindrawtool.cpp
}

# 2. The rubber segment to the cursor is stored, so the shape grows by a
#    vertex per mouse move and finishes with hundreds.
m_cursor_stored() {
  perl -0pi -e 's/    refreshSketch\(m_canvas->transform\(\)\.toWorld\(event->pos\(\)\), true\);/    m_pending.append(m_canvas->transform().toWorld(event->pos()));\n    refreshSketch({}, false);/' src/mesh/domaindrawtool.cpp
}

# 3. A right-click does not finish, so a shape can never be committed.
m_right_click_inert() {
  perl -0pi -e 's/    if \(event->button\(\) == Qt::RightButton\)\n    \{\n      finish\(\);\n      return true;\n    \}//' src/mesh/domaindrawtool.cpp
}

# 4. The finishing right-click is stored as a vertex too, so every shape has
#    one corner nobody meant to place.
m_finish_click_stored() {
  perl -0pi -e 's/    if \(event->button\(\) == Qt::RightButton\)\n    \{\n      finish\(\);/    if (event->button() == Qt::RightButton)\n    {\n      m_pending.append(m_canvas->transform().toWorld(event->pos()));\n      finish();/' src/mesh/domaindrawtool.cpp
}

# 5. A point waits for a right-click like everything else, so the single
#    click that is the whole gesture does nothing.
m_point_needs_finish() {
  perl -0pi -e 's/    if \(m_part == DomainPart::ForcedPoints\)\n    \{\n      finish\(\);\n      return true;\n    \}//' src/mesh/domaindrawtool.cpp
}

# 6. Shapes with too few vertices are committed, so two clicks make a
#    boundary the user has to discover is degenerate.
m_no_minimum() {
  perl -0pi -e 's/    if \(!m_model \|\| drawn\.size\(\) < minimumVertices\(m_part\)\)/    if (!m_model)/' src/mesh/domaindrawtool.cpp
}

# 7. Every part demands three vertices, so a breakline of two and a point of
#    one are silently thrown away.
m_uniform_minimum() {
  perl -0pi -e 's/      case DomainPart::Breaklines:\n        return 2;\n\n      case DomainPart::ForcedPoints:\n        return 1;/      case DomainPart::Breaklines:\n      case DomainPart::ForcedPoints:\n        return 3;/' src/mesh/domaindrawtool.cpp
}

# 8. Abandoning a shape commits it, so switching tools puts a half-drawn
#    boundary into the domain.
m_cancel_commits() {
  perl -0pi -e 's/    m_pending\.clear\(\);\n    m_canvas->setSketch\(\{\}, false\);\n  \}\n\n  QCursor DomainDrawTool::idleCursor/    finish();\n  }\n\n  QCursor DomainDrawTool::idleCursor/' src/mesh/domaindrawtool.cpp
}

# 9. The sketch is left behind after the shape is committed, so the finished
#    shape is drawn twice -- once by the layer, once by a stale sketch.
m_sketch_not_cleared() {
  perl -0pi -e 's/    m_pending\.clear\(\);\n    m_canvas->setSketch\(\{\}, false\);\n\n    if \(!m_model/    m_pending.clear();\n\n    if (!m_model/' src/mesh/domaindrawtool.cpp
}

# 10. Drawing a boundary appends rather than replaces, so correcting one
#     leaves two rings and a domain that means nothing.
m_boundary_appends() {
  perl -0pi -e 's/    m_domain\.boundary = ring;/    for (const QPointF \&p : ring) { m_domain.boundary.append(p); }/' src/mesh/meshdomainmodel.cpp
}

# 11. A custom tool leaves m_toolKind naming the previous one, so
#     setToolKind() decides the right tool is already installed and the
#     custom one runs for good.
m_custom_tool_untracked() {
  perl -0pi -e 's/    m_tool = std::move\(tool\);\n    m_customTool = true;/    m_tool = std::move(tool);/' src/map/mapcanvas.cpp
}

# 12. Nothing is drawn while a shape is being drawn, so the user clicks
#     blind until the shape appears all at once.
m_no_sketch_shown() {
  perl -0pi -e 's/    m_canvas->setSketch\(shown, closed\);//' src/mesh/domaindrawtool.cpp
}

run_mutation "1-vertices-stored-in-pixels"      m_pixels_not_world
run_mutation "2-cursor-stored-as-a-vertex"      m_cursor_stored
run_mutation "3-right-click-does-not-finish"    m_right_click_inert
run_mutation "4-finishing-click-left-a-corner"  m_finish_click_stored
run_mutation "5-a-point-waits-for-a-right-click" m_point_needs_finish
run_mutation "6-too-few-vertices-committed"     m_no_minimum
run_mutation "7-every-part-demands-three"       m_uniform_minimum
run_mutation "8-abandoning-commits"             m_cancel_commits
run_mutation "9-sketch-left-behind"             m_sketch_not_cleared
run_mutation "10-boundary-appended-not-replaced" m_boundary_appends
run_mutation "11-custom-tool-not-tracked"       m_custom_tool_untracked
run_mutation "12-nothing-drawn-while-drawing"   m_no_sketch_shown

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_domaindraw -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
