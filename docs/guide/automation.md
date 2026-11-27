@page composer_guide_automation 5. Monitor a command-line run

Pass `--json-progress` for machine-readable events. Read only lines starting with `HYDROCOUPLE_EVENT` as JSON events. Libraries may write ordinary log messages to the same stream.

```shell
HydroCoupleComposer --run interval-transfer.json --components /path/to/plugins --json-progress
```

The `hydrocouple.run-events/1` stream includes a run ID and sequence number. Use `progress` events to follow completed workflow steps and component clocks. Check the terminal `finished` event for success, diagnostics and the manifest path when one was written. Check the process exit status as well. [Command-line event contract](https://github.com/HydroCouple/HydroCoupleComposer/blob/v2/README.md)
