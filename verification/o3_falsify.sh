#!/usr/bin/env bash
# O3 falsifiers: per-layer drawing options, saved in lockstep.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
DIALOG=src/ui/dialogs/ogcservicedialog.cpp
SOURCE=src/layers/ogctilesource.cpp
RESTORER=src/layers/layerrestorer.cpp
BUILD=build/darwin
OBJDIR="$BUILD/CMakeFiles/composer_core.dir/src"

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    # Stale-relink hardening: force the mutated TUs to recompile.
    rm -f "$OBJDIR/ui/dialogs/ogcservicedialog.cpp.o" \
          "$OBJDIR/layers/ogctilesource.cpp.o" \
          "$OBJDIR/layers/layerrestorer.cpp.o"
    cmake --build "$BUILD" --target test_ogcservicedialog >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_ogcservicedialog" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { for f in "$DIALOG" "$SOURCE" "$RESTORER"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'OgcServiceDialogTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the layer's options are never offered.
cp "$DIALOG" "$DIALOG.bak"
perl -0pi -e 's/      m_format->addItem\(tr\("\(default\)"\)\);\n      m_format->addItems\(formats\);\n//s' "$DIALOG"
rebuild
gate_ok 'OgcServiceDialogTest.TheChosenLayersOptionsReachTheRequest'
check "M1 formats never offered" red $?
restore

# M2: what was chosen never reaches the source.
cp "$DIALOG" "$DIALOG.bak"
perl -0pi -e 's/        m_wms, QStringList\{choice\.layerId\}, chosenFormat\(\), chosenStyle\(\)\);/        m_wms, QStringList{choice.layerId});/s' "$DIALOG"
rebuild
gate_ok 'OgcServiceDialogTest.TheChosenLayersOptionsReachTheRequest'
check "M2 choice dropped on the way to the source" red $?
restore

# M3: the choice is a session whim — saved recipes forget it.
cp "$DIALOG" "$DIALOG.bak"
perl -0pi -e 's/      if \(!chosenFormat\(\)\.isEmpty\(\)\)\n      \{\n        object\.insert\(QStringLiteral\("format"\), chosenFormat\(\)\);\n      \}//s' "$DIALOG"
rebuild
gate_ok 'OgcServiceDialogTest.ChosenOptionsTravelIntoTheSavedRecipeAndBack'
check "M3 format not saved" red $?
restore

# M4: saved, but not applied on the way back — the lockstep half.
cp "$RESTORER" "$RESTORER.bak"
perl -0pi -e 's/              capabilities, listOf\(entry, QStringLiteral\("layers"\)\),\n              textOf\(entry, QStringLiteral\("format"\)\),\n              textOf\(entry, QStringLiteral\("style"\)\)\);/              capabilities, listOf(entry, QStringLiteral("layers")));/s' "$RESTORER"
rebuild
gate_ok 'OgcServiceDialogTest.ChosenOptionsTravelIntoTheSavedRecipeAndBack'
check "M4 saved options not applied on restore" red $?
restore

trap - EXIT
rebuild; gate_ok 'OgcServiceDialogTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
