@page composer_guide_example 3. Check a conservative interval transfer

This two-component example checks whether water, heat and nitrogen reach a receiver unchanged over two unequal time intervals. Build the `hcc_intervalsource` and `hcc_intervalreceiver` plugins from HydroCoupleComponents and locate the directory containing both libraries. Copy the supplied composition into a writable directory and create an `out` folder next to it. [Example composition JSON](https://github.com/HydroCouple/HydroCoupleComponents/blob/main/compositions/interval-transfer.json) · [Source and receiver configuration](https://github.com/HydroCouple/HydroCoupleComponents/blob/main/docs/INTERVAL_TRANSFER_COMPONENTS.md)

Open the file in Composer and inspect the connections from `controlled-source` to `conservative-receiver`. Each transfers one field of the same interval, including the transfer ID, interval bounds and species masses. Leave the receiver’s `scale` at 1. Run from the desktop or issue the following command from the directory containing the JSON. Substitute the path to your plugin directory.

```shell
HydroCoupleComposer --run interval-transfer.json --components /path/to/plugins
```

**Expected deliveries with receiver scale 1**

| Interval | Water (m³) | Enthalpy (J) | NH₄–N (kg N) | NO₃–N (kg N) |
|----------|------------|--------------|--------------|--------------|
| 0–60 s   | 2          | 100          | 0.20         | 0.10         |
| 60–90 s  | 3          | 150          | 0.30         | 0.15         |
| Total    | 5          | 250          | 0.50         | 0.25         |


Inspect `out/interval-receipts.csv` to check what the receiver accepted. The quantities above are already integrated over their respective intervals, so sum them directly. The composition also requests the writer artifact `out/interval-results.csv` and the run manifest `out/interval-manifest.json`.
