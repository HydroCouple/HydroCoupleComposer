#!/bin/bash
# C4b falsification run: the ray, the ground it lands on, and the pick.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
LOG=verification/c4/falsify_scenepick.log
: > "$LOG"

FILES=(src/scene/camera.cpp src/pick/terrainray.cpp src/scene/sceneview.cpp)

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
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_picking test_camera -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  # Judged by exit status as well as output: a mutation that makes a suite
  # *die* prints no "[  FAILED  ]" line, and a harness that only greps for one
  # reads the crash as a mutation that survived.
  local out=""
  local status=0

  for binary in test_picking test_camera; do
    local part
    part=$(QT_QPA_PLATFORM=offscreen $BUILD/tests/gui/$binary 2>&1)
    local part_status=$?
    out="$out$part"

    if [ "$part_status" -ne 0 ]; then
      status=$part_status
    fi
  done

  echo "=== $name ===" >> "$LOG"
  echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep -E '^\[  FAILED  \] (Pick|Camera)' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (a suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The unprojection skips the perspective divide.
m_no_divide() {
  perl -0pi -e 's/      return qFuzzyIsNull\(clip\.w\(\)\) \? clip\.toVector3D\(\)\n                                    : clip\.toVector3D\(\) \/ clip\.w\(\);/      return clip.toVector3D();/' src/scene/camera.cpp
}

# 2. Normalized device y is not flipped against widget y.
m_no_y_flip() {
  perl -0pi -e 's/    const float ndcY =\n      float\(1\.0 - 2\.0 \* pixel\.y\(\) \/ double\(viewport\.height\(\)\)\);/    const float ndcY =\n      float(2.0 * pixel.y() \/ double(viewport.height()) - 1.0);/' src/scene/camera.cpp
}

# 3. The ray points back at the eye.
m_reversed_ray() {
  perl -0pi -e 's/    const QVector3D along = farPoint - nearPoint;/    const QVector3D along = nearPoint - farPoint;/' src/scene/camera.cpp
}

# 4. The model matrix is left in, so exaggeration drags the ray sideways.
m_model_left_in() {
  perl -0pi -e 's/      modelViewProjection\(aspect\)\.inverted\(&invertible\);/      (modelViewProjection(aspect) * modelMatrix()).inverted(\&invertible);/' src/scene/camera.cpp
}

# 5. The terrain is ignored; every ray meets the ground plane.
m_ignore_terrain() {
  perl -0pi -e 's/    if \(!terrain\)\n    \{/    if (true)\n    {/' src/pick/terrainray.cpp
}

# 6. The crossing is accepted at the march's own resolution.
m_no_bisection() {
  perl -0pi -e 's/          for \(int halving = 0; halving < kBisections; \+\+halving\)/          for (int halving = 0; halving < 0; ++halving)/' src/pick/terrainray.cpp
}

# 7. The step comes from the terrain alone, however coarse.
m_terrain_step_only() {
  perl -0pi -e 's/      std::clamp\(terrain->terrainResolution\(\) \* 0\.5,\n                 reach \/ double\(kMaxSteps\), reach \/ double\(kMinSteps\)\);/      std::max(terrain->terrainResolution() * 0.5, reach \/ double(kMaxSteps));/' src/pick/terrainray.cpp
}

# 8. An eye under the terrain is answered with the ground anyway.
m_underground_answers() {
  perl -0pi -e 's/        else if \(haveSample \|\| i == 0\)/        else if (false)/' src/pick/terrainray.cpp
}

# 9. The click tolerance is zero, so only an exact hit counts.
m_no_tolerance() {
  perl -0pi -e 's/    const double tolerance =\n      groundUnder\(pixel \+ QPoint\(int\(kPickRadiusPixels\), 0\), offset\)\n        \? std::hypot\(offset\.x\(\) - ground\.x\(\), offset\.y\(\) - ground\.y\(\)\)\n        : 0\.0;/    const double tolerance = 0.0;\n    Q_UNUSED(offset)/' src/scene/sceneview.cpp
}

# 10. The scene walks the stack bottom-first.
m_scene_bottom_first() {
  perl -0pi -e 's/    for \(MapLayer \*layer : stack->layers\(\)\)/    for (MapLayer *layer : stack->renderOrder())/' src/scene/sceneview.cpp
}

# 11. A hidden layer is pickable in the scene.
m_scene_pick_hidden() {
  perl -0pi -e 's/    for \(MapLayer \*layer : stack->layers\(\)\)\n    \{\n      if \(!layer->isVisible\(\)\)/    for (MapLayer *layer : stack->layers())\n    {\n      if (false)/' src/scene/sceneview.cpp
}

# 12. An orbit selects whatever it started on.
m_orbit_selects() {
  perl -0pi -e 's/    if \(wasOrbiting && event->button\(\) == Qt::LeftButton &&\n        std::abs\(travelled\.x\(\)\) <= kClickSlopPixels &&\n        std::abs\(travelled\.y\(\)\) <= kClickSlopPixels\)/    if (wasOrbiting \&\& event->button() == Qt::LeftButton)/' src/scene/sceneview.cpp
}

# 13. Picking in the scene does not touch the selection.
m_scene_no_select() {
  perl -0pi -e 's/        stack->selectOnly\(layer, feature\);/        ;/' src/scene/sceneview.cpp
}

run_mutation "1-no-perspective-divide"  m_no_divide
run_mutation "2-ndc-y-not-flipped"      m_no_y_flip
run_mutation "3-ray-reversed"           m_reversed_ray
run_mutation "4-model-matrix-left-in"   m_model_left_in
run_mutation "5-terrain-ignored"        m_ignore_terrain
run_mutation "6-no-bisection"           m_no_bisection
run_mutation "7-terrain-step-only"      m_terrain_step_only
run_mutation "8-underground-answers"    m_underground_answers
run_mutation "9-zero-tolerance"         m_no_tolerance
run_mutation "10-scene-bottom-up"       m_scene_bottom_first
run_mutation "11-scene-picks-hidden"    m_scene_pick_hidden
run_mutation "12-orbit-selects"         m_orbit_selects
run_mutation "13-scene-does-not-select" m_scene_no_select

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_picking test_camera -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $BUILD/tests/gui/test_picking 2>&1 | tail -3
QT_QPA_PLATFORM=offscreen $BUILD/tests/gui/test_camera 2>&1 | tail -3
