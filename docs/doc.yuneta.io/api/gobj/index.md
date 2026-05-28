# GObj API

The core GObject API: registering gclasses, creating instances, reading and
writing attributes, sending and publishing events, walking the runtime tree,
and the resource/authz/info/stats interfaces. Every page lists the functions
in its area with signatures and parameters.

## In this section

- [Startup](startup.md) — `gobj_start_up()` / `gobj_end()` and process-wide
  initialisation.
- [GClass](gclass.md) — defining and registering a gclass.
- [Creation](creation.md) — `gobj_create*()` and destruction.
- [Attributes](attrs.md) — reading and writing typed attributes.
- [Operations](op.md) — start/stop/play/pause and tree operations.
- [Events & State](events_state.md) — sending events and inspecting the FSM.
- [Publish / Subscribe](publish.md) — the event pub/sub system.
- [Resource](resource.md) — the resource interface.
- [Authorization](authz.md) — permission checks.
- [Info](info.md) — informational queries.
- [Stats](stats.md) — statistics counters.
- [Node](node.md) — treedb node operations.
