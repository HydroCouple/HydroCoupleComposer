#!/usr/bin/env bash
# C1 falsifiers: document chain-edit API + applySpec change detection.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
SRC=src/project/compositiondocument.cpp
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
restore() { [ -f "$SRC.bak" ] && mv "$SRC.bak" "$SRC" && touch "$SRC"; }

rebuild; gate_ok 'CompositionDocument*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: applySpec regresses to the size-only diff.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    const bool componentsDiffer = spec\.components != m_spec\.components;\n    const bool connectionsDiffer = spec\.connections != m_spec\.connections;/    const bool componentsDiffer = spec.components.size() != m_spec.components.size();\n    const bool connectionsDiffer = spec.connections.size() != m_spec.connections.size();/s' "$SRC"
rebuild
gate_ok 'CompositionDocumentChains.EditingAConnectionsChainIsUndoableAndSignalsConnectionsChanged'
check "M1 size-only diff restored" red $?
restore

# M2: insert ignores the index and always lands at the chain head.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    link->adaptedOutputs\.insert\(link->adaptedOutputs\.begin\(\) \+ index, step\);/    link->adaptedOutputs.insert(link->adaptedOutputs.begin(), step);/s' "$SRC"
rebuild
gate_ok 'CompositionDocumentChains.RemovingAnAdapterStepRenumbersItsSiblings'
check "M2 insert index ignored" red $?
restore

# M3: the role collision probe is skipped.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    if \(std::any_of\(m_spec\.connections\.begin\(\), m_spec\.connections\.end\(\),\n                    \[&probe\]\(const ConnectionSpec &existing\)\n                    \{ return sameConnection\(existing, probe\); \}\)\)\n    \{\n      return false;\n    \}\n//s' "$SRC"
rebuild
gate_ok 'CompositionDocumentChains.ChangingTheRoleRefusesACollidingIdentity'
check "M3 role collision unchecked" red $?
restore

# M4: out-of-range removals clamp instead of refusing.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    ConnectionSpec \*link = findConnection\(after, connection\);\n\n    if \(!link \|\| index < 0 \|\|\n        index >= static_cast<int>\(link->adaptedOutputs\.size\(\)\)\)\n    \{\n      return false;\n    \}\n\n    link->adaptedOutputs\.erase\(link->adaptedOutputs\.begin\(\) \+ index\);/    ConnectionSpec *link = findConnection(after, connection);\n\n    if (!link || link->adaptedOutputs.empty())\n    {\n      return false;\n    }\n\n    index = std::clamp(index, 0, static_cast<int>(link->adaptedOutputs.size()) - 1);\n    link->adaptedOutputs.erase(link->adaptedOutputs.begin() + index);/s' "$SRC"
rebuild
gate_ok 'CompositionDocumentChains.RemovingAnAdapterStepRenumbersItsSiblings'
check "M4 removal clamps out-of-range" red $?
restore

trap - EXIT
rebuild; gate_ok 'CompositionDocument*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
