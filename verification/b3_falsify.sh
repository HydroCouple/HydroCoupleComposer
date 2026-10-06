#!/usr/bin/env bash
# B3 falsifiers: SimulationManager's standalone-factory resolver hookup.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
SRC=src/simulation/simulationmanager.cpp
BUILD=build/darwin
GATE='SimulationTest.ARunBuildsAdapterChainsFromStandaloneFactoryLibraries'

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target test_simulationmanager >/dev/null 2>&1; }
gate_ok() { "$BUILD/tests/gui/test_simulationmanager" --gtest_filter="$GATE" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { [ -f "$SRC.bak" ] && mv "$SRC.bak" "$SRC" && touch "$SRC"; }

rebuild; gate_ok; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the resolver is never installed.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    d->initializer->setAdapterFactoryResolver\(/    if (false) d->initializer->setAdapterFactoryResolver(/s' "$SRC"
rebuild
gate_ok; check "M1 resolver never installed" red $?
restore

# M2: factory libraries are never realised (only the SDK factory rides).
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    for \(HydroCouple::IComponentInfo \*info : d->registry->entries\(\n           ComponentRegistry::ComponentKind::AdapterFactory\)\)/    for (HydroCouple::IComponentInfo *info :\n         std::vector<HydroCouple::IComponentInfo *>{})/s' "$SRC"
rebuild
gate_ok; check "M2 libraries never realised" red $?
restore

trap - EXIT
rebuild; gate_ok; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
