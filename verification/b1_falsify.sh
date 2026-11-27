#!/usr/bin/env bash
# B1 falsifiers: registry kinds + adapter-factory creation.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
SRC=src/plugins/componentregistry.cpp
BUILD=build/darwin

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target test_component_loader >/dev/null 2>&1; }
gate_ok() { "$BUILD/tests/gui/test_component_loader" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { [ -f "$SRC.bak" ] && mv "$SRC.bak" "$SRC" && touch "$SRC"; }

rebuild; gate_ok 'LoaderTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: every info classifies as Model.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    if \(dynamic_cast<HydroCouple::IModelComponentInfo \*>\(info\)\)\n    \{\n      return ComponentKind::Model;\n    \}/    return ComponentKind::Model;\n    if (dynamic_cast<HydroCouple::IModelComponentInfo *>(info))\n    {\n      return ComponentKind::Model;\n    }/s' "$SRC"
rebuild
gate_ok 'LoaderTest.RegistryTellsModelComponentsFromAdapterFactories'
check "M1 kindOf always Model" red $?
restore

# M2: the factory cast always fails.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/    auto \*factoryInfo =\n      dynamic_cast<HydroCouple::IAdaptedOutputFactoryComponentInfo \*>\(info\);/    auto *factoryInfo =\n      static_cast<HydroCouple::IAdaptedOutputFactoryComponentInfo *>(nullptr);\n    (void)info;/s' "$SRC"
rebuild
gate_ok 'LoaderTest.CreatesAnAdapterFactoryFromItsLibrary'
check "M2 factory creation broken" red $?
restore

# M3: the refusal stops saying where adapters belong.
cp "$SRC" "$SRC.bak"
perl -0pi -e 's/adapters "\n                           "attach to connections, they are not placed as "\n                           "components"/adapters go elsewhere"/s' "$SRC"
rebuild
gate_ok 'LoaderTest.RegistryTellsModelComponentsFromAdapterFactories'
check "M3 refusal loses its guidance" red $?
restore

trap - EXIT
rebuild; gate_ok 'LoaderTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
