#!/bin/bash
# Falsification: a coverage fetched from a WCS, once it is bytes.
#
# Every mutation still produces a layer that decodes and draws. What they
# break is where on the earth it lands, what its numbers mean, whether a
# refusal is read as one, and whether the memory behind it is ever given
# back.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_wcscoveragelayer
LOG=verification/e4/falsify_wcscoveragelayer.log
: > "$LOG"

FILES=(src/layers/wcscoveragelayer.cpp src/layers/gdalrasterlayer.cpp)

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

  if ! cmake --build $BUILD --target test_wcscoveragelayer -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$($TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] WcsCoverageLayer' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── where it lands, and what its numbers mean ───────────────────────────────

# 1. Drop the geotransform, so the coverage is placed at the origin with one
#    unit per cell. It still draws; it is just not in the Netherlands.
m_no_geotransform() {
  perl -0pi -e 's/    m_extent = extentOf\(dataset\);/    m_extent = QRectF(0, 0, dataset->GetRasterXSize(), dataset->GetRasterYSize());/' src/layers/gdalrasterlayer.cpp
}

# 2. Ignore the projection the coverage declares, so national-grid meters are
#    read as degrees.
m_no_crs() {
  perl -0pi -e 's/        setCrs\(SpatialReference::fromDefinition\(QString::fromUtf8\(wkt\),\n                                                crsMessage\)\);//' src/layers/gdalrasterlayer.cpp
}

# 3. Treat a single float band as a picture, so elevations are shown as gray
#    levels and never stretched over their own range.
m_data_is_a_picture() {
  perl -0pi -e 's/    m_colorImage = m_bandCount >= 3\n                   && dataset->GetRasterBand\(1\)->GetRasterDataType\(\) == GDT_Byte;/    m_colorImage = true;/' src/layers/gdalrasterlayer.cpp
}

# ── the memory behind it ────────────────────────────────────────────────────

# 4. Never unlink the buffer, so every coverage ever opened is held until the
#    program exits.
m_never_unlink() {
  perl -0pi -e 's/    if \(!m_memoryPath\.isEmpty\(\)\)\n    \{\n      VSIUnlink\(m_memoryPath\.toUtf8\(\)\.constData\(\)\);\n    \}//' src/layers/wcscoveragelayer.cpp
}

# 5. Let GDAL borrow the buffer rather than take it over, so nothing ever
#    frees it. A leak with no visible symptom in-process -- recorded here
#    because it is real, not because a gate can see it.
m_borrow_ownership() {
  perl -0pi -e 's/      static_cast<vsi_l_offset>\(body\.size\(\)\), TRUE\);/      static_cast<vsi_l_offset>(body.size()), FALSE);/' src/layers/wcscoveragelayer.cpp
}

# 6. One fixed /vsimem name for every coverage, which is right for the WFS
#    layer beside this one and wrong here: a raster keeps its dataset open.
m_shared_vsimem_name() {
  perl -0pi -e 's/      QStringLiteral\("\/vsimem\/hydrocouple-wcs-%1"\)\n        \.arg\(QUuid::createUuid\(\)\.toString\(QUuid::WithoutBraces\)\);/      QStringLiteral("\/vsimem\/hydrocouple-wcs-response");/' src/layers/wcscoveragelayer.cpp
}

# 7. Free the copy after handing it over, so GDAL reads memory that has
#    been returned.
m_free_after_handover() {
  perl -0pi -e 's/    VSIFCloseL\(file\);/    VSIFCloseL(file);\n    CPLFree(buffer);/' src/layers/wcscoveragelayer.cpp
}

# ── reading a refusal ───────────────────────────────────────────────────────

# 8. Report a generic failure instead of what the service actually said, so
#    "you named an axis this coverage has not got" becomes "not a coverage".
m_swallow_the_exception() {
  perl -0pi -e 's/      const QString reported =\n        HydroCouple::Ogc::wcsExceptionText\(body\);/      const QString reported;/' src/layers/wcscoveragelayer.cpp
}

# 9. Accept an empty body, so a layer with no raster reaches the map.
m_accept_empty() {
  perl -0pi -e 's/    if \(body\.isEmpty\(\)\)\n    \{\n      message = QObject::tr\("The service returned nothing for %1\."\)\.arg\(name\);\n\n      return nullptr;\n    \}//' src/layers/wcscoveragelayer.cpp
}

# 10. Report the /vsimem path as the layer's source, which names nothing a
#     user could open.
m_expose_vsimem_path() {
  perl -0pi -e 's/    return QStringLiteral\("%1 \(%2\)"\)\.arg\(m_serviceUrl, m_coverageId\);/    return m_memoryPath;/' src/layers/wcscoveragelayer.cpp
}

run_mutation 1-no-geotransform       m_no_geotransform
run_mutation 2-no-crs                m_no_crs
run_mutation 3-data-is-a-picture     m_data_is_a_picture
run_mutation 4-never-unlink          m_never_unlink
run_mutation 5-borrow-ownership      m_borrow_ownership
run_mutation 6-shared-vsimem-name    m_shared_vsimem_name
run_mutation 7-free-after-handover   m_free_after_handover
run_mutation 8-swallow-the-exception m_swallow_the_exception
run_mutation 9-accept-empty          m_accept_empty
run_mutation 10-expose-vsimem-path   m_expose_vsimem_path

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
