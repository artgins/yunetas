# Operating Yuneta

The operational manual for running, debugging, and administering yunos in
production. These chapters cover the agent that supervises every node, the
full yuno lifecycle, and the runtime subsystems behind them.

## In this section

- [Entry point (main + watcher)](../../yunos/c/yuno_agent/ENTRY_POINT.md) —
  what `main()` does, [`yuneta_entry_point`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/entry_point.c#L286), and the `ydaemon` watcher.
- [Yuno lifecycle](../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md) — how the agent
  creates, runs, kills, updates, and deletes yunos.
- [Debugging a yuno](../../yunos/c/yuno_agent/DEBUGGING.md) — trace levels, the
  log infrastructure, and logcenter.
- [Inter-process communication](../../yunos/c/yuno_agent/IPC.md) — events,
  ievents, gates, and the SPA case.
- [Realms (multi-tenancy)](../../yunos/c/yuno_agent/REALMS.md) — on-disk layout,
  CRUD, and what is not realm-scoped.
- [Scaffolding new yunos](../../yunos/c/yuno_agent/SCAFFOLDING.md) — the
  `yuno-skeleton` templates and banner conventions.
- [Auth, permissions, TLS](../../yunos/c/yuno_agent/YUNO_AUTH.md) — OIDC,
  `auth_bff`, [`C_AUTHZ`](#gclass-c-authz), and cert-sync.
- [gobj framework crash course](../../yunos/c/yuno_agent/GOBJ.md) — gclass,
  `mt_*`, SData, and the runtime tree.
- [timeranger2 + treedb crash course](../../yunos/c/yuno_agent/YUNO_TREEDB.md) —
  the persistence and graph-memory layers.
