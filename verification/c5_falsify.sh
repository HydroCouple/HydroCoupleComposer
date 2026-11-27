#!/usr/bin/env bash
# C5 falsifiers: adapter inspector + selection routing.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
WINDOW=src/ui/composermainwindow.cpp
INSPECTOR=src/configurator/adapterinspector.cpp
BUILD=build/darwin

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target test_adapterinspector >/dev/null 2>&1; }
gate_ok() { "$BUILD/tests/gui/test_adapterinspector" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { for f in "$WINDOW" "$INSPECTOR"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'InspectorTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: selection routing loses the adapter branch.
cp "$WINDOW" "$WINDOW.bak"
perl -0pi -e 's/      if \(auto \*adapter = qgraphicsitem_cast<AdapterNodeItem \*>\(item\)\)\n      \{\n        m_adapterInspector->setChainStep\(adapter->connection\(\),\n                                         adapter->stepIndex\(\)\);\n        m_configurator->setComponent\(QString\(\)\);\n        return;\n      \}\n//s' "$WINDOW"
rebuild
gate_ok 'InspectorTest.SelectingAnAdapterNodeShowsItsChainStepInTheInspector'
check "M1 adapter branch dropped" red $?
restore

# M2: edits land on the wrong chain step.
cp "$INSPECTOR" "$INSPECTOR.bak"
perl -0pi -e 's/                    m_document->setConnectionAdapterArgument\(\n                      identity, stepIndex, argumentId, payload\);/                    m_document->setConnectionAdapterArgument(\n                      identity, stepIndex + 1, argumentId, payload);/s' "$INSPECTOR"
rebuild
gate_ok 'InspectorTest.EditingAChainStepArgumentIsRecordedInTheDocument'
check "M2 edit lands on the wrong step" red $?
restore

# M3: a dangling address leaves the panel stale. (Pattern anchors on the
# code lines only: the neighboring comment carries an em-dash, which
# byte-wise patterns miss.)
cp "$INSPECTOR" "$INSPECTOR.bak"
perl -0pi -e 's/      m_hasAddress = false;\n      m_title->setText\(tr\("Nothing selected\."\)\);\n      return;/      return;/s' "$INSPECTOR"
rebuild
gate_ok 'InspectorTest.TheInspectorEmptiesWhenItsAddressStopsResolving'
check "M3 dangling address left stale" red $?
restore

# M4: a JSON parse failure is refused invisibly.
cp "$INSPECTOR" "$INSPECTOR.bak"
perl -0pi -e 's/                    editor->setStyleSheet\(\n                      QStringLiteral\("border: 1px solid #c04040;"\)\);/                    ;/s' "$INSPECTOR"
rebuild
gate_ok 'InspectorTest.EditingAChainStepArgumentIsRecordedInTheDocument'
check "M4 refusal invisible" red $?
restore

trap - EXIT
rebuild; gate_ok 'InspectorTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
