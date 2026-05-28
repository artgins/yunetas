# Runtime API

The runtime services that surround a running yuno: the entry point, the
process environment, the `C_YUNO` root gobj, timers, inter-event messaging,
the simple DB, command execution, and gclass registration.

## In this section

- [Entry Point](entry_point.md) — `yuneta_entry_point()` and process bring-up.
- [Environment](environment.md) — process-wide environment and paths.
- [Yuno](yuno.md) — the `C_YUNO` root object.
- [Timer](timer.md) — high- and low-level timer helpers.
- [Inter-Event Messaging](msg_ievent.md) — RPC-style messaging between yunos.
- [DB Simple](dbsimple.md) — the lightweight key-value store.
- [Run Command](run_command.md) — executing commands programmatically.
- [GClass Registration](registration.md) — registering the built-in gclasses.
