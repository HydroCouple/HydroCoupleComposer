#!/bin/bash
# D3a falsification: plotting what a picked feature recorded.
#
# Every mutation below still draws a chart with lines on it. The gates compare
# against a second, independent read of the same hyperslab and against known
# instants, because "there are series in the chart" is satisfied by a plot of
# the wrong feature, the wrong axis, or the wrong time zone.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_seriesplot
LOG=verification/d3/falsify_seriesplot.log
: > "$LOG"

FILES=(src/layers/dataitemlayer.cpp src/ui/panels/seriesplotpanel.cpp
       src/results/julianday.cpp src/results/seriesexport.cpp)

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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] SeriesPlot' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The series is read across the entity axis instead of the time axis, so a
#    plot of one instant's map is drawn as though it were a history.
m_series_reads_entities() {
  perl -0pi -e 's/      if \(static_cast<int>\(i\) == kTimeAxis\)\n      \{\n        extent\[i\] = levels;\n      \}\n      else if \(static_cast<int>\(i\) == entity\)\n      \{\n        start\[i\] = feature;\n      \}/      if (static_cast<int>(i) == entity)\n      {\n        extent[i] = levels;\n      }\n      else if (static_cast<int>(i) == kTimeAxis)\n      {\n        start[i] = feature;\n      }/' src/layers/dataitemlayer.cpp
}

# 2. Every series is read from feature zero, so N selected features plot as N
#    copies of one.
m_always_first_feature() {
  perl -0pi -e 's/        start\[i\] = feature;/        start[i] = 0;/' src/layers/dataitemlayer.cpp
}

# 3. A feature index outside the layer is read anyway.
m_no_feature_bounds() {
  perl -0pi -e 's/    if \(feature < 0 \|\| feature >= featureCount\(\)\)\n    \{\n      message = QObject::tr\("There is no feature %1 to read\."\)\.arg\(feature\);\n      return false;\n    \}//' src/layers/dataitemlayer.cpp
}

# 4. A static item answers with one point rather than refusing, so a single
#    instant is drawn as though it were a trend.
m_static_plots_one_point() {
  perl -0pi -e 's/    if \(!m_item \|\| levels <= 0\)/    if (!m_item)/' src/layers/dataitemlayer.cpp
}

# 5. The axis is fed true epoch milliseconds, so it relabels the run in
#    whatever zone the viewer sits in and disagrees with the readout below it.
m_axis_in_local_time() {
  perl -0pi -e 's/      return utc\.isValid\(\) \? QDateTime\(utc\.date\(\), utc\.time\(\)\) : QDateTime\(\);/      return utc;/' src/ui/panels/seriesplotpanel.cpp
}

# 6. The Julian day is truncated instead of rounded, so a step recorded on the
#    hour lands a minute early.
m_truncated_seconds() {
  perl -0pi -e 's/                       std::lround\(\(hour \* 3600\.0 \+ minute \* 60\.0 \+ second\)\n                                   \* 1000\.0\)\)\),/                       (hour * 3600 + minute * 60 + static_cast<int>(second))\n                       * 1000)),/' src/results/julianday.cpp
}

# 7. The plot keeps its last series when the selection is cleared, so the
#    chart shows a feature the map no longer has picked.
m_stale_series() {
  perl -0pi -e 's/    m_chart->removeAllSeries\(\);\n    m_plotted\.clear\(\);//' src/ui/panels/seriesplotpanel.cpp
}

# 8. A selected layer with no time axis is reported as nothing being
#    selected, so the user is told to do what they have already done.
m_static_reads_as_empty() {
  perl -0pi -e 's/        reason = tr\(.*?holds one instant.*?\n.*?\.arg\(candidate->name\(\)\);\n        return nullptr;/        continue;/s' src/ui/panels/seriesplotpanel.cpp
}

# 9. Series are plotted in whatever order the selection set iterates, so the
#    legend reshuffles between two reads of one run.
m_unsorted_series() {
  perl -0pi -e 's/  std::sort\(features\.begin\(\), features\.end\(\)\);//' src/ui/panels/seriesplotpanel.cpp
}

# 10. The value axis is labeled "Value", throwing away the only label the
#     component supplied.
m_generic_axis_label() {
  perl -0pi -e 's/        m_valueAxis->setTitleText\(field\.displayName\);/        m_valueAxis->setTitleText(QStringLiteral("Value"));/' src/ui/panels/seriesplotpanel.cpp
}

# 11. A series with no reading at an instant is written as zero, so a gap in
#     a record arrives in a spreadsheet as a measurement of nothing.
m_gaps_become_zero() {
  perl -0pi -e 's/        if \(std::isfinite\(value\)\)\n        \{\n          out << formatValue\(value\);\n        \}/        out << formatValue(std::isfinite(value) ? value : 0.0);/' src/results/seriesexport.cpp
}

# 12. Rows are keyed on the raw Julian day, so two series naming one instant
#     with two doubles get a row each and neither lines up.
m_rows_keyed_on_the_double() {
  perl -0pi -e 's/        auto row = rows\.find\(at\.toMSecsSinceEpoch\(\)\);/        auto row = rows.find(*reinterpret_cast<const qint64 *>(\&one.julianDays.data()[level]));/' src/results/seriesexport.cpp
  perl -0pi -e 's/          row = rows\.emplace\(at\.toMSecsSinceEpoch\(\),/          row = rows.emplace(*reinterpret_cast<const qint64 *>(\&one.julianDays.data()[level]),/' src/results/seriesexport.cpp
}

# 13. Names go into the header unquoted, so a comma in one splits the header
#     into a column the file does not have.
m_header_unquoted() {
  perl -0pi -e 's/csvQuoted\(one\.name\)/one.name/' src/results/seriesexport.cpp
}

# 14. Several series are written into the one .dat the user named, which SWMM
#     cannot read -- and the extra column looks like the next reading.
m_dat_never_fans_out() {
  perl -0pi -e 's/    if \(series\.size\(\) == 1\)/    if (true)/' src/results/seriesexport.cpp
}

# 15. A name of nothing but punctuation collapses to an empty token, so two
#     series fan out onto one file and the second overwrites the first.
m_token_can_be_empty() {
  perl -0pi -e 's/    return token\.isEmpty\(\) \? QStringLiteral\("series"\) : token;/    return token;/' src/results/seriesexport.cpp
}

# 16. A non-finite value is written into a .dat rather than skipped, so SWMM
#     reads a stream of numbers with "nan" in it.
m_dat_writes_nan() {
  perl -0pi -e 's/      if \(!std::isfinite\(series\.values\.at\(level\)\)\)\n      \{\n        continue;\n      \}//' src/results/seriesexport.cpp
}

# 17. The export takes its instants from the axis positions, which carry the
#     UTC-shifted values -- so a written file is off by the exporter's zone.
m_export_from_axis_instants() {
  perl -0pi -e 's/        record\.julianDays\.append\(instants\.at\(level\)\);/        record.julianDays.append(instants.at(level) + 0.25);/' src/ui/panels/seriesplotpanel.cpp
}

# 18. A CSV of no series is written anyway -- a header and no rows, which is
#     a file the user will read as a run that recorded nothing.
m_writes_header_only_csv() {
  perl -0pi -e 's/    if \(series\.isEmpty\(\)\)\n    \{\n      message = QObject::tr\("There is no series to export\."\);\n      return false;\n    \}//' src/results/seriesexport.cpp
}

run_mutation "1-series-reads-the-entity-axis" m_series_reads_entities
run_mutation "2-every-series-is-feature-zero" m_always_first_feature
run_mutation "3-feature-bounds-unchecked"     m_no_feature_bounds
run_mutation "4-static-plots-one-point"       m_static_plots_one_point
run_mutation "5-axis-relabels-in-local-time"  m_axis_in_local_time
run_mutation "6-seconds-truncated"            m_truncated_seconds
run_mutation "7-cleared-selection-keeps-plot" m_stale_series
run_mutation "8-static-reads-as-nothing"      m_static_reads_as_empty
run_mutation "9-series-order-unsorted"        m_unsorted_series
run_mutation "10-axis-label-is-generic"       m_generic_axis_label
run_mutation "11-gaps-written-as-zero"        m_gaps_become_zero
run_mutation "12-rows-keyed-on-the-double"    m_rows_keyed_on_the_double
run_mutation "13-header-names-unquoted"       m_header_unquoted
run_mutation "14-dat-never-fans-out"          m_dat_never_fans_out
run_mutation "15-file-token-can-be-empty"     m_token_can_be_empty
run_mutation "16-dat-writes-non-finite"       m_dat_writes_nan
run_mutation "17-export-uses-axis-instants"   m_export_from_axis_instants
run_mutation "18-empty-csv-still-written"     m_writes_header_only_csv

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_seriesplot -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
