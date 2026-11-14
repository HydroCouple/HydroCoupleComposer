#!/bin/bash
# C5a falsification run: the layer properties dialog.
#
# The thing being falsified is not "the form has controls" but two claims the
# slice makes: that the dialog opens for EVERY layer with only the tabs that
# layer can fill, and that what it shows and writes is the layer's own state.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_layerproperties
LOG=verification/c5/falsify_properties.log
: > "$LOG"

FILES=(src/ui/dialogs/layerpropertiesdialog.cpp src/map/maplayer.cpp)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }

  # A perl pattern that has quietly stopped matching reads as SURVIVED, which
  # is the most flattering possible way for a falsification run to be wrong.
  local changed=0
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_layerproperties -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  # Bounded, and judged by exit status as well as output: a mutation that
  # makes the suite die prints no "[  FAILED  ]" line at all, and a harness
  # that only greps for one reads the crash as a survivor.
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] LayerProperties' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The old gate is back: only styled layers get a dialog.
m_old_gate() {
  perl -0pi -e 's/    return layer != nullptr;/    return layer \&\& layer->style() != nullptr;/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 2. Symbology is offered to layers that have no style — an empty form, which
#    is the failure the old gate was avoiding.
m_symbology_always() {
  perl -0pi -e 's/  QWidget \*LayerPropertiesDialog::buildSymbologyTab\(\)\n  \{\n    if \(!hasStyle\(\)\)\n    \{\n      return nullptr;\n    \}\n/  QWidget *LayerPropertiesDialog::buildSymbologyTab()\n  {\n/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 3. Metadata is offered to a layer that credits nobody.
m_metadata_always() {
  perl -0pi -e 's/    if \(attribution\.isEmpty\(\)\)\n    \{\n      return nullptr;\n    \}\n//' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 4. The name is never written back.
m_no_name() {
  perl -0pi -e 's/    m_layer->setName\(m_nameEdit->text\(\)\);/    ;/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 5. Percent is written as a fraction: 40% becomes an opacity of 40.
m_opacity_units() {
  perl -0pi -e 's/    m_layer->setOpacity\(m_opacitySpin->value\(\) \/ 100\.0\);/    m_layer->setOpacity(m_opacitySpin->value());/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 6. The form opens on defaults rather than on the layer, so applying it
#    quietly re-shows a layer the user had hidden.
m_no_load() {
  perl -0pi -e 's/    m_nameEdit->setText\(m_layer->name\(\)\);\n    m_visibleCheck->setChecked\(m_layer->isVisible\(\)\);\n    m_opacitySpin->setValue\(qRound\(m_layer->opacity\(\) \* 100\.0\)\);/    ;/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 7. A style-less layer reports failure from apply(), as the old dialog did.
m_styleless_fails() {
  perl -0pi -e 's/      \/\/ edited through style\(\), where the layer never sees the change\.\n      return true;/      return false;/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 9. An in-memory layer shows an empty source row, which reads as a value the
#    dialog failed to find rather than as one that does not exist.
m_blank_source() {
  perl -0pi -e 's/                 factLabel\(source\.isEmpty\(\) \? tr\("Built in memory"\) : source,/                 factLabel(source,/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 10. The base forgets the provenance it was told at construction.
m_base_forgets() {
  perl -0pi -e 's/  QString MapLayer::sourceDescription\(\) const\n  \{\n    return m_sourceDescription;\n  \}/  QString MapLayer::sourceDescription() const\n  {\n    return {};\n  }/' src/map/maplayer.cpp
}

# 11. Features are counted as fields.
m_counts_fields() {
  perl -0pi -e 's/                   factLabel\(QString::number\(provider->featureCount\(\)\), page,/                   factLabel(QString::number(provider->attributeFields().size()), page,/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 12. Every extent is reported as empty.
m_extent_always_empty() {
  perl -0pi -e 's/      if \(extent\.isEmpty\(\)\)/      if (true)/' src/ui/dialogs/layerpropertiesdialog.cpp
}

run_mutation "1-old-canstyle-gate"      m_old_gate
run_mutation "2-symbology-always"       m_symbology_always
run_mutation "3-metadata-always"        m_metadata_always
run_mutation "4-name-not-applied"       m_no_name
run_mutation "5-opacity-percent-raw"    m_opacity_units
run_mutation "6-form-not-loaded"        m_no_load
run_mutation "7-styleless-apply-fails"  m_styleless_fails
run_mutation "9-blank-source-row"       m_blank_source
run_mutation "10-base-forgets-source"   m_base_forgets
run_mutation "11-counts-fields"         m_counts_fields
run_mutation "12-extent-always-empty"   m_extent_always_empty

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_layerproperties -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
