#!/bin/bash
# D1 falsification run: reopening a finished run and browsing its catalog.
#
# What is being falsified is that the browser reports the manifest rather than
# a plausible reconstruction of it. Most of these mutations still produce a
# tree full of rows -- which is why the gates compare against the file.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_runbrowser
LOG=verification/d1/falsify_runbrowser.log
: > "$LOG"

FILES=(src/results/runsession.cpp src/results/runbrowsermodel.cpp
       src/ui/panels/runbrowserpanel.cpp)

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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] RunBrowser' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Components are listed alphabetically instead of in catalog order, so the
#    tree stops reflecting the order things were produced in.
m_sorted_components() {
  perl -0pi -e 's/    return ids;/    ids.sort();\n    return ids;/' src/results/runsession.cpp
}

# 2. A component is reported as holding every entry in the run, not its own.
m_all_entries() {
  perl -0pi -e 's/      if \(entry\.componentId == wanted\)/      if (true)/' src/results/runsession.cpp
}

# 3. The shape is reported as a count of values rather than as extents.
m_shape_count() {
  perl -0pi -e 's/      return extents\.join\(QStringLiteral\(" × "\)\);/      return QString::number(entry.elementCount());/' src/results/runbrowsermodel.cpp
}

# 4. The shape is reported with its extents reversed -- time last.
m_shape_reversed() {
  perl -0pi -e 's/      for \(const int64_t extent : entry\.shape\)\n      \{\n        extents\.append\(QString::number\(extent\)\);\n      \}/      for (auto it = entry.shape.rbegin(); it != entry.shape.rend(); ++it)\n      {\n        extents.append(QString::number(*it));\n      }/' src/results/runbrowsermodel.cpp
}

# 5. A static field is given a time axis it never had.
m_invent_time() {
  perl -0pi -e 's/        return QObject::tr\("static"\);/        return QObject::tr("1 steps, 0 → 0 julian_day");/' src/results/runbrowsermodel.cpp
}

# 6. Units come from the artifact name rather than from the catalog.
m_wrong_units() {
  perl -0pi -e 's/            return QString::fromStdString\(entry->units\);/            return QString::fromStdString(entry->format);/' src/results/runbrowsermodel.cpp
}

# 7. Every row claims to belong to the first run, so a second run's rows read
#    the first run's catalog -- and a comparison compares a run with itself.
m_one_run() {
  perl -0pi -e 's/    return run\(locate\(index\)\.run\);/    return run(0);/' src/results/runbrowsermodel.cpp
}

# 8. An item is reported as having children, so a view descends into a number.
m_items_have_children() {
  perl -0pi -e 's/      case Level::Item:\n        return 0;\n    \}\n\n    return 0;\n  \}\n\n  int RunBrowserModel::columnCount/      case Level::Item:\n        return 1;\n    }\n\n    return 0;\n  }\n\n  int RunBrowserModel::columnCount/' src/results/runbrowsermodel.cpp
}

# 9. Opening a component is not cached, so every read reopens every artifact.
m_no_cache() {
  perl -0pi -e 's/    m_opened\.insert\(componentId, component\);//' src/results/runsession.cpp
}

# 10. A failed open is reported as success, so a moved artifact yields a
#     component with nothing in it and no explanation.
m_ignore_failure() {
  perl -0pi -e 's/    if \(component->status\(\)\n        == HydroCouple::IModelComponent::ComponentStatus::Failed\)/    if (false)/' src/results/runsession.cpp
}

# 11. Closing acts on the highlighted row rather than on the run it is inside,
#     so closing from a selected item closes the wrong run or nothing.
m_close_wrong_row() {
  perl -0pi -e 's/    while \(index\.isValid\(\) && index\.parent\(\)\.isValid\(\)\)\n    \{\n      index = index\.parent\(\);\n    \}//' src/ui/panels/runbrowserpanel.cpp
}

run_mutation "1-components-sorted"         m_sorted_components
run_mutation "2-component-holds-everything" m_all_entries
run_mutation "3-shape-as-a-count"          m_shape_count
run_mutation "4-shape-reversed"            m_shape_reversed
run_mutation "5-static-given-a-time-axis"  m_invent_time
run_mutation "6-units-from-the-format"     m_wrong_units
run_mutation "7-every-row-is-the-first-run" m_one_run
run_mutation "8-items-have-children"       m_items_have_children
run_mutation "9-components-reopened"       m_no_cache
run_mutation "10-failed-open-ignored"      m_ignore_failure
run_mutation "11-close-uses-the-wrong-row" m_close_wrong_row

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_runbrowser -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
