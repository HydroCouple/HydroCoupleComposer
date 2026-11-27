#!/usr/bin/env bash
# S4.3 falsifiers: the workflow/execution panel.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
PANEL=src/simulation/executionpanel.cpp
BUILD=build/darwin
OBJ="$BUILD/CMakeFiles/composer_core.dir/src/simulation/executionpanel.cpp.o"

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    # Stale-relink hardening: force the mutated TU to recompile.
    rm -f "$OBJ"
    cmake --build "$BUILD" --target test_executionpanel >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_executionpanel" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { [ -f "$PANEL.bak" ] && mv "$PANEL.bak" "$PANEL" && touch "$PANEL"; }

rebuild; gate_ok 'ExecutionPanelTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the panel stops observing the document — external edits never reach
# the widgets.
cp "$PANEL" "$PANEL.bak"
perl -0pi -e 's/      connect\(m_document, &CompositionDocument::compositionChanged, this,\n              &ExecutionPanel::refresh\);\n//s' "$PANEL"
rebuild
gate_ok 'ExecutionPanelTest.ThePanelShowsTheDocumentsWorkflowAndFollowsExternalEdits'
check "M1 document observation dropped" red $?
restore

# M2: workflow edits never reach the document.
cp "$PANEL" "$PANEL.bak"
perl -0pi -e 's/    m_document->setWorkflow\(workflow\);/    ;/s' "$PANEL"
rebuild
gate_ok 'ExecutionPanelTest.EditingTheWorkflowThroughThePanelIsUndoable'
check "M2 workflow commit dropped" red $?
restore

# M3: the open-without-manifest refusal is invisible.
cp "$PANEL" "$PANEL.bak"
perl -0pi -e 's/      manifestEdit->setStyleSheet\(\n        QStringLiteral\("border: 1px solid #c04040;"\)\);/      ;/s' "$PANEL"
rebuild
gate_ok 'ExecutionPanelTest.OpenModeRequiresAManifestAndTheRefusalIsVisible'
check "M3 refusal invisible" red $?
restore

# M4: the stage preview abandons bindingStages() for flat document order.
cp "$PANEL" "$PANEL.bak"
perl -0pi -e 's/    QStringList lines;\n    int position = 1;/    QStringList all;\n    for (const auto \&component : spec.components)\n      all.append(QString::fromStdString(component.id));\n    return QStringLiteral("1. %1").arg(all.join(QStringLiteral(", ")));\n    QStringList lines;\n    int position = 1;/s' "$PANEL"
rebuild
gate_ok 'ExecutionPanelTest.TheStagePreviewOrdersProvidersBeforeConsumers'
check "M4 staged order abandoned" red $?
restore

trap - EXIT
rebuild; gate_ok 'ExecutionPanelTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
