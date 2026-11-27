#!/bin/bash
# D2 falsification run: showing a chosen time level, and one clock across
# layers recorded on different axes.
#
# The mutations that matter are the ones that still show numbers: a layer
# pinned to the wrong level, or a clock that shares a step number instead of
# an instant, both produce a full and plausible map.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_timecontroller
LOG=verification/d2/falsify_time.log
: > "$LOG"

FILES=(src/layers/dataitemlayer.cpp src/results/timecontroller.cpp)

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

  if ! cmake --build $BUILD --target test_timecontroller --target test_dataitemlayers -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  local caught=0
  local died=0

  for suite in test_timecontroller test_dataitemlayers; do
    local out_file="$LOG.run"
    QT_QPA_PLATFORM=offscreen "$BUILD/tests/gui/$suite" > "$out_file" 2>&1 &
    local pid=$!
    local waited=0

    while kill -0 $pid 2>/dev/null && [ $waited -lt 120 ]; do
      sleep 1
      waited=$((waited + 1))
    done

    if kill -0 $pid 2>/dev/null; then
      kill -9 $pid 2>/dev/null
      wait $pid 2>/dev/null
      echo "MUTATION $name: CAUGHT ($suite hung)" | tee -a "$LOG"
      rm -f "$out_file"
      return
    fi

    wait $pid
    local status=$?

    echo "=== $name / $suite ===" >> "$LOG"
    grep '^\[  FAILED  \]' "$out_file" >> "$LOG"

    if grep -q "\[  FAILED  \]" "$out_file"; then
      caught=1
    elif [ "$status" -ne 0 ]; then
      died=1
    fi

    rm -f "$out_file"
  done

  if [ "$caught" -eq 1 ]; then
    echo "MUTATION $name: CAUGHT" | tee -a "$LOG"
  elif [ "$died" -eq 1 ]; then
    echo "MUTATION $name: CAUGHT (a suite died)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The state before this slice: every non-entity axis pinned to its last
#    index, so a layer shows the end of the run whatever the clock says.
m_always_last() {
  perl -0pi -e 's/        start\[i\] = int\(i\) == timeAxis && timeIndex >= 0\n                     \? std::min<int64_t>\(timeIndex, shape\[i\] - 1\)\n                     : shape\[i\] - 1;/        start[i] = shape[i] - 1;/' src/layers/dataitemlayer.cpp
}

# 2. Time is taken to be some axis other than the one the interface puts it
#    on, so the chosen level is applied to the wrong dimension.
m_wrong_time_axis() {
  perl -0pi -e 's/    constexpr int kTimeAxis = 0;/    constexpr int kTimeAxis = 1;/' src/layers/dataitemlayer.cpp
}

# 3. An unchosen level means the first rather than the last, so a running
#    component shows the beginning of its run.
m_default_first() {
  perl -0pi -e 's/    return m_timeIndex < 0 \? count - 1 : std::min\(m_timeIndex, count - 1\);/    return m_timeIndex < 0 ? 0 : std::min(m_timeIndex, count - 1);/' src/layers/dataitemlayer.cpp
}

# 4. The nearest level is found by index arithmetic rather than by instant,
#    so layers on different axes drift apart.
m_nearest_by_index() {
  perl -0pi -e 's/      const double distance = std::abs\(timeAt\(index\) - julianDay\);/      const double distance = std::abs(double(index) - julianDay);/' src/layers/dataitemlayer.cpp
}

# 5. Setting the instant announces itself but never moves the layers, so the
#    slider slides and the map does not change.
m_share_step_number() {
  perl -0pi -e 's/    m_current = wanted;\n\n    applyToLayers\(\);\n\n    Q_EMIT currentChanged\(m_current\);/    m_current = wanted;\n\n    Q_EMIT currentChanged(m_current);/' src/results/timecontroller.cpp
}

# 6. The span is one layer's rather than every layer's, so a slider cannot
#    reach instants only the second layer recorded.
m_first_layer_only() {
  perl -0pi -e 's/        for \(int index = 0; index < item->timeCount\(\); \+\+index\)\n        \{\n          gathered\.append\(item->timeAt\(index\)\);\n        \}/        for (int index = 0; index < item->timeCount(); ++index)\n        {\n          gathered.append(item->timeAt(index));\n        }\n        break;/' src/results/timecontroller.cpp
}

# 7. Instants are deduplicated by equality rather than within a tolerance, so
#    two layers recording the same moment give the slider two stops that look
#    identical.
m_exact_dedup() {
  perl -0pi -e 's/      if \(distinct\.isEmpty\(\)\n          \|\| std::abs\(instant - distinct\.last\(\)\) > kSameInstant\)/      if (distinct.isEmpty() || instant != distinct.last())/' src/results/timecontroller.cpp
}

# 8. Stepping wraps at the ends instead of stopping.
m_wrap() {
  perl -0pi -e 's/    setStep\(std::clamp\(base \+ delta, 0, int\(m_steps\.size\(\)\) - 1\)\);/    setStep(((base + delta) % int(m_steps.size()) + int(m_steps.size())) % int(m_steps.size()));/' src/results/timecontroller.cpp
}

# 9. Every layer is moved to the clock's own step number rather than to its
#    own nearest level -- the same confusion as 5, from the other end.
m_step_static() {
  perl -0pi -e 's/      item->setTimeIndex\(item->nearestTime\(m_current\)\);/      item->setTimeIndex(step());/' src/results/timecontroller.cpp
}

# 10. Nothing is told the instant changed, so the views never repaint.
m_silent() {
  perl -0pi -e 's/    Q_EMIT currentChanged\(m_current\);//' src/results/timecontroller.cpp
}

run_mutation "1-always-the-last-level"    m_always_last
run_mutation "2-time-pinned-wrong-axis"   m_wrong_time_axis
run_mutation "3-unchosen-means-first"     m_default_first
run_mutation "4-nearest-by-index"         m_nearest_by_index
run_mutation "5-slider-moves-nothing"     m_share_step_number
run_mutation "6-span-is-one-layers"       m_first_layer_only
run_mutation "7-exact-dedup"              m_exact_dedup
run_mutation "8-stepping-wraps"           m_wrap
run_mutation "9-level-is-the-step-number" m_step_static
run_mutation "10-change-goes-unannounced" m_silent

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_timecontroller --target test_dataitemlayers -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
