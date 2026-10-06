#!/usr/bin/env bash
# S4.4 falsifiers: SimulationManager forwards ModelInitializer progress.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
MANAGER=src/simulation/simulationmanager.cpp
BUILD=build/darwin
OBJ="$BUILD/CMakeFiles/composer_core.dir/src/simulation/simulationmanager.cpp.o"
GATE='SimulationTest.StartReportsStagedInitializationProgress'

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    # Stale-relink hardening: force the mutated TU to recompile.
    rm -f "$OBJ"
    cmake --build "$BUILD" --target test_simulationmanager >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_simulationmanager" --gtest_filter="$GATE" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { [ -f "$MANAGER.bak" ] && mv "$MANAGER.bak" "$MANAGER" && touch "$MANAGER"; }

rebuild; gate_ok; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the callback is never installed — the apply happens silently.
cp "$MANAGER" "$MANAGER.bak"
perl -0pi -e 's/    d->initializer->setProgressCallback\(\n      \[this\]\(int stage, int stageCount, const std::string &componentId,\n             const std::string &activity\)\n      \{\n.*?\n      \}\);\n\n//s' "$MANAGER"
rebuild
gate_ok; check "M1 progress callback never installed" red $?
restore

# M2: the stage is forwarded 0-based.
cp "$MANAGER" "$MANAGER.bak"
perl -0pi -e 's/          stage \+ 1, stageCount, QString::fromStdString\(componentId\),/          stage, stageCount, QString::fromStdString(componentId),/s' "$MANAGER"
rebuild
gate_ok; check "M2 stage forwarded zero-based" red $?
restore

# M3: component and activity swapped in the forward.
cp "$MANAGER" "$MANAGER.bak"
perl -0pi -e 's/          stage \+ 1, stageCount, QString::fromStdString\(componentId\),\n          QString::fromStdString\(activity\)\);/          stage + 1, stageCount, QString::fromStdString(activity),\n          QString::fromStdString(componentId));/s' "$MANAGER"
rebuild
gate_ok; check "M3 component and activity swapped" red $?
restore

trap - EXIT
rebuild; gate_ok; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
