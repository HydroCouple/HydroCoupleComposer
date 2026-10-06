#!/usr/bin/env bash
# Load Directory fix falsifiers: a component plugin is a CMake MODULE, which
# on macOS is named ".so" and not ".dylib".
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
LIBRARY=src/plugins/componentlibrary.cpp
REGISTRY=src/plugins/componentregistry.cpp
BUILD=build/darwin
OBJDIR="$BUILD/CMakeFiles/composer_core.dir/src"
GATE='LoaderTest.AModuleStyleFileNameIsScannedLikeAnyOtherPlugin'

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    # Stale-relink hardening: force the mutated TUs to recompile.
    rm -f "$OBJDIR/plugins/componentlibrary.cpp.o" \
          "$OBJDIR/plugins/componentregistry.cpp.o"
    cmake --build "$BUILD" --target test_component_loader >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_component_loader" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { for f in "$LIBRARY" "$REGISTRY"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'LoaderTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the regression itself — only the SHARED spelling is scanned, which is
# what shipped and found nothing but this suite's own fixtures.
cp "$REGISTRY" "$REGISTRY.bak"
perl -0pi -e 's/      directory\.entryInfoList\(ComponentLibrary::libraryFilters\(\),\n                              QDir::Files \| QDir::NoSymLinks, QDir::Name\);/      directory.entryInfoList(QStringList{QStringLiteral("*") + ComponentLibrary::librarySuffix()},\n                              QDir::Files | QDir::NoSymLinks, QDir::Name);/s' "$REGISTRY"
rebuild
gate_ok "$GATE"
check "M1 only the SHARED suffix scanned" red $?
restore

# M2: the module spelling drops out of the filter list.
cp "$LIBRARY" "$LIBRARY.bak"
perl -0pi -e 's/    return \{QStringLiteral\("\*\.dylib"\), QStringLiteral\("\*\.so"\),\n            QStringLiteral\("\*\.bundle"\)\};/    return {QStringLiteral("*.dylib")};/s' "$LIBRARY"
rebuild
gate_ok "$GATE"
check "M2 module extension dropped from the filters" red $?
restore

trap - EXIT
rebuild; gate_ok 'LoaderTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
