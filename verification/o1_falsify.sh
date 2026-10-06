#!/usr/bin/env bash
# O1 falsifiers: pasted XYZ tile templates, tested by fetching one tile.
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

# M1: a template is not recognized — it goes down the capabilities ladder
# like any other address.
cp "$DIALOG" "$DIALOG.bak"
perl -0pi -e 's/    if \(XyzTileSource::isTemplate\(m_url->text\(\)\)\)\n    \{\n      probeTemplate\(\);\n\n      return;\n    \}\n//s' "$DIALOG"
rebuild
gate_ok 'OgcServiceDialogTest.ATileTemplateIsTestedByFetchingOneTile'
check "M1 template not recognized" red $?
restore

# M2: the probe accepts whatever came back — the Test half of Test
# Connection is gone, and a page under a 200 becomes a basemap.
cp "$DIALOG" "$DIALOG.bak"
perl -0pi -e 's/        if \(!tile\.loadFromData\(response\.body\)\)\n        \{\n.*?\n          return;\n        \}\n/        (void)tile.loadFromData(response.body);\n/s' "$DIALOG"
rebuild
gate_ok 'OgcServiceDialogTest.ATemplateThatDoesNotAnswerWithATileIsRefused'
check "M2 probe accepts anything" red $?
restore

# M3: the tile URL ignores the tile — every zoom and column asks for the
# same square.
cp "$SOURCE" "$SOURCE.bak"
perl -0pi -e 's/    url\.replace\(QLatin1String\("\{x\}"\), QString::number\(tile\.x\)\);\n    url\.replace\(QLatin1String\("\{y\}"\), QString::number\(tile\.y\)\);/    url.replace(QLatin1String("{x}"), QStringLiteral("0"));\n    url.replace(QLatin1String("{y}"), QStringLiteral("0"));/s' "$SOURCE"
rebuild
gate_ok 'OgcServiceDialogTest.ATileTemplateIsTestedByFetchingOneTile'
check "M3 tile coordinates ignored" red $?
restore

# M4: a saved template is restored the way a service is — a capabilities
# request for a document that was never there.
cp "$RESTORER" "$RESTORER.bak"
perl -0pi -e 's/    if \(type == QLatin1String\("xyz"\)\)\n    \{/    if (false)\n    \{/s' "$RESTORER"
rebuild
gate_ok 'OgcServiceDialogTest.ATemplateBasemapIsRebuiltWithoutAskingAnything'
check "M4 template restored as a service" red $?
restore

trap - EXIT
rebuild; gate_ok 'OgcServiceDialogTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
