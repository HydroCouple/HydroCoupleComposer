#!/bin/bash
# D4a falsification: one variable, two runs, drawn as the difference.
#
# Every mutation below still draws a difference map -- one value per feature,
# over a plausible range, in plausible colours. The gates are on signed
# numbers: which way round the subtraction went, which instant of the other
# run was used, which entity was subtracted from which, and whether the two
# runs were on the same ground at all.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_difference
BROWSER=$BUILD/tests/gui/test_runbrowser
LOG=verification/d4/falsify_difference.log
: > "$LOG"

FILES=(src/layers/differencelayer.cpp src/results/timecontroller.cpp
       src/results/runbrowsermodel.cpp src/ui/panels/runbrowserpanel.cpp
       src/ui/composermainwindow.cpp)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_one() {
  local binary="$1"; shift
  local name="$1"; shift
  local prefix="$1"; shift

  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen "$binary" > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 240 ]; do
    sleep 1
    waited=$((waited + 1))
  done

  if kill -0 $pid 2>/dev/null; then
    kill -9 $pid 2>/dev/null
    wait $pid 2>/dev/null
    echo "CAUGHT (hung -- $prefix never finished)"
    return
  fi

  wait $pid
  local status=$?

  local out
  out=$(cat "$out_file")
  rm -f "$out_file"
  echo "=== $name / $prefix ===" >> "$LOG"
  echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')"
  elif [ "$status" -ne 0 ]; then
    echo "CAUGHT ($prefix died, status $status)"
  else
    echo ""
  fi
}

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

  if ! cmake --build $BUILD --target test_difference --target test_runbrowser -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  local verdict
  verdict=$(run_one "$TEST" "$name" "the difference suite")

  if [ -z "$verdict" ]; then
    verdict=$(run_one "$BROWSER" "$name" "the browser suite")
  fi

  if [ -z "$verdict" ]; then
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  else
    echo "MUTATION $name: $verdict" | tee -a "$LOG"
  fi
}

# ── the subtraction ─────────────────────────────────────────────────────────

# 1. Subtracted the other way round, so every colour lands on the far side of
#    the ramp -- as readable a map as the right one, and the wrong sign.
m_reversed() {
  perl -0pi -e 's/      values\.append\(mine\.at\(entity\) - theirs\.at\(entity\)\);/      values.append(theirs.at(entity) - mine.at(entity));/' src/layers/differencelayer.cpp
}

# 2. Paired by level index rather than by instant, so two runs that reported
#    at different rates compare hour three against hour thirty.
m_paired_by_level() {
  perl -0pi -e 's/    return m_other->timeCount\(\) > 0 \? m_other->nearestTime\(julianDay\)\n                                    : m_other->timeIndex\(\);/    return m_other->timeCount() > 0 ? timeIndex() : m_other->timeIndex();/' src/layers/differencelayer.cpp
}

# 3. Every entity is subtracted from entity zero, so the magnitudes are right
#    and they are in the wrong places.
m_first_entity() {
  perl -0pi -e 's/      values\.append\(mine\.at\(entity\) - theirs\.at\(entity\)\);/      values.append(mine.at(entity) - theirs.at(0));/' src/layers/differencelayer.cpp
}

# 4. A run with no time axis is read at level zero of a series it does not
#    have rather than at its only level.
m_steady_read_as_series() {
  perl -0pi -e 's/    return m_other->timeCount\(\) > 0 \? m_other->nearestTime\(julianDay\)\n                                    : m_other->timeIndex\(\);/    return m_other->timeCount() > 0 ? m_other->nearestTime(julianDay) : 0;/' src/layers/differencelayer.cpp
}

# ── what is refused ─────────────────────────────────────────────────────────

# 5. Only the counts are compared, so two runs over different meshes of the
#    same size difference happily and mean nothing.
m_only_counts_checked() {
  perl -0pi -e 's/    const QVector<VectorFeature> &here = left\.features\(\);/    message.clear();\n    return true;\n    const QVector<VectorFeature> \&here = left.features();/' src/layers/differencelayer.cpp
}

# 6. The vertex tolerance is a fixed number rather than one taken from the
#    extent, so a mesh in degrees compares as though it were in metres.
m_absolute_tolerance() {
  perl -0pi -e 's/    const double tolerance =\n      diagonal > 0\.0 \? diagonal \* 1e-9 : 1e-9;/    const double tolerance = 1e6;/' src/layers/differencelayer.cpp
}

# 7. Two runs of different sizes are differenced as far as the shorter one
#    goes, so half a comparison is drawn as a whole one.
m_mismatched_sizes_allowed() {
  perl -0pi -e 's/    if \(left\.featureCount\(\) != right\.featureCount\(\)\)\n    \{\n      message =\n        QObject::tr\("The two runs recorded %1 and %2 features, so they are "\n                    "not the same geometry\."\)\n          \.arg\(left\.featureCount\(\)\)\n          \.arg\(right\.featureCount\(\)\);\n      return false;\n    \}//' src/layers/differencelayer.cpp
}

# ── the difference as a layer ───────────────────────────────────────────────

# 8. Breaks come from the level on screen, so a comparison recomputes its
#    colours every step and the step that disagreed most cannot be seen.
m_breaks_from_one_level() {
  perl -0pi -e 's/    if \(field != m_valueAttribute \|\| levels < 1\)/    if (true)/' src/layers/differencelayer.cpp
}

# 9. The clock drives only the runs, not the comparison drawn from them, so a
#    difference map stands still while the maps beside it animate.
m_clock_skips_derived_layers() {
  perl -0pi -e 's/      auto \*item = dynamic_cast<ITimeLayer \*>\(layer\);\n\n      if \(!item\)/      auto *item = dynamic_cast<DataItemLayer *>(layer);\n\n      if (!item)/' src/results/timecontroller.cpp
  perl -0pi -e 's/#include "layers\/timelayer\.h"/#include "layers\/dataitemlayer.h"/' src/results/timecontroller.cpp
}

# 10. The value field is named generically, throwing away the caption the
#     component gave the variable being differenced.
m_generic_field_name() {
  perl -0pi -e 's/      field\.displayName = QObject::tr\("Δ %1"\)\.arg\(variable\);/      field.displayName = QObject::tr("Difference");/' src/layers/differencelayer.cpp
}

# 11. The series pairs by level while the map pairs by instant, so a plot
#     read beside a difference map is a second, different subtraction.
m_series_pairs_by_level() {
  perl -0pi -e 's/      const int theirLevel =\n        m_other->timeCount\(\) > 0 \? otherLevelFor\(timeAt\(level\)\) : 0;/      const int theirLevel = level;/' src/layers/differencelayer.cpp
}

# ── the catalog and the button ──────────────────────────────────────────────

# 12. Any run holding the component counts as carrying the item, so a run
#     that recorded something else entirely is offered as a comparison.
m_component_only_lookup() {
  perl -0pi -e 's/      if \(session && session->manifest\(\)\.entry\(component, item\)\)/      if (session \&\& !session->manifest().entriesFor(component).empty())/' src/results/runbrowsermodel.cpp
}

# 13. Comparing is offered with one run open, and then refused.
m_compare_always_offered() {
  perl -0pi -e 's/    m_compareButton->setEnabled\(currentItemIndex\(\)\.isValid\(\) && m_model\n                                && m_model->runCount\(\) > 1\);/    m_compareButton->setEnabled(currentItemIndex().isValid());/' src/ui/panels/runbrowserpanel.cpp
}

# 14. A run compared against itself is drawn rather than refused -- a map of
#     zeros, which looks like two runs that agreed perfectly.
m_self_comparison_drawn() {
  perl -0pi -e 's/    if \(base == other\)\n    \{\n      message = tr\("A run compared against itself is zero everywhere\. Pick "\n                   "a different run\."\);\n      return false;\n    \}//' src/ui/composermainwindow.cpp
}

run_mutation "1-subtracted-the-other-way-round"  m_reversed
run_mutation "2-paired-by-level-not-instant"     m_paired_by_level
run_mutation "3-everything-minus-entity-zero"    m_first_entity
run_mutation "4-steady-baseline-read-as-series"  m_steady_read_as_series
run_mutation "5-only-the-counts-are-compared"    m_only_counts_checked
run_mutation "6-tolerance-not-from-the-extent"   m_absolute_tolerance
run_mutation "7-mismatched-sizes-allowed"        m_mismatched_sizes_allowed
run_mutation "8-breaks-from-the-level-on-screen" m_breaks_from_one_level
run_mutation "9-clock-skips-derived-layers"      m_clock_skips_derived_layers
run_mutation "10-value-field-named-generically"  m_generic_field_name
run_mutation "11-series-pairs-by-level"          m_series_pairs_by_level
run_mutation "12-component-only-catalog-lookup"  m_component_only_lookup
run_mutation "13-compare-offered-with-one-run"   m_compare_always_offered
run_mutation "14-self-comparison-is-drawn"       m_self_comparison_drawn

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_difference --target test_runbrowser -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
QT_QPA_PLATFORM=offscreen $BROWSER 2>&1 | tail -3
