#!/bin/bash
# D4b falsification: overlaying what more than one run recorded.
#
# Every mutation below still draws a plot -- curves of the right shape over a
# plausible axis. The gates are on which run each curve came from, whether it
# was read at the place that was picked, and whether the ground the two runs
# stand on was checked at all.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_seriesplot
LOG=verification/d4/falsify_overlay.log
: > "$LOG"

FILES=(src/ui/panels/seriesplotpanel.cpp src/layers/featurelayer.cpp)

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

  if ! cmake --build $BUILD --target test_seriesplot -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] SeriesPlot' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Only the selected run is plotted, so the overlay never happens and the
#    panel behaves exactly as it did before this slice.
m_no_overlay() {
  perl -0pi -e 's/    if \(!overlaysOtherRuns\(\)\)\n    \{\n      return plottable;\n    \}/    return plottable;/' src/ui/panels/seriesplotpanel.cpp
}

# 2. Every time-recorded layer is overlaid regardless of where it stands, so
#    a run recorded somewhere else entirely is drawn on top of this one.
m_ground_unchecked() {
  perl -0pi -e 's/      if \(sameGeometry\(\*selected, \*candidate, elsewhere\)\)/      Q_UNUSED(elsewhere)\n      if (true)/' src/ui/panels/seriesplotpanel.cpp
}

# 3. Only the feature counts are compared, which is exactly what makes a run
#    from somewhere else look like a comparable one.
m_counts_only() {
  perl -0pi -e 's/    const QVector<VectorFeature> &here = left\.features\(\);/    message.clear();\n    return true;\n    const QVector<VectorFeature> \&here = left.features();/' src/layers/featurelayer.cpp
}

# 4. Each overlaid layer is read at its own selection rather than at the one
#    the user made -- which is empty, so the overlay silently draws nothing.
m_own_selection() {
  perl -0pi -e 's/      QList<int> features\(sources\.first\(\)->selection\(\)\.begin\(\),\n                          sources\.first\(\)->selection\(\)\.end\(\)\);/      QList<int> features(source->selection().begin(),\n                          source->selection().end());/' src/ui/panels/seriesplotpanel.cpp
}

# 5. Overlaid curves are not named by their run, so two lines on one chart
#    carry no provenance at all.
m_no_provenance() {
  perl -0pi -e 's/        line->setName\(overlaid\n                        \? tr\("%1 — feature %2"\)\.arg\(source->name\(\)\)\.arg\(feature\)\n                        : tr\("Feature %1"\)\.arg\(feature\)\);/        line->setName(tr("Feature %1").arg(feature));/' src/ui/panels/seriesplotpanel.cpp
}

# 6. Every series is named by its run, even when there is one -- noise the
#    chart title already carries.
m_always_prefixed() {
  perl -0pi -e 's/    const bool overlaid = sources\.size\(\) > 1;/    const bool overlaid = true;/' src/ui/panels/seriesplotpanel.cpp
}

# 7. The chart keeps the selected run's title while carrying two, so a plot
#    of two runs is labeled with the name of one of them.
m_title_of_one_run() {
  perl -0pi -e 's/    m_chart->setTitle\(m_layers\.size\(\) == 1 \? m_layers\.first\(\)->name\(\)\n                                           : QString\(\)\);/    m_chart->setTitle(m_layers.isEmpty() ? QString() : m_layers.first()->name());/' src/ui/panels/seriesplotpanel.cpp
}

# 8. The time axis is the selected run's own span, so a longer run overlaid
#    on it is cut off where the shorter one stopped.
m_axis_from_first_run() {
  perl -0pi -e 's/          earliest = std::min\(earliest, instants\.at\(level\)\);\n          latest = std::max\(latest, instants\.at\(level\)\);/          if (m_plotted.isEmpty()) { earliest = std::min(earliest, instants.at(level)); latest = std::max(latest, instants.at(level)); }/' src/ui/panels/seriesplotpanel.cpp
}

# 9. One layer's caption is put on an axis carrying two quantities.
m_first_caption_wins() {
  perl -0pi -e 's/    m_valueAxis->setTitleText\(oneCaption && !sharedCaption\.isEmpty\(\)\n                                \? sharedCaption\n                                : tr\("Value"\)\);/    m_valueAxis->setTitleText(sharedCaption.isEmpty() ? tr("Value") : sharedCaption);/' src/ui/panels/seriesplotpanel.cpp
}

# 10. The plot asks for a data-item layer by name again, so the difference
#     between two runs -- the thing the phase exists for -- cannot be drawn.
m_concrete_class_again() {
  perl -0pi -e 's/      const auto \*recorded = dynamic_cast<const ITimeLayer \*>\(candidate\);\n\n      if \(!recorded \|\| recorded->timeCount\(\) <= 0\)\n      \{\n        if \(untimed\.isEmpty\(\)\)/      const auto *recorded = dynamic_cast<const DataItemLayer *>(candidate);\n\n      if (!recorded || recorded->timeCount() <= 0)\n      {\n        if (untimed.isEmpty())/' src/ui/panels/seriesplotpanel.cpp
  perl -0pi -e 's/#include "layers\/featurelayer\.h"/#include "layers\/dataitemlayer.h"\n#include "layers\/featurelayer.h"/' src/ui/panels/seriesplotpanel.cpp
}

# 11. The overlay switch is ignored, so turning it off leaves every run on
#     the chart.
m_switch_ignored() {
  perl -0pi -e 's/    return m_overlayCheck->isChecked\(\);/    return true;/' src/ui/panels/seriesplotpanel.cpp
}

run_mutation "1-overlay-never-happens"          m_no_overlay
run_mutation "2-ground-not-checked"             m_ground_unchecked
run_mutation "3-only-counts-compared"           m_counts_only
run_mutation "4-read-at-its-own-selection"      m_own_selection
run_mutation "5-overlaid-curves-unnamed"        m_no_provenance
run_mutation "6-single-run-also-prefixed"       m_always_prefixed
run_mutation "7-chart-titled-with-one-run"      m_title_of_one_run
run_mutation "8-axis-from-the-first-run-only"   m_axis_from_first_run
run_mutation "9-first-caption-wins"             m_first_caption_wins
run_mutation "10-concrete-class-asked-for"      m_concrete_class_again
run_mutation "11-overlay-switch-ignored"        m_switch_ignored

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_seriesplot -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
