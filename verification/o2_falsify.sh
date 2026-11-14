#!/usr/bin/env bash
# O2 falsifiers: saved named connections, credentials bound to the machine.
set -u
cd "$(dirname "$0")/.."

export QT_QPA_PLATFORM=offscreen
STORE=src/layers/serviceconnections.cpp
DIALOG=src/ui/dialogs/ogcservicedialog.cpp
BUILD=build/darwin
OBJDIR="$BUILD/CMakeFiles/composer_core.dir/src"

pass=0; fail=0
note() { printf '%s\n' "$*"; }
rebuild() {
    # Stale-relink hardening: force the mutated TUs to recompile.
    rm -f "$OBJDIR/layers/serviceconnections.cpp.o" \
          "$OBJDIR/ui/dialogs/ogcservicedialog.cpp.o"
    cmake --build "$BUILD" --target test_serviceconnections >/dev/null 2>&1 || note "REBUILD FAILED"
}
gate_ok() { "$BUILD/tests/gui/test_serviceconnections" --gtest_filter="$1" >/dev/null 2>&1; }
check() {
    local label=$1 expect=$2 rc=$3
    if [ "$expect" = red ] && [ "$rc" -ne 0 ]; then note "CAUGHT   $label"; pass=$((pass+1))
    elif [ "$expect" = green ] && [ "$rc" -eq 0 ]; then note "GREEN    $label"; pass=$((pass+1))
    else note "SURVIVED $label"; fail=$((fail+1)); fi
}
restore() { for f in "$STORE" "$DIALOG"; do
                [ -f "$f.bak" ] && mv "$f.bak" "$f" && touch "$f"; done; }

rebuild; gate_ok 'ServiceConnectionsTest.*'; check "baseline" green $?
trap 'restore; rebuild' EXIT

# M1: the password is stored as it was typed.
cp "$STORE" "$STORE.bak"
perl -0pi -e 's/    settings\.setValue\(QStringLiteral\("password"\),\n                      encrypt\(connection\.password\)\);/    settings.setValue(QStringLiteral("password"),\n                      connection.password.toUtf8());/s' "$STORE"
rebuild
gate_ok 'ServiceConnectionsTest.ThePasswordIsNeverOnDiskInTheClear'
check "M1 password stored in the clear" red $?
restore

# M2: the key stops depending on the machine — a copied settings file
# decrypts anywhere.
cp "$STORE" "$STORE.bak"
perl -0pi -e 's/      return QPasswordDigestor::deriveKeyPbkdf2\(\n        QCryptographicHash::Sha256, machineId, salt, kIterations, kKeyBytes\);/      Q_UNUSED(machineId)\n      return QPasswordDigestor::deriveKeyPbkdf2(\n        QCryptographicHash::Sha256, QByteArrayLiteral("fixed"), salt,\n        kIterations, kKeyBytes);/s' "$STORE"
rebuild
gate_ok 'ServiceConnectionsTest.AnotherMachineGetsTheAddressButNotThePassword'
check "M2 key no longer machine-bound" red $?
restore

# M3: one fixed salt — the same secret encrypts to the same bytes for
# every user of the program.
cp "$STORE" "$STORE.bak"
perl -0pi -e 's/    const QByteArray salt = randomBytes\(kSaltBytes\);\n    const QByteArray iv = randomBytes\(kIvBytes\);/    const QByteArray salt = QByteArray(kSaltBytes, 7);\n    const QByteArray iv = QByteArray(kIvBytes, 9);/s' "$STORE"
rebuild
gate_ok 'ServiceConnectionsTest.ThePasswordIsNeverOnDiskInTheClear'
check "M3 fixed salt and IV" red $?
restore

# M4: a wrong key is handed back as rubbish instead of nothing.
cp "$STORE" "$STORE.bak"
perl -0pi -e 's/    return plain\.isEmpty\(\) \? QString\(\) : QString::fromUtf8\(plain\);/    return QString::fromUtf8(plain.isEmpty() ? body : plain);/s' "$STORE"
rebuild
gate_ok 'ServiceConnectionsTest.AnotherMachineGetsTheAddressButNotThePassword'
check "M4 undecryptable password handed back" red $?
restore

# M5: the store accepts a connection with no address, so a named
# connection can be empty. Aimed at the store rather than at the dialog:
# the store is the only guard, the button having been left to it.
cp "$STORE" "$STORE.bak"
perl -0pi -e 's/    if \(!connection\.isValid\(\)\)\n    \{\n      return false;\n    \}//s' "$STORE"
rebuild
gate_ok 'ServiceConnectionsTest.TheDialogSavesAndReloadsWhatWasTypedIn'
check "M5 store accepts an address-less connection" red $?
restore

trap - EXIT
rebuild; gate_ok 'ServiceConnectionsTest.*'; check "restored" green $?

note "----"; note "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
