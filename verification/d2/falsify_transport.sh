#!/bin/bash
# D2c falsification run: playing a run, and the transport over it.
#
# Every mutation below leaves a panel that looks and behaves like a media
# player -- the slider slides, the buttons click. What they change is whether
# the clock and the panel are the same clock, and whether playback stops where
# the record does.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_timecontrols
LOG=verification/d2/falsify_transport.log
: > "$LOG"

FILES=(src/results/timecontroller.cpp src/ui/panels/timecontrolpanel.cpp)

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

  if ! cmake --build $BUILD --target test_timecontrols -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] TimeControls' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Playback runs past the end of the record, so the clock keeps ticking on a
#    run that finished and the play button never comes back up.
m_never_stops() {
  perl -0pi -e 's/      if \(!m_looping\)\n      \{\n        pause\(\);\n        return;\n      \}//' src/results/timecontroller.cpp
}

# 2. Cycling stops at the end like anything else -- the toggle does nothing.
m_cycle_is_inert() {
  perl -0pi -e 's/      if \(!m_looping\)/      if (true)/' src/results/timecontroller.cpp
}

# 3. Play on a finished run leaves the clock parked on the last step, so the
#    button appears dead on exactly the run someone has just watched.
m_no_replay() {
  perl -0pi -e 's/    if \(step\(\) == m_steps\.size\(\) - 1\)\n    \{\n      setStep\(0\);\n    \}//' src/results/timecontroller.cpp
}

# 4. Stopping is not reported, so a play button tracks its own clicks and
#    shows a run as playing after it has finished.
m_silent_stop() {
  perl -0pi -e 's/    m_timer->stop\(\);\n\n    Q_EMIT playingChanged\(false\);/    m_timer->stop();/' src/results/timecontroller.cpp
}

# 5. Speed is recorded but never reaches the running timer, so an animation
#    can only be slowed down by stopping it first.
m_speed_not_live() {
  perl -0pi -e 's/    if \(m_timer\)\n    \{\n      \/\/ Applied while running.*?\n.*?\n.*?\n      m_timer->setInterval\(static_cast<int>\(std::lround\(1000\.0 \/ m_speed\)\)\);\n    \}//s' src/results/timecontroller.cpp
}

# 6. The panel stops following the clock, so a time set from anywhere else --
#    a play tick, a plot cursor -- leaves the slider behind.
m_panel_deaf() {
  perl -0pi -e 's/      connect\(m_clock, &TimeController::currentChanged, this,\n              \[this\]\(double\) \{ refreshPosition\(\); \}\);//' src/ui/panels/timecontrolpanel.cpp
}

# 7. The clock re-announces an instant it is already showing, so a view that
#    both follows and drives it -- which is what a slider is -- has nothing
#    ending the exchange between them.
m_clock_reannounces() {
  perl -0pi -e 's/    if \(std::abs\(wanted - m_current\) <= kSameInstant\)\n    \{\n      return;\n    \}//' src/results/timecontroller.cpp
}

# 8. The transport stays lit at the ends of the record, so pressing step at
#    the last time silently does nothing.
m_always_enabled() {
  perl -0pi -e 's/    m_forwardButton->setEnabled\(has && step != last\);/    m_forwardButton->setEnabled(has);/' src/ui/panels/timecontrolpanel.cpp
}

# 9. The readout is half a day out -- a Julian day printed as if it began at
#    midnight, which is the classic way to be one row off in a results table.
m_half_day_out() {
  perl -0pi -e 's/        julianDay, year, month, day, hour, minute, second\);/        julianDay + 0.5, year, month, day, hour, minute, second);/' src/ui/panels/timecontrolpanel.cpp
}

# 10. An empty stack gets a formatted instant instead of a dash, so a window
#     with nothing loaded reports a date in 4713 BC.
m_format_nothing() {
  perl -0pi -e 's/      m_time->setText\(QStringLiteral\("—"\)\);/      m_time->setText(formatInstant(m_clock ? m_clock->current() : 0.0));/' src/ui/panels/timecontrolpanel.cpp
}

# 11. The speed box keeps its own default rather than showing the clock's.
m_speed_box_lies() {
  perl -0pi -e 's/      m_speed->setValue\(m_clock->speed\(\)\);//' src/ui/panels/timecontrolpanel.cpp
}

run_mutation "1-playback-never-stops"        m_never_stops
run_mutation "2-cycle-does-nothing"          m_cycle_is_inert
run_mutation "3-play-on-a-finished-run"      m_no_replay
run_mutation "4-stopping-is-not-reported"    m_silent_stop
run_mutation "5-speed-is-not-live"           m_speed_not_live
run_mutation "6-panel-stops-following"       m_panel_deaf
run_mutation "7-clock-reannounces-an-instant" m_clock_reannounces
run_mutation "8-transport-lit-at-the-ends"   m_always_enabled
run_mutation "9-readout-half-a-day-out"      m_half_day_out
run_mutation "10-nothing-loaded-shows-a-date" m_format_nothing
run_mutation "11-speed-box-keeps-its-own"    m_speed_box_lies

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_timecontrols -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
