#!/bin/bash
# Falsification: a WMS and a WMTS drawn as a slippy map.
#
# Every mutation still produces a source that fetches and a map that draws
# something. What they break is which ground a tile is of, which way up it
# is, whether a service that cannot be drawn says so, and whether a refused
# tile is asked for once or once per frame.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_ogctilesource
LOG=verification/e2/falsify_ogctilesource.log
: > "$LOG"

FILES=(src/layers/ogctilesource.cpp)

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

  if ! cmake --build $BUILD --target test_ogctilesource -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$($TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] (Wms|Wmts|Ogc)' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── which ground a tile is of ───────────────────────────────────────────────

# 1. A tile is asked for as the whole world rather than as its own square,
#    so every tile of the map is the same picture.
m_whole_world_per_tile() {
  perl -0pi -e 's/    request\.extent = TileGrid::tileExtent\(tile\);/    request.extent = TileGrid::worldExtent();/' src/layers/ogctilesource.cpp
}

# 2. Row and column change places, which for a WMTS transposes the map --
#    every tile exists, and each is somewhere else.
m_row_and_column_swapped() {
  perl -0pi -e 's/                            matrix->identifier, tile\.y, tile\.x, m_style,/                            matrix->identifier, tile.x, tile.y, m_style,/' src/layers/ogctilesource.cpp
}

# 3. The pyramid level is taken to be the zoom number, which is right only
#    for a server that happens to number its levels that way.
m_level_is_the_zoom_number() {
  perl -0pi -e 's/    const HydroCouple::Ogc::WmtsTileMatrix \*matrix = matrixForZoom\(tile\.zoom\);\n\n    if \(!matrix\)\n    \{\n      return \{\};\n    \}/    const HydroCouple::Ogc::WmtsTileMatrix *matrix = matrixForZoom(tile.zoom);\n\n    if (!matrix)\n    {\n      static HydroCouple::Ogc::WmtsTileMatrix guessed;\n      guessed.identifier = QString::number(tile.zoom);\n      matrix = \&guessed;\n    }/' src/layers/ogctilesource.cpp
}

# 4. A basemap is asked for with a transparent background, so a service
#    that honors it draws the map over nothing.
m_transparent_basemap() {
  perl -0pi -e 's/    request\.transparent = false;/    request.transparent = true;/' src/layers/ogctilesource.cpp
}

# ── what a service can actually draw ────────────────────────────────────────

# 5. Web Mercator is asked for in this program's preferred spelling rather
#    than one the server listed, so a server offering only the URN form is
#    sent a code it never advertised.
m_own_spelling() {
  perl -0pi -e 's/        if \(identifier\.code == code\)\n        \{\n          return advertised;\n        \}/        if (identifier.code == code)\n        {\n          return QStringLiteral("EPSG:") + code;\n        }/' src/layers/ogctilesource.cpp
}

# 6. A service that publishes nothing in Web Mercator is used anyway, and
#    answers an exception report per tile.
m_no_projection_check() {
  perl -0pi -e 's/        if \(spelling\.isEmpty\(\)\)\n        \{/        if (false)\n        {/' src/layers/ogctilesource.cpp
}

# 7. A pyramid laid out on the server\x27s own grid is drawn as if it were
#    the slippy-map one.
m_any_pyramid_accepted() {
  perl -0pi -e 's/    if \(!set->isWebMercatorQuad\(\)\)/    if (false)/' src/layers/ogctilesource.cpp
}

# 8. A layer is accepted whether or not the service publishes it.
m_unknown_layer_accepted() {
  perl -0pi -e 's/      if \(!found\)\n      \{/      if (false)\n      {/' src/layers/ogctilesource.cpp
}

# ── fetching ────────────────────────────────────────────────────────────────

# 9. Tiles already asked for are asked for again, so a repaint costs a
#    request per tile on screen.
m_no_pending_check() {
  perl -0pi -e 's/    if \(url\.isEmpty\(\) \|\| m_cache\.contains\(url\) \|\| m_pending\.contains\(url\)\n        \|\| m_refused\.contains\(url\)\)/    if (url.isEmpty())/' src/layers/ogctilesource.cpp
}

# 10. A tile the service refuses is forgotten, so it is asked for again on
#     every repaint, forever.
m_refusal_forgotten() {
  perl -0pi -e 's/                        m_refused\.insert\(url\);//' src/layers/ogctilesource.cpp
}

# 11. Whatever came back is kept as a tile, so an XML exception report is
#     cached as a null image and drawn as a hole in the map.
m_cache_anything() {
  perl -0pi -e 's/                      if \(image\.isNull\(\)\)/                      if (false)/' src/layers/ogctilesource.cpp
}

# 12. Nothing is kept, so every tile is re-fetched the moment it is drawn
#     again.
m_no_cache() {
  perl -0pi -e 's/                      m_cache\.insert\(url, new QImage\(image\)\);//' src/layers/ogctilesource.cpp
}

run_mutation "1-whole-world-per-tile"     m_whole_world_per_tile
run_mutation "2-row-and-column-swapped"   m_row_and_column_swapped
run_mutation "3-level-is-the-zoom-number" m_level_is_the_zoom_number
run_mutation "4-transparent-basemap"      m_transparent_basemap
run_mutation "5-own-spelling"             m_own_spelling
run_mutation "6-no-projection-check"      m_no_projection_check
run_mutation "7-any-pyramid-accepted"     m_any_pyramid_accepted
run_mutation "8-unknown-layer-accepted"   m_unknown_layer_accepted
run_mutation "9-no-pending-check"         m_no_pending_check
run_mutation "10-refusal-forgotten"       m_refusal_forgotten
run_mutation "11-cache-anything"          m_cache_anything
run_mutation "12-no-cache"                m_no_cache

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_ogctilesource -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
$TEST 2>&1 | tail -3
