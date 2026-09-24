# gobj-c

**Core GObject framework for Yuneta (C).** This is the foundation the rest of the C kernel and every yuno is built on.

## What it provides

- **GClass / GObj**: class + instance model with **finite-state machines** (`states`, `events`, action callbacks)
- **SData**: typed attribute schema (`SDATA()` macros) used for configuration, stats and authz
- **Event bus**: publish/subscribe between gobjs, with automatic cleanup on `gobj_destroy()`. Delivery is synchronous; `gobj_post_event()` is the same send, deferred to the next cycle of the event loop, which is how an action leaves the stack it is standing on
- **Logging**: structured JSON logger (`gobj_log_info/warn/error`), trace levels, log handlers (stdout, file, UDP). The file handler writes through the **rotatory** (`rotatory.h`):
    - A full disk stops only the log file on it, and the file writes again when the space is back. Each change is one line on stdout/syslog that names the file: *"rotatory(): stop logging to '<path>' because full disk: ..."* and *"rotatory(): logging to '<path>' again: ..., N records were dropped"*.
    - A clock set back across midnight empties no file: an existing file is emptied only when it was last written before the day its name is used for.
    - Retention: `rotatory_remove_old_files(hr, keep_days, jn_removed, &bytes)` removes the files of the rotatory older than `keep_days`, and `rotatory_keep_all_old_files(hr, TRUE)` keeps every piece of a day (`.OLD.1`, `.OLD.2`, ...). The agent audit uses both (`audit_keep_days`).
- **Commands**: the command parser gives the handler a new kw with the keys of the caller's kw (`kw_update_missing()`). A binary field of it (the `gbuffer` of an event) holds its own reference, so each kw can be released with `KW_DECREF`. To keep the buffer, take it out of the kw with `KW_EXTRACT` and use that reference; never take a second one.
- **Helpers**: `kw_*` JSON helpers, `gbuffer_t` byte buffer, `dl_list_t` intrusive lists, strings, paths, regex, base64, hashes, …
    - `mkrdir()` returns -1, with the log *"Not a directory: the path exists and is not a directory"*, when the path (or a part of it) exists and is not a directory; `rmrdir()` / `rmrcontentdir()` never follow a symbolic link.
    - The peer address of a `gbuffer_t` has its length: `gbuffer_setaddr(gbuf, addr, addrlen)` and `gbuffer_getaddrlen(gbuf)`, so an IPv6 peer (28 bytes) is kept whole.
- **Memory tracking**: `GBMEM_MALLOC` / `gbmem_malloc` — tracked allocations surface leaks at shutdown when `CONFIG_DEBUG_TRACK_MEMORY` is enabled. The report says what is still busy; to find out WHO allocated it, two environment variables read by any yuno: `YUNETA_TRACK_MEM_DUMP=1` prints the bytes of each leaked block, and `YUNETA_TRACK_MEM=<ref_min>-<ref_max>[:<size>,...]` logs a stack for every allocation inside that window (recipe in [`DEBUGGING.md`](../../../yunos/c/yuno_agent/DEBUGGING.md) §11.7)

## Key headers

| Header | What's in it |
|---|---|
| `src/gobj.h` | GClass definition macros, SData types, FSM API |
| `src/gobj.c` | Core GObj runtime (~12 kLoC) |
| `src/kwid.c/h` | `kw_*` JSON keyword helpers |
| `src/glogger.c/h` | Structured JSON logger |
| `src/gbuffer.c/h` | Growable byte buffer |
| `src/helpers.c/h` | Paths, timers, strings, static NSS replacements for `CONFIG_FULLY_STATIC` |
| `src/dl_list.c/h` | Intrusive doubly-linked list |

## Typical GClass layout

```c
typedef struct { ... } priv_t;                    // private data

PRIVATE sdata_desc_t attrs_table[] = {            // attribute schema
    SDATA(DTP_STRING, "name", SDF_RD, "default", "description"),
    SDATA_END()
};

PRIVATE EV_ACTION ST_IDLE[] = {                   // FSM state
    {EV_FOO, ac_foo, 0},
    {0,0,0}
};

PRIVATE json_t *ac_foo(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);

PRIVATE GOBJ_DEFINE_GCLASS(MY_CLASS);             // GClass descriptor
```

See `kernel/c/root-linux/src/c_*.c` and any yuno for full real-world examples, and the top-level `CLAUDE.md` for the broader architecture.
