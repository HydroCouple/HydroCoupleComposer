#!/usr/bin/env bash
# C3 falsifiers: spliced adapter nodes + routed edge + selection survival.
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

# M1: the edge ignores its adapters and routes port to port.
cp "$ITEMS" "$ITEMS.bak"
perl -0pi -e 's/    for \(AdapterNodeItem \*adapter : m_adapters\)\n    \{\n      if \(!adapter\)\n      \{\n        continue;\n      \}\n\n      points\.append\(adapter->anchorIn\(\)\);\n      points\.append\(adapter->anchorOut\(\)\);\n    \}\n//s' "$ITEMS"
rebuild
gate_ok 'CanvasTest.TheEdgeRoutesThroughItsAdapterNodesInChainOrder'
check "M1 edge ignores adapters" red $?
restore

# M2: the drag commits to the item only, never the document.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/                  \/\/ Through the document, so the move joins undo history\.\n                  m_document->moveConnectionAdapter\(identity, stepIndex,\n                                                    position\);/                  (void)identity; (void)stepIndex; (void)position;/s' "$SCENE"
rebuild
gate_ok 'CanvasTest.DraggingAnAdapterNodePersistsItsPositionInTheSidecar'
check "M2 drag never reaches the document" red $?
restore

# M3: selection no longer survives a rebuild.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/    for \(const QPair<ConnectionSpec, int> &address : selectedAdapters\)/    selectedAdapters.clear();\n    for (const QPair<ConnectionSpec, int> \&address : selectedAdapters)/s' "$SCENE"
rebuild
gate_ok 'CanvasTest.DraggingAnAdapterNodePersistsItsPositionInTheSidecar'
check "M3 selection survival dropped" red $?
restore

# M4: default placement stacks every step on the midpoint.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/          const qreal fraction =\n            static_cast<qreal>\(index \+ 1\) \/ static_cast<qreal>\(stepCount \+ 1\);/          const qreal fraction = 0.5;/s' "$SCENE"
rebuild
gate_ok 'CanvasTest.AnAdaptedConnectionGrowsASplicedAdapterNodePerChainStep'
check "M4 default placement stacks" red $?
restore

trap - EXIT
rebuild; gate_ok 'CanvasTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
