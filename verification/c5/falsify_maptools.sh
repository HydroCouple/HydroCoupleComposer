#!/bin/bash
# C5c falsification run: the map's gesture sets, and drag zoom.
#
# The degenerate-drag mutation is the one to watch: a zero-area QRect reports
# itself null, so the guard that looks obviously right is true for a rectangle
# that is merely thin -- the same trap C4a paid for with QRectF and a point
# layer's bounds.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_maptools
LOG=verification/c5/falsify_maptools.log
: > "$LOG"

FILES=(src/map/maptool.cpp src/map/mapcanvas.cpp
       src/layers/featurelayer.cpp)

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

  if ! cmake --build $BUILD --target test_maptools -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  # Judged on exit status as well as output. One of these mutations makes the
  # suite die rather than fail -- a tool destroyed mid-gesture leaves a band
  # behind that the next event writes through -- and a harness that only
  # greps for [  FAILED  ] reads that crash as a survivor.
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] MapTools' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The degenerate-drag guard written the way that looks right: isNull() is
#    true for a zero-area rectangle, so a one-pixel drag is thrown away -- and
#    a genuine click frames a rectangle of nothing.
m_isnull_guard() {
  perl -0pi -e 's/    if \(rectangle\.width\(\) > kClickSlopPixels &&\n        rectangle\.height\(\) > kClickSlopPixels\)/    if (!rectangle.isNull())/' src/map/maptool.cpp
}

# 2. A click under the zoom tool frames the degenerate rectangle instead of
#    zooming about the point.
m_no_click_zoom() {
  perl -0pi -e 's/    m_canvas->zoomAtPixel\(kClickZoomFactor, pixel\);/    m_canvas->zoomToScreenRect(QRect(pixel, pixel));/' src/map/maptool.cpp
}

# 3. The dragged rectangle is framed with the data margin, so the view is
#    always wider than what was asked for.
m_margin() {
  perl -0pi -e 's/    setVisibleExtent\(world\);/    setVisibleExtent(world, 0.25);/' src/map/mapcanvas.cpp
}

# 4. The band is never hidden when the gesture ends.
m_band_persists() {
  perl -0pi -e 's/    if \(m_band\)\n    \{\n      m_band->hide\(\);\n    \}//' src/map/maptool.cpp
}

# 5. Switching tools keeps the old tool alive, so its band outlives it.
m_keep_old_tool() {
  perl -0pi -e 's/      case MapToolKind::Pan:\n        m_tool = std::make_unique<PanTool>\(this\);\n        break;/      case MapToolKind::Pan:\n        if (!m_tool) m_tool = std::make_unique<PanTool>(this);\n        break;/' src/map/mapcanvas.cpp
}

# 6. The pan tool stops identifying on a click.
m_pan_no_pick() {
  perl -0pi -e 's/      m_canvas->pickAndSelectAt\(event->pos\(\)\);/      ;/' src/map/maptool.cpp
}

# 7. The pan tool picks on every release, so a drag that ends over a feature
#    selects it.
m_pick_always() {
  perl -0pi -e 's/    if \(std::abs\(travelled\.x\(\)\) <= kClickSlopPixels &&\n        std::abs\(travelled\.y\(\)\) <= kClickSlopPixels\)/    if (true)/' src/map/maptool.cpp
}

# 8. Panning drags the world the wrong way.
m_pan_inverted() {
  perl -0pi -e 's/    m_canvas->panByPixels\(QPointF\(event->pos\(\) - m_lastPosition\)\);/    m_canvas->panByPixels(QPointF(m_lastPosition - event->pos()));/' src/map/maptool.cpp
}

# 9. The tool switch does nothing, so both faces leave the map as it was.
m_switch_dead() {
  perl -0pi -e 's/      case MapToolKind::ZoomIn:\n        m_tool = std::make_unique<ZoomInTool>\(this\);\n        break;/      case MapToolKind::ZoomIn:\n        m_tool = std::make_unique<PanTool>(this);\n        break;/' src/map/mapcanvas.cpp
}

# 10. The rubber band tracks the wrong corner, so dragging up and left draws
#     nothing.
m_band_not_normalized() {
  perl -0pi -e 's/    m_band->setGeometry\(QRect\(m_origin, event->pos\(\)\)\.normalized\(\)\);/    m_band->setGeometry(QRect(m_origin, m_origin));/' src/map/maptool.cpp
}


# 12. The band's bounds test uses QRectF::intersects(), which answers false
#     for an empty rectangle -- and the bounds of a vertical conduit, a
#     horizontal one or a point are all empty. This is the defect the slice
#     shipped with until its own test caught it.
m_intersects_bounds() {
  perl -0pi -e 's/        if \(bounds\.left\(\) > box\.right\(\) \|\| bounds\.right\(\) < box\.left\(\)\n            \|\| bounds\.top\(\) > box\.bottom\(\) \|\| bounds\.bottom\(\) < box\.top\(\)\)/        if (!box.intersects(bounds))/' src/layers/featurelayer.cpp
}

# 13. The band takes only features with a vertex inside it, so a conduit that
#     runs clean across the box is missed.
m_vertices_only() {
  perl -0pi -e 's/          hit = crossesRect\(part, box\)/          hit = false \&\& crossesRect(part, box)/' src/layers/featurelayer.cpp
}

# 14. The band replaces the selection with one feature rather than all of
#     them, which is what the old single-selection stack did.
m_band_selects_one() {
  perl -0pi -e 's/        m_model->selectOnly\(features, caught\);/        m_model->selectOnly(features, *caught.constBegin());/' src/map/mapcanvas.cpp
}

# 15. The band selects in every layer it crosses rather than the topmost that
#     catches anything, so the table shows a fraction of the highlight.
m_band_all_layers() {
  perl -0pi -e 's/        m_model->selectOnly\(features, caught\);\n\n        \/\/ The count, not one index: a band that caught forty features has\n        \/\/ no single feature to name, and -1 would read as "nothing"\.\n        Q_EMIT featurePicked\(features, \*caught\.constBegin\(\)\);\n\n        return;/        features->setSelection(caught);/' src/map/mapcanvas.cpp
}

# 16. Zooming out ignores how big a box was dragged and steps by two.
m_zoomout_fixed_step() {
  perl -0pi -e 's/    zoomAtPixel\(factor, rectangle\.center\(\)\);/    zoomAtPixel(0.5, rectangle.center());/' src/map/mapcanvas.cpp
}

# 17. Zooming out fits the box into the viewport instead of the other way
#     round, so it zooms in.
m_zoomout_inverted() {
  perl -0pi -e 's/      std::min\(double\(rectangle\.width\(\)\) \/ double\(width\(\)\),\n               double\(rectangle\.height\(\)\) \/ double\(height\(\)\)\);/      std::min(double(width()) \/ double(rectangle.width()),\n               double(height()) \/ double(rectangle.height()));/' src/map/mapcanvas.cpp
}

# 18. A click under Zoom Out zooms in.
m_zoomout_click_in() {
  perl -0pi -e 's/    m_canvas->zoomAtPixel\(1\.0 \/ kClickZoomFactor, pixel\);/    m_canvas->zoomAtPixel(kClickZoomFactor, pixel);/' src/map/maptool.cpp
}

# 19. The Select tool's click stops selecting.
m_select_click_dead() {
  perl -0pi -e 's/  void SelectTool::useClick\(const QPoint &pixel\)\n  \{\n    m_canvas->pickAndSelectAt\(pixel\);/  void SelectTool::useClick(const QPoint \&pixel)\n  {\n    Q_UNUSED(pixel)/' src/map/maptool.cpp
}

run_mutation "1-isnull-degenerate-guard"  m_isnull_guard
run_mutation "2-click-frames-nothing"     m_no_click_zoom
run_mutation "3-dragged-rect-gets-margin" m_margin
run_mutation "4-band-never-hidden"        m_band_persists
run_mutation "5-switch-keeps-old-tool"    m_keep_old_tool
run_mutation "6-pan-stops-identifying"    m_pan_no_pick
run_mutation "7-pan-picks-after-a-drag"   m_pick_always
run_mutation "8-pan-inverted"             m_pan_inverted
run_mutation "9-tool-switch-is-dead"      m_switch_dead
run_mutation "10-band-tracks-one-corner"  m_band_not_normalized
run_mutation "12-intersects-empty-bounds" m_intersects_bounds
run_mutation "13-band-takes-vertices-only" m_vertices_only
run_mutation "14-band-selects-only-one"   m_band_selects_one
run_mutation "15-band-selects-every-layer" m_band_all_layers
run_mutation "16-zoomout-fixed-step"      m_zoomout_fixed_step
run_mutation "17-zoomout-inverted"        m_zoomout_inverted
run_mutation "18-zoomout-click-zooms-in"  m_zoomout_click_in
run_mutation "19-select-click-dead"       m_select_click_dead

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_maptools -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
