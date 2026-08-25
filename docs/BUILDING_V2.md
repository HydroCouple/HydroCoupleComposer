# Building HydroCoupleComposer 2.0

Composer 2.0 is built with CMake + vcpkg and Qt 6, following the conventions of
[openswmm.gui](https://github.com/HydroCouple/openswmm.gui). The HydroCouple
interfaces and the HydroCouple SDK are consumed as **prebuilt installed
packages** via `find_package` — build and install them first, exactly as
openswmm.gui consumes a prebuilt openswmm.engine.

> The legacy qmake project (`HydroCoupleComposer.pro`, Qt 5, HydroCouple v1)
> still sits in the tree on this branch and is **not** part of this build. It
> is retired once the v2 tree reaches feature parity (plan item A1).

## Prerequisites

| Requirement | Notes |
|---|---|
| CMake ≥ 3.21 | `cmake --version` |
| Ninja | Generator used by every preset |
| Qt 6.5+ | Modules: Widgets, OpenGL(Widgets), Network, Concurrent, Svg, Charts, Quick, QuickWidgets, Qml, PrintSupport, Xml, **ShaderTools** |
| vcpkg | `VCPKG_ROOT` exported; the manifest pins baseline `9e593bb…`, matching openswmm.engine, HydroCoupleSDK, and FVQual |
| A C++20 compiler | AppleClang 15+, GCC 12+, MSVC 19.3+ |

Export Qt's location so presets and plain configures both find it:

```bash
export QT_ROOT_DIR="$HOME/Qt/6.9.3/macos"   # adjust to your kit
export VCPKG_ROOT="$HOME/path/to/vcpkg"
```

If vcpkg reports `no version database entry for …`, its checkout predates the
pinned baseline. Fast-forward it and re-bootstrap the tool (the tool binary
must be new enough for the registry's schema):

```bash
git -C "$VCPKG_ROOT" fetch origin
git -C "$VCPKG_ROOT" checkout 9e593bb18ea69cc5095e012465dcd675a822ed0d
"$VCPKG_ROOT"/bootstrap-vcpkg.sh
```

## 1. Build and install the dependencies

Both installs use an **absolute** `--prefix`; a relative one silently installs
into the build directory.

```bash
# HydroCouple — header-only interface definitions
cd ../HydroCouple
cmake -S . -B build/darwin -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/Darwin" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/darwin --target install

# HydroCoupleSDK — implementation, IO writers/readers, meshing tools
cd ../HydroCoupleSDK
cmake -S . -B build/darwin -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/Darwin" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_MANIFEST_FEATURES="tools;netcdf;hdf5;geopackage" \
  -DHydroCouple_DIR="$PWD/../HydroCouple/install/Darwin/lib/cmake/HydroCouple" \
  -DBUILD_TOOLS=ON -DUSE_NETCDF=ON -DUSE_HDF5=ON -DUSE_GEOPACKAGE=ON
cmake --build build/darwin --target install
```

> **Two switches, not one.** `VCPKG_MANIFEST_FEATURES` only decides which
> *dependencies* vcpkg installs; the SDK's own `BUILD_TOOLS` / `USE_*` options
> default to `OFF` and must be set as well. Setting only the former yields a
> core-only SDK that silently lacks the results readers (Phase D) and the
> meshing tools (Phase E).

Composer searches `../HydroCouple/install/<system>` and
`../HydroCoupleSDK/install/<system>` first, so the layout above needs no
further configuration. Override with `-DHydroCouple_DIR=` /
`-DHydroCoupleSDK_DIR=` when installing elsewhere.

## 2. Configure, build, and test Composer

```bash
cmake --preset Darwin          # or Linux / Windows / *-debug
cmake --build --preset Darwin
ctest --preset Darwin
```

The GUI suite runs headless under `QT_QPA_PLATFORM=offscreen`, which the test
properties set automatically.

## Troubleshooting

- **`SIGKILL` when launching tests on macOS** — a stale ad-hoc signature on a
  bundled dependency. Re-sign it: `codesign --force --sign - <dylib>`.
- **A changed test behaves strangely** — partial `--target` builds leave other
  test binaries stale. Do a full build before trusting any verdict.
- **Missing vtable / unresolved `moc` symbols** — a `Q_OBJECT` header that is
  not listed in the target's sources. AUTOMOC skips headers it is not given;
  add the header to `COMPOSER_SOURCES`.
