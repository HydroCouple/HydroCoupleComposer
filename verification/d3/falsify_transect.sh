#!/bin/bash
# D3d falsification: a section line cut through a layered mesh.
#
# Every mutation below still produces a section -- cells of the right count,
# over a plausible range, drawn in plausible colours. The gates compare
# against the fixture's arithmetic: which column the line met first, how far
# along it that was, which end of the column is the surface, and what colour
# the map would have given the cell.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_transect
LOG=verification/d3/falsify_transect.log
: > "$LOG"

FILES=(src/results/transect.cpp src/layers/meshlayer.cpp
       src/ui/panels/transectpanel.cpp src/map/maptool.cpp
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
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_transect -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] Transect' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── the slice ───────────────────────────────────────────────────────────────

# 1. Spans are sorted by cell rather than left in the order the line met
#    them, so a section drawn right to left comes out mirrored.
m_spans_sorted_by_ring() {
  perl -0pi -e 's/    travelled \+= length;/    std::sort(spans.begin(), spans.end(), [](const TransectSpan &a, const TransectSpan &b) { return a.ring < b.ring; });\n    travelled += length;/' src/results/transect.cpp
}

# 2. The interval is tested at its own boundary rather than at its middle.
#    Only the middle is guaranteed to be strictly inside, and the boundary is
#    exactly where two cells meet -- so this is ambiguous precisely where the
#    answer matters.
m_sampled_at_the_boundary() {
  perl -0pi -e 's/        const QPointF middle = segment\.pointAt\(\(from \+ to\) \/ 2\.0\);/        const QPointF middle = segment.pointAt(from);/' src/results/transect.cpp
}

# 2b. Inside and outside alternate by parity from the start of the segment --
#     the scanline shortcut. It agrees with the right answer whenever the
#     line starts outside and crosses cleanly, and disagrees on a line that
#     starts inside a cell or runs along a shared edge.
m_inside_by_parity() {
  perl -0pi -e 's/        const QPointF middle = segment\.pointAt\(\(from \+ to\) \/ 2\.0\);/        if ((step % 2) == 0) { continue; }\n        const QPointF middle = segment.pointAt((from + to) \/ 2.0);/' src/results/transect.cpp
}

# 3. The full scan is dropped, so a line lying wholly inside one cell -- which
#    crosses nothing -- reports an empty section.
m_no_full_scan() {
  perl -0pi -e 's/          for \(int ring = 0; ring < rings\.size\(\); \+\+ring\)\n          \{\n            if \(rings\.at\(ring\)\.containsPoint\(middle, Qt::OddEvenFill\)\)\n            \{\n              found = ring;\n              break;\n            \}\n          \}//' src/results/transect.cpp
}

# 4. Distance restarts at each segment, so a polyline's later cells are
#    reported at the distances of its first segment.
m_distance_not_accumulated() {
  perl -0pi -e 's/        const double start = travelled \+ from \* length;\n        const double end = travelled \+ to \* length;/        const double start = from * length;\n        const double end = to * length;/' src/results/transect.cpp
}

# 5. Abutting spans of one cell are never merged, so a polyline bending
#    inside a cell draws a seam through the middle of it.
m_never_merged() {
  perl -0pi -e 's/        if \(!spans\.isEmpty\(\) && spans\.back\(\)\.ring == found/        if (false \&\& !spans.isEmpty() \&\& spans.back().ring == found/' src/results/transect.cpp
}

# ── the layer's section ─────────────────────────────────────────────────────

# 6. The ring index is used as the column, so a mesh whose features and faces
#    diverge is cut on the wrong columns.
m_ring_index_is_column() {
  perl -0pi -e 's/      const int column = columns\.at\(span\.ring\);/      const int column = span.ring;/' src/layers/meshlayer.cpp
}

# 7. The column is read top-down from the bed, so every section is drawn
#    upside down.
m_column_inverted() {
  perl -0pi -e 's/        cell\.topElevation = m_layering\.z\(column, layer\);\n        cell\.bottomElevation = m_layering\.z\(column, layer \+ 1\);/        cell.topElevation = m_layering.z(column, m_layering.layerCount - layer);\n        cell.bottomElevation = m_layering.z(column, m_layering.layerCount - layer - 1);/' src/layers/meshlayer.cpp
}

# 8. The length is the extent of the cells rather than of the line, so a line
#    that began over nothing is trimmed to where the mesh starts.
m_length_from_cells() {
  perl -0pi -e 's/      section\.length \+= QLineF\(line\.at\(vertex\), line\.at\(vertex \+ 1\)\)\.length\(\);/      section.length += 0.0;/' src/layers/meshlayer.cpp
  perl -0pi -e 's/    if \(section\.cells\.isEmpty\(\)\)/    for (const TransectCell \&c : section.cells) { section.length = std::max(section.length, c.endDistance); }\n    if (section.cells.isEmpty())/' src/layers/meshlayer.cpp
}

# 9. A layering with no values is cut anyway, so the section is a picture of
#    a model that computed zeros.
m_missing_values_cut() {
  perl -0pi -e 's/    if \(m_cellValues\.size\(\) != m_layering\.cellCount\(\)\)\n    \{\n      \/\/ Said rather than cut.*?\n      return false;\n    \}/    if (m_cellValues.size() != m_layering.cellCount()) { const_cast<QVector<double> \&>(m_cellValues).resize(int(m_layering.cellCount())); }/s' src/layers/meshlayer.cpp
}

# 10. A flat mesh is cut from whatever the empty layering holds.
m_flat_is_cut() {
  perl -0pi -e 's/    if \(!isLayered\(\)\)\n    \{\n      message = QObject::tr\("..%1.. has no vertical layering to cut\."\)\n                  \.arg\(name\(\)\);\n      return false;\n    \}//' src/layers/meshlayer.cpp
}

# 11. A degenerate line -- a click, or one point -- is cut as though it were
#     a section.
m_degenerate_line_cut() {
  perl -0pi -e 's/    if \(line\.size\(\) < 2\)\n    \{\n      message = QObject::tr\("A section needs a line of at least two points\."\);\n      return false;\n    \}//' src/layers/meshlayer.cpp
}

# ── the panel and the tool ──────────────────────────────────────────────────

# 12. The section keeps the values it was first cut with, so stepping a run
#     animates a map above a section that never moves.
m_section_never_recut() {
  perl -0pi -e 's/      connect\(m_model, &LayerStackModel::renderChanged, this,\n              \[this\] \{ refresh\(\); \}\);//' src/ui/panels/transectpanel.cpp
}

# 13. A cleared line leaves the last section standing over ground nobody is
#     looking at.
m_cleared_line_kept() {
  perl -0pi -e 's/    if \(m_line\.size\(\) < 2\)\n    \{\n      showMessage\(tr\("Draw a section line across ..%1.. to cut through it\."\)\n                    \.arg\(source->name\(\)\)\);\n      return;\n    \}/    if (m_line.size() < 2) { return; }/' src/ui/panels/transectpanel.cpp
}

# 14. Cells are filled from a colour of the panel's own rather than from the
#     layer's classification, so the section and the map disagree while both
#     look authoritative.
m_section_has_its_own_ramp() {
  perl -0pi -e 's/      const QColor color =\n        m_layer \? m_layer->colorForCellValue\(cell\.value\) : QColor\(Qt::gray\);/      const QColor color(int(cell.value) % 256, 128, 64);/' src/ui/panels/transectpanel.cpp
}

# 15. Elevation runs down the widget rather than up, so the bed is drawn
#     above the surface.
m_elevation_upside_down() {
  perl -0pi -e 's/      return area\.bottom\(\) - area\.height\(\) \* \(\(elevation - lowest\) \/ span\);/      return area.top() + area.height() * ((elevation - lowest) \/ span);/' src/ui/panels/transectpanel.cpp
}

# 16. The drawn line is kept in pixels rather than in world coordinates, so
#     a section slides off the ground it was cut across when the view pans.
m_line_kept_in_pixels() {
  perl -0pi -e 's/    m_canvas->setTransectLine\(\n      QPolygonF\(\{m_canvas->transform\(\)\.toWorld\(m_origin\),\n                 m_canvas->transform\(\)\.toWorld\(event->pos\(\)\)\}\)\);\n\n    return true;\n  \}\n\n  QCursor TransectTool::idleCursor/    m_canvas->setTransectLine(\n      QPolygonF({QPointF(m_origin), QPointF(event->pos())}));\n\n    return true;\n  }\n\n  QCursor TransectTool::idleCursor/s' src/map/maptool.cpp
}

# 17. The drag is judged on its bounding box, so a section cut straight down
#     a channel -- a vertical drag, no width at all -- is thrown away.
m_vertical_drag_discarded() {
  perl -0pi -e 's/    if \(std::abs\(travelled\.x\(\)\) <= kClickSlopPixels\n        && std::abs\(travelled\.y\(\)\) <= kClickSlopPixels\)/    if (std::abs(travelled.x()) <= kClickSlopPixels\n        || std::abs(travelled.y()) <= kClickSlopPixels)/' src/map/maptool.cpp
}

# 18. A tool swapped mid-drag leaves its half-drawn line on the map.
m_half_drawn_line_left() {
  perl -0pi -e 's/    if \(m_drawing\)\n    \{\n      m_drawing = false;\n      m_canvas->setTransectLine\(\{\}\);\n    \}//' src/map/maptool.cpp
}

# 19. The cancel clears unconditionally, so a finished section disappears the
#     moment the user goes back to panning.
m_finished_line_cleared() {
  perl -0pi -e 's/    if \(m_drawing\)\n    \{\n      m_drawing = false;\n      m_canvas->setTransectLine\(\{\}\);\n    \}/    m_drawing = false;\n    m_canvas->setTransectLine({});/' src/map/maptool.cpp
}

# 20. The outgoing tool is never told the gesture is over, so a half-drawn
#     line outlives the tool that was drawing it.
m_tool_never_cancelled() {
  perl -0pi -e 's/    if \(m_tool\)\n    \{\n      m_tool->cancel\(\);\n    \}//' src/map/mapcanvas.cpp
}

run_mutation "1-spans-sorted-by-cell"           m_spans_sorted_by_ring
run_mutation "2-interval-sampled-at-its-edge"   m_sampled_at_the_boundary
run_mutation "2b-inside-decided-by-parity"      m_inside_by_parity
run_mutation "3-line-inside-one-cell-lost"      m_no_full_scan
run_mutation "4-distance-restarts-per-segment"  m_distance_not_accumulated
run_mutation "5-abutting-spans-never-merged"    m_never_merged
run_mutation "6-ring-index-used-as-column"      m_ring_index_is_column
run_mutation "7-column-read-upside-down"        m_column_inverted
run_mutation "8-length-trimmed-to-the-cells"    m_length_from_cells
run_mutation "9-missing-values-cut-as-zeros"    m_missing_values_cut
run_mutation "10-flat-mesh-is-cut"              m_flat_is_cut
run_mutation "11-degenerate-line-is-cut"        m_degenerate_line_cut
run_mutation "12-section-never-recut"           m_section_never_recut
run_mutation "13-cleared-line-keeps-section"    m_cleared_line_kept
run_mutation "14-section-has-its-own-ramp"      m_section_has_its_own_ramp
run_mutation "15-elevation-drawn-upside-down"   m_elevation_upside_down
run_mutation "16-line-kept-in-pixels"           m_line_kept_in_pixels
run_mutation "17-vertical-drag-discarded"       m_vertical_drag_discarded
run_mutation "18-half-drawn-line-left-behind"   m_half_drawn_line_left
run_mutation "19-finished-line-cleared"         m_finished_line_cleared
run_mutation "20-outgoing-tool-not-cancelled"   m_tool_never_cancelled

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_transect -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
