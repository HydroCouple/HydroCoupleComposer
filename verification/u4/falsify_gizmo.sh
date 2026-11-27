#!/usr/bin/env bash
# U4 falsifiers: the orientation cue's arithmetic — which way its arms
# point, that each one is a solid, what a click on one answers, and what
# view that answer names. Every mutation must bite; a pattern that no
# longer matches is reported as NOT APPLIED rather than read as a
# survivor, and exit status is judged as well as output, because a gate
# that crashes prints no [FAILED] line.
#
# The drawing itself is not falsified here and cannot be: the corner
# viewport needs a graphics device, and QRhiWidget cannot make one under
# the offscreen platform (D31). Those are the hand-off's to confirm by
# eye.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}
TARGET=${TARGET:-test_axisgizmo}
BIN=${BIN:-$BUILD/tests/gui/test_axisgizmo}

GIZMO=src/scene/axisgizmo.cpp

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
restore() { [ -f "$GIZMO.bak" ] && mv "$GIZMO.bak" "$GIZMO" && touch "$GIZMO"; return 0; }
trap 'restore; rebuild' EXIT
run() { if mutate "$GIZMO" "$2"; then rebuild; gate_ok "$3"; check "$1" red $?; fi; restore; }

rebuild; gate_ok 'AxisGizmoTest.*'; check "baseline" green $?

# ── the geometry ─────────────────────────────────────────────────────────
run "G1 a segment count of two is trusted, so a cone is a flat sliver" \
  's/    const int segments = std::max\(3, coneSegments\);/    const int segments = coneSegments;/s' \
  'AxisGizmoTest.ASmallSegmentCountIsRaisedRatherThanDegenerate'

run "G2 all three arms are drawn in one colour, so the cue says nothing" \
  's/        Arm\{ \{ 0\.0f, 1\.0f, 0\.0f \}, QColor\(86, 166, 75\) \},\n        Arm\{ \{ 0\.0f, 0\.0f, 1\.0f \}, QColor\(66, 122, 214\) \},/        Arm{ { 0.0f, 1.0f, 0.0f }, QColor(214, 69, 65) },\n        Arm{ { 0.0f, 0.0f, 1.0f }, QColor(214, 69, 65) },/s' \
  'AxisGizmoTest.ItIsThreeArmsOfSolidGeometry'

run "G3 the up arm crosses with Z, which is itself, so it collapses" \
  's/      const QVector3D helper = std::abs\(axis\.z\(\)\) > 0\.9f\n                                 \? QVector3D\(1\.0f, 0\.0f, 0\.0f\)\n                                 : QVector3D\(0\.0f, 0\.0f, 1\.0f\);/      const QVector3D helper = QVector3D(0.0f, 0.0f, 1.0f);/s' \
  'AxisGizmoTest.EveryArmIsASolidRatherThanACollapsedStick'

run "G4 north is -Y, so the gizmo points the opposite way to the map" \
  's/        Arm\{ \{ 0\.0f, 1\.0f, 0\.0f \}, QColor\(86, 166, 75\) \},/        Arm{ { 0.0f, -1.0f, 0.0f }, QColor(86, 166, 75) },/s' \
  'AxisGizmoTest.TheGreenArmIsTheOnePointingNorth'

# ── the view, and the reason it borrows a Camera ─────────────────────────
run "G5 the eye is hand-written again and disagrees about north" \
  's/    Camera camera;\n    camera\.setTarget\(QVector3D\(0\.0f, 0\.0f, 0\.0f\)\);\n    camera\.setDistance\(1\.0\);\n    camera\.setAzimuth\(azimuthDegrees\);\n    camera\.setElevation\(elevationDegrees\);\n\n    QMatrix4x4 view;\n    view\.lookAt\(camera\.eye\(\), QVector3D\(0\.0f, 0\.0f, 0\.0f\),\n                QVector3D\(0\.0f, 0\.0f, 1\.0f\)\);/    const double a = azimuthDegrees * M_PI \/ 180.0;\n    const double e = elevationDegrees * M_PI \/ 180.0;\n    const double h = std::cos(e);\n    const QVector3D eye(float(h * std::sin(a)), float(h * std::cos(a)),\n                        float(std::sin(e)));\n\n    QMatrix4x4 view;\n    view.lookAt(eye, QVector3D(0.0f, 0.0f, 0.0f),\n                QVector3D(0.0f, 0.0f, 1.0f));/s' \
  'AxisGizmoTest.TheGizmoCannotDisagreeWithTheCameraAboutNorth'

run "G6 the camera's elevation guard is dropped, so a plan view collapses" \
  's/    Camera camera;\n    camera\.setTarget\(QVector3D\(0\.0f, 0\.0f, 0\.0f\)\);\n    camera\.setDistance\(1\.0\);\n    camera\.setAzimuth\(azimuthDegrees\);\n    camera\.setElevation\(elevationDegrees\);\n\n    QMatrix4x4 view;\n    view\.lookAt\(camera\.eye\(\), QVector3D\(0\.0f, 0\.0f, 0\.0f\),\n                QVector3D\(0\.0f, 0\.0f, 1\.0f\)\);/    const double a = azimuthDegrees * M_PI \/ 180.0;\n    const double e = elevationDegrees * M_PI \/ 180.0;\n    const double h = std::cos(e);\n    const QVector3D eye(float(h * std::sin(a)), float(-h * std::cos(a)),\n                        float(std::sin(e)));\n\n    QMatrix4x4 view;\n    view.lookAt(eye, QVector3D(0.0f, 0.0f, 0.0f),\n                QVector3D(0.0f, 0.0f, 1.0f));/s' \
  'AxisGizmoTest.LookingStraightDownFlattensUpToAPoint'

run "G7 the box is drawn to the arms, so an arrowhead is clipped away" \
  's/    projection\.ortho\(-1\.4f, 1\.4f, -1\.4f, 1\.4f, -4\.0f, 4\.0f\);/    projection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -4.0f, 4.0f);/s' \
  'AxisGizmoTest.TheProjectionIgnoresEverythingButRotation'

run "G8 the camera is parked further out, so an arm clips past the far plane" \
  's/    camera\.setDistance\(1\.0\);/    camera.setDistance(3.0);/s' \
  'AxisGizmoTest.TheProjectionIgnoresEverythingButRotation'

# ── the hit test ─────────────────────────────────────────────────────────
run "G9 every click is a near-tie, so the nearest arm to the viewer wins" \
  's/      constexpr double kSameSpot = 1\.0e-3;/      constexpr double kSameSpot = 1.0;/s' \
  'AxisGizmoTest.ClickingAnArmPicksItAndClickingBesideItPicksNothing'

run "G10 the tolerance is not enforced, so a click anywhere hits an arm" \
  's/      if \(distance > tolerance\)\n      \{\n        continue;\n      \}//s' \
  'AxisGizmoTest.ClickingAnArmPicksItAndClickingBesideItPicksNothing'

run "G11 depth alone decides, so the arm facing the viewer swallows clicks" \
  's/      if \(distance < bestDistance - kSameSpot\n          \|\| \(distance < bestDistance \+ kSameSpot\n              && projected\.z\(\) < bestDepth\)\)/      if (projected.z() < bestDepth)/s' \
  'AxisGizmoTest.ClickingAnArmPicksItAndClickingBesideItPicksNothing'

# ── the views the answers name ───────────────────────────────────────────
run "G12 looking down spins the map to north as well" \
  's/        elevationDegrees = 90\.0;\n\n        return true;/        azimuthDegrees = 0.0;\n        elevationDegrees = 90.0;\n\n        return true;/s' \
  'AxisGizmoTest.TheAnswersAreTheViewsTheyName'

run "G13 north means standing north of the target, so the view faces away" \
  's/        azimuthDegrees = 0\.0;\n        elevationDegrees = 0\.0;/        azimuthDegrees = 180.0;\n        elevationDegrees = 0.0;/s' \
  'AxisGizmoTest.TheAnswersAreTheViewsTheyName:AxisGizmoTest.SnappingToAnArmThenClickingItAgainIsStable'

run "G14 east is west" \
  's/        azimuthDegrees = 90\.0;\n        elevationDegrees = 0\.0;/        azimuthDegrees = 270.0;\n        elevationDegrees = 0.0;/s' \
  'AxisGizmoTest.TheAnswersAreTheViewsTheyName'

run "G15 a missed click reports a view, so a stray press moves the camera" \
  's/      case GizmoAxis::None:\n        break;\n    \}\n\n    return false;/      case GizmoAxis::None:\n        break;\n    }\n\n    return true;/s' \
  'AxisGizmoTest.TheAnswersAreTheViewsTheyName'

# ── the corner, and the click path into it ──────────────────────
run "G16 a corner is stored under another corner's name" \
  's/      case GizmoCorner::TopLeft:\n        return QStringLiteral\("TopLeft"\);/      case GizmoCorner::TopLeft:\n        return QStringLiteral("TopRight");/s' \
  'AxisGizmoTest.TheCornerNamesSurviveARoundTripAndNonsenseIsACorner'

run "G17 an unknown name falls back to a corner nothing round-trips to" \
  's/    return GizmoCorner::BottomLeft;\n  \}\n\n  QString gizmoCornerName/    return GizmoCorner::BottomRight;\n  }\n\n  QString gizmoCornerName/s' \
  'AxisGizmoTest.TheCornerNamesSurviveARoundTripAndNonsenseIsACorner'

run "G18 bottom is measured from the top, so two corners are the same one" \
  's/    const int bottom = viewport\.height\(\) - side - kInset;/    const int bottom = kInset;/s' \
  'AxisGizmoTest.EachCornerIsTheCornerItNames'

run "G19 the requested size is trusted, so a big gizmo hangs off a small view" \
  's/    const int side =\n      std::min\(\{ size, viewport\.width\(\) - 2 \* kInset,\n                 viewport\.height\(\) - 2 \* kInset \}\);/    const int side = size;/s' \
  'AxisGizmoTest.AGizmoTooBigForItsViewShrinksRatherThanOverflowing'

run "G20 a view with no room is given a negative viewport, which isEmpty() calls empty" \
  's/    if \(side <= 0\)\n    \{\n      return \{\};\n    \}\n\n    const int left/    const int left/s' \
  'AxisGizmoTest.AGizmoTooBigForItsViewShrinksRatherThanOverflowing'

run "G21 Y is not flipped, so a click on north answers south" \
  's/    const double y =\n      1\.0 - 2\.0 \* double\(point\.y\(\) - rect\.y\(\)\) \/ double\(rect\.height\(\)\);/    const double y =\n      2.0 * double(point.y() - rect.y()) \/ double(rect.height()) - 1.0;/s' \
  'AxisGizmoTest.AClickInTheCornerBecomesAPointInTheGizmosSquare:AxisGizmoTest.ClickingTheNorthArmOnScreenAnswersNorth'

run "G22 an empty rect divides by zero and answers NaN, which reads as no arm" \
  's/    if \(rect\.width\(\) <= 0 \|\| rect\.height\(\) <= 0\)\n    \{\n      return \{ 2\.0, 2\.0 \};\n    \}\n\n//s' \
  'AxisGizmoTest.AClickInTheCornerBecomesAPointInTheGizmosSquare'

note ""
note "u4: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
