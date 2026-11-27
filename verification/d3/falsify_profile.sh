#!/bin/bash
# D3c falsification: the water column under a picked face.
#
# Every mutation below still draws a profile -- a line with the right number
# of points, over a plausible range. The gates compare against the mesh the
# test built: which column, which layer order, and what height each value
# actually sits at.

set -u
cd "$(dirname "$0")/../.." || exit 1

BUILD=build/darwin
TEST=$BUILD/tests/gui/test_layeredmesh
LOG=verification/d3/falsify_profile.log
: > "$LOG"

FILES=(src/layers/meshlayer.cpp src/ui/panels/profileplotpanel.cpp)

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

  if ! cmake --build $BUILD --target test_layeredmesh -j 8 >> "$LOG" 2>&1; then
    echo "MUTATION $name: BUILD FAILED (inconclusive)" | tee -a "$LOG"
    return
  fi

  local out_file="$LOG.run"
  QT_QPA_PLATFORM=offscreen $TEST > "$out_file" 2>&1 &
  local pid=$!
  local waited=0

  while kill -0 $pid 2>/dev/null && [ $waited -lt 240 ]; do
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
    echo "MUTATION $name: CAUGHT -- $(echo "$out" | grep '^\[  FAILED  \] LayeredMesh' | head -2 | sed 's/\[  FAILED  \] //' | tr '\n' ' ')" | tee -a "$LOG"
  elif [ "$status" -ne 0 ]; then
    echo "MUTATION $name: CAUGHT (the suite died, status $status)" | tee -a "$LOG"
  else
    echo "MUTATION $name: *** SURVIVED ***" | tee -a "$LOG"
  fi
}

# 1. Every profile is read from column zero, so several selected columns plot
#    as several copies of one.
m_always_first_column() {
  perl -0pi -e 's/      values\.append\(m_cellValues\.at\(\n        static_cast<int>\(m_layering\.cell\(column, layer\)\)\)\);/      values.append(m_cellValues.at(static_cast<int>(m_layering.cell(0, layer))));/' src/layers/meshlayer.cpp
}

# 2. The cell index is transposed -- layer-major rather than column-major --
#    which reads a real value from the wrong cell of the same mesh.
m_transposed_cell_index() {
  perl -0pi -e 's/        static_cast<int>\(m_layering\.cell\(column, layer\)\)\)\);/        static_cast<int>(layer * m_layering.columnCount() + column)));/' src/layers/meshlayer.cpp
}

# 3. Values are plotted at an interface rather than at the layer's center, so
#    each one sits on a boundary it shares with the cell above.
m_elevation_at_interface() {
  perl -0pi -e 's/      elevations\.append\(\n        \(m_layering\.z\(column, layer\) \+ m_layering\.z\(column, layer \+ 1\)\) \/ 2\.0\);/      elevations.append(m_layering.z(column, layer));/' src/layers/meshlayer.cpp
}

# 4. A layering carrying no values profiles as a column of zeros, which reads
#    as a model that computed them.
m_missing_values_are_zero() {
  perl -0pi -e 's/    if \(m_cellValues\.size\(\) != m_layering\.cellCount\(\)\)\n    \{.*?\n      return false;\n    \}/    if (false) { return false; }/s' src/layers/meshlayer.cpp
}

# 5. A column index outside the mesh is read anyway.
m_no_column_bounds() {
  perl -0pi -e 's/    if \(column < 0 \|\| column >= m_layering\.columnCount\(\)\)\n    \{\n      message = QObject::tr\("There is no column %1 to profile\."\)\.arg\(column\);\n      return false;\n    \}//' src/layers/meshlayer.cpp
}

# 6. A flat mesh is profiled from whatever the layering happens to hold.
m_flat_is_profiled() {
  perl -0pi -e 's/    if \(!isLayered\(\)\)\n    \{\n      message = QObject::tr\("..%1.. has no vertical layering to profile\."\)\n                  \.arg\(name\(\)\);\n      return false;\n    \}//' src/layers/meshlayer.cpp
}

# 7. A selection of edges is profiled as though it named faces, so the column
#    drawn is not the one picked.
m_edges_profiled_as_faces() {
  perl -0pi -e 's/      if \(candidate->entity\(\) != MeshEntity::Face\)/      if (false)/' src/ui/panels/profileplotpanel.cpp
}

# 8. Value and elevation are plotted the other way round, so a profile takes
#    a time series' shape and reads as one.
m_axes_swapped() {
  perl -0pi -e 's/        line->append\(values\.at\(layer\), elevations\.at\(layer\)\);/        line->append(elevations.at(layer), values.at(layer));/' src/ui/panels/profileplotpanel.cpp
}

# 9. The exported profile drops its elevations, so a written column is a list
#    of values with no heights against them.
m_profile_without_elevations() {
  perl -0pi -e 's/        record\.julianDays\.append\(elevations\.at\(layer\)\);/        record.julianDays.append(layer);/' src/ui/panels/profileplotpanel.cpp
}

# 10. The panel keeps its last profile when the selection is cleared.
m_stale_profile() {
  perl -0pi -e 's/    m_chart->removeAllSeries\(\);\n    m_profiles\.clear\(\);//' src/ui/panels/profileplotpanel.cpp
}

# 11. The value axis is labeled generically, throwing away the name the
#     values were attached under.
m_generic_value_axis() {
  perl -0pi -e 's/      m_valueAxis->setTitleText\(source->valueAttribute\(\)\);/      m_valueAxis->setTitleText(QStringLiteral("Value"));/' src/ui/panels/profileplotpanel.cpp
}

run_mutation "1-every-profile-is-column-zero" m_always_first_column
run_mutation "2-cell-index-transposed"        m_transposed_cell_index
run_mutation "3-value-sits-at-an-interface"   m_elevation_at_interface
run_mutation "4-missing-values-read-as-zero"  m_missing_values_are_zero
run_mutation "5-column-bounds-unchecked"      m_no_column_bounds
run_mutation "6-flat-mesh-is-profiled"        m_flat_is_profiled
run_mutation "7-edges-profiled-as-faces"      m_edges_profiled_as_faces
run_mutation "8-profile-axes-swapped"         m_axes_swapped
run_mutation "9-profile-loses-its-elevations" m_profile_without_elevations
run_mutation "10-cleared-selection-keeps-it"  m_stale_profile
run_mutation "11-value-axis-is-generic"       m_generic_value_axis

restore
touch "${FILES[@]}"
cmake --build $BUILD --target test_layeredmesh -j 8 >> "$LOG" 2>&1
echo
echo "--- restored, clean run ---"
QT_QPA_PLATFORM=offscreen $TEST 2>&1 | tail -3
