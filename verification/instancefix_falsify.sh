#!/usr/bin/env bash
# Edit-time instance falsifiers: the live component must describe the
# DOCUMENT's component, and must be realised again when that block changes.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
INSTANCES=src/project/componentinstances.cpp
BUILD=build/darwin
OBJ="$BUILD/CMakeFiles/composer_core.dir/src/project/componentinstances.cpp.o"

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    # Stale-relink hardening: force the mutated TU to recompile.
    rm -f "$OBJ"
    cmake --build "$BUILD" --target test_compositioncanvas >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_compositioncanvas" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { [ -f "$INSTANCES.bak" ] && mv "$INSTANCES.bak" "$INSTANCES" && touch "$INSTANCES"; }

rebuild; gate_ok 'CanvasTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the regression itself — arguments are never applied, so every box on
# the canvas is a DEFAULT component rather than the document's.
cp "$INSTANCES" "$INSTANCES.bak"
perl -0pi -e 's/          \(void\)argument->initialize\(\n            payload\.dump\(\),\n            HydroCouple::IArgument::ArgumentInputType::JSON, argumentMessage\);/          (void)argumentMessage;/s' "$INSTANCES"
rebuild
gate_ok 'CanvasTest.TheDocumentsArgumentsReachTheLiveComponent'
check "M1 document arguments never applied" red $?
restore

# M2: the instance is never realised again, so configuring a component
# leaves the canvas describing the old one for the session.
cp "$INSTANCES" "$INSTANCES.bak"
perl -0pi -e 's/      connect\(m_document, &CompositionDocument::compositionChanged, this,\n              \[this\] \{ dropInstancesWhoseSpecChanged\(\); \}\);/      ;/s' "$INSTANCES"
rebuild
gate_ok 'CanvasTest.ChangingAnArgumentRealisesTheComponentAgain'
check "M2 stale instance kept after an argument change" red $?
restore

# M3: everything is dropped on any change — correct ports, but a component
# rebuilt on every unrelated edit (and identity lost under the canvas).
cp "$INSTANCES" "$INSTANCES.bak"
perl -0pi -e 's/      const bool stale =\n        !now\.has_value\(\) \|\| !m_realisedFrom\.contains\(id\)\n        \|\| !\(m_realisedFrom\.value\(id\) == \*now\);/      const bool stale = true;/s' "$INSTANCES"
rebuild
gate_ok 'CanvasTest.ChangingAnArgumentRealisesTheComponentAgain'
check "M3 unrelated edits churn every instance" red $?
restore

# M4: the binding skip removed. GREEN on purpose — two mechanisms cover
# this property and the gate cannot tell them apart. Every argument type
# that ships refuses a sigil object it does not know (a mesh argument reads
# @uri and rejects the rest; the scalar ones reject a bare object), so the
# skip's effect is invisible from outside. It stays because it is where the
# Q9 contract belongs — a host must not hand a component a reference it has
# not resolved — and because that refusal is the ARGUMENT's promise, not
# this class's to assume. This arm exists so the redundancy is recorded
# rather than mistaken for coverage.
cp "$INSTANCES" "$INSTANCES.bak"
perl -0pi -e 's/        if \(HydroCouple::SDK::IO::isArgumentBinding\(payload\)\)\n        \{\n          continue;\n        \}//s' "$INSTANCES"
rebuild
gate_ok 'CanvasTest.ABoundArgumentIsNotResolvedWhileEditing'
check "M4 skip removed — the argument layer still refuses" green $?
restore

trap - EXIT
rebuild; gate_ok 'CanvasTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
