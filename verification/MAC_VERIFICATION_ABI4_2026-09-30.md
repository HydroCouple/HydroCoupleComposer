# Mac verification of the Composer's ABI 4 port — 2026-09-30

**Answers:** `HydroCouple/plans/composer/COMPOSER_ABI4_HANDOFF_2026-09-30.md` ·
**Tree:** HydroCoupleComposer `v2` at `fbd9384` plus the seven ported files
(uncommitted) · **Against:** HydroCouple `98bfbaa` and HydroCoupleSDK
`2b960aa`, both installed fresh · **Machine:** Darwin 25.6.0, Apple silicon,
Apple clang, macOS SDK 27, Qt 6.9.3, Ninja.

## Results

| # | Check | Result |
|---|---|---|
| 1 | Build against ABI 4 | **424/424, 0 errors.** No new warnings from the seven files. The two that appear in them (`spatialstubs.h:285` `-Wreorder-ctor`, `test_dataitemlayers.cpp:491` `-Wunused-result`) are on lines the port did not touch and are identical at HEAD. |
| 2 | `test_dataitemlayers` / `test_difference` | **31/31** (after the fixture fix below; 30/31 before) / **13/13**. The three surface cases (node/edge/face) pass. |
| 3 | grep `MeshDataObjectType\|patchDimension\|vertexDimension()\|edgeDimension()` | **0 hits.** The `m_edgeDimension`/`cellEdgeDimension` grid members remain (4). |
| 4 | EPSG:5498 through `SpatialReference::fromAuthority` (optional) | **Passes.** Vertical authority `EPSG`, SRID **5703**, `Meters`, a 203-character WKT. EPSG:4326 gives empty, 0, empty and `Unknown`. Checked with a temporary test, since removed. |
| — | Full suite (`ctest -j1`, offscreen) | **64/65.** The one failure, `test_viewhandoff`, predates this port (below). |

## Fix: `tests/gui/test_dataitemlayers.cpp`, `AnEdgeItemOnASurfaceWithNoEdgesIsRefused`

**Cause.** The test builds "a surface with no edges" by clearing only the
declared `edgeNodes` of `twoTriangleMesh()`. Since the SDK's ABI 4 port
(`2b960aa`, fix 4), `MeshViewAdapter` derives an edge from every face loop
and `edgeCount()` counts them. That mesh therefore still has the triangles'
5 edges, and the Composer correctly drew the item on them.

**Fix.** Also clear `faceNodes` and `faceNodeOffsets`, so the surface has no
edges at all. The variable was already named `faceless`.

**Effect.** 31/31. The gate was seen to fail: with the caller planted to
ignore `appendMeshEdges`' refusal, the test fails. A first plant that only
removed the `edgeCount() <= 0` guard was still refused, by the function's
`return !features.isEmpty()`, so the refusal has two layers.

## `test_viewhandoff` fails before the port too (3 of 36)

`TheMapArrivesLookingAtWhatTheSceneShowed` (relative difference 0.15205),
`ARoundTripThroughTheUiPreservesTheExtent` (0.09218) and
`ATiltedRoundTripKeepsEverythingItShowed` fail. **A/B:** the Composer at
`fbd9384` (pre-port), built in a worktree against the **ABI 2** installs,
fails the same three tests **with the same numbers to every digit**. The
test also never reaches ported code: it hands `MeshLayer::create` a
`MeshDefinition`, and the port's `meshlayer.cpp` change is in
`meshFromGrid`, where it only widens integers. So this is not ABI 4 and not
the port. It is open, with the likely suspects being the offscreen
platform's geometry or the newer macOS SDK. `fbd9384` is the newest
commit and changes this behaviour.

## Environment repairs this needed (no source changes)

- **Installs.** The Composer consumes *installed* packages, and both were
  ABI 2 (`HydroCouple/install/Darwin`, `HydroCoupleSDK/install/Darwin`).
  They were moved aside to the session scratchpad and installed fresh:
  - **The interface** from a clean `98bfbaa` worktree. The live tree carries
    a peer's uncommitted `hydrocouplecomponentabi.h`.
  - **The SDK** from a new static-triplet, full-feature Release tree,
    `HydroCoupleSDK/build-static-full` (358 passed, 1 skipped). An install
    from `build-macos-full` was rejected: it is the dynamic triplet, so the
    installed `libHydroCoupleSDK.dylib` needed eight vcpkg dylibs that
    `cmake --install` does not copy (`libgdal`, `libnetcdf`, `libhdf5`, …,
    even the debug `libyaml-cppd`). The Composer's tests then failed to load.
    The static install is self-contained.
  - **Peer header.** The SDK install copies `hydrocouple*.h` from the
    sibling tree, so the peer header was removed from the install.
- **Composer build tree.** `build/darwin` pins the macOS 26.5 SDK, which a
  later Xcode removed (`Qt6::PrintSupport` includes a non-existent path;
  CUPS and OpenGL are cached the same way). Left untouched. A fresh tree,
  `build/darwin-abi4`, was configured from the `Darwin` preset.
