#!/usr/bin/env bash
# U2a falsifiers: which editor an argument gets. Almost every mutation
# here is a *reordering* rather than a wrong value, because the order of
# the chain is what this code is — each test asks whether one signal still
# beats another. Every mutation must bite; a pattern that no longer
# matches is reported as NOT APPLIED rather than read as a survivor, and
# exit status is judged as well as output.
#
# What is NOT falsified here is the gathering half — the dynamic_casts and
# the validComponentDataItemTypes scan — which needs a loaded component
# library. That is test_configurator's, on a machine with a build.
set -u
cd "$(dirname "$0")/../.."

export QT_QPA_PLATFORM=offscreen
BUILD=${BUILD:-build/darwin}
TARGET=${TARGET:-test_argumentkinds}
BIN=${BIN:-$BUILD/tests/gui/test_argumentkinds}

DESC=src/configurator/argumentdescriptor.cpp

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
restore() { [ -f "$DESC.bak" ] && mv "$DESC.bak" "$DESC" && touch "$DESC"; return 0; }
trap 'restore; rebuild' EXIT
run() { if mutate "$DESC" "$2"; then rebuild; gate_ok "$3"; check "$1" red $?; fi; restore; }

rebuild; gate_ok 'ArgumentKindTest.*'; check "baseline" green $?

# ── the typed interfaces stop being consulted ────────────────────────────
run "K1 a time series is read from its shape instead, so it is a table" \
  's/    if \(facts\.isTimeSeries\)\n    \{\n      return ArgumentEditorKind::TimeSeries;\n    \}\n\n//s' \
  'ArgumentKindTest.ATypedInterfaceBeatsWhateverTheShapeSuggests'

run "K2 a surface is read from its shape instead" \
  's/    if \(facts\.isPolyhedralSurface\)\n    \{\n      return ArgumentEditorKind::Mesh;\n    \}\n\n//s' \
  'ArgumentKindTest.ATypedInterfaceBeatsWhateverTheShapeSuggests'

run "K3 a raster is read from its shape instead" \
  's/    if \(facts\.isRaster\)\n    \{\n      return ArgumentEditorKind::Raster;\n    \}\n\n//s' \
  'ArgumentKindTest.ATypedInterfaceBeatsWhateverTheShapeSuggests'

run "K4 geometry is read from its shape instead" \
  's/    if \(facts\.isGeometry\)\n    \{\n      return ArgumentEditorKind::Geometry;\n    \}\n\n//s' \
  'ArgumentKindTest.ATypedInterfaceBeatsWhateverTheShapeSuggests'

run "K5 an id table is read from its shape instead" \
  's/    if \(facts\.isIdBased\)\n    \{\n      return ArgumentEditorKind::IdTable;\n    \}\n\n//s' \
  'ArgumentKindTest.ATypedInterfaceBeatsWhateverTheShapeSuggests'

# ── the order among them ─────────────────────────────────────────────────
run "K6 time beats space, so a moving mesh loses its geometry" \
  's/    if \(facts\.isRaster\)/    if (facts.isTimeSeries)\n    {\n      return ArgumentEditorKind::TimeSeries;\n    }\n\n    if (facts.isRaster)/s' \
  'ArgumentKindTest.SpaceBeatsTimeWhenAnArgumentIsBoth'

run "K7 identifiers beat time, so a table of series loses its plot" \
  's/    if \(facts\.isTimeSeries\)\n    \{\n      return ArgumentEditorKind::TimeSeries;\n    \}/    if (facts.isIdBased)\n    {\n      return ArgumentEditorKind::IdTable;\n    }\n\n    if (facts.isTimeSeries)\n    {\n      return ArgumentEditorKind::TimeSeries;\n    }/s' \
  'ArgumentKindTest.SpaceBeatsTimeWhenAnArgumentIsBoth'

# ── the value definition ─────────────────────────────────────────────────
run "K8 a categorical grid becomes a combo box holding one of its values" \
  's/    if \(facts\.hasCategories && facts\.isScalar\)/    if (facts.hasCategories)/s' \
  'ArgumentKindTest.ACategoricalGridIsATableNotAChoice'

run "K9 a duration is just a quantity, so hours cannot be typed" \
  's/    if \(facts\.isScalar && facts\.unitIsPureTime\n        && \(isRealKind\(facts\.dataKind\) \|\| isIntegerKind\(facts\.dataKind\)\)\)\n    \{\n      return ArgumentEditorKind::Duration;\n    \}\n\n//s' \
  'ArgumentKindTest.AUnitOfPureTimeIsADurationAndAnythingElseIsAQuantity'

run "K10 every unit is a duration" \
  's/    if \(facts\.isScalar && facts\.unitIsPureTime\n        && \(isRealKind/    if (facts.isScalar\n        \&\& (isRealKind/s' \
  'ArgumentKindTest.AUnitOfPureTimeIsADurationAndAnythingElseIsAQuantity'

run "K11 a grid with a unit becomes a single spin box" \
  's/    if \(facts\.isScalar && facts\.hasUnit\n        && \(isRealKind\(facts\.dataKind\) \|\| isIntegerKind\(facts\.dataKind\)\)\)/    if (facts.hasUnit)/s' \
  'ArgumentKindTest.AUnitOfPureTimeIsADurationAndAnythingElseIsAQuantity'

run "K12 anything the component will take as a CRS becomes a CRS picker" \
  's/    if \(facts\.acceptsSpatialReference\n        && facts\.dataKind == HydroCouple::DataKind::String\)/    if (facts.acceptsSpatialReference)/s' \
  'ArgumentKindTest.ACrsIsOneTheComponentWillOnlyTakeAsACrs'

run "K13 every string is a CRS" \
  's/    if \(facts\.acceptsSpatialReference\n        && facts\.dataKind == HydroCouple::DataKind::String\)/    if (facts.dataKind == HydroCouple::DataKind::String)/s' \
  'ArgumentKindTest.ACrsIsOneTheComponentWillOnlyTakeAsACrs'

# ── text and files ───────────────────────────────────────────────────────
run "K14 a kinetics file is a path box, so its contents cannot be edited" \
  's/    if \(facts\.hasFileFilters\n        && facts\.dataKind == HydroCouple::DataKind::String\n        && facts\.isScalar\)\n    \{\n      return ArgumentEditorKind::LongText;\n    \}\n\n//s' \
  'ArgumentKindTest.TextReadFromAFileIsTextRatherThanAPath'

run "K15 any file becomes a text pane, including opaque bytes" \
  's/    if \(facts\.hasFileFilters\n        && facts\.dataKind == HydroCouple::DataKind::String\n        && facts\.isScalar\)/    if (facts.hasFileFilters)/s' \
  'ArgumentKindTest.TextReadFromAFileIsTextRatherThanAPath'

run "K16 a grid of text from a file becomes one long string" \
  's/    if \(facts\.hasFileFilters\n        && facts\.dataKind == HydroCouple::DataKind::String\n        && facts\.isScalar\)/    if (facts.hasFileFilters\n        \&\& facts.dataKind == HydroCouple::DataKind::String)/s' \
  'ArgumentKindTest.TextReadFromAFileIsTextRatherThanAPath'

# ── the pre-existing behavior ───────────────────────────────────────────
run "K17 a grid is no longer a table" \
  's/    if \(!facts\.isScalar\)\n    \{\n      return ArgumentEditorKind::Table;\n    \}\n\n//s' \
  'ArgumentKindTest.TheFallbackChainStillDecidesWhatItAlwaysDid'

run "K18 file filters stop meaning a path" \
  's/    if \(facts\.hasFileFilters\)\n    \{\n      return ArgumentEditorKind::FilePath;\n    \}\n\n//s' \
  'ArgumentKindTest.TheFallbackChainStillDecidesWhatItAlwaysDid'

run "K19 two kinds answer to one name" \
  's/      case ArgumentEditorKind::Duration:\n        return QStringLiteral\("Duration"\);/      case ArgumentEditorKind::Duration:\n        return QStringLiteral("Quantity");/s' \
  'ArgumentKindTest.EveryKindHasANameAndNoTwoShareOne'

# ── the fallback that keeps arguments on screen ──────────────────
run "K20 the dock falls back to a kind it cannot draw, so arguments vanish" \
  's/  ArgumentEditorKind genericEditorKind\(const ArgumentFacts &facts\)\n  \{/  ArgumentEditorKind genericEditorKind(const ArgumentFacts \&facts)\n  {\n    if (facts.isTimeSeries)\n    {\n      return ArgumentEditorKind::TimeSeries;\n    }\n/s' \
  'ArgumentKindTest.NoArgumentLosesTheEditorItUsedToHave'

run "K21 the fallback picks up the new chain's opinion about files" \
  's/  ArgumentEditorKind genericEditorKind\(const ArgumentFacts &facts\)\n  \{\n([\s\S]*?)    if \(facts\.hasFileFilters\)/  ArgumentEditorKind genericEditorKind(const ArgumentFacts \&facts)\n  {\n$1    if (facts.hasFileFilters\n        \&\& facts.dataKind == HydroCouple::DataKind::String\n        \&\& facts.isScalar)\n    {\n      return ArgumentEditorKind::LongText;\n    }\n\n    if (facts.hasFileFilters)/s' \
  'ArgumentKindTest.TheFallbackIsExactlyTheOldChain'

run "K22 the fallback stops seeing grids as tables" \
  's/  ArgumentEditorKind genericEditorKind\(const ArgumentFacts &facts\)\n  \{\n([\s\S]*?)    if \(!facts\.isScalar\)\n    \{\n      return ArgumentEditorKind::Table;\n    \}\n\n/  ArgumentEditorKind genericEditorKind(const ArgumentFacts \&facts)\n  {\n$1/s' \
  'ArgumentKindTest.TheFallbackIsExactlyTheOldChain'

note ""
note "u2a: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
