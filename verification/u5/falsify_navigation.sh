#!/usr/bin/env bash
# U5 falsifiers: the 3D view's navigation — which way a drag turns the
# camera, what a wheel notch does and which way round, which press pans,
# and where the named views go. Every mutation must bite; a pattern that
# no longer matches is reported as NOT APPLIED rather than read as a
# survivor, and exit status is judged as well as output.
#
# What is NOT falsified here is the ribbon wiring and the tab-switch
# hand-off: those need a main window, and they are test_viewhandoff's and
# the hand-off's to confirm on a machine with a build.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}
TARGET=${TARGET:-test_navigation}
BIN=${BIN:-$BUILD/tests/gui/test_navigation}

NAV=src/scene/navigation.cpp

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
restore() { [ -f "$NAV.bak" ] && mv "$NAV.bak" "$NAV" && touch "$NAV"; return 0; }
trap 'restore; rebuild' EXIT
run() { if mutate "$NAV" "$2"; then rebuild; gate_ok "$3"; check "$1" red $?; fi; restore; }

rebuild; gate_ok 'NavigationTest.*'; check "baseline" green $?

# ── the stored names ─────────────────────────────────────────────────────
run "N1 a link setting is stored under the other one's name" \
  's/    if \(link == ViewLink::Never\)\n    \{\n      return QStringLiteral\("Never"\);\n    \}/    if (link == ViewLink::Never)\n    {\n      return QStringLiteral("OnTabSwitch");\n    }/s' \
  'NavigationTest.TheStoredNamesSurviveARoundTripAndNonsenseIsTheOldBehaviour'

run "N2 an unreadable link setting silently turns linking off" \
  's/    \/\/ Anything else, including a settings file hand-edited into nonsense,\n    \/\/ is the behaviour the application had before this was a preference\.\n    return ViewLink::OnTabSwitch;/    return ViewLink::Never;/s' \
  'NavigationTest.TheStoredNamesSurviveARoundTripAndNonsenseIsTheOldBehaviour'

run "N3 an unreadable pan setting turns shift-drag on for everyone" \
  's/    return PanModifier::MiddleDrag;\n  \}\n\n  QString panModifierName/    return PanModifier::ShiftDrag;\n  }\n\n  QString panModifierName/s' \
  'NavigationTest.TheStoredNamesSurviveARoundTripAndNonsenseIsTheOldBehaviour'

# ── the orbit ────────────────────────────────────────────────────────────
run "N4 dragging right turns the scene left" \
  's/    step\.azimuth = -double\(travel\.x\(\)\) \* rate;/    step.azimuth = double(travel.x()) * rate;/s' \
  'NavigationTest.DraggingRightTurnsTheSceneRight'

run "N5 dragging down tilts the wrong way" \
  's/    step\.elevation = double\(travel\.y\(\)\) \* rate;/    step.elevation = -double(travel.y()) * rate;/s' \
  'NavigationTest.DraggingRightTurnsTheSceneRight'

run "N6 the sensitivity preference is trusted, so zero freezes the camera" \
  's/    const double rate =\n      std::clamp\(degreesPerPixel, kMinDegreesPerPixel, kMaxDegreesPerPixel\);/    const double rate = degreesPerPixel;/s' \
  'NavigationTest.ASensitivityOfZeroIsNotAViewThatCannotTurn'

run "N7 only the upper bound is enforced, so zero still freezes it" \
  's/      std::clamp\(degreesPerPixel, kMinDegreesPerPixel, kMaxDegreesPerPixel\);/      std::min(degreesPerPixel, kMaxDegreesPerPixel);/s' \
  'NavigationTest.ASensitivityOfZeroIsNotAViewThatCannotTurn'

# ── the wheel ────────────────────────────────────────────────────────────
run "N8 the wheel zooms out when pushed forward" \
  's/    const double exponent = inverted \? -notches : notches;/    const double exponent = inverted ? notches : -notches;/s' \
  'NavigationTest.TheWheelZoomsInForwardsAndTheSettingReversesIt'

run "N9 inverting is a different amount rather than the reciprocal" \
  's/    return std::pow\(1\.0 \/ perNotch, exponent\);/    return inverted ? std::pow(1.0 \/ (perNotch + 0.1), exponent)\n                    : std::pow(1.0 \/ perNotch, exponent);/s' \
  'NavigationTest.TheWheelZoomsInForwardsAndTheSettingReversesIt'

run "N10 the wheel reads eighths of a degree as notches, so it is 15x too fast" \
  's/    const double notches = double\(angleDeltaY\) \/ 120\.0;/    const double notches = double(angleDeltaY) \/ 8.0;/s' \
  'NavigationTest.TheWheelZoomsInForwardsAndTheSettingReversesIt'

run "N11 the notch size is trusted, so 1.0 is a dead wheel" \
  's/    const double perNotch =\n      std::clamp\(zoomPerNotch, kMinZoomPerNotch, kMaxZoomPerNotch\);/    const double perNotch = zoomPerNotch;/s' \
  'NavigationTest.AZoomPerNotchOfOneWouldBeAWheelThatDoesNothing'

# ── the pan ──────────────────────────────────────────────────────────────
run "N12 middle-drag stops panning under the shift-drag setting" \
  's/    if \(button == Qt::MiddleButton \|\| button == Qt::RightButton\)\n    \{\n      return true;\n    \}/    if ((button == Qt::MiddleButton || button == Qt::RightButton)\n        \&\& setting == PanModifier::MiddleDrag)\n    {\n      return true;\n    }/s' \
  'NavigationTest.MiddleAndRightAlwaysPanWhateverTheSettingSays'

run "N13 shift-drag pans whether it was asked for or not" \
  's/    return setting == PanModifier::ShiftDrag && button == Qt::LeftButton\n           && modifiers\.testFlag\(Qt::ShiftModifier\);/    return button == Qt::LeftButton \&\& modifiers.testFlag(Qt::ShiftModifier);/s' \
  'NavigationTest.ShiftDragPansOnlyWhenAskedFor'

run "N14 a plain left drag pans, so nothing can orbit" \
  's/    return setting == PanModifier::ShiftDrag && button == Qt::LeftButton\n           && modifiers\.testFlag\(Qt::ShiftModifier\);/    return setting == PanModifier::ShiftDrag \&\& button == Qt::LeftButton;/s' \
  'NavigationTest.ShiftDragPansOnlyWhenAskedFor'

# ── the named views ──────────────────────────────────────────────────────
run "N15 reset resets to a tilt no new view ever had" \
  's/    constexpr double kDefaultElevation = 45\.0;/    constexpr double kDefaultElevation = 30.0;/s' \
  'NavigationTest.ResetIsWhereANewViewStarts'

run "N16 reset leaves the azimuth alone, so north is wherever it was" \
  's/    azimuth = 0\.0;\n    elevation = kDefaultElevation;/    elevation = kDefaultElevation;/s' \
  'NavigationTest.ResetIsWhereANewViewStarts'

run "N17 looking down spins the map to north as well" \
  's/      case NamedView::Top:[\s\S]*?elevation = 90\.0;\n\n        return;/      case NamedView::Top:\n        azimuth = 0.0;\n        elevation = 90.0;\n\n        return;/s' \
  'NavigationTest.TopLooksDownWithoutSpinningTheMap'

run "N18 top view and the gizmo's up arm disagree about how far down" \
  's/      case NamedView::Top:([\s\S]*?)elevation = 90\.0;/      case NamedView::Top:$1elevation = 80.0;/s' \
  'NavigationTest.TopViewAndTheGizmosUpArmMeanTheSameThing'

note ""
note "u5: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
