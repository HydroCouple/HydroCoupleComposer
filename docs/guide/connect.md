@page composer_guide_connect 2. Configure and connect model instances

1.  **Add the instances.** Choose the required types from the library and add them to the composition. Use a distinct instance ID for each model. Saved connections and recorded variables use that ID.
2.  **Set arguments.** Select an instance and enter its required parameters or input paths. Initialize it so it can advertise its inputs and outputs. Some components derive ports from configuration, such as one output for each configured chemical species.
3.  **Connect an output to an input.** Inspect the value kind, units, dimensions and support of both endpoints. An interval total in cubic metres and a rate in cubic metres per second require an explicit time conversion. If a compatible adapter is available, configure it and check its output against the receiving port.
4.  **Validate and run.** Check required arguments, component paths and unresolved connections. Read the diagnostic for the named instance or port, correct it, and run the composition. Save the composition so the same setup can be run again from the command line.

[Composer workspaces and architecture](https://github.com/HydroCouple/HydroCoupleComposer/blob/v2/docs/mainpage.md)
