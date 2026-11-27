#!/usr/bin/env bash
# C2 falsifiers: sidecar adapter positions.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
PRES=src/project/presentation.cpp
DOC=src/project/compositiondocument.cpp
BUILD=build/darwin

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target test_compositiondocument >/dev/null 2>&1; }
gate_ok() { "$BUILD/tests/gui/test_compositiondocument" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { for f in "$PRES" "$DOC"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'PresentationAdapters.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: rename no longer rekeys chains.
cp "$PRES" "$PRES.bak"
perl -0pi -e 's/      if \(entry\.fromComponent != fromId && entry\.toComponent != fromId\)\n      \{\n        continue;\n      \}/      continue;/s' "$PRES"
rebuild
gate_ok 'PresentationAdapters.RenamingAComponentRekeysItsAdapterChains'
check "M1 rename stops rekeying" red $?
restore

# M2: the role falls out of the chain identity (two roles merge).
cp "$PRES" "$PRES.bak"
perl -0pi -e 's/    return fromComponent \+ separator \+ output \+ separator \+ toComponent \+\n           separator \+ input \+ separator \+ role;/    (void)role;\n    return fromComponent + separator + output + separator + toComponent +\n           separator + input;/s' "$PRES"
rebuild
gate_ok 'PresentationAdapters.TheSidecarRoundTripsAdapterPositionsKeyedByConnection'
check "M2 role dropped from identity" red $?
restore

# M3: the sidecar never writes the adapters block.
cp "$PRES" "$PRES.bak"
perl -0pi -e 's/    if \(!m_adapterChains\.isEmpty\(\)\)\n    \{/    if (false)\n    {/s' "$PRES"
rebuild
gate_ok 'PresentationAdapters.TheSidecarRoundTripsAdapterPositionsKeyedByConnection'
check "M3 adapters block never written" red $?
restore

# M4: phantom chain steps take positions anyway.
cp "$DOC" "$DOC.bak"
perl -0pi -e 's/    if \(link == m_spec\.connections\.end\(\) \|\| index < 0 \|\|\n        index >= static_cast<int>\(link->adaptedOutputs\.size\(\)\)\)\n    \{\n      return false;\n    \}/    if (link == m_spec.connections.end() || index < 0)\n    {\n      return false;\n    }/s' "$DOC"
rebuild
gate_ok 'PresentationAdapters.MovingAnAdapterIsUndoableAndMerges'
check "M4 phantom steps accepted" red $?
restore

trap - EXIT
rebuild; gate_ok 'PresentationAdapters.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
