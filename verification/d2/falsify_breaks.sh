#!/bin/bash
# D2b falsification run: class breaks that hold still while the map animates.
#
# The claim is that a graduated theme on a recorded layer means the same thing
# at every step. Every mutation below still produces a colored map -- which is
# exactly the problem: a theme recomputed per frame looks right in any single
# screenshot and is wrong the moment two frames are compared.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_timecontroller
LOG=verification/d2/falsify_breaks.log
: > "$LOG"

FILES=(src/layers/dataitemlayer.cpp src/render/layerstyle.cpp
       src/map/layerstackmodel.cpp)

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

  if ! cmake --build $BUILD --target test_timecontroller -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] TimeController' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The layer answers the classifier with the level on screen -- the state
#    before this slice, and the one that looks correct in any one frame.
m_frame_only() {
  perl -0pi -e 's/    return valuesAcrossTime\(\);/    return FeatureLayer::numericValues(field);/' src/layers/dataitemlayer.cpp
}

# 2. Only the first level is pooled, so a run whose values grow leaves its
#    later features outside the last class -- and undrawn.
m_first_level_only() {
  perl -0pi -e 's/    const int levels = timeCount\(\);/    const int levels = std::min(1, static_cast<int>(timeCount()));/' src/layers/dataitemlayer.cpp
}

# 3. The record is pooled again from level zero on every read, so an
#    animation costs a pass through the run per frame.
m_repool_every_read() {
  perl -0pi -e 's/    for \(int level = m_acrossTimeLevels; level < levels; \+\+level\)/    for (int level = 0; level < levels; ++level)/' src/layers/dataitemlayer.cpp
}

# 4. Every field is answered with the record, so a style naming a column this
#    layer does not have is classified over the values anyway.
m_any_field_pools() {
  perl -0pi -e 's/    if \(!m_item \|\| field != m_valueAttribute \|\| timeCount\(\) < 1\)/    if (!m_item)/' src/layers/dataitemlayer.cpp
}

# 5. A static item takes the pooled path, which has no levels to read: the
#    classifier is handed nothing and the layer loses its classes.
m_static_pools() {
  perl -0pi -e 's/ \|\| timeCount\(\) < 1\)/)/' src/layers/dataitemlayer.cpp
}

# 6. The classifier asks for the values through the non-virtual path, so the
#    layer's answer never reaches it.
m_style_bypasses_provider() {
  perl -0pi -e 's/      return m_classification\.classify\(provider\.numericValues\(m_attribute\)\);/      return m_classification.classify(provider.IAttributeProvider::numericValues(m_attribute));/' src/render/layerstyle.cpp
}

# 7. The legend is answered from the count stored when the layer arrived
#    rather than from the style, so it stops following what is drawn.
m_legend_from_the_cache() {
  perl -0pi -e 's/    const LayerStyle \*style = layer \? layer->style\(\) : nullptr;\n\n    return style \? static_cast<int>\(style->legendItems\(\)\.size\(\)\) : 0;/    return m_legendCounts.value(layer, 0);/' src/map/layerstackmodel.cpp
}

# 8. A re-theme that changes the class count is announced as new text rather
#    than as new rows, so a view keeps addressing legend rows that are gone.
m_legend_not_structural() {
  perl -0pi -e 's/    if \(current != previous\)/    if (false)/' src/map/layerstackmodel.cpp
}

# 9. A legend row is labeled with the layer rather than with the class it
#    stands for -- a legend that names everything and explains nothing.
m_legend_labels_the_layer() {
  perl -0pi -e 's/        case Qt::DisplayRole:\n          return item\.label;/        case Qt::DisplayRole:\n          return owner->name();/' src/map/layerstackmodel.cpp
}

run_mutation "1-classify-the-frame-on-screen" m_frame_only
run_mutation "2-pool-only-the-first-level"    m_first_level_only
run_mutation "3-repool-on-every-read"         m_repool_every_read
run_mutation "4-every-field-pools"            m_any_field_pools
run_mutation "5-static-takes-the-pooled-path" m_static_pools
run_mutation "6-style-bypasses-the-layer"     m_style_bypasses_provider

run_mutation "7-legend-from-the-cache"        m_legend_from_the_cache
run_mutation "8-legend-change-is-not-rows"    m_legend_not_structural
run_mutation "9-legend-labels-the-layer"      m_legend_labels_the_layer

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_timecontroller -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
