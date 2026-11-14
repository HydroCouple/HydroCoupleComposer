#!/bin/bash
# Falsification: the raster's ramp and stretch are editable, and edits land.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_layerproperties
LOG=verification/t5/falsify_rasterramp.log
: > "$LOG"

FILES=(src/ui/dialogs/layerpropertiesdialog.cpp src/layers/gdalrasterlayer.cpp)

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
  for f in "${FILES[@]}"; do cmp -s "$f" "$f.orig" || changed=1; done
  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"; return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_layerproperties -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. A raster gets no symbology page -- the pre-slice silence.
m_no_raster_page() {
  perl -0pi -e 's/    if \(auto \*raster = dynamic_cast<GdalRasterLayer \*>\(m_layer\);\n        raster && !raster->isColorImage\(\)\)/    if (false)/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 2. The apply half is missing: the combo edits a copy nobody reads.
m_apply_missing() {
  perl -0pi -e 's/      raster->setRampName\(m_rasterRampCombo->currentText\(\)\);\n      raster->setValueRange\(m_rasterMinimumSpin->value\(\),\n                            m_rasterMaximumSpin->value\(\)\);/      (void)raster;/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 3. The load half shows defaults rather than the layer's own state.
m_load_shows_defaults() {
  perl -0pi -e 's/      m_rasterRampCombo->setCurrentText\(raster->rampName\(\)\);\n\n      double minimum = 0\.0;\n      double maximum = 1\.0;\n      raster->valueRange\(minimum, maximum\);\n      m_rasterMinimumSpin->setValue\(minimum\);\n      m_rasterMaximumSpin->setValue\(maximum\);/      (void)raster;/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 4. A stretch change never repaints.
m_stretch_never_repaints() {
  perl -0pi -e 's/    m_minimum = minimum;\n    m_maximum = maximum;\n    m_groundValid = false;\n    notifyAppearanceChanged\(\);/    m_minimum = minimum;\n    m_maximum = maximum;/' src/layers/gdalrasterlayer.cpp
}

# 5. An upside-down stretch is accepted, dividing by a non-positive span.
m_backwards_stretch() {
  perl -0pi -e 's/    if \(!\(maximum > minimum\)\n        \|\| \(qFuzzyCompare\(minimum, m_minimum\)\n            && qFuzzyCompare\(maximum, m_maximum\)\)\)/    if (qFuzzyCompare(minimum, m_minimum) \&\& qFuzzyCompare(maximum, m_maximum))/' src/layers/gdalrasterlayer.cpp
}

run_mutation 1-no-raster-page        m_no_raster_page
run_mutation 2-apply-missing         m_apply_missing
run_mutation 3-load-shows-defaults   m_load_shows_defaults
run_mutation 4-stretch-never-repaints m_stretch_never_repaints
run_mutation 5-backwards-stretch     m_backwards_stretch

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
