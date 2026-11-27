# Custom model components

Composer loads stamped HydroCouple model-component libraries and configures
instances through their ordinary arguments. The tested CF2 reference source and
receiver live in `HydroCoupleComponents/components/intervalreceiver`; its
`docs/INTERVAL_TRANSFER_COMPONENTS.md` describes the complete typed-port profile,
acceptance semantics and developer checklist. Open the supplied
`compositions/interval-transfer.json` to inspect and run the example.

## Assigning instance identity

Component type IDs identify a library definition. A composition's component `id`
identifies one instance of that definition and must remain distinct across the
graph, output records and transfer IDs. Both edit-time discovery and run-time
creation use `ComponentRegistry::assignInstanceIdentity` before arguments are
applied and before initialization.

For SDK-free components, expose `hydrocouple.instance_id` as a rank-one scalar
String configuration argument (shape `[1]`, role Configuration). Its String
initializer accepts the host's requested ID and updates the component's `id()`.
Accept JSON `{ "values": ["requested-id"] }` for normal argument round trips.
Reject empty IDs, conflicting reassignment and assignment after initialization.
The host verifies the published ID. A conflicting saved argument is rejected
before physical advancement. This convention uses the existing ABI and does
not require linking a component to HydroCoupleSDK.

SDK components retain `AbstractModelComponent::setInstanceId`. A component with
neither mechanism can be used only when its published factory identity already
matches the document. Other configurations receive an explicit diagnostic.

## Configured port metadata

Apply arguments, then initialize an edit-time instance to discover its actual
inputs and outputs. This must not prepare a physical simulation, advance time or
write result files. The existing ComponentInstances cache recreates an instance
after its argument block changes; panels query that same model. Run-time
instances are separate. A missing configuration produces a visible failure and
can be retried after the document is edited.

The reference receiver configures its species list into typed scalar amount
ports. Connections use stable field/species IDs and declared units, so species
order can differ. This initial profile requires the same species set, one direct
donor, and explicit matching fields. It rejects unsupported mappings; it does
not establish general mesh remapping, arbitrary unit conversion or adaptive
coupling. Those remain component-program deliverables.

## Running and testing

Build compatible component libraries and add their folder to Composer's search
paths. GUI and headless hosts search installed locations, then
`HYDROCOUPLE_COMPONENT_PATH`, then caller/preferences directories. Paths are canonicalized and deduplicated; refreshing keeps already-loaded
libraries alive. The loader validates the shared interface/toolchain ABI stamp.
A component library must outlive all its instances and factories.

For cross-repository acceptance, configure these optional CMake file paths:

- `COMPOSER_INTERVAL_SOURCE_LIBRARY`: the built `hcc_intervalsource` module.
- `COMPOSER_INTERVAL_RECEIVER_LIBRARY`: the built `hcc_intervalreceiver` module.
- `COMPOSER_OPENSWMM_COMPONENT_LIBRARY`: the SDK-free `openswmm_model_component` module.

With all three supplied, `test_customcomponents` exercises actual library
loading, portable identities, configured metadata and the application's
SimulationManager. Test compositions and receipts are retained under
`tests/artifacts/customcomponents`. The component collection also has direct
conservation, invalid-input, replay and buffer-contract tests.

This is the first CF2 reference package. Native library-backed hosting, general
OpenSWMM configured metadata, split execution, the full GUI authoring route and
coordinated restart retain their separate completion gates. Additional component
outputs are discoverable through their declared exchange items and recording
manifests; the engine's 1D MSX output uses ordinary pollutant columns in `.out`.
