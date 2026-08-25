# HydroCoupleComposer {#mainpage}

**HydroCoupleComposer** is the graphical host for
[HydroCouple 2.0](https://www.hydrocouple.org/HydroCouple/) model components:
it composes them, configures them, runs them, and visualises what they
produced.

Composer is deliberately *model-agnostic*. It knows the interfaces and the
[SDK](https://www.hydrocouple.org/HydroCoupleSDK/), never an individual model:
a stored run is reopened from its run manifest through
`HydroCouple::SDK::Component::ResultsModelComponent`, so results can be browsed
and plotted for a component whose library is not even installed.

## Architecture at a glance

| Area | Namespace / directory | Role |
|---|---|---|
| Application shell | `HydroCouple::Composer` — `core/`, `ui/` | Identity, settings, main window |
| Component loading | `plugins/` | `ComponentLibrary`, `ComponentRegistry` |
| Composition document | `project/` | Composition Spec v1 + presentation sidecar |
| Canvas & configurator | `canvas/`, `configurator/` | Graph editing, schema-driven argument editors |
| Map & rendering | `map/`, `render/`, `layers/` | CRS-aware 2D map and 3D scene over one layer stack |
| Results | `results/` | Run browser, themed animation, plots and slices |
| Meshing | `meshing/` | Domain editing and the SDK meshing tools |

## Two rules worth knowing before reading the code

**The Qt keywords are disabled project-wide.** `HydroCouple::ISignal` declares
`virtual void emit(Args...)`, and Qt's `emit` macro would rewrite that
declaration into something that cannot compile. Composer is where Qt and the
Qt-free interfaces meet, so it is built with `QT_NO_KEYWORDS` and uses
`Q_SIGNALS:`, `Q_SLOTS:` and `Q_EMIT` throughout.

**Component libraries are loaded by ABI stamp, not by trust.** A component
library exports two C entry points (see @ref componentabi.h). The loader reads
the pure-C stamp and compares it with its own *before* it calls any C++ across
the library boundary, because a C++ ABI mismatch cannot be recovered from once
it has been invoked.

## Building

See @ref BUILDING_V2.md for prerequisites, the dependency install order, and
the build traps worth knowing about.
