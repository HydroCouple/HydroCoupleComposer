#!/usr/bin/env bash
# U6 falsifiers: one selection across the canvas and the layer views.
# Every mutation must bite. A pattern that no longer matches is reported as
# NOT APPLIED rather than read as a survivor, and exit status is judged as
# well as output — S5 kills the process rather than failing a test.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}

STACK=src/map/layerstackmodel.cpp
HUB=src/ui/selectionhub.cpp
HUBH=include/ui/selectionhub.h
SCENE=src/canvas/compositionscene.cpp
CANVAS=src/map/mapcanvas.cpp
TOOL=src/map/maptool.cpp
WINDOW=src/ui/composermainwindow.cpp

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    local target=${1:-test_selectionhub}
    cmake --build "$BUILD" --target "$target" >/dev/null 2>&1 \
      || note "REBUILD FAILED ($target)"
}

# A gtest filter that matches no test exits zero. Read as a gate result
# that is exactly backwards: "the gate passed", and so "the mutation
# survived". A misaimed filter therefore reports a missing gate as a
# present one, which is the one failure mode a falsifier must not have —
# it is silent, and it makes every line around it worth less. U6's V1
# spent a round in that state, naming MapToolsTest against a script that
# only ever ran the selection binary.
#
# So every filter is asked whether it names anything before it is trusted.
FILTER_EMPTY=0
matched() {
    timeout 60 "$1" --gtest_list_tests --gtest_filter="$2" 2>/dev/null \
      | grep -qE '^  [A-Za-z]'
}

gate_ok() {
    FILTER_EMPTY=0
    # $1 filter, $2 target. A mutation in the map's tools is answered by
    # the map's suite, not by this one: V1 named MapToolsTest while this
    # script only ever built and ran the selection binary, so its filter
    # matched nothing, the run exited zero, and that read as a survivor.
    local target=${2:-test_selectionhub}
    matched "$BUILD/tests/gui/$target" "$1" || FILTER_EMPTY=1
    timeout 120 "$BUILD/tests/gui/$target" --gtest_filter="$1" >/dev/null 2>&1
}
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$FILTER_EMPTY" = 1 ]; then
        note "NO SUCH GATE $label"; fail=$((fail+1)); return
    fi
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
mutate() {
    cp "$1" "$1.bak"; perl -0pi -e "$2" "$1"
    if cmp -s "$1" "$1.bak"; then note "NOT APPLIED in $1"; fail=$((fail+1)); return 1; fi
    return 0
}
restore() { for f in "$STACK" "$HUB" "$HUBH" "$SCENE" "$CANVAS" "$TOOL" "$WINDOW"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }
trap 'restore; rebuild' EXIT
run() {
    # $1 label, $2 file, $3 perl, $4 filter, $5 target (optional)
    if mutate "$2" "$3"; then
        rebuild "${5:-test_selectionhub}"
        gate_ok "$4" "${5:-test_selectionhub}"
        check "$1" red $?
    fi
    restore
}

rebuild; gate_ok 'SelectionHubTest.*'; check "baseline" green $?

# ── provenance and lookup ────────────────────────────────────────────────
run "P1 provenance is read off a layer type, not the interface" "$HUB" \
  's/    const auto \*provenance = dynamic_cast<const IComponentLayer \*>\(layer\);\n\n    return provenance \? provenance->componentId\(\) : QString\(\);/    Q_UNUSED(layer);\n\n    return QString();/s' \
  'SelectionHubTest.AComponentIsAskedOfTheInterfaceNotOfALayerType'

run "P2 an empty id matches every layer that recorded none" "$HUB" \
  's/    if \(!m_stack \|\| componentId\.isEmpty\(\)\)/    if (!m_stack)/s' \
  'SelectionHubTest.LayersOfAComponentAreFoundTopFirstAndOnlyByAnId'

# ── canvas → layers ──────────────────────────────────────────────────────
run "H1 a component with no layers clears the tree's current row" "$HUB" \
  's/        if \(!layers\.isEmpty\(\)\)\n        \{\n          Q_EMIT currentLayerRequested\(layers\.first\(\)\);\n        \}/        Q_EMIT currentLayerRequested(layers.isEmpty() ? nullptr : layers.first());/s' \
  'SelectionHubTest.AComponentWithNoLayersAsksForNothing'

# ── layers → canvas ──────────────────────────────────────────────────────
run "H2 a pick with no component behind it stays quiet" "$HUB" \
  's/        Q_EMIT componentSelectionRequested\(componentOf\(layer\)\);\n      \}\);\n  \}\n\n  void SelectionHub::runItemShown/        if (!componentOf(layer).isEmpty())\n        {\n          Q_EMIT componentSelectionRequested(componentOf(layer));\n        }\n      });\n  }\n\n  void SelectionHub::runItemShown/s' \
  'SelectionHubTest.PickingAFeatureNamesTheComponentThatProducedIt'

run "H3 the re-entrancy guard is gone (recurses; caught by exit status)" "$HUBH" \
  's/        if \(m_forwarding\)\n        \{\n          return;\n        \}\n\n        m_forwarding = true;\n        work\(\);\n        m_forwarding = false;/        work();/s' \
  'SelectionHubTest.TheTwoDirectionsDoNotChaseEachOther:SelectionHubTest.AHostileWiringStillTerminates'

# ── the scene's half ─────────────────────────────────────────────────────
run "C1 selecting a component accumulates instead of replacing" "$SCENE" \
  's/    clearSelection\(\);\n\n    if \(!wanted\)/    if (!wanted)/s' \
  'SelectionHubTest.SelectingAComponentSelectsThatOneAndClearsTheRest'

# ── additive selection ───────────────────────────────────────────────────
run "S1 Shift is ignored, so everything replaces" "$STACK" \
  's/    if \(modifiers\.testFlag\(Qt::ShiftModifier\)\)\n    \{\n      return SelectionMode::Add;\n    \}//s' \
  'SelectionHubTest.ModifiersChooseReplaceAddAndToggle'

run "S2 Ctrl toggles nothing" "$STACK" \
  's/      return SelectionMode::Toggle;/      return SelectionMode::Replace;/s' \
  'SelectionHubTest.ModifiersChooseReplaceAddAndToggle'

run "S3 Toggle adds instead of taking back out" "$STACK" \
  's/      if \(mode == SelectionMode::Toggle && combined\.contains\(feature\)\)\n      \{\n        combined\.remove\(feature\);\n      \}\n      else\n      \{\n        combined\.insert\(feature\);\n      \}/      combined.insert(feature);/s' \
  'SelectionHubTest.AddGrowsTheSelectionAndToggleTakesBackOut'

run "S4 Add starts from empty rather than from what is selected" "$STACK" \
  's/    QSet<int> combined = target->selection\(\);/    QSet<int> combined;/s' \
  'SelectionHubTest.AddGrowsTheSelectionAndToggleTakesBackOut'

run "S5 a layer with no features is selected into (null deref — segfaults)" "$STACK" \
  's/    if \(mode == SelectionMode::Replace \|\| !target\)/    if (mode == SelectionMode::Replace)/s' \
  'SelectionHubTest.ALayerWithNoFeaturesIsNotSelectedInto'

# ── the views pass the modifiers on ──────────────────────────────────────
run "V1 the map drops the modifiers, so a click always replaces" "$TOOL" \
  's/      m_canvas->pickAndSelectAt\(event->pos\(\),\n                                selectionModeFor\(event->modifiers\(\)\)\);/      m_canvas->pickAndSelectAt(event->pos());/s' \
  'MapToolsTest.TheModifiersReachTheCanvasFromAClick' test_maptools

trap - EXIT
restore; rebuild; gate_ok 'SelectionHubTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
