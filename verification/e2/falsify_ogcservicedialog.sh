#!/bin/bash
# Falsification: pasting an address and getting a basemap out of it.
#
# Every mutation still connects, still lists something and still adds a
# layer. What they break is whether the service that was found is the one
# at that address, and whether a layer this map cannot draw is offered as
# though it could be.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_ogcservicedialog
LOG=verification/e2/falsify_ogcservicedialog.log
: > "$LOG"

FILES=(src/ui/dialogs/ogcservicedialog.cpp)

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

  if ! cmake --build $BUILD --target test_ogcservicedialog -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$($TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] OgcServiceDialog' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. An address is asked in one dialect only, so a WMTS -- which refuses a
#    WMS request rather than answering it -- looks like nothing at all.
m_no_second_dialect() {
  perl -0pi -e 's/    if \(asked == ServiceKind::Wms\)\n    \{\n      ask\(ServiceKind::Wmts\);\n\n      return;\n    \}//' src/ui/dialogs/ogcservicedialog.cpp
}

# 2. The answer is read as whatever was asked for rather than as what came
#    back, so a server that answers in the other service is misparsed.
m_trust_what_was_asked() {
  perl -0pi -e 's/    const ServiceKind kind = HydroCouple::Ogc::detectServiceKind\(body\);/    const ServiceKind kind = asked;/' src/ui/dialogs/ogcservicedialog.cpp
}

# 3. Every layer is offered, whether or not this map can draw it, so
#    choosing one adds a basemap that fetches an exception per tile.
m_offer_everything() {
  perl -0pi -e 's/      if \(WmsTileSource::webMercatorSpelling\(layer\)\.isEmpty\(\)\)/      if (false)/' src/ui/dialogs/ogcservicedialog.cpp
  perl -0pi -e 's/        if \(!set \|\| !set->isWebMercatorQuad\(\)\)/        if (false)/' src/ui/dialogs/ogcservicedialog.cpp
}

# 4. A source that says it cannot be drawn is handed to the map anyway,
#    where it fetches an exception per tile forever.
m_ignore_unusable_source() {
  perl -0pi -e 's/    if \(!source->isUsable\(\)\)\n    \{\n      return nullptr;\n    \}//' src/ui/dialogs/ogcservicedialog.cpp
}

# 5. The headings in a server's own layer tree are offered as layers, and
#    a GetMap naming one is answered with an exception.
m_group_layers_offered() {
  perl -0pi -e 's/      if \(!layer\.isRequestable\(\)\)\n      \{\n        continue;\n      \}//' src/ui/dialogs/ogcservicedialog.cpp
}

# 6. Nothing is chosen when the list fills, so Add is dead until the user
#    guesses that they must click a row first.
m_no_initial_selection() {
  perl -0pi -e 's/        m_layers->setCurrentRow\(row\);//' src/ui/dialogs/ogcservicedialog.cpp
}

# 7. What the server said went wrong is replaced by this program's guess.
m_lose_the_servers_message() {
  perl -0pi -e 's/    m_statusText = !refusal\.message\.isEmpty\(\)\n                     \? refusal\.message\n                     : \(error\.isEmpty\(\)\n                          \? tr\("That address is not a WMS, WMTS, WFS or WCS\."\)\n                          : error\);/    m_statusText = tr("That address is not a WMS, WMTS, WFS or WCS.");/' src/ui/dialogs/ogcservicedialog.cpp
}

# 8. Anything typed is fetched, so a stray word becomes a request to
#    whatever host Qt makes of it.
m_fetch_anything_typed() {
  perl -0pi -e 's/    if \(url\.isEmpty\(\)\)\n    \{\n      m_statusText = tr\("That is not a web address\."\);\n      m_status->setText\(m_statusText\);\n\n      return;\n    \}//' src/ui/dialogs/ogcservicedialog.cpp
}

# 9. Only the two map services are tried, so a feature service reads as
#    nothing at all -- the case this phase exists for.
m_no_wfs_dialect() {
  perl -0pi -e 's/    if \(asked == ServiceKind::Wmts\)\n    \{\n      ask\(ServiceKind::Wfs\);\n\n      return;\n    \}//' src/ui/dialogs/ogcservicedialog.cpp
}

# 10. A collection is fetched over the whole country the service holds
#     rather than over the ground the map is looking at.
m_ignore_the_view() {
  perl -0pi -e 's/      request\.extent = m_preferredExtent;//' src/ui/dialogs/ogcservicedialog.cpp
}

# 11. The dialog accepts before the features are in hand, so a collection
#     holding nothing over that ground closes it on an empty layer.
m_accept_before_fetching() {
  perl -0pi -e 's/      if \(m_kind == ServiceKind::Wfs\)\n      \{\n        fetchFeaturesThenAccept\(\);\n\n        return;\n      \}//' src/ui/dialogs/ogcservicedialog.cpp
}

# 12. Whatever format the collection lists first is asked for, which for
#     this service is GML -- readable only with a schema and a driver.
m_no_format_preference() {
  perl -0pi -e 's/    request\.outputFormat =\n      HydroCouple::Ogc::preferredOutputFormat\(\*type, m_wfs\.outputFormats\);//' src/ui/dialogs/ogcservicedialog.cpp
}

# ── the fourth dialect ──────────────────────────────────────────────────────

# A. Stop after three dialects, so a coverage service is reported as not a
#    service at all.
m_ladder_stops_at_three() {
  perl -0pi -e 's/    if \(asked == ServiceKind::Wfs\)\n    \{\n      ask\(ServiceKind::Wcs\);\n\n      return;\n    \}//' src/ui/dialogs/ogcservicedialog.cpp
}

# B. Ask for the coverage without describing it first, so the subset is
#    written against axes nobody has read.
m_skip_describe() {
  perl -0pi -e 's/        describeThenFetchCoverage\(\);/        accept();/' src/ui/dialogs/ogcservicedialog.cpp
}

# C. Ignore the view and always ask for the coverage whole, which for a
#    national elevation model is a country when a field was wanted.
m_whole_coverage() {
  perl -0pi -e 's/            wanted = wanted\.intersected\(view\);//' src/ui/dialogs/ogcservicedialog.cpp
}

# D. Ask at native resolution, which for half-metre cells over a country is
#    millions of them.
m_unbounded_size() {
  perl -0pi -e 's/    request\.size =\n      QSize\(width, qBound\(1, static_cast<int>\(width \* aspect\), 4096\)\);//' src/ui/dialogs/ogcservicedialog.cpp
}

# E. Fetch a coverage whose axes cannot be chosen between rather than
#    refusing it, so a time series is asked for as though it were a map.
m_fetch_any_dimensions() {
  perl -0pi -e 's/    if \(!description\.isTwoDimensional\(\)\)/    if (false)/' src/ui/dialogs/ogcservicedialog.cpp
}

run_mutation "1-no-second-dialect"        m_no_second_dialect
run_mutation "2-trust-what-was-asked"     m_trust_what_was_asked
run_mutation "3-offer-everything"         m_offer_everything
run_mutation "4-ignore-unusable-source"   m_ignore_unusable_source
run_mutation "5-group-layers-offered"     m_group_layers_offered
run_mutation "6-no-initial-selection"     m_no_initial_selection
run_mutation "7-lose-servers-message"     m_lose_the_servers_message
run_mutation "8-fetch-anything-typed"     m_fetch_anything_typed
run_mutation "9-no-wfs-dialect"          m_no_wfs_dialect
run_mutation "10-ignore-the-view"        m_ignore_the_view
run_mutation "11-accept-before-fetching" m_accept_before_fetching
run_mutation "12-no-format-preference"   m_no_format_preference

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_ogcservicedialog -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
$TEST 2>&1 | tail -3
run_mutation A-ladder-stops-at-three  m_ladder_stops_at_three
run_mutation B-skip-describe          m_skip_describe
run_mutation C-whole-coverage         m_whole_coverage
run_mutation D-unbounded-size         m_unbounded_size
run_mutation E-fetch-any-dimensions   m_fetch_any_dimensions

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
