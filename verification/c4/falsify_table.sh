#!/bin/bash
# C4c falsification run: the attribute table and the selection it shares.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_attributetable
LOG=verification/c4/falsify_table.log
: > "$LOG"

FILES=(src/ui/panels/attributetablemodel.cpp src/ui/panels/attributetablepanel.cpp)

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

  if ! cmake --build $BUILD --target test_attributetable -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  # Bounded, because one of these mutations does not fail the suite — it
  # makes it loop forever. The two directions of the selection are the same
  # wire read from both ends, and without the guard each end answers the
  # other. A harness that only knows how to read a failure would wedge.
  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 60 ]; do
    sleep 1
    waited=$((waited + 1))
  done

  if kill -0 $pid 2>/dev/null; then
    kill -9 $pid 2>/dev/null
    wait $pid 2>/dev/null
    echo "MUTATION $name: CAUGHT (hung -- the suite never finished)" | tee -a "$LOG"
    return
  fi

  wait $pid 2>/dev/null

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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] Attribute' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Rows are fields and columns are features.
m_transposed() {
  perl -0pi -e 's/    return m_provider->featureCount\(\);/    return int(m_fields.size());/' src/ui/panels/attributetablemodel.cpp
}

# 2. A cell reports children, so a view descends into it.
m_children() {
  perl -0pi -e 's/    if \(parent\.isValid\(\) \|\| !m_provider\)\n    \{\n      return 0;\n    \}/    if (!m_provider)\n    {\n      return 0;\n    }/' src/ui/panels/attributetablemodel.cpp
}

# 3. The row header is a display number rather than a feature index.
m_one_based_rows() {
  perl -0pi -e 's/      return section;/      return section + 1;/' src/ui/panels/attributetablemodel.cpp
}

# 4. Column headers lose the field name.
m_blank_headers() {
  perl -0pi -e 's/    return field\.unit\.isEmpty\(\)\n             \? name\n             : QStringLiteral\("%1 \(%2\)"\)\.arg\(name, field\.unit\);/    return QStringLiteral("column %1").arg(section);/' src/ui/panels/attributetablemodel.cpp
}

# 5. Values are snapshotted when the layer is set instead of read through.
m_stale_values() {
  perl -0pi -e 's/    return m_provider->attributeValue\(index\.row\(\),\n                                      m_fields\.at\(index\.column\(\)\)\.name\);/    static QHash<int, QVariant> cache;\n    const int key = index.row() * 1000 + index.column();\n    if (!cache.contains(key))\n      cache.insert(key, m_provider->attributeValue(index.row(), m_fields.at(index.column()).name));\n    return cache.value(key);/' src/ui/panels/attributetablemodel.cpp
  perl -0pi -e 's/#include "map\/maplayer.h"/#include "map\/maplayer.h"\n\n#include <QHash>/' src/ui/panels/attributetablemodel.cpp
}

# 6. The layer's selection is never copied onto the rows.
m_no_read() {
  perl -0pi -e 's/    auto \*features = dynamic_cast<FeatureLayer \*>\(m_table->layer\(\)\);\n\n    m_syncing = true;/    FeatureLayer *features = nullptr;\n\n    m_syncing = true;/' src/ui/panels/attributetablepanel.cpp
}

# 7. The rows are never copied onto the layer's selection.
m_no_write() {
  perl -0pi -e 's/    m_model->selectOnly\(m_table->layer\(\),\n                        rows\.isEmpty\(\) \? -1 : rows\.first\(\)\.row\(\)\);/    ;/' src/ui/panels/attributetablepanel.cpp
}

# 8. The two directions echo each other.
m_no_guard() {
  perl -0pi -e 's/    if \(m_syncing \|\| !m_model\)\n    \{\n      return;\n    \}\n\n    const QModelIndexList rows =/    if (!m_model)\n    {\n      return;\n    }\n\n    const QModelIndexList rows =/' src/ui/panels/attributetablepanel.cpp
}

# 9. The table does not follow the selection to another layer.
m_no_follow() {
  perl -0pi -e 's/      if \(features && !features->selection\(\)\.isEmpty\(\) &&\n          features != m_table->layer\(\)\)/      if (false)/' src/ui/panels/attributetablepanel.cpp
}

# 10. Every layer is offered a table, including the ones with no attributes.
m_offer_everything() {
  perl -0pi -e 's/        if \(dynamic_cast<const IAttributeProvider \*>\(layer\)\)/        if (true)/' src/ui/panels/attributetablepanel.cpp
}

# 11. A removed layer leaves the chooser pointing nowhere.
m_no_fallback() {
  perl -0pi -e 's/      index = m_chooser->count\(\) > 0 \? 0 : -1;/      index = -1;/' src/ui/panels/attributetablepanel.cpp
}

run_mutation "1-rows-transposed"        m_transposed
run_mutation "2-cells-have-children"    m_children
run_mutation "3-one-based-row-header"   m_one_based_rows
run_mutation "4-blank-column-headers"   m_blank_headers
run_mutation "5-values-snapshotted"     m_stale_values
run_mutation "6-selection-not-read"     m_no_read
run_mutation "7-selection-not-written"  m_no_write
run_mutation "8-directions-echo"        m_no_guard
run_mutation "9-does-not-follow-layer"  m_no_follow
run_mutation "10-offers-every-layer"    m_offer_everything
run_mutation "11-no-fallback-on-remove" m_no_fallback

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_attributetable -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
