#!/bin/bash
# C4a falsification run. Each mutation breaks one property the suite claims.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_picking
LOG=verification/c4/falsify_pick.log
: > "$LOG"

FILES=(src/layers/featurelayer.cpp src/map/mapcanvas.cpp
       include/pick/centroidindex.h)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }

  # A pattern that no longer matches applies nothing and reports nothing, so
  # the run reads as a surviving mutation when it is really a stale script.
  local changed=0
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_picking -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  # Bounded, and judged by the exit status as well as the output. A mutation
  # that makes the suite *die* -- a stack overflow from an echo between two
  # signals, say -- prints no "[  FAILED  ]" line at all, and a harness that
  # only greps for one reads the crash as a mutation that survived.
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] Pick' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The index answers from the nearest centroid alone.
m_no_widen() {
  perl -0pi -e 's/        const double reach = m_maxRadius \+ std::max\(0\.0, extraReach\);/        return -1;\n        const double reach = m_maxRadius + std::max(0.0, extraReach);/' include/pick/centroidindex.h
}

# 2. The widened search is unsorted, so a click between two features takes
#    whichever the tree happened to visit first.
m_unsorted() {
  perl -0pi -e 's/                                   nanoflann::SearchParameters\(0\.0f, true\)\);/                                   nanoflann::SearchParameters(0.0f, false));/' include/pick/centroidindex.h
}

# 3. A feature's reach is measured from a corner rather than the centre, so
#    the widened search is not wide enough.
m_half_reach() {
  perl -0pi -e 's/        0\.5 \* std::hypot\(maximumX - minimumX, maximumY - minimumY\), feature\);/        0.1 * std::hypot(maximumX - minimumX, maximumY - minimumY), feature);/' src/layers/featurelayer.cpp
}

# 4. Zero-area bounding rectangles are dropped -- every point layer with them.
m_drop_degenerate() {
  perl -0pi -e 's/      if \(vertices == 0\)\n      \{\n        continue;\n      \}/      if (vertices == 0 || (maximumX <= minimumX \&\& maximumY <= minimumY))\n      {\n        continue;\n      }/' src/layers/featurelayer.cpp
}

# 5. Distance to a segment is measured to the infinite line through it.
m_infinite_line() {
  perl -0pi -e 's/        0\.0, 1\.0\);/        -1.0e9, 1.0e9);/' src/layers/featurelayer.cpp
}

# 6. A polygon is hit only from inside, never from its edge.
m_inside_only() {
  perl -0pi -e 's/      if \(distanceToOutline\(point, part\) <= tolerance\)\n      \{\n        return true;\n      \}/      if (m_kind != GeometryKind::Polygon \&\&\n          distanceToOutline(point, part) <= tolerance)\n      {\n        return true;\n      }/' src/layers/featurelayer.cpp
}

# 7. The index is never rebuilt, so a feature added later is unpickable.
m_stale_index() {
  perl -0pi -e 's/  void FeatureLayer::addFeature\(VectorFeature feature\)\n  \{\n    m_pickIndexValid = false;/  void FeatureLayer::addFeature(VectorFeature feature)\n  {/' src/layers/featurelayer.cpp
}

# 8. Out-of-range indices are selected as given.
m_unchecked_selection() {
  perl -0pi -e 's/      if \(feature >= 0 && feature < m_features\.size\(\)\)\n      \{\n        kept\.insert\(feature\);\n      \}/      kept.insert(feature);/' src/layers/featurelayer.cpp
}

# 9. Selecting does not announce a redraw.
m_silent_selection() {
  perl -0pi -e 's/    m_selection = std::move\(kept\);\n\n    \/\/ Selection is an appearance/    m_selection = std::move(kept);\n    return;\n\n    \/\/ Selection is an appearance/' src/layers/featurelayer.cpp
}

# 10. A selected feature is drawn like any other.
m_no_highlight() {
  perl -0pi -e 's/        if \(m_selection\.contains\(i\)\)/        if (false)/' src/layers/featurelayer.cpp
}

# 11. The canvas walks the stack bottom-first, so a buried feature wins.
m_bottom_first() {
  perl -0pi -e 's/    for \(MapLayer \*layer : m_model->layers\(\)\)\n    \{\n      if \(!layer->isVisible\(\)\)\n      \{\n        continue;\n      \}\n\n      auto \*features = dynamic_cast<FeatureLayer \*>\(layer\);/    for (MapLayer *layer : m_model->renderOrder())\n    {\n      if (!layer->isVisible())\n      {\n        continue;\n      }\n\n      auto *features = dynamic_cast<FeatureLayer *>(layer);/' src/map/mapcanvas.cpp
}

# 12. A hidden layer is still pickable.
m_pick_hidden() {
  perl -0pi -e 's/    for \(MapLayer \*layer : m_model->layers\(\)\)\n    \{\n      if \(!layer->isVisible\(\)\)\n      \{\n        continue;\n      \}/    for (MapLayer *layer : m_model->layers())\n    {\n      if (false)\n      {\n        continue;\n      }/' src/map/mapcanvas.cpp
}

# 13. A drag selects whatever it started on.
m_drag_selects() {
  perl -0pi -e 's/      if \(std::abs\(travelled\.x\(\)\) <= kClickSlopPixels &&\n          std::abs\(travelled\.y\(\)\) <= kClickSlopPixels\)/      if (true)/' src/map/mapcanvas.cpp
}

# 14. Selecting in one layer leaves the others' selections standing.
m_keep_other_selections() {
  perl -0pi -e 's/              else\n              \{\n                features->clearSelection\(\);\n              \}/              else\n              {\n                ;\n              }/' src/map/mapcanvas.cpp
}

run_mutation "1-nearest-centroid-only"   m_no_widen
run_mutation "2-unsorted-candidates"     m_unsorted
run_mutation "3-reach-too-small"         m_half_reach
run_mutation "4-degenerate-boxes-dropped" m_drop_degenerate
run_mutation "5-infinite-line-distance"  m_infinite_line
run_mutation "6-polygon-inside-only"     m_inside_only
run_mutation "7-stale-pick-index"        m_stale_index
run_mutation "8-unchecked-selection"     m_unchecked_selection
run_mutation "9-silent-selection"        m_silent_selection
run_mutation "10-no-highlight"           m_no_highlight
run_mutation "11-stack-walked-bottom-up" m_bottom_first
run_mutation "12-hidden-layer-pickable"  m_pick_hidden
run_mutation "13-drag-selects"           m_drag_selects
run_mutation "14-other-selections-kept"  m_keep_other_selections

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_picking -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -4
