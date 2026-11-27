@page composer_guide_project 1. Open a composition and load its libraries

Use a Composer v2 build with compatible HydroCouple 2.0 component libraries. For a source build, install the matching interfaces and SDK first, then build the Qt desktop host. Build the model plugins against the same interface and SDK versions with a compatible C++ toolchain. [Composer v2 build instructions](https://github.com/HydroCouple/HydroCoupleComposer/blob/v2/docs/BUILDING_V2.md) · [Custom component libraries](https://github.com/HydroCouple/HydroCoupleComposer/blob/v2/docs/CUSTOM_COMPONENTS.md)

Open a composition JSON file to edit an existing project, or start with an empty composition. Load the libraries that provide the model types you need. The component library lists types you can instantiate. The composition canvas shows the particular instances participating in this run. For example, two catchments may share one library and have separate parameters, connections and instance IDs.

### Composition JSON
Stores model instance IDs, component types, arguments, connections and run settings. A machine reproducing the simulation needs access to the referenced inputs and compatible component libraries.

### Composer sidecar
Stores presentation information such as the layout of the canvas. For a composition named `river.json`, Composer uses `river.composer.json` for the companion file.

### Run manifest
Describes the completed run and locates its recorded artifacts. Open this file to review an earlier run. Keep the result files at the paths recorded in it.
