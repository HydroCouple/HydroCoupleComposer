@page composer_guide_diagnostics 6. Resolve common problems

### A saved component type cannot be found
Inspect the instance’s `info.component_info_id` in the composition. Load the library that exports that type. For a headless run, pass the directory containing the built plugins with `--components`.

### A library is found but rejected
Read its loader diagnostic. Composer checks an ABI stamp before calling into a component library. If it reports a mismatch, rebuild the plugin against the compatible interfaces, SDK and toolchain. [Plugin ABI contract](https://github.com/HydroCouple/HydroCoupleComposer/blob/v2/include/plugins/componentabi.h)

### A port is missing
Review the instance’s arguments and initialization diagnostics. For interval transfers, both ends must advertise the same configured species IDs. Then check the saved output and input IDs against the initialized ports.

### A run opens but a series cannot be read
Locate the variable’s artifact in the manifest, verify the path from the manifest directory and confirm that your build includes a reader for that format. For a missing time plot, inspect the variable’s dimensions and time metadata.
