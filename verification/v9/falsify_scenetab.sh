#!/bin/bash
# Falsification: the dialog's 3D tab.

set -u
cd "$(dirname "$0")/../.." || exit 1

export QT_QPA_PLATFORM=offscreen

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_layerproperties
LOG=verification/v9/falsify_scenetab.log
: > "$LOG"

FILES=(src/ui/dialogs/layerpropertiesdialog.cpp include/layers/meshlayer.h)

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

  if ! cmake --build $BUILD --target test_layerproperties -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Omit the 3D tab for a layer with no scene source -- the old silence.
m_absent_tab_again() {
  perl -0pi -e 's/    \/\/ The tab exists either way\. A point layer.s user checked "Draw this\n    \/\/ layer", saw nothing in 3D, and was told nothing; an absent tab is the\n    \/\/ same silence with better manners\. The explanation is the control\.\n    if \(!scene\)\n    \{/    if (!scene)\n    \{\n      delete page;\n      return nullptr;\n    \}\n    if (false)\n    \{/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 2. The 3D toggle never applies.
m_toggle_never_applies() {
  perl -0pi -e 's/    if \(m_shownIn3dCheck\)\n    \{\n      m_layer->setShownIn3D\(m_shownIn3dCheck->isChecked\(\)\);\n    \}\n\n    if \(ISceneSource \*scene = sceneSource\(\); scene && m_drapeCombo\)/    if (ISceneSource *scene = sceneSource(); scene \&\& m_drapeCombo)/' src/ui/dialogs/layerpropertiesdialog.cpp
}

# 3. The mesh advertises extrusion again, so the dialog offers the dead
#    Extruded row and the height spin that does nothing.
m_mesh_extrudes_again() {
  perl -0pi -e 's/      \[\[nodiscard\]\] bool supportsExtrusion\(\) const override\n      \{\n        return false;\n      \}/      [[nodiscard]] bool supportsExtrusion() const override\n      \{\n        return true;\n      \}/' include/layers/meshlayer.h
}

# 4. The scene controls go back to the Rendering tab.
m_controls_on_rendering() {
  perl -0pi -e 's/    return page;\n  \}\n\n  QWidget \*LayerPropertiesDialog::buildSceneTab\(\)\n  \{\n    auto \*page = new QWidget\(this\);\n    page->setObjectName\(QStringLiteral\("sceneTab"\)\);\n\n    auto \*form = new QFormLayout\(page\);\n\n    ISceneSource \*scene = sceneSource\(\);/    ISceneSource *scene = sceneSource();\n    \{\n    QWidget *scenePage = page;\n    (void)scenePage;\n    \}\n    if (true) \{/' src/ui/dialogs/layerpropertiesdialog.cpp
}

run_mutation 1-absent-tab-again      m_absent_tab_again
run_mutation 2-toggle-never-applies  m_toggle_never_applies
run_mutation 3-mesh-extrudes-again   m_mesh_extrudes_again

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
