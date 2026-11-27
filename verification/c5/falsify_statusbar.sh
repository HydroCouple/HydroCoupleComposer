#!/bin/bash
# C5e falsification run: map scale, coordinates and CRS on the status bar.
#
# The scale is the part worth attacking. Every way it breaks still produces a
# plausible number, so the mutations below are the specific wrong numbers:
# ignoring the CRS's units, assuming 96 DPI, forgetting the cosine of
# latitude, and inverting the ratio.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_mapstatusbar
LOG=verification/c5/falsify_statusbar.log
: > "$LOG"

FILES=(src/map/mapcanvas.cpp src/ui/panels/mapstatusbar.cpp
       src/gis/spatialreference.cpp)

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

  if ! cmake --build $BUILD --target test_mapstatusbar -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] MapStatusBar' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The units of the map's CRS are ignored: a world unit is taken as a meter
#    whatever the system says, so a foot-based state plane reads like a
#    metric one over the same ground.
m_ignore_units() {
  perl -0pi -e 's/    return m_crs->linearUnitsToMetres\(\);/    return 1.0;/' src/map/mapcanvas.cpp
}

# 2. A degree is the same distance everywhere, so a Norwegian map reads like
#    an equatorial one.
m_no_cosine() {
  perl -0pi -e 's/\* std::abs\(std::cos\(qDegreesToRadians\(latitude\)\)\);/;\n      Q_UNUSED(latitude)/' src/map/mapcanvas.cpp
}

# 3. The screen's DPI is wrong, as a hard-coded 96 is on a Retina display.
#    Note what this can and cannot show: under the offscreen platform the
#    screen reports 96, so substituting the fallback is inert and proves
#    nothing. Doubling it proves the DPI is in the formula at all, which is
#    the part that is falsifiable here.
m_assume_dpi() {
  perl -0pi -e 's/      return dpi > 0\.0 \? dpi : kFallbackDpi;/      return (dpi > 0.0 ? dpi : kFallbackDpi) * 2.0;/' src/map/mapcanvas.cpp
}

# 4. The ratio is inverted, so zooming in makes the denominator grow.
m_inverted() {
  perl -0pi -e 's/    return groundMetresPerPixel \/ \(kMetresPerInch \/ screenDpi\(this\)\);/    return (kMetresPerInch \/ screenDpi(this)) \/ groundMetresPerPixel;/' src/map/mapcanvas.cpp
}

# 5. Setting a scale is not the inverse of reading one.
m_set_not_inverse() {
  perl -0pi -e 's/    m_transform\.zoomAt\(current \/ denominator,/    m_transform.zoomAt(denominator \/ current,/' src/map/mapcanvas.cpp
}

# 6. The bar never follows the view, so the readout goes stale the moment
#    anyone zooms.
m_no_follow() {
  perl -0pi -e 's/      connect\(m_canvas, &MapCanvas::scaleChanged, this,\n              \[this\]\(double\) \{ refreshScale\(\); \}\);/      ;/' src/ui/panels/mapstatusbar.cpp
}

# 7. A typed scale is parsed as the whole string, so "1:12,000" reads as 1.
m_parse_whole() {
  perl -0pi -e 's/      QString digits = text\.section\(QLatin1Char\(.:.\), -1\);/      QString digits = text;/' src/ui/panels/mapstatusbar.cpp
}

# 8. Unreadable text is left standing, so the field shows a scale the view is
#    not at.
m_keep_garbage() {
  perl -0pi -e 's/      \/\/ Unreadable: put back what the map is actually at, rather than\n      \/\/ leaving the field showing something the view does not match\.\n      refreshScale\(\);\n\n      return;/      return;/' src/ui/panels/mapstatusbar.cpp
}

# 9. The CRS indicator is set once and never follows the map.
m_crs_static() {
  perl -0pi -e 's/      connect\(m_canvas, &MapCanvas::crsChanged, this,\n              \[this\] \{ refreshCrs\(\); \}\);/      ;/' src/ui/panels/mapstatusbar.cpp
}

# 10. The controls stay live where they mean nothing.
m_always_live() {
  perl -0pi -e 's/    m_scaleCombo->setEnabled\(live && m_canvas\);\n    m_crsButton->setEnabled\(live && m_canvas\);/    m_scaleCombo->setEnabled(true);\n    m_crsButton->setEnabled(true);\n    Q_UNUSED(live)/' src/ui/panels/mapstatusbar.cpp
}

run_mutation "1-units-ignored"          m_ignore_units
run_mutation "2-degree-is-flat"         m_no_cosine
run_mutation "3-dpi-assumed"            m_assume_dpi
run_mutation "4-ratio-inverted"         m_inverted
run_mutation "5-set-not-inverse"        m_set_not_inverse
run_mutation "6-readout-never-follows"  m_no_follow
run_mutation "7-scale-parsed-whole"     m_parse_whole
run_mutation "8-garbage-left-standing"  m_keep_garbage
run_mutation "9-crs-indicator-static"   m_crs_static
run_mutation "10-controls-always-live"  m_always_live

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_mapstatusbar -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
