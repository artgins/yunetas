# stress/c

Stress-test programs for the C kernel. These push specific subsystems with aggressive workloads to surface resource leaks, race conditions and long-run instability. They are not run as part of `yunetas test`. Run them by hand.

| Directory | What it stresses |
|---|---|
| `listen` | Many concurrent TCP listeners / accepts through the `yev_loop` |

Run the binaries directly from `$YUNETAS_OUTPUTS/bin/` and observe with `top`, `/proc/<pid>/status` or the built-in memory tracker (`CONFIG_DEBUG_TRACK_MEMORY`).
