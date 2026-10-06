#!/usr/bin/env bash
# Auto-layout falsifiers: layered placement, left to right by dependency.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
LAYOUT=src/canvas/graphlayout.cpp
SCENE=src/canvas/compositionscene.cpp
BUILD=build/darwin
OBJDIR="$BUILD/CMakeFiles/composer_core.dir/src/canvas"

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    rm -f "$OBJDIR/graphlayout.cpp.o" "$OBJDIR/compositionscene.cpp.o"
    cmake --build "$BUILD" --target test_graphlayout test_compositioncanvas >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/$1" --gtest_filter="$2" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { for f in "$LAYOUT" "$SCENE"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild
gate_ok test_graphlayout 'GraphLayoutTest.*'; check "baseline (layout)" green $?
trap 'restore; rebuild' EXIT

# M1: rank ignores the graph — everything lands in one column, so a
# provider no longer stands left of what it feeds.
cp "$LAYOUT" "$LAYOUT.bak"
perl -0pi -e 's/        if \(!waiting\)\n        \{\n          layer\.append\(id\);\n        \}/        layer.append(id);/s' "$LAYOUT"
rebuild
gate_ok test_graphlayout 'GraphLayoutTest.AProviderStandsLeftOfWhatItFeeds'
check "M1 ranking ignores providers" red $?
restore

# M2: rows do not advance, so a column stacks every component at one point.
cp "$LAYOUT" "$LAYOUT.bak"
perl -0pi -e 's/        positions\.insert\(id, QPointF\(x, y\)\);\n        y \+= size\.height\(\) \+ options\.rowGap;/        positions.insert(id, QPointF(x, y));/s' "$LAYOUT"
rebuild
gate_ok test_graphlayout 'GraphLayoutTest.ComponentsInOneColumnDoNotOverlap'
check "M2 components stacked on one another" red $?
restore

# M3: a cycle is dropped instead of arranged — the layout returns without
# placing the components it could not rank.
cp "$LAYOUT" "$LAYOUT.bak"
perl -0pi -e 's/      if \(layer\.isEmpty\(\)\)\n      \{\n        layer = remaining;\n      \}/      if (layer.isEmpty())\n      {\n        break;\n      }/s' "$LAYOUT"
rebuild
gate_ok test_graphlayout 'GraphLayoutTest.AFeedbackLoopIsArrangedRatherThanRefused'
check "M3 a feedback loop is dropped" red $?
restore

# M4: the column advances by a constant, so a wide component overlaps the
# column beside it.
cp "$LAYOUT" "$LAYOUT.bak"
perl -0pi -e 's/      x \+= columnWidth\[column\] \+ options\.columnGap;/      x += options.columnGap;/s' "$LAYOUT"
rebuild
gate_ok test_graphlayout 'GraphLayoutTest.WiderComponentsGetAWiderColumn'
check "M4 column width ignores its members" red $?
restore

# M5: the layout is not one undo step, so undoing it takes one Ctrl+Z per
# component.
cp "$SCENE" "$SCENE.bak"
perl -0pi -e 's/    m_document->undoStack\(\)->beginMacro\(tr\("Lay out components"\)\);//s; s/    m_document->undoStack\(\)->endMacro\(\);//s' "$SCENE"
rebuild
gate_ok test_compositioncanvas 'CanvasTest.AutoLayoutIsOneUndoStepAndOrdersProvidersFirst'
check "M5 layout is not one undo step" red $?
restore

trap - EXIT
rebuild
gate_ok test_graphlayout 'GraphLayoutTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
