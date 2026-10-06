#!/bin/bash
# Falsification: one tool at a time, and exaggeration behaves like a 3D tool.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_viewhandoff
LOG=verification/v3/falsify_toolgroup.log
: > "$LOG"

FILES=(src/ui/composermainwindow.cpp src/scene/sceneview.cpp)

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

  if ! cmake --build $BUILD --target test_viewhandoff -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"; return
  fi

  local out; out=$(timeout 300 $TEST 2>&1); local status=$?
  echo "=== $name ===" >> "$LOG"; echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if [ "$status" -eq 124 ]; then
    echo "MUTATION $name: CAUGHT (hung)" | tee -a "$LOG"
  elif echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] ViewHandoffTest' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Give the scene tools their own group again -- the two-lit-selects bug.
m_two_groups_again() {
  perl -0pi -e 's/    for \(QAction \*tool : \{m_orbitToolAction, m_sceneSelectToolAction,\n                          m_sceneZoomInToolAction, m_sceneZoomOutToolAction\}\)\n    \{\n      toolGroup->addAction\(tool\);/    auto *sceneToolGroup = new QActionGroup(this);\n    sceneToolGroup->setExclusive(true);\n    for (QAction *tool : \{m_orbitToolAction, m_sceneSelectToolAction,\n                          m_sceneZoomInToolAction, m_sceneZoomOutToolAction\})\n    \{\n      sceneToolGroup->addAction(tool);/' src/ui/composermainwindow.cpp
}

# 2. Exaggeration acts on the hidden view.
m_exaggeration_hidden() {
  perl -0pi -e 's/              m_sceneView->setVerticalExaggeration\(factor\);\n\n              \/\/ Brought forward like every tool on this strip: stretching\n              \/\/ relief nobody can see is the spin equivalent of zooming a\n              \/\/ hidden canvas\.\n              m_workspace->setCurrentWidget\(m_sceneView\);/              m_sceneView->setVerticalExaggeration(factor);/' src/ui/composermainwindow.cpp
}

# 3. The old reframe: exaggeration discards the framing the map handed over.
m_exaggeration_reframes() {
  perl -0pi -e 's/    update\(\);\n    Q_EMIT cameraChanged\(\);\n  \}\n\n  double SceneView::verticalExaggeration/    zoomToFullExtent();\n  }\n\n  double SceneView::verticalExaggeration/' src/scene/sceneview.cpp
}

run_mutation 1-two-groups-again      m_two_groups_again
run_mutation 2-exaggeration-hidden   m_exaggeration_hidden
run_mutation 3-exaggeration-reframes m_exaggeration_reframes

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
