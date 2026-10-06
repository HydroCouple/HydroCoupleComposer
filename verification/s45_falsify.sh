#!/usr/bin/env bash
# S4.5 falsifiers: the binding chip (B5a demo-critical subset).
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
CONF=src/configurator/componentconfigurator.cpp
BUILD=build/darwin
OBJ="$BUILD/CMakeFiles/composer_core.dir/src/configurator/componentconfigurator.cpp.o"

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    # Stale-relink hardening: force the mutated TU to recompile.
    rm -f "$OBJ"
    cmake --build "$BUILD" --target test_configurator >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_configurator" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { [ -f "$CONF.bak" ] && mv "$CONF.bak" "$CONF" && touch "$CONF"; }

rebuild; gate_ok 'ConfiguratorTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the chip never appears — bound arguments fall back to the path box.
cp "$CONF" "$CONF.bak"
perl -0pi -e 's/          if \(bound\)\n          \{\n            auto \*chip = new QWidget\(this\);/          if (false)\n          \{\n            auto *chip = new QWidget(this);/s' "$CONF"
rebuild
gate_ok 'ConfiguratorTest.ABoundArgumentShowsItsProvenanceChip'
check "M1 chip suppressed" red $?
restore

# M2: the dangling state is never worn.
cp "$CONF" "$CONF.bak"
perl -0pi -e 's/            if \(m_document &&\n                !m_document->componentIds\(\)\.contains\(boundProvider\)\)/            if (false)/s' "$CONF"
rebuild
gate_ok 'ConfiguratorTest.ADanglingBindingWearsARedBadge'
check "M2 dangling badge dropped" red $?
restore

# M3: unbind touches nothing.
cp "$CONF" "$CONF.bak"
perl -0pi -e 's/                      if \(m_document &&\n                          m_document->clearArgument\(m_componentId, id\)\)/                      if (false \&\& m_document \&\&\n                          m_document->clearArgument(m_componentId, id))/s' "$CONF"
rebuild
gate_ok 'ConfiguratorTest.UnbindingRestoresTheFileRow'
check "M3 unbind is a no-op" red $?
restore

trap - EXIT
rebuild; gate_ok 'ConfiguratorTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
