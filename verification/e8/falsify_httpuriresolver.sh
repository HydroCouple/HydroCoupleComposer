#!/bin/bash
# Falsification: a model argument reading an http(s) URI.
#
# A blocking resolver over an async client adds a failure mode the rest of
# this program does not have: not a wrong answer but a hang, when the loop is
# entered with nothing left to quit it. Mutations run under a watchdog for
# exactly that reason -- a mutation that hangs is CAUGHT, not inconclusive.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_httpuriresolver
LOG=verification/e8/falsify_httpuriresolver.log
: > "$LOG"

FILES=(src/core/httpuriresolver.cpp src/core/composerapplication.cpp)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() { for f in "${FILES[@]}"; do cp "$f.orig" "$f"; touch "$f"; done; }
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }

  local changed=0
  for f in "${FILES[@]}"; do cmp -s "$f" "$f.orig" || changed=1; done
  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"; return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_httpuriresolver -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 60 $TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if [ "$status" -eq 124 ]; then
    echo "MUTATION $name: CAUGHT (hung -- the watchdog killed it)" | tee -a "$LOG"
  elif echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] HttpUriResolverTest' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── the wiring ──────────────────────────────────────────────────────────────

# 1. Build the resolver and never hand it to the SDK, so every https argument
#    is told no resolver is installed by the host that has one.
m_not_installed() {
  perl -0pi -e 's/    HydroCouple::SDK::IO::setUriResolver\(m_uriResolver\.get\(\)\);//' src/core/composerapplication.cpp
}

# 2. Leave the SDK holding a pointer to a destroyed resolver. Unfalsifiable
#    here: a test process has one QApplication and it dies at the end of the
#    run, so nothing calls readReference() after the resolver is gone. Unlike
#    the guards this program has removed, this one CAN fire -- main() destroys
#    the application and a component plugin's static destructors run after --
#    it just cannot be staged.
m_never_uninstalled() {
  perl -0pi -e 's/    \/\/ The SDK holds a bare pointer, so it must stop holding this one first\.\n    HydroCouple::SDK::IO::setUriResolver\(nullptr\);//' src/core/composerapplication.cpp
}

# ── what it claims ──────────────────────────────────────────────────────────

# 3. Claim every scheme, so an ftp: or s3: address is asked for over HTTP and
#    fails as a network error rather than as something unsupported.
m_claims_everything() {
  perl -0pi -e 's/    return scheme == "http" \|\| scheme == "https";/    return true;/' src/core/httpuriresolver.cpp
}

# 4. Claim file: too, so the host takes over reading local files by being
#    present -- and reads them over a network stack.
m_claims_file() {
  perl -0pi -e 's/    return scheme == "http" \|\| scheme == "https";/    return scheme == "http" || scheme == "https" || scheme == "file";/' src/core/httpuriresolver.cpp
}

# ── the body ────────────────────────────────────────────────────────────────

# 5. Take the body as a C string, so a coverage stops at its first NUL.
m_body_stops_at_nul() {
  perl -0pi -e 's/    contents = answer\.body\.toStdString\(\);/    contents = answer.body.constData();/' src/core/httpuriresolver.cpp
}

# 6. Treat any answer as the answer, so a 404 body is parsed as data.
m_ignore_ok() {
  perl -0pi -e 's/    if \(!answer\.ok\)/    if (false)/' src/core/httpuriresolver.cpp
}

# 7. Replace the server's reason with a generic one.
m_lose_the_error() {
  perl -0pi -e 's/      message = answer\.error\.isEmpty\(\)\n                  \? "Could not read " \+ uri\n                  : answer\.error\.toStdString\(\);/      message = "Could not read " + uri;/' src/core/httpuriresolver.cpp
}

# ── the loop ────────────────────────────────────────────────────────────────

# 8. Enter the event loop even when the request never started, so a caller
#    waits for a callback that is never coming.
m_await_a_dead_request() {
  perl -0pi -e 's/    if \(id == 0\)\n    \{\n      message = "Could not ask for " \+ uri;\n      return false;\n    \}//' src/core/httpuriresolver.cpp
}

# 9. Admit user input while a component is initializing on the stack below.
#    Unfalsifiable here -- no gate can deliver a click to a nested loop -- and
#    recorded rather than dropped, because the protection is real: the
#    configurator that started the fetch can be closed from inside it.
m_admit_user_input() {
  perl -0pi -e 's/    loop\.exec\(QEventLoop::ExcludeUserInputEvents\);/    loop.exec();/' src/core/httpuriresolver.cpp
}

run_mutation 1-not-installed         m_not_installed
run_mutation 2-never-uninstalled     m_never_uninstalled
run_mutation 3-claims-everything     m_claims_everything
run_mutation 4-claims-file           m_claims_file
run_mutation 5-body-stops-at-nul     m_body_stops_at_nul
run_mutation 6-ignore-ok             m_ignore_ok
run_mutation 7-lose-the-error        m_lose_the_error
run_mutation 8-await-a-dead-request  m_await_a_dead_request
run_mutation 9-admit-user-input      m_admit_user_input

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
