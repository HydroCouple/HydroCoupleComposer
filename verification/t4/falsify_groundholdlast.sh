#!/bin/bash
# Falsification: declined ground-plane samples hold the survey's edge.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_groundplane
LOG=verification/t4/falsify_groundholdlast.log
: > "$LOG"

FILES=(src/scene/groundplane.cpp)

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

  if ! cmake --build $BUILD --target test_groundplane -j 8 >> "$LOG" 2>&1; then
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

# 1. The old behavior: declined samples stay at zero, nothing holds.
m_declined_stay_zero() {
  perl -0pi -e 's/          answered\[at\] = terrain->elevationAt\(/          answered[at] = 0; (void)terrain->elevationAt(/' src/scene/groundplane.cpp
}

# 2. The forward hold runs and the leading run never takes its height.
m_no_leading_hold() {
  perl -0pi -e 's/        \/\/ The leading run takes the first height the row does answer\.\n        holding = false;\n\n        for \(int column = across - 1; column >= 0; --column\)\n        \{\n          if \(answered\[start \+ size_t\(column\)\]\)\n          \{\n            held = heights\[start \+ size_t\(column\)\];\n            holding = true;\n          \}\n          else if \(holding\)\n          \{\n            heights\[start \+ size_t\(column\)\] = held;\n          \}\n        \}//' src/scene/groundplane.cpp
}

# 3. Unanswered rows stay at zero instead of copying their nearest
#    answered neighbor.
m_rows_stay_zero() {
  perl -0pi -e 's/        if \(source >= 0\)\n        \{\n          std::copy_n\(heights\.begin\(\) \+ long\(source\) \* across, across,\n                      heights\.begin\(\) \+ long\(row\) \* across\);\n        \}//' src/scene/groundplane.cpp
}

# 4 (retired). "A world with no answers chases held garbage" has no
#    independent mutation: held initializes to zero per row, so every way of
#    mis-holding an all-declined grid writes the zeros that were already
#    there. The AWorldWithNoAnswers gate stays as a tripwire for future
#    edits that seed held from somewhere else.

run_mutation 1-declined-stay-zero  m_declined_stay_zero
run_mutation 2-no-leading-hold     m_no_leading_hold
run_mutation 3-rows-stay-zero      m_rows_stay_zero

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
