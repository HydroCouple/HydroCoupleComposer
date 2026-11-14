#!/usr/bin/env bash
# C4 falsifiers: adapter offerings + insertion through the menu seams.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
SRC=src/canvas/compositionscene.cpp
BUILD=build/darwin

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target test_compositioncanvas >/dev/null 2>&1; }
gate_ok() { "$BUILD/tests/gui/test_compositioncanvas" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { [ -f "$SRC.bak" ] && mv "$SRC.bak" "$SRC" && touch "$SRC"; }

rebuild; gate_ok 'CanvasTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: insertion stops capturing the offering's argument defaults.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/          std::unique_ptr<HydroCouple::IAdaptedOutput> scratch =\n            factory->createAdaptedOutput\(offering, output, input\);/          std::unique_ptr<HydroCouple::IAdaptedOutput> scratch;/s' "$SRC"
rebuild
gate_ok 'CanvasTest.InsertingAnAdapterThroughTheMenuMatchesTheHandWrittenDocument'
check "M1 defaults never captured" red $?
restore

# M2: the scratch instance is left registered with the live output.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/            output->removeAdaptedOutput\(scratch\.get\(\)\);/            (void)scratch;/s' "$SRC"
rebuild
gate_ok 'CanvasTest.InsertingAnAdapterThroughTheMenuMatchesTheHandWrittenDocument'
check "M2 scratch left registered" red $?
restore

# M3: standalone factories vanish from the offerings.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    if \(m_instances\)\n    \{\n      for \(HydroCouple::IAdaptedOutputFactory \*factory :\n           m_instances->adapterFactories\(\)\)\n      \{\n        if \(factory\)\n        \{\n          factories\.push_back\(factory\);\n        \}\n      \}\n    \}\n\n    for \(HydroCouple::IAdaptedOutputFactory \*factory : factories\)\n    \{\n      for \(HydroCouple::IIdentity \*offering :\n           factory->getAvailableAdaptedOutputIds\(output, input\)\)/    for (HydroCouple::IAdaptedOutputFactory *factory : factories)\n    {\n      for (HydroCouple::IIdentity *offering :\n           factory->getAvailableAdaptedOutputIds(output, input))/s' "$SRC"
rebuild
gate_ok 'CanvasTest.TheInsertMenuOffersOnlyAdaptersTheFactoriesReportAvailable'
check "M3 standalone factories not consulted" red $?
restore

trap - EXIT
rebuild; gate_ok 'CanvasTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
