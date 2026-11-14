#!/usr/bin/env bash
# U1 falsifiers: the start page closes, comes back, and its list can be
# managed. Every mutation must bite. A mutation that fails to apply is
# reported as NOT APPLIED rather than read as a survivor, and a gate that
# crashes rather than fails is judged by exit status.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}

PAGE=src/ui/welcomepage.cpp
WINDOW=src/ui/composermainwindow.cpp

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target test_welcome >/dev/null 2>&1 || note "REBUILD FAILED"; }

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
    matched "$BUILD/tests/gui/test_welcome" "$1" || FILTER_EMPTY=1
    timeout 120 "$BUILD/tests/gui/test_welcome" --gtest_filter="$1" >/dev/null 2>&1; }
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
    cp "$1" "$1.bak"
    perl -0pi -e "$2" "$1"
    if cmp -s "$1" "$1.bak"; then note "NOT APPLIED in $1"; fail=$((fail+1)); return 1; fi
    return 0
}
restore() { for f in "$PAGE" "$WINDOW"; do [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }
trap 'restore; rebuild' EXIT

run() { # label, file, perl, gtest filter
    if mutate "$2" "$3"; then rebuild; gate_ok "$4"; check "$1" red $?; fi
    restore
}

rebuild; gate_ok 'WelcomeTest.*'; check "baseline" green $?

# ── the page's recent list ───────────────────────────────────────────────
run "W1 the clear button is always alive" "$PAGE" \
  's/m_clearRecent->setEnabled\(!paths\.isEmpty\(\)\);/m_clearRecent->setEnabled(true);/s' \
  'WelcomeTest.TheRecentListCanBeEmptiedAndEntriesDropped'

run "W2 clearing the list does nothing" "$PAGE" \
  's/                m_recent->clear\(\);//s' \
  'WelcomeTest.TheRecentListCanBeEmptiedAndEntriesDropped'

run "W3 a page with no store still offers to clear one" "$PAGE" \
  's/      m_clearRecent->setEnabled\(false\);\n\n      return;/      return;/s' \
  'WelcomeTest.*'

run "W4 the placeholder is treated as a remembered document" "$PAGE" \
  's/    if \(path\.isEmpty\(\)\)\n    \{\n      \/\/ The "nothing opened yet" placeholder is a message, not an entry\.\n      return nullptr;\n    \}//s' \
  'WelcomeTest.TheRecentMenuOpensAndForgetsButOffersNothingOnThePlaceholder'

run "W5 Remove from List opens instead of forgetting" "$PAGE" \
  's/\[this, path\] \{ m_recent->forget\(path\); \}/[this, path] { Q_EMIT openRecentRequested(path); }/s' \
  'WelcomeTest.TheRecentMenuOpensAndForgetsButOffersNothingOnThePlaceholder'

run "W6 Open forgets the entry as well" "$PAGE" \
  's/\[this, path\] \{ Q_EMIT openRecentRequested\(path\); \}\);\n    connect\(menu->addAction\(tr\("Remove from List"\)\)/[this, path] { Q_EMIT openRecentRequested(path); m_recent->forget(path); });\n    connect(menu->addAction(tr("Remove from List"))/s' \
  'WelcomeTest.TheRecentMenuOpensAndForgetsButOffersNothingOnThePlaceholder'

# ── the tab ──────────────────────────────────────────────────────────────
run "T1 no tab offers to close" "$WINDOW" \
  's/    if \(index < 0\)\n    \{\n      return;\n    \}\n\n    QTabBar \*bar = m_workspace->tabBar\(\);/    return;\n\n    QTabBar *bar = m_workspace->tabBar();/s' \
  'WelcomeTest.OnlyTheWelcomeTabOffersToClose'

run "T2 closing switches away instead of closing" "$WINDOW" \
  's/    m_workspace->removeTab\(index\);\n\n    \/\/ Closing the start page is a thing done to this session/    m_workspace->setCurrentWidget(m_canvas);\n\n    \/\/ Closing the start page is a thing done to this session/s' \
  'WelcomeTest.ClosingTakesTheTabAndKeepsThePage'

run "T3 closing sends the user to the composition wherever they were" "$WINDOW" \
  's/    m_workspace->removeTab\(index\);\n\n    \/\/ Closing the start page is a thing done to this session/    m_workspace->removeTab(index);\n    m_workspace->setCurrentWidget(m_canvas);\n\n    \/\/ Closing the start page is a thing done to this session/s' \
  'WelcomeTest.ClosingTheStartPageFromAnotherTabLeavesYouOnIt'

run "T4 the restored tab loses its close button" "$WINDOW" \
  's/      installWelcomeCloseButton\(index\);//s' \
  'WelcomeTest.HelpWelcomeBringsItBackAsTheSamePage'

run "T5 it comes back at the end rather than where it was" "$WINDOW" \
  's/      index = m_workspace->insertTab\(0, m_welcome,/      index = m_workspace->addTab(m_welcome,/s' \
  'WelcomeTest.HelpWelcomeBringsItBackAsTheSamePage'

run "T6 a fresh close button on every restore (one orphan per cycle)" "$WINDOW" \
  's/    if \(!m_welcomeCloseButton\)\n    \{/    if (true)\n    {/s' \
  'WelcomeTest.HelpWelcomeBringsItBackAsTheSamePage'

# T7 -- "asking for it again adds a second copy" -- was here and is gone.
# It survived, and the reason is that it cannot do what it claims:
# QTabWidget::insertTab() with a widget the tab widget already holds moves
# it rather than duplicating it, so with the guard removed the count is
# unchanged, the close button is re-set to the same button, and the
# current widget is the same one. The gate asserting the count in
# HelpWelcomeBringsItBackAsTheSamePage is correct and stays; there is no
# outcome left for a mutation to change.
#
# The guard itself stays too. It is not dead code -- it says what the
# function means, and it keeps the tab where it is rather than moving it
# to index 0 -- but with the workspace's fixed tab order those two are
# the same place, so nothing observable distinguishes them. Writing an
# assertion for a difference nobody can see would be worse than removing
# a mutation that cannot bite.

trap - EXIT
restore; rebuild; gate_ok 'WelcomeTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
