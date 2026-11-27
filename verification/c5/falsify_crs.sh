#!/bin/bash
# C5b falsification run: the CRS catalog, the chooser, and assignment.
#
# Mutation 1 is the defect this slice found and fixed rather than one invented
# for the harness: setCrs() did not invalidate the projection cache, so a layer
# whose declared system was corrected went on drawing where the old one had
# put it.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_crs
LOG=verification/c5/falsify_crs.log
: > "$LOG"

FILES=(src/map/maplayer.cpp src/gis/crscatalog.cpp
       src/ui/dialogs/crsselectiondialog.cpp)

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

  if ! cmake --build $BUILD --target test_crs -j 8 >> "$LOG" 2>&1; then
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] Crs' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The original defect: assigning a layer's own CRS leaves the cached
#    projection standing, so the layer keeps drawing in the old place.
m_no_invalidate() {
  perl -0pi -e 's/    onProjectionChanged\(\);\n\n    \/\/ The extent is unchanged/    \/\/ The extent is unchanged/' src/map/maplayer.cpp
}

# 2. Deprecated systems are offered.
m_keep_deprecated() {
  perl -0pi -e 's/        if \(kind == CrsKind::Any \|\| info->bDeprecated\)/        if (kind == CrsKind::Any)/' src/gis/crscatalog.cpp
}

# 3. Systems a map cannot be drawn in are offered as projected.
m_keep_nonmap() {
  perl -0pi -e 's/          return CrsKind::Any;/          return CrsKind::Projected;/' src/gis/crscatalog.cpp
}

# 4. The query is matched as one phrase, so word order starts to matter.
m_phrase_match() {
  perl -0pi -e 's/    const QStringList words =\n      query\.split\(QLatin1Char\(. .\), Qt::SkipEmptyParts\);/    const QStringList words = query.isEmpty() ? QStringList() : QStringList{query};/' src/gis/crscatalog.cpp
}

# 5. Codes are not searched, only names and areas.
m_no_code_search() {
  perl -0pi -e 's/      const QString haystack = QStringLiteral\("%1 %2 %3"\)\n                                 \.arg\(entry\.name, entry\.areaName,\n                                      entry\.authCode\(\)\);/      const QString haystack = QStringLiteral("%1 %2").arg(entry.name, entry.areaName);/' src/gis/crscatalog.cpp
}

# 6. The kind filter passes everything.
m_ignore_kind() {
  perl -0pi -e 's/      if \(kind != CrsKind::Any \&\& entry\.kind != kind\)/      if (false)/' src/gis/crscatalog.cpp
}

# 7. The selection is compared against the member the list clear() has already
#    blanked, so a search after choosing loses the choice.
m_selection_lost() {
  perl -0pi -e 's/      if \(!wanted\.isEmpty\(\) \&\& entry\.authCode\(\) == wanted\)/      if (!m_selectedAuthCode.isEmpty() \&\& entry.authCode() == m_selectedAuthCode)/' src/ui/dialogs/crsselectiondialog.cpp
}

# 8. A system filtered out of the list stays selected invisibly, so OK acts on
#    something the user can no longer see.
m_keep_invisible() {
  perl -0pi -e 's/    if \(reselect\)\n    \{\n      m_list->setCurrentItem\(reselect\);\n      m_list->scrollToItem\(reselect\);\n    \}/    m_selectedAuthCode = wanted;\n\n    if (reselect)\n    {\n      m_list->setCurrentItem(reselect);\n      m_list->scrollToItem(reselect);\n    }/' src/ui/dialogs/crsselectiondialog.cpp
}

# 9. Authority and code are swapped when the current system is preselected.
m_swapped_authcode() {
  perl -0pi -e 's/      QStringLiteral\("%1:%2"\)\.arg\(authority\)\.arg\(current->authSRID\(\)\);/      QStringLiteral("%1:%2").arg(QString::number(current->authSRID()), authority);/' src/ui/dialogs/crsselectiondialog.cpp
}

# 10. The chosen code is parsed from the wrong half of the token.
m_wrong_section() {
  perl -0pi -e 's/    const int code =\n      m_selectedAuthCode\.section\(QLatin1Char\(.:.\), 1, 1\)\.toInt\(\);/    const int code = m_selectedAuthCode.section(QLatin1Char(0x3a), 0, 0).toInt();/' src/ui/dialogs/crsselectiondialog.cpp
}

run_mutation "1-assign-keeps-old-projection" m_no_invalidate
run_mutation "2-offers-deprecated"           m_keep_deprecated
run_mutation "3-offers-undrawable-systems"   m_keep_nonmap
run_mutation "4-phrase-not-words"            m_phrase_match
run_mutation "5-codes-not-searched"          m_no_code_search
run_mutation "6-kind-filter-ignored"         m_ignore_kind
run_mutation "7-search-loses-selection"      m_selection_lost
run_mutation "8-invisible-selection-kept"    m_keep_invisible
run_mutation "9-authority-code-swapped"      m_swapped_authcode
run_mutation "10-code-parsed-from-authority" m_wrong_section

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_crs -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
