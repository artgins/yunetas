# API Reference

The complete C API surface of the Yuneta kernel, plus the JavaScript port.
The C reference is grouped by subsystem; the JavaScript section mirrors the
same API for the browser and Node.js.

## In this section

- [GObj](gobj/index.md) — the core GObject API: startup, gclass registration,
  creation, attributes, operations, events/state, pub/sub, resources, authz,
  info, stats, and nodes.
- [Helpers](helpers/index.md) — string, JSON, time, file-system, buffer, and
  miscellaneous utility functions.
- [Logging](logging/index.md) — log, trace, the UDP log handler, and the
  rotatory file backend.
- [Parsers](parsers/index.md) — the command and stats parsers.
- [Testing](testing/testing.md) — the test harness API.
- [Timeranger2](timeranger2/index.md) — the time-series store and its message,
  msg2db, queue, treedb, and fs_watcher layers.
- [Event Loop](yev_loop/yev_loop.md) — the io_uring async event loop.
- [TLS](ytls/ytls.md) — the runtime-selectable TLS abstraction.
- [JWT (libjwt)](libjwt.md) — JWT creation and validation.
- [Runtime](runtime/index.md) — entry point, environment, yuno, timer,
  inter-event messaging, dbsimple, run-command, and gclass registration.
- [JavaScript](js/index.md) — the ES6 / browser port of the framework.
