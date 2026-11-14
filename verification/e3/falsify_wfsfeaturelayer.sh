#!/bin/bash
# Falsification: features fetched from a service, and made into a domain.
#
# Every mutation still produces a layer with features in it. What they break
# is whether the features are the ones the service sent, whether a refusal
# is told apart from gibberish, and whether two answers in flight at once
# stay separate.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_wfsfeaturelayer
LOG=verification/e3/falsify_wfsfeaturelayer.log
: > "$LOG"

FILES=(src/layers/wfsfeaturelayer.cpp src/layers/ogrfeatureloader.cpp)

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

  if ! cmake --build $BUILD --target test_wfsfeaturelayer -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$($TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] Wfs' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The response is left in memory after a successful decode, so it is
#    both a leak and the bytes the next fetch would decode instead of its
#    own.
m_no_unlink_on_success() {
  perl -0pi -e 's/    GDALClose\(dataset\);\n    VSIUnlink\(path\.toUtf8\(\)\.constData\(\)\);/    GDALClose(dataset);/' src/layers/wfsfeaturelayer.cpp
}

# 2. The response is left in memory when it could not be opened at all --
#    the case where something unreadable is what gets left behind.
m_no_unlink_on_failure() {
  perl -0pi -e 's/                  : said;\n      VSIUnlink\(path\.toUtf8\(\)\.constData\(\)\);/                  : said;/' src/layers/wfsfeaturelayer.cpp
}

# 3. A refusal is reported as gibberish, so the service's own account of
#    what is wrong -- which names the collection -- is thrown away.
m_lose_the_refusal() {
  perl -0pi -e 's/      const QString said = refusal\(body\);\n\n      message = said\.isEmpty\(\)\n                  \? QObject::tr\("The service&#39;s answer was not features this "\n                                "program can read\."\)\n                  : said;/      message = QObject::tr("The response could not be read.");/' src/layers/wfsfeaturelayer.cpp
  perl -0pi -e "s/      const QString said = refusal\(body\);\n\n      message = said\.isEmpty\(\)\n                  \? QObject::tr\(\"The service's answer was not features this \"\n                                \"program can read\.\"\)\n                  : said;/      message = QObject::tr(\"The response could not be read.\");/" src/layers/wfsfeaturelayer.cpp
}

# 4. An answer holding nothing is accepted as a layer, so an empty
#    collection joins the map as an empty layer nobody can explain.
m_accept_empty() {
  perl -0pi -e 's/    if \(read == 0\)/    if (false)/' src/layers/wfsfeaturelayer.cpp
}

# 5. The system the service answered in is not read, so the features are
#    drawn as though they were already in the map's own coordinates.
m_drop_the_crs() {
  perl -0pi -e 's/        contents\.crsWkt = QString::fromUtf8\(wkt\);//' src/layers/ogrfeatureloader.cpp
}

# 6. The attributes are dropped, so a fetched layer can be seen but not
#    classified, queried or labelled.
m_drop_the_fields() {
  perl -0pi -e 's/      contents\.fields\.append\(entry\);//' src/layers/ogrfeatureloader.cpp
}

# 7. Features whose geometry could not be read are kept anyway, as empty
#    ones that draw nothing and select as though they were there.
m_keep_unreadable_geometry() {
  perl -0pi -e 's/      if \(collectOgrGeometry\(feature->GetGeometryRef\(\), entry\.parts,\n                             entry\.kind\)\)/      if (true)/' src/layers/ogrfeatureloader.cpp
}

run_mutation "1-no-unlink-on-success"     m_no_unlink_on_success
run_mutation "2-no-unlink-on-failure"     m_no_unlink_on_failure
run_mutation "3-lose-the-refusal"         m_lose_the_refusal
run_mutation "4-accept-empty"             m_accept_empty
run_mutation "5-drop-the-crs"             m_drop_the_crs
run_mutation "6-drop-the-fields"          m_drop_the_fields
run_mutation "7-keep-unreadable-geometry" m_keep_unreadable_geometry

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_wfsfeaturelayer -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
$TEST 2>&1 | tail -3
