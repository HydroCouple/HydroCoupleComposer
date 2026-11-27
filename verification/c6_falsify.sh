#!/usr/bin/env bash
# C6 falsifiers: binding-edge deletion, snap radius, mixed selections.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
ITEMS=src/canvas/canvasitems.cpp
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
restore() { for f in "$ITEMS" "$SCENE"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'CanvasTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: binding edges lose their selectable flag again.
cp "$ITEMS" "$ITEMS.bak"
perl -0pi -e 's/    setFlag\(ItemIsSelectable, true\); \/\/ Selectable, so Delete can remove it\.\n//s' "$ITEMS"
rebuild
gate_ok 'CanvasTest.ABindingEdgeCanBeSelectedAndDeletedRemovingTheBinding'
check "M1 binding edge unselectable" red $?
restore

# M2: the release reverts to exact hits.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/      PortItem \*target = portNear\(event->scenePos\(\), 12\.0\);/      PortItem *target = portAt(event->scenePos());/s' "$SCENE"
rebuild
gate_ok 'CanvasTest.AReleaseNearAPortStillCompletesTheConnection'
check "M2 release exact-hit only" red $?
restore

# M3: adapter removals run lowest-index-first, renumbering the rest away.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/              \{ return lhs\.second > rhs\.second; \}\);/              { return lhs.second < rhs.second; });/s' "$SCENE"
rebuild
gate_ok 'CanvasTest.DeletingASelectionCountsAdaptersConnectionsAndChainsOnce'
check "M3 removal order ascending" red $?
restore

# M4: steps whose connection is also selected are counted twice.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/      if \(!parentSelected &&\n          m_document->removeConnectionAdapter\(address\.first, address\.second\)\)/      if (m_document->removeConnectionAdapter(address.first, address.second))/s' "$SCENE"
rebuild
gate_ok 'CanvasTest.DeletingASelectionCountsAdaptersConnectionsAndChainsOnce'
check "M4 double-count with parent" red $?
restore

trap - EXIT
rebuild; gate_ok 'CanvasTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
