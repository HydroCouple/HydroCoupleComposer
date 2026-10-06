#!/usr/bin/env bash
# Welcome-page falsifiers: somewhere to start, and what was open last.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
RECENT=src/ui/recentcompositions.cpp
PAGE=src/ui/welcomepage.cpp
WINDOW=src/ui/composermainwindow.cpp
BUILD=build/darwin
OBJDIR="$BUILD/CMakeFiles/composer_core.dir/src/ui"

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    rm -f "$OBJDIR/recentcompositions.cpp.o" "$OBJDIR/welcomepage.cpp.o" \
          "$OBJDIR/composermainwindow.cpp.o"
    cmake --build "$BUILD" --target test_welcome >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_welcome" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { for f in "$RECENT" "$PAGE" "$WINDOW"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'WelcomeTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the list is append-only, so reopening a document lists it twice and
# pushes everything else out.
cp "$RECENT" "$RECENT.bak"
perl -0pi -e 's/    current\.removeAll\(absolute\);\n    current\.prepend\(absolute\);/    current.append(absolute);/s' "$RECENT"
rebuild
gate_ok 'WelcomeTest.TheMostRecentComesFirstAndIsNotListedTwice'
check "M1 duplicates and wrong order" red $?
restore

# M2: nothing is ever forgotten, so the list grows without bound.
cp "$RECENT" "$RECENT.bak"
perl -0pi -e 's/    while \(current\.size\(\) > limit\)\n    \{\n      current\.removeLast\(\);\n    \}//s' "$RECENT"
rebuild
gate_ok 'WelcomeTest.OnlyTheLastTenAreKept'
check "M2 the list never forgets" red $?
restore

# M2b: the limit is a constant again, so the preference is decoration.
cp "$RECENT" "$RECENT.bak"
perl -0pi -e 's/std::max\(1, PreferencesManager::instance\(\)->recentLimit\(\)\)/10/s' "$RECENT"
rebuild
gate_ok 'WelcomeTest.OnlyTheLastTenAreKept'
check "M2b the recent limit is not read from the preference" red $?
restore

# M3: the page opens documents itself instead of asking, so opening from
# the welcome list would bypass the path that records it as recent.
cp "$PAGE" "$PAGE.bak"
perl -0pi -e 's/                Q_EMIT openRecentRequested\(\n                  item->data\(Qt::UserRole\)\.toString\(\)\);//s' "$PAGE"
rebuild
gate_ok 'WelcomeTest.ThePageListsWhatWasOpenedAndAsksToOpenIt'
check "M3 the page never asks to open" red $?
restore

# M4: the box writes the preference but does not follow it, so a change
# made in the preferences dialog leaves the page showing the old answer.
cp "$PAGE" "$PAGE.bak"
perl -0pi -e 's/                m_showOnStartUp->setChecked\(prefs->showWelcomeOnStartUp\(\)\);//s' "$PAGE"
rebuild
gate_ok 'WelcomeTest.TheStartUpPreferenceIsRememberedBothWays'
check "M4 the page does not follow the preference" red $?
restore

# M4b: the box follows the preference but never writes it.
cp "$PAGE" "$PAGE.bak"
perl -0pi -e 's/\[prefs\]\(bool shows\) \{ prefs->setShowWelcomeOnStartUp\(shows\); \}/[](bool) {}/s' "$PAGE"
rebuild
gate_ok 'WelcomeTest.TheStartUpPreferenceIsRememberedBothWays'
check "M4b the page never writes the preference" red $?
restore

# M5: opening a document leaves the user staring at the welcome page.
cp "$WINDOW" "$WINDOW.bak"
perl -0pi -e 's/    if \(m_workspace && m_canvas\)\n    \{\n      m_workspace->setCurrentWidget\(m_canvas\);\n    \}//s' "$WINDOW"
rebuild
gate_ok 'WelcomeTest.TheWindowOpensOnTheWelcomeTabAndLeavesItWhenADocumentOpens'
check "M5 welcome page never gets out of the way" red $?
restore

trap - EXIT
rebuild; gate_ok 'WelcomeTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
