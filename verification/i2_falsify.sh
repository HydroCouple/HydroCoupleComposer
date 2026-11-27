#!/usr/bin/env bash
# I2 falsifiers: the app icon wiring.
#
# Verifies the built artifacts (bundle icns + CFBundleIconFile + Retina
# membership + windowIcon gate), then mutates each wiring point and expects
# the corresponding check to go red. CAUGHT = the mutation was detected.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
BUILD=build/darwin
APP_CPP=src/core/composerapplication.cpp
CMAKELISTS=CMakeLists.txt

pass=0
fail=0

note() { printf '%s\n' "$*"; }

rebuild() { cmake --build "$BUILD" --target "$@" >/dev/null 2>&1; }

bundle_ok() {
    [ -f "$BUILD/HydroCoupleComposer.app/Contents/Resources/HydroCoupleComposer.icns" ] || return 1
    local plist_icon
    plist_icon=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIconFile" \
        "$BUILD/HydroCoupleComposer.app/Contents/Info.plist" 2>/dev/null)
    [ -n "$plist_icon" ] || return 1
    # Retina membership: the 2015 icns lacked every @2x slice.
    python3 - <<'EOF'
import struct, sys
data = open('build/darwin/HydroCoupleComposer.app/Contents/Resources/HydroCoupleComposer.icns','rb').read()
pos, members = 8, set()
while pos < len(data):
    members.add(data[pos:pos+4].decode('ascii','replace'))
    pos += struct.unpack('>I', data[pos+4:pos+8])[0]
sys.exit(0 if {'ic10','ic13','ic14'} <= members else 1)
EOF
}

gate_ok() {
    "$BUILD/tests/gui/test_icons" \
        --gtest_filter='IconTest.TheApplicationShipsItsWindowIcon' \
        >/dev/null 2>&1
}

check() { # label expectation(red|green) actual_rc
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then
        note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then
        note "GREEN    $label"; pass=$((pass+1))
    else
        note "SURVIVED $label"; fail=$((fail+1))
    fi
}

# ── Baseline must be green before any mutation means anything ───────────────
rebuild HydroCoupleComposer test_icons
bundle_ok; check "baseline: bundle carries a Retina icns" green $?
gate_ok;   check "baseline: windowIcon gate" green $?

# ── M1: the setWindowIcon call is deleted ───────────────────────────────────
cp "$APP_CPP" "$APP_CPP.bak"
trap 'for f in "$APP_CPP" "$CMAKELISTS"; do [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; rebuild HydroCoupleComposer test_icons' EXIT
perl -0pi -e 's/setWindowIcon\(\s*QIcon\(QStringLiteral\(":\/branding\/hydrocouplecomposer\.png"\)\)\);//s' "$APP_CPP"
rebuild test_icons
gate_ok; check "M1 setWindowIcon deleted" red $?
mv "$APP_CPP.bak" "$APP_CPP"; touch "$APP_CPP"

# ── M2: the branding resource is dropped from the build ─────────────────────
cp "$CMAKELISTS" "$CMAKELISTS.bak"
perl -0pi -e 's/qt_add_resources\(composer_core "composer_branding".*?hydrocouplecomposer\.png\)\n//s' "$CMAKELISTS"
rebuild test_icons
gate_ok; check "M2 branding resource dropped" red $?
mv "$CMAKELISTS.bak" "$CMAKELISTS"; touch "$CMAKELISTS"

# ── M3: the bundle icon wiring is dropped ───────────────────────────────────
cp "$CMAKELISTS" "$CMAKELISTS.bak"
perl -0pi -e 's/\$<\$<PLATFORM_ID:Darwin>:\$\{composer_icon_macos\}>\n//s;
              s/^\s*MACOSX_BUNDLE_ICON_FILE "HydroCoupleComposer\.icns"\)/)/m' "$CMAKELISTS"
rebuild HydroCoupleComposer
bundle_ok; check "M3 bundle icon wiring dropped" red $?
mv "$CMAKELISTS.bak" "$CMAKELISTS"; touch "$CMAKELISTS"

# ── Restore and prove the tree is green again ───────────────────────────────
trap - EXIT
rebuild HydroCoupleComposer test_icons
bundle_ok; check "restored: bundle carries a Retina icns" green $?
gate_ok;   check "restored: windowIcon gate" green $?

note "----"
note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
