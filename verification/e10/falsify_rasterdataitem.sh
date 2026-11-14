#!/bin/bash
# Falsification: a component's raster data item on the map.
#
# Every mutation still draws something. What they break is whether it is over
# the right ground, whether the shading says anything about the values, and
# whether a hole in a survey is admitted or coloured in.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_dataitemlayers
LOG=verification/e10/falsify_rasterdataitem.log
: > "$LOG"

FILES=(src/layers/rasterdataitemlayer.cpp)

# No mutation exists for "count no-data in the range": std::min and std::max
# both answer with the value they already had when a NaN is involved, so an
# explicit skip there was redundant and has been removed rather than gated.
# What can go wrong is the all-holes case, which mutation 8 now covers.

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

  if ! cmake --build $BUILD --target test_dataitemlayers -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 120 $TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if [ "$status" -eq 124 ]; then
    echo "MUTATION $name: CAUGHT (hung -- the watchdog killed it)" | tee -a "$LOG"
  elif echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] DataItemLayerTest' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── is it a raster ──────────────────────────────────────────────────────────

# 1. Claim any data item, so a geometry item is drawn as a raster with no
#    cells rather than as the features it is.
m_claims_anything() {
  perl -0pi -e 's/    return raster && raster->raster\(\);/    return item != nullptr;/' src/layers/rasterdataitemlayer.cpp
}

# ── is it over the right ground ─────────────────────────────────────────────

# 2. Read the extent as though rows ran up, so the raster is drawn mirrored
#    about its own top edge and lands north of where it belongs.
m_rows_run_up() {
  perl -0pi -e 's/    const double bottom = top \+ m_geoTransform\[5\] \* m_height;/    const double bottom = top - m_geoTransform[5] * m_height;/' src/layers/rasterdataitemlayer.cpp
}

# 3. Take the cell size as one, so the raster covers its own pixel count in
#    world units -- a metre grid drawn as a four-by-three-metre square.
m_unit_cells() {
  perl -0pi -e 's/    const double right = left \+ m_geoTransform\[1\] \* m_width;/    const double right = left + m_width;/' src/layers/rasterdataitemlayer.cpp
}

# 4. Drop the item's CRS, so a raster in metres is drawn as though it were
#    in degrees and lands off the coast of Africa.
m_no_crs() {
  perl -0pi -e 's/      if \(!reference->srText\(\)\.empty\(\)\)/      if (false)/' src/layers/rasterdataitemlayer.cpp
}

# ── does the shading say anything ───────────────────────────────────────────

# 5. Shade over a fixed nought-to-one range, so any raster of real values is
#    one flat colour at the top of the ramp.
m_fixed_range() {
  perl -0pi -e 's/        const double position =\n          qFuzzyIsNull\(span\) \? 0\.5 : \(value - m_minimum\) \/ span;/        const double position = value;/' src/layers/rasterdataitemlayer.cpp
}

# 6. Read the band as though the rows were the bands, so the second band of
#    a two-band raster is the first band's second row.
m_band_is_a_row() {
  perl -0pi -e 's/    const std::array<int64_t, 3> start\{m_band, 0, 0\};/    const std::array<int64_t, 3> start{0, m_band, 0};/' src/layers/rasterdataitemlayer.cpp
}

# 7. Ignore a band change, so the band control is decoration.
m_band_never_changes() {
  perl -0pi -e 's/    m_band = band;\n\n    QString message;/    QString message;/' src/layers/rasterdataitemlayer.cpp
}

# ── is a hole a hole ────────────────────────────────────────────────────────

# 8. Leave an all-holes raster with a range running from the largest double
#    there is down to the lowest, which shades into nonsense.
m_nodata_in_range() {
  perl -0pi -e 's/    if \(m_minimum > m_maximum\)\n    \{/    if (false)\n    \{/' src/layers/rasterdataitemlayer.cpp
}

# 9. Give a hole a colour -- the bottom of the ramp, which over a survey
#    reads as a lake.
m_nodata_is_a_colour() {
  perl -0pi -e 's/          image\.setPixelColor\(column, row, QColor\(0, 0, 0, 0\)\);\n          continue;/          image.setPixelColor(column, row, m_ramp.colorAt(0.0));\n          continue;/' src/layers/rasterdataitemlayer.cpp
}

# ── does it refuse what it cannot draw ──────────────────────────────────────

# 10. Accept a raster item whose shape is not band, row and column, and read
#     whatever the first slab happens to hold.
m_any_shape() {
  perl -0pi -e 's/    if \(shape\.size\(\) != 3\)/    if (shape.size() < 2)/' src/layers/rasterdataitemlayer.cpp
}

run_mutation 1-claims-anything      m_claims_anything
run_mutation 2-rows-run-up          m_rows_run_up
run_mutation 3-unit-cells           m_unit_cells
run_mutation 4-no-crs               m_no_crs
run_mutation 5-fixed-range          m_fixed_range
run_mutation 6-band-is-a-row        m_band_is_a_row
run_mutation 7-band-never-changes   m_band_never_changes
run_mutation 8-empty-range-unfixed  m_nodata_in_range
run_mutation 9-nodata-is-a-colour   m_nodata_is_a_colour
run_mutation 10-any-shape           m_any_shape

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
