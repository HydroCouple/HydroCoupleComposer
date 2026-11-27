#!/usr/bin/env bash
# U3 falsifiers: what a point layer and a filled ring contribute to the 3D
# view. Every mutation must bite; a pattern that no longer matches is
# reported as NOT APPLIED rather than read as a survivor, and exit status is
# judged as well as output.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}

FEATURE=src/layers/featurelayer.cpp
MESH=src/layers/meshlayer.cpp
DIALOG=src/ui/dialogs/layerpropertiesdialog.cpp

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() { cmake --build "$BUILD" --target "${1:-test_scenemarkers}" >/dev/null 2>&1 || note "REBUILD FAILED"; }

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
    matched "$BUILD/tests/gui/${2:-test_scenemarkers}" "$1" || FILTER_EMPTY=1
    timeout 120 "$BUILD/tests/gui/${2:-test_scenemarkers}" --gtest_filter="$1" >/dev/null 2>&1; }
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
restore() { for f in "$FEATURE" "$MESH" "$DIALOG"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }
trap 'restore; rebuild' EXIT
run() { if mutate "$2" "$3"; then rebuild "${5:-test_scenemarkers}"; gate_ok "$4" "${5:-test_scenemarkers}"; check "$1" red $?; fi; restore; }

rebuild; gate_ok 'MarkerTest.*'; check "baseline" green $?

# ── markers ──────────────────────────────────────────────────────────────
run "M1 point layers contribute nothing to the scene again" "$FEATURE" \
  's/    \/\/ Every kind now, points included[\s\S]*?return this;\n  \}/    return m_kind == GeometryKind::Point ? nullptr : this;\n  }/s' \
  'MarkerTest.APointLayerContributesToTheSceneAtAll'

run "M2 the extent is judged by isEmpty, so collinear points vanish" "$FEATURE" \
  's/    const double diagonal = std::hypot\(box\.width\(\), box\.height\(\)\);\n\n    if \(diagonal <= 0\.0\)/    const double diagonal = box.isEmpty() ? 0.0 : std::hypot(box.width(), box.height());\n\n    if (diagonal <= 0.0)/s' \
  'MarkerTest.PointsInARowStillHaveAnExtentToSizeFrom'

run "M3 an empty marker batch is appended anyway" "$FEATURE" \
  's/      if \(!markers\.isEmpty\(\)\)\n      \{\n        batches\.append\(std::move\(markers\)\);\n      \}/      batches.append(std::move(markers));/s' \
  'MarkerTest.ASinglePointHasNoExtentToSizeFromAndDrawsNothing'

run "M4 a marker is anchored at its point rather than centred on it" "$FEATURE" \
  's/        into\.addVertex\(centre \+ direction \* half, direction, color\);/        into.addVertex(centre + direction * half + QVector3D(0.0f, 0.0f, half), direction, color);/s' \
  'MarkerTest.AMarkerIsCentredOnItsPointAndSizedFromTheExtent'

run "M5 a draped marker ignores the terrain" "$FEATURE" \
  's/            z = \(sampled\.isEmpty\(\) \? 0\.0 : sampled\.first\(\)\) \+ policy\.offset;/            z = policy.offset;/s' \
  'MarkerTest.AMarkerSitsOnTheTerrainWhenItIsDrapedAndFlatWhenNot'

# ── filled rings ─────────────────────────────────────────────────────────
run "M6 concave rings are filled too, spanning the notch" "$FEATURE" \
  's/        if \(filling && path\.size\(\) >= 3 && isConvexRing\(path\)\)/        if (filling \&\& path.size() >= 3)/s' \
  'MarkerTest.AConcaveRingIsLeftAsAnOutlineRatherThanFilledWrongly'

run "M7 rings fill whether the layer was asked or not" "$FEATURE" \
  's/    const bool filling = m_fillRings && supportsRingFill\(\);/    const bool filling = supportsRingFill();/s' \
  'MarkerTest.AConvexRingFillsAndOnlyWhenAsked'

run "M8 a line layer is offered ring filling" "$FEATURE" \
  's/    return m_kind == GeometryKind::Polygon;/    return true;/s' \
  'MarkerTest.ALineLayerIsNeverOfferedRingFilling'

# ── the mesh's node entity ───────────────────────────────────────────────
# Aimed at test_scene3d, not test_layeredmesh. The gate for this lives
# where the old "supplies no geometry" gate lived — it is that gate,
# rewritten to the truth U3 made — and it was written after this script
# was, so this line pointed at a suite that never asserted it.
run "M9 a node mesh draws nothing in 3D again" "$MESH" \
  's/      SceneGeometry markers = markerGeometry\(nullptr, style\(\)\);\n\n      if \(!markers\.isEmpty\(\)\)\n      \{\n        batches\.append\(std::move\(markers\)\);\n      \}\n\n      return batches;/      return batches;/s' \
  'Scene3DTest.ANodeMeshSuppliesMarkersRatherThanNothing' test_scene3d

# ── the dialog ───────────────────────────────────────────────────────────
run "D1 the peel range can be inverted into one that shows nothing" "$DIALOG" \
  's/                if \(m_peelLastSpin->value\(\) < value\)\n                \{\n                  m_peelLastSpin->setValue\(value\);\n                \}//s' \
  'LayerPropertiesTest.*' test_layerproperties

trap - EXIT
restore; rebuild; gate_ok 'MarkerTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
