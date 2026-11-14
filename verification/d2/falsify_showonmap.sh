#!/bin/bash
# D2a falsification: a recorded item becomes a layer on the map.
#
# The bridge between browsing a run and seeing it. Every mutation below still
# produces a map with something drawn on it -- which is why the gates compare
# against the manifest rather than counting features.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_runbrowser
LOG=verification/d2/falsify_showonmap.log
: > "$LOG"

FILES=(src/ui/composermainwindow.cpp src/ui/panels/runbrowserpanel.cpp)

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

  if ! cmake --build $BUILD --target test_runbrowser -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] RunBrowser' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. An item with no geometry is handed to the layer anyway, so a CSV of
#    values fails as though the run were broken instead of being explained.
m_no_spatial_check() {
  perl -0pi -e 's/    if \(!DataItemLayer::isSpatial\(item\)\)\n    \{\n      \/\/ Said plainly/    if (false)\n    {\n      \/\/ Said plainly/' src/ui/composermainwindow.cpp
}

# 2. The layer does not say which run it came from, so two runs of the same
#    model produce two layers with the same name -- and comparing them is
#    what holding two runs open is for.
m_no_run_in_name() {
  perl -0pi -e 's/    layer->setName\(tr\("%1 — %2 — %3"\)\n                     \.arg\(session->title\(\), componentId, layer->name\(\)\)\);/    layer->setName(tr("%1 — %2").arg(componentId, layer->name()));/' src/ui/composermainwindow.cpp
}

# 3. The layer is built and never added, so the run browser reports success
#    and the map is unchanged.
m_never_added() {
  perl -0pi -e 's/    m_layerStack->addLayer\(added\);//' src/ui/composermainwindow.cpp
}

# 4. The map is not brought forward, so a layer added from a dock at the
#    bottom of the window lands behind whichever tab is showing.
m_no_bring_forward() {
  perl -0pi -e 's/    \/\/ Brought forward, because a layer added from a dock at the bottom of the\n    \/\/ window is otherwise drawn behind whichever tab happens to be showing\.\n    m_workspace->setCurrentWidget\(m_mapCanvas\);//' src/ui/composermainwindow.cpp
}

# 5. Any selected row offers to be drawn, including a component row -- which
#    carries a component id and no item to ask for.
m_any_row_shows() {
  perl -0pi -e 's/    return index\.data\(RunBrowserModel::ItemIdRole\)\.toString\(\)\.isEmpty\(\)\n             \? QModelIndex\(\)\n             : index;/    return index;/' src/ui/panels/runbrowserpanel.cpp
}

# 6. The request names the row that was clicked rather than the item's own
#    identifiers, so the wrong recorded variable is drawn.
m_wrong_ids() {
  perl -0pi -e 's/                index\.data\(RunBrowserModel::ItemIdRole\)\.toString\(\)\);/                QString());/' src/ui/panels/runbrowserpanel.cpp
}

run_mutation "1-no-geometry-check"        m_no_spatial_check
run_mutation "2-layer-does-not-name-run"  m_no_run_in_name
run_mutation "3-layer-never-added"        m_never_added
run_mutation "4-map-not-brought-forward"  m_no_bring_forward
run_mutation "5-any-row-can-be-shown"     m_any_row_shows
run_mutation "6-request-names-no-item"    m_wrong_ids

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_runbrowser -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
