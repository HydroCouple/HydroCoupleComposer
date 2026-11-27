#!/bin/bash
# C3c-2 falsification run. Same contract as falsify.sh: each mutation breaks
# one property the suite claims, and a survivor means the property is not
# actually being checked.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_groundplane
LOG=verification/c3c/falsify_ground.log
: > "$LOG"

FILES=(src/scene/groundplane.cpp src/scene/scenerenderer.cpp
       src/layers/gdalrasterlayer.cpp src/layers/tilelayer.cpp
       resources/shaders/ground.vert resources/shaders/ground.frag)

backup() { for f in "${FILES[@]}"; do cp "$f" "$f.orig"; done; }
restore() {
  for f in "${FILES[@]}"; do
    cp "$f.orig" "$f"
    touch "$f"
  done
}
cleanup() { restore; for f in "${FILES[@]}"; do rm -f "$f.orig"; done; }
trap cleanup EXIT

backup

run_mutation() {
  local name="$1"; shift
  restore
  "$@" || { echo "MUTATION $name: could not be applied" | tee -a "$LOG"; return; }
  # A pattern that no longer matches applies nothing and reports nothing, so
  # the run reads as a surviving mutation when it is really a stale script.
  local changed=0
  for f in "${FILES[@]}"; do
    cmp -s "$f" "$f.orig" || changed=1
  done

  if [ "$changed" -eq 0 ]; then
    echo "MUTATION $name: NOT APPLIED (stale pattern) -- inconclusive" | tee -a "$LOG"
    return
  fi

  for f in "${FILES[@]}"; do touch "$f"; done

  if ! cmake --build $BUILD --target test_groundplane -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  # Bounded, and judged by the exit status as well as the output. A mutation
  # that makes the suite *die* -- a stack overflow from an echo between two
  # signals, say -- prints no "[  FAILED  ]" line at all, and a harness that
  # only greps for one reads the crash as a mutation that survived.
  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 120 ]; do
    sleep 1
    waited=$((waited + 1))
  done

  if kill -0 $pid 2>/dev/null; then
    kill -9 $pid 2>/dev/null
    wait $pid 2>/dev/null
    echo "MUTATION $name: CAUGHT (hung -- the suite never finished)" | tee -a "$LOG"
    return
  fi

  wait $pid
  local status=$?

  local out
  out=$(cat "$out_file")
  rm -f "$out_file"
  echo "=== $name ===" >> "$LOG"
  echo "$out" | grep '^\[  FAILED  \]' >> "$LOG"

  if echo "$out" | grep -q "\[  FAILED  \]"; then
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] Ground' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. The texture's V axis is flipped: north and south swap.
m_flip_v() {
  perl -0pi -e 's/    batch\.textureMap\[1\] = float\(box\.bottom\(\)\);/    batch.textureMap[1] = float(box.top());\n    batch.textureMap[3] = -batch.textureMap[3];/' src/scene/scenerenderer.cpp
}

# 2. The texture's axes are transposed: east and north swap.
m_transpose() {
  perl -0pi -e 's/    vTexCoord   = vec2\(\(position\.x - ubuf\.texMap\.x\) \* ubuf\.texMap\.z,\n                       \(ubuf\.texMap\.y - position\.y\) \* ubuf\.texMap\.w\);/    vTexCoord   = vec2((ubuf.texMap.y - position.y) * ubuf.texMap.w,\n                       (position.x - ubuf.texMap.x) * ubuf.texMap.z);/' resources/shaders/ground.vert
}

# 3. The texture is mapped from the origin rather than the extent's corner.
m_no_origin() {
  perl -0pi -e 's/    batch\.textureMap\[0\] = float\(box\.left\(\)\);/    batch.textureMap[0] = 0.0f;/' src/scene/scenerenderer.cpp
}

# 4. The extent asked for is used instead of the one the transform shows.
m_requested_extent() {
  perl -0pi -e 's/    ground\.extent = transform\.visibleExtent\(\);/    ground.extent = box;/' src/scene/groundplane.cpp
}

# 5. The image is square regardless of the extent's shape.
m_square_image() {
  perl -0pi -e 's/    const QSize size\(std::max\(1, int\(std::lround\(box\.width\(\) \* scale\)\)\),\n                     std::max\(1, int\(std::lround\(box\.height\(\) \* scale\)\)\)\);/    const QSize size(maximumPixels, maximumPixels);/' src/scene/groundplane.cpp
}

# 6. The ground plane is never subdivided, so it cannot follow relief.
m_no_subdivision() {
  perl -0pi -e 's/    const int columns = terrain \? divisionsFor\(box\.width\(\), step\) : 1;\n    const int rows = terrain \? divisionsFor\(box\.height\(\), step\) : 1;/    const int columns = 1;\n    const int rows = 1;/' src/scene/groundplane.cpp
}

# 7. Normals point straight up, so relief does not shade.
m_flat_normals() {
  perl -0pi -e 's/          QVector3D\(float\(-slopeX\), float\(-slopeY\), 1\.0f\), white\);/          QVector3D(0.0f, 0.0f, 1.0f), white);/' src/scene/groundplane.cpp
}

# 8. The ground plane keeps the terrain's depth, and loses to it.
m_no_ground_nudge() {
  perl -0pi -e 's/    gl_Position\.z -= ubuf\.params\.z \* gl_Position\.w;//' resources/shaders/ground.vert
}

# 9. No-data is painted opaque instead of left transparent.
m_opaque_nodata() {
  perl -0pi -e 's/    float alpha = texel\.a \* vColor\.a \* ubuf\.params\.x;/    float alpha = vColor.a * ubuf.params.x;/' resources/shaders/ground.frag
}

# 10. A basemap reports the planet as its bounds.
m_basemap_bounds() {
  perl -0pi -e 's/  Bounds3D TileLayer::sceneBounds\(\) const\n  \{\n    return \{\};\n  \}/  Bounds3D TileLayer::sceneBounds() const\n  {\n    Bounds3D bounds;\n    const QRectF box = extent().normalized();\n    bounds.expandTo(QVector3D(float(box.left()), float(box.top()), 0.0f));\n    bounds.expandTo(QVector3D(float(box.right()), float(box.bottom()), 0.0f));\n    return bounds;\n  }/' src/layers/tilelayer.cpp
}

# 11. A basemap textures its own extent -- the planet -- instead of the focus.
m_basemap_own_extent() {
  perl -0pi -e 's/      m_ground = renderLayerToImage\(const_cast<TileLayer &>\(\*this\), focus,\n                                    kGroundTexturePixels\);/      m_ground = renderLayerToImage(const_cast<TileLayer \&>(*this), extent(),\n                                    kGroundTexturePixels);/' src/layers/tilelayer.cpp
}

# 12. A raster textures the scene's focus rather than all of itself.
m_raster_focus() {
  perl -0pi -e 's/      m_ground = renderLayerToImage\(const_cast<GdalRasterLayer &>\(\*this\),\n                                    m_extent, kGroundTexturePixels\);/      m_ground = renderLayerToImage(const_cast<GdalRasterLayer \&>(*this),\n                                    context.focus, kGroundTexturePixels);/' src/layers/gdalrasterlayer.cpp
}

run_mutation "1-texture-v-flipped"      m_flip_v
run_mutation "2-texture-transposed"     m_transpose
run_mutation "3-texture-origin-dropped" m_no_origin
run_mutation "4-requested-not-shown"    m_requested_extent
run_mutation "5-square-texture"         m_square_image
run_mutation "6-no-subdivision"         m_no_subdivision
run_mutation "7-flat-normals"           m_flat_normals
run_mutation "8-no-ground-nudge"        m_no_ground_nudge
run_mutation "9-opaque-nodata"          m_opaque_nodata
run_mutation "10-basemap-frames-planet" m_basemap_bounds
run_mutation "11-basemap-own-extent"    m_basemap_own_extent
run_mutation "12-raster-uses-focus"     m_raster_focus

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_groundplane -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -4
