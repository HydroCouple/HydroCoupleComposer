#!/usr/bin/env bash
# B2 falsifiers: palette sections + drag/drop defense.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
PALETTE=src/canvas/componentpalettemodel.cpp
SCENE=src/canvas/compositionscene.cpp
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
restore() { for f in "$PALETTE" "$SCENE"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'CanvasTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: factory rows regain the drag flag.
cp "$PALETTE" "$PALETTE.bak"
perl -0pi -e 's/    if \(ComponentRegistry::kindOf\(row\.info\) ==\n        ComponentRegistry::ComponentKind::Model\)\n    \{\n      flags \|= Qt::ItemIsDragEnabled;\n    \}/    flags |= Qt::ItemIsDragEnabled;/s' "$PALETTE"
rebuild
gate_ok 'CanvasTest.AdapterFactoryRowsAreNotDraggable'
check "M1 factory rows draggable again" red $?
restore

# M2: the drop-side kind check is skipped.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/    if \(m_instances && m_instances->registry\(\)\)/    if (false)/s' "$SCENE"
rebuild
gate_ok 'CanvasTest.DroppingAFactoryOnTheCanvasLeavesTheDocumentUntouched'
check "M2 drop-side check skipped" red $?
restore

# M3: the sections collapse (factories listed as plain rows, no headings).
cp "$PALETTE" "$PALETTE.bak"
perl -0pi -e 's/      const bool sectioned = !models\.empty\(\) && !factories\.empty\(\);/      const bool sectioned = false;/s' "$PALETTE"
rebuild
gate_ok 'CanvasTest.PaletteListsAdapterFactoriesUnderTheirOwnHeading'
check "M3 headings collapsed" red $?
restore

trap - EXIT
rebuild; gate_ok 'CanvasTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
