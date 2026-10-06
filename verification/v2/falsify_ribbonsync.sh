#!/bin/bash
# Falsification: the ribbon and the workspace stay truthful to each other.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_viewhandoff
LOG=verification/v2/falsify_ribbonsync.log
: > "$LOG"

FILES=(src/ui/composermainwindow.cpp)

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

# 1. Never move the ribbon -- the pre-slice behaviour.
m_ribbon_never_follows() {
  perl -0pi -e 's/              if \(arriving == m_mapCanvas\)\n              \{\n                m_ribbon->setCurrentTab\(QStringLiteral\("map"\)\);\n              \}\n              else if \(arriving == m_sceneView\)\n              \{\n                m_ribbon->setCurrentTab\(QStringLiteral\("scene"\)\);\n              \}\n              else if \(arriving == m_canvas\)\n              \{\n                m_ribbon->setCurrentTab\(QStringLiteral\("home"\)\);\n              \}/              (void)arriving;/' src/ui/composermainwindow.cpp
}

# 2. Steal the palettes too -- retarget the ribbon whatever tab it shows, so
#    picking a mesh tool loses the mesh palette.
m_steal_palettes() {
  perl -0pi -e 's/              const QString current = m_ribbon->currentTab\(\);\n\n              if \(current != QLatin1String\("map"\) &&\n                  current != QLatin1String\("scene"\)\)\n              \{\n                return;\n              \}\n\n//' src/ui/composermainwindow.cpp
}

# 3. The ribbon's map tab raises nothing -- clicking it leaves the 3D view up.
m_ribbon_raises_nothing() {
  perl -0pi -e 's/              if \(id == QLatin1String\("map"\)\)\n              \{\n                m_workspace->setCurrentWidget\(m_mapCanvas\);\n              \}\n              else if \(id == QLatin1String\("scene"\)\)\n              \{\n                m_workspace->setCurrentWidget\(m_sceneView\);\n              \}/              (void)id;/' src/ui/composermainwindow.cpp
}

# 4. Every ribbon tab raises a view, so browsing the View palette yanks the
#    workspace to the map.
m_palettes_raise_views() {
  perl -0pi -e 's/              else if \(id == QLatin1String\("scene"\)\)\n              \{\n                m_workspace->setCurrentWidget\(m_sceneView\);\n              \}/              else if (id == QLatin1String("scene"))\n              \{\n                m_workspace->setCurrentWidget(m_sceneView);\n              \}\n              else\n              \{\n                m_workspace->setCurrentWidget(m_mapCanvas);\n              \}/' src/ui/composermainwindow.cpp
}

# 5. Cross the mappings: the scene view selects the ribbon's map tab. The
#    original mutation here removed a re-entrancy guard and SURVIVED; tracing
#    it showed both setters are no-ops when already current and the mappings
#    agree, so no exchange can ping-pong. The guard was removed from the code
#    (fifth unfalsifiable defence this program); what CAN go wrong is the
#    mapping itself, so that is what gets mutated.
m_crossed_mapping() {
  perl -0pi -e 's/              else if \(arriving == m_sceneView\)\n              \{\n                m_ribbon->setCurrentTab\(QStringLiteral\("scene"\)\);\n              \}/              else if (arriving == m_sceneView)\n              \{\n                m_ribbon->setCurrentTab(QStringLiteral("map"));\n              \}/' src/ui/composermainwindow.cpp
}

run_mutation 1-ribbon-never-follows   m_ribbon_never_follows
run_mutation 2-steal-palettes         m_steal_palettes
run_mutation 3-ribbon-raises-nothing  m_ribbon_raises_nothing
run_mutation 4-palettes-raise-views   m_palettes_raise_views
run_mutation 5-crossed-mapping        m_crossed_mapping

echo
echo "--- summary ---"
grep -E "MUTATION" "$LOG" | sed 's/^/  /'
