#!/bin/bash
# Falsification: layers surviving a save and an open.
#
# Every mutation still saves a file that opens. What they break is whether
# the layers come back, whether they come back as themselves, whether one
# bad entry takes the rest with it, and whether a password reaches a file
# that gets checked in.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_layerpersistence
LOG=verification/e5/falsify_layerpersistence.log
: > "$LOG"

FILES=(src/project/presentation.cpp src/layers/layerrestorer.cpp)

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

  if ! cmake --build $BUILD --target test_layerpersistence -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$($TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] LayerPersistence' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# ── whether they come back at all ───────────────────────────────────────────

# 1. Write the layers and never read them, so every composition opens with
#    an empty map and a file that looks perfectly correct.
m_never_read() {
  perl -0pi -e 's/    if \(layers\.isArray\(\)\)\n    \{\n      m_layers = layers\.toArray\(\);\n    \}//' src/project/presentation.cpp
}

# 2. Read them and never write them.
m_never_write() {
  perl -0pi -e 's/    if \(!m_layers\.isEmpty\(\)\)\n    \{\n      root\.insert\(QStringLiteral\("layers"\), m_layers\);\n    \}//' src/project/presentation.cpp
}

# 3. A composition holding only layers counts as empty, so saving skips it
#    and the work is gone at the next open.
m_layers_do_not_count() {
  perl -0pi -e 's/    return m_components\.isEmpty\(\) && m_meshDomain\.isEmpty\(\)\n           && m_layers\.isEmpty\(\);/    return m_components.isEmpty() \&\& m_meshDomain.isEmpty();/' src/project/presentation.cpp
}

# 4. Clear() forgets to drop them, so opening a second composition shows the
#    first one's layers as well.
m_clear_keeps_layers() {
  perl -0pi -e 's/    m_layers = QJsonArray\{\};//' src/project/presentation.cpp
}

# ── whether they come back as themselves ────────────────────────────────────

# 5. Drop the recipe from a restored layer, so it survives one round trip
#    and vanishes on the second.
m_no_recipe_forward() {
  perl -0pi -e 's/      layer->setPersistentState\(entry\);//' src/layers/layerrestorer.cpp
}

# 6. Ignore the saved name, so every layer comes back called whatever its
#    file or service happened to call it.
m_ignore_name() {
  perl -0pi -e 's/      if \(!name\.isEmpty\(\)\)\n      \{\n        layer->setName\(name\);\n      \}//' src/layers/layerrestorer.cpp
}

# ── whether one failure takes the rest ──────────────────────────────────────

# 7. Report a missing file without saying which, so the user is told a layer
#    failed and not where to look.
m_anonymous_failure() {
  perl -0pi -e 's/      message = tr\("%1 is no longer where this composition left it: %2"\)\n                  \.arg\(textOf\(entry, QStringLiteral\("name"\)\), path\);/      message = tr("A layer could not be restored.");/' src/layers/layerrestorer.cpp
}

# 8. Silently skip a type this version does not know, so a composition
#    written by a later one quietly loses layers.
m_silent_unknown_type() {
  perl -0pi -e 's/      onFailed\(tr\("%1 was saved as a kind of layer this version does not "\n                  "know: %2\."\)\.arg\(name, type\)\);//' src/layers/layerrestorer.cpp
}

# 9. Give up on the whole array at the first entry that will not rebuild.
m_stop_at_first_failure() {
  perl -0pi -e 's/      else\n      \{\n        onFailed\(message\);\n      \}\n\n      return;/      else\n      {\n        onFailed(message);\n      }\n\n      return;\n      \/\/ mutation/' src/layers/layerrestorer.cpp
  perl -0pi -e 's/      if \(value\.isObject\(\)\)\n      \{\n        restoreOne\(value\.toObject\(\), onLayer, onFailed\);\n      \}/      if (value.isObject())\n      {\n        restoreOne(value.toObject(), onLayer, onFailed);\n        return;\n      }/' src/layers/layerrestorer.cpp
}

# ── whether a service layer waits ───────────────────────────────────────────

# 10. Never count what is outstanding, so a caller cannot tell whether the
#     map is finished loading.
m_no_pending_count() {
  perl -0pi -e 's/    \+\+m_pending;\n\n    \/\/ No credentials\./    \/\/ No credentials./' src/layers/layerrestorer.cpp
}

run_mutation 1-never-read            m_never_read
run_mutation 2-never-write           m_never_write
run_mutation 3-layers-do-not-count   m_layers_do_not_count
run_mutation 4-clear-keeps-layers    m_clear_keeps_layers
run_mutation 5-no-recipe-forward     m_no_recipe_forward
run_mutation 6-ignore-name           m_ignore_name
run_mutation 7-anonymous-failure     m_anonymous_failure
run_mutation 8-silent-unknown-type   m_silent_unknown_type
run_mutation 9-stop-at-first-failure m_stop_at_first_failure
run_mutation 10-no-pending-count     m_no_pending_count

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
