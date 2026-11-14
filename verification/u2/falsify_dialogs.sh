#!/usr/bin/env bash
# U2b falsifiers: the window that edits one argument.
#
# The mutations that matter are the ones that break the contract rather
# than the arithmetic: committing on a keystroke, closing on a refusal,
# passing a parse error on to the component, or reporting success with
# nothing listening. Every mutation must bite; a pattern that no longer
# matches is reported as NOT APPLIED rather than read as a survivor, and
# exit status is judged as well as output.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}
TARGET=${TARGET:-test_argumentdialogs}
BIN=${BIN:-$BUILD/tests/gui/test_argumentdialogs}

BASE=src/ui/dialogs/argumenteditordialog.cpp
RAW=src/ui/dialogs/rawargumentdialog.cpp
FACTORY=src/ui/dialogs/argumenteditorfactory.cpp

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target "$TARGET" >/dev/null 2>&1 || note "REBUILD FAILED"; }

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
    matched "$BIN" "$1" || FILTER_EMPTY=1
    timeout 120 "$BIN" --gtest_filter="$1" >/dev/null 2>&1; }
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
restore() { for f in "$BASE" "$RAW" "$FACTORY"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; return 0; }
trap 'restore; rebuild' EXIT
run() { if mutate "$2" "$3"; then rebuild; gate_ok "$4"; check "$1" red $?; fi; restore; }

rebuild; gate_ok 'ArgumentDialogTest.*'; check "baseline" green $?

# ── the contract ─────────────────────────────────────────────────────────
run "B1 typing commits, so Cancel cannot discard anything" "$RAW" \
  's/    setEditor\(m_editor\);/    connect(m_editor, \&QPlainTextEdit::textChanged, this,\n            [this] { apply(); });\n\n    setEditor(m_editor);/s' \
  'ArgumentDialogTest.NothingReachesTheComponentUntilTheUserAsks:ArgumentDialogTest.CancellingAfterEditsLeavesTheComponentAlone'

run "B2 OK closes whether the component took it or not" "$BASE" \
  's/              if \(apply\(\)\)\n              \{\n                accept\(\);\n              \}/              apply();\n              accept();/s' \
  'ArgumentDialogTest.OkClosesOnlyWhenTheComponentTookIt'

run "B3 a refusal is recorded as a commit anyway" "$BASE" \
  's/    if \(!m_committer\(m_descriptor\.id, offered, message\)\)\n    \{/    m_committer(m_descriptor.id, offered, message);\n\n    if (false)\n    {/s' \
  'ArgumentDialogTest.ARefusalIsShownAndNothingIsRecorded'

run "B4 a dialog with nothing listening reports success" "$BASE" \
  's/      showRefusal\(tr\("This editor is not connected to its component\."\)\);\n\n      return false;/      return true;/s' \
  'ArgumentDialogTest.ADialogWithNothingListeningSaysSo'

run "B5 the component's own message is replaced with a generic one" "$BASE" \
  's/      showRefusal\(message\.isEmpty\(\)\n                    \? tr\("The component refused this value\."\)\n                    : message\);/      showRefusal(tr("The component refused this value."));/s' \
  'ArgumentDialogTest.ARefusalIsShownAndNothingIsRecorded'

run "B6 an old refusal is left on the strip after an acceptance" "$BASE" \
  's/    clearRefusal\(\);\n\n    \/\/ The accepted payload becomes the baseline/    \/\/ The accepted payload becomes the baseline/s' \
  'ArgumentDialogTest.ARefusalIsClearedOnceSomethingIsAccepted'

# ── the dialog's own validation ──────────────────────────────────────────
run "B7 unparseable text is sent to the component to refuse" "$BASE" \
  's/    if \(!validate\(message\)\)\n    \{[\s\S]*?      return false;\n    \}\n\n//s' \
  'ArgumentDialogTest.TextThatIsNotJsonIsTheDialogsRefusalNotTheComponents'

run "B8 the raw editor accepts anything as JSON" "$RAW" \
  's/    if \(parsed\.is_discarded\(\)\)\n    \{\n      message = tr\("This is not valid JSON\."\);\n\n      return false;\n    \}//s' \
  'ArgumentDialogTest.TextThatIsNotJsonIsTheDialogsRefusalNotTheComponents'

# Anchored on the comment inside payload()'s branch, not on
# "is_discarded" — validate() tests the same thing four lines earlier, and
# a non-greedy match from there swallowed both functions and simply failed
# to build. A mutation that does not compile is not a falsified gate.
run "B9 unparseable text yields a null payload rather than what was opened" "$RAW" \
  's/      \/\/ What the window opened with[\s\S]*?      return m_opened;/      return nlohmann::json();/s' \
  'ArgumentDialogTest.TextThatIsNotJsonIsTheDialogsRefusalNotTheComponents'

# ── hydration ────────────────────────────────────────────────────────────
run "B10 the window opens empty rather than on its payload" "$RAW" \
  's/    m_editor->setPlainText\(\n      QString::fromStdString\(payload\.dump\(2\)\)\);/    m_editor->clear();/s' \
  'ArgumentDialogTest.ItHydratesFromItsPayloadAndGivesTheSameOneBack'

run "B11 the baseline is not recorded, so a bad parse loses the payload" "$RAW" \
  's/    m_opened = payload;\n//s' \
  'ArgumentDialogTest.TextThatIsNotJsonIsTheDialogsRefusalNotTheComponents'

# ── the factory and the frame ────────────────────────────────────────────
run "B12 the factory returns nothing for a kind it cannot type" "$FACTORY" \
  's/    return new RawArgumentDialog\(descriptor, parent\);/    return hasTypedEditor(descriptor.kind)\n             ? new RawArgumentDialog(descriptor, parent)\n             : nullptr;/s' \
  'ArgumentDialogTest.EveryKindGetsAWindowAndTheFactorySaysWhichAreTyped'

run "B13 every kind claims a typed editor it does not have" "$FACTORY" \
  's/    \/\/ Nothing yet\. Written as a switch[\s\S]*?    return false;\n  \}/    return true;\n  }/s' \
  'ArgumentDialogTest.EveryKindGetsAWindowAndTheFactorySaysWhichAreTyped'

run "B14 every window is titled the same, so a row of them is unusable" "$BASE" \
  's/    setWindowTitle\(m_descriptor\.caption\);/    setWindowTitle(tr("Edit argument"));/s' \
  'ArgumentDialogTest.TheWindowNamesTheArgumentItEdits'

run "B15 the window is not named after its argument, so two cannot be told apart" "$BASE" \
  's/    setObjectName\(QStringLiteral\("argumentDialog_"\) \+ m_descriptor\.id\);/    setObjectName(QStringLiteral("argumentDialog"));/s' \
  'ArgumentDialogTest.TheWindowNamesTheArgumentItEdits'

note ""
note "u2b: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
