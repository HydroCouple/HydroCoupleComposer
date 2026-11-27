@page composer_guide_results 4. Review a recorded run

To try results review with existing data, extract the [recorded run linked from the SDK worked example](../HydroCoupleSDK/guide/example.html). In the Results workspace, open `run.json` from the extracted folder. Composer reads the stored values through the SDK and can display them without loading either original model library.

1.  Expand the catchment in the run browser and select `runoff`. The example contains 24 time samples at three reaches. Select a reach to inspect its series.
2.  Select the channel’s `flow` at the same reach to examine its response. Both variables are unitless synthetic values. For a physical model, check units, timestamps and spatial support before comparing two series.
3.  Use map or 3D views for variables with spatial support. The serial example indexes reaches numerically. Use the series view to inspect those values.

[Run session and recorded variables](https://github.com/HydroCouple/HydroCoupleComposer/blob/v2/include/results/runsession.h)
