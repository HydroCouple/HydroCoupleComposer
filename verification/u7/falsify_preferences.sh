#!/usr/bin/env bash
# U7 falsifiers: the preferences manager, its dialog, and the consumers that
# read it. Every mutation must bite; a mutation that fails to apply is
# reported as NOT APPLIED rather than read as a survivor, and a gate that
# crashes rather than fails is judged by exit status.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}

MANAGER=src/core/preferencesmanager.cpp
DIALOG=src/ui/dialogs/preferencesdialog.cpp
MAPCANVAS=src/map/mapcanvas.cpp
MAPTOOL=src/map/maptool.cpp
FEATURE=src/layers/featurelayer.cpp
EDITTOOL=src/mesh/domainedittool.cpp
WINDOW=src/ui/composermainwindow.cpp

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    cmake --build "$BUILD" --target "$1" >/dev/null 2>&1 || note "REBUILD FAILED ($1)"
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
    # $1 target, $2 filter — bounded so a hang reports rather than wedges.
    timeout 120 "$BUILD/tests/gui/$1" --gtest_filter="$2" >/dev/null 2>&1
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
    # $1 file, $2 perl expression. Compares against the backup so a pattern
    # that no longer matches is reported instead of silently passing.
    cp "$1" "$1.bak"
    perl -0pi -e "$2" "$1"
    if cmp -s "$1" "$1.bak"; then note "NOT APPLIED in $1"; fail=$((fail+1)); return 1; fi
    return 0
}
restore() { for f in "$MANAGER" "$DIALOG" "$MAPCANVAS" "$MAPTOOL" "$FEATURE" "$EDITTOOL" "$WINDOW"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }
trap 'restore' EXIT

run() {
    # $1 label, $2 file, $3 perl, $4 target, $5 filter
    if mutate "$2" "$3"; then
        rebuild "$4"; gate_ok "$4" "$5"; check "$1" red $?
    fi
    restore
}

for t in test_preferences test_preferencesdialog test_welcome test_maptools test_picking test_domainedit; do rebuild "$t"; done
gate_ok test_preferences 'PreferencesTest.*' && gate_ok test_preferencesdialog 'PreferencesDialogTest.*'; check "baseline" green $?

# ── the manager ──────────────────────────────────────────────────────────
run "P1 an unchanged value is announced" "$MANAGER" \
  's/    if \(this->value\(group, name\) == value\)\n    \{[^}]*return true;\n    \}//s' \
  test_preferences 'PreferencesTest.AnUnchangedValueIsStoredSilently'

run "P2 a stored string is handed on untyped" "$MANAGER" \
  's/    if \(stored\.userType\(\) != key->fallback\.userType\(\)\n        && !stored\.convert\(key->fallback\.metaType\(\)\)\)\n    \{\n      return key->fallback;\n    \}//s' \
  test_preferences 'PreferencesTest.ValuesWrittenAsIniTextComeBackWithTheirTypes:PreferencesTest.AStoredValueOfTheWrongTypeFallsBackToTheDefault'

run "P3 reset announces every key" "$MANAGER" \
  's/      if \(differed\)\n      \{\n        Q_EMIT preferenceChanged\(key\.group, key\.name\);\n      \}/      Q_EMIT preferenceChanged(key.group, key.name);/s' \
  test_preferences 'PreferencesTest.ResetForgetsWhatWasSetAndAnnouncesOnlyThat'

run "P4 keys stored under Qt's reserved [General] section" "$MANAGER" \
  's/return QStringLiteral\("preferences\/"\) \+ folded \+ QLatin1Char\(\x27\/\x27\)\n             \+ name;/return folded + QLatin1Char(\x27\/\x27) + name;/s' \
  test_preferences 'PreferencesTest.ValuesWrittenAsIniTextComeBackWithTheirTypes'

run "P5 a colour is stored as an opaque variant" "$MANAGER" \
  's/key->fallback\.userType\(\) == QMetaType::QColor/false/s' \
  test_preferences 'PreferencesTest.AColourIsStoredAsTextAnyoneCanEdit'

# ── the dialog ───────────────────────────────────────────────────────────
run "D1 apply drops the CRS" "$DIALOG" \
  's/    prefs\.setDefaultMapCrs\(crs\.isEmpty\(\) \? QStringLiteral\("EPSG:3857"\) : crs\);/    Q_UNUSED(crs);/s' \
  test_preferencesdialog 'PreferencesDialogTest.EveryWidgetRoundTripsThroughOkAndAFreshDialog'

run "D2 cancel applies" "$DIALOG" \
  's/connect\(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject\);/connect(buttons, \&QDialogButtonBox::rejected, this, [this]{ apply(); reject(); });/s' \
  test_preferencesdialog 'PreferencesDialogTest.CancelAfterEditingEverythingChangesNothing'

run "D3 openAtCategory opens nothing" "$DIALOG" \
  's/        m_categories->setCurrentRow\(row\);\n\n        return;/        return;/s' \
  test_preferencesdialog 'PreferencesDialogTest.TheCategoriesAreTheManagersGroupsAndOpenByName'

run "D4 the theme radios are cleared rather than set (the bug this slice found)" "$DIALOG" \
  's/    \(theme == QLatin1String\("Light"\)  \? m_themeLight\n     : theme == QLatin1String\("Dark"\) \? m_themeDark\n                                      : m_themeSystem\)\n      ->setChecked\(true\);/    m_themeLight->setChecked(theme == QLatin1String("Light"));\n    m_themeDark->setChecked(theme == QLatin1String("Dark"));\n    m_themeSystem->setChecked(!m_themeLight->isChecked() \&\& !m_themeDark->isChecked());/s' \
  test_preferencesdialog 'PreferencesDialogTest.ResetRestoresTheDefaultsAndTheWidgetsFollow'

# ── the consumers ────────────────────────────────────────────────────────
run "C1 the map's pick tolerance is a constant again" "$MAPCANVAS" \
  's/PreferencesManager::instance\(\)->pickTolerancePixels\(\)\n      \/ m_transform\.scale\(\)/6.0 \/ m_transform.scale()/s' \
  test_picking 'PickTest.ThePickToleranceIsAPreferenceReadOnEveryClick'

run "C2 the tools' drag threshold is a constant again" "$MAPTOOL" \
  's/return PreferencesManager::instance\(\)->dragThresholdPixels\(\);/return 3;/s' \
  test_maptools 'MapToolsTest.TheDragThresholdIsAPreferenceReadOnEveryGesture'

run "C3 the selection colour is a constant again" "$FEATURE" \
  's/return PreferencesManager::instance\(\)->selectionColor\(\);/return QColor(0, 200, 255);/s' \
  test_picking 'PickTest.TheSelectionColourIsAPreferenceReadAsTheLayerPaints'

run "C4 the snap reach is a constant again" "$EDITTOOL" \
  's/      PreferencesManager::instance\(\)->snapTolerancePixels\(\),/      10.0,/s' \
  test_domainedit 'DomainEditTest.TheSnapReachIsAPreferenceReadOnEveryDrag'

run "C5 the window ignores the start-up preference" "$WINDOW" \
  's/    if \(!PreferencesManager::instance\(\)->showWelcomeOnStartUp\(\)\)\n    \{\n      m_workspace->setCurrentWidget\(m_canvas\);\n    \}/    m_workspace->setCurrentWidget(m_canvas);/s' \
  test_welcome 'WelcomeTest.TheWindowOpensOnTheWelcomeTabAndLeavesItWhenADocumentOpens'

run "C6 the appearance action no longer writes the preference" "$WINDOW" \
  's/                PreferencesManager::instance\(\)->setThemeMode\(\n                  ThemeManager::modeToString\(mode\)\);/                Q_UNUSED(mode);/s' \
  test_welcome 'WelcomeTest.TheAppearanceActionsAndThePreferenceAreOneSetting'

trap - EXIT
restore
for t in test_preferences test_preferencesdialog test_welcome test_maptools test_picking test_domainedit; do rebuild "$t"; done
gate_ok test_preferences 'PreferencesTest.*' && gate_ok test_preferencesdialog 'PreferencesDialogTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
