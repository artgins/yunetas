# gobj-c

The core GObject framework of Yuneta, in C. The rest of the C kernel and every yuno are built on it.

## What it provides

- GClass / GObj: a class and instance model with finite-state machines (`states`, `events`, action callbacks).
- SData: a typed attribute schema (`SDATA()` macros) for configuration, stats, and authz.
- Event bus: publish/subscribe between gobjs, with automatic cleanup on `gobj_destroy()`. Delivery is synchronous. `gobj_post_event()` is the same send, deferred to the next cycle of the event loop. An action uses it to leave the stack that it runs on.
- Logging: a structured JSON logger (`gobj_log_info/warn/error`), trace levels, and log handlers (stdout, file, UDP). The file handler writes through the rotatory (`rotatory.h`), a log file that rotates by date and size:
    - A full disk stops only the log file on it. The file writes again when space is available again. Each change writes one line on stdout/syslog that names the file: *"rotatory(): stop logging to '<path>' because full disk: ..."* and *"rotatory(): logging to '<path>' again: ..., N records were dropped"*.
    - A clock set back across midnight empties no file. The rotatory empties an existing file only when its last write is before the day that its name is for.
    - Retention: `rotatory_remove_old_files(hr, keep_days, jn_removed, &bytes)` removes the files of the rotatory that are older than `keep_days`. `rotatory_keep_all_old_files(hr, TRUE)` keeps every piece of a day (`.OLD.1`, `.OLD.2`, ...). The agent audit uses both (`audit_keep_days`).
- Commands: the command parser gives the handler a new kw with the keys of the kw of the caller (`kw_update_missing()`). A binary field of it (the `gbuffer` of an event) holds its own reference. Thus you can release each kw with `KW_DECREF`. To keep the buffer, take it out of the kw with `KW_EXTRACT` and use that reference. Never take a second one.
- Helpers: `kw_*` JSON helpers, the `gbuffer_t` byte buffer, `dl_list_t` intrusive lists, strings, paths, regex, base64, hashes, and more.
    - If the path (or a part of it) exists and is not a directory, `mkrdir()` returns -1, with the log *"Not a directory: the path exists and is not a directory"*. `rmrdir()` / `rmrcontentdir()` never follow a symbolic link.
    - The peer address of a `gbuffer_t` has its length: `gbuffer_setaddr(gbuf, addr, addrlen)` and `gbuffer_getaddrlen(gbuf)`. Thus an IPv6 peer address (28 bytes) stays whole.
- Memory tracking: `GBMEM_MALLOC` / `gbmem_malloc`. If `CONFIG_DEBUG_TRACK_MEMORY` is enabled, the tracked allocations show the leaks at shutdown. The report tells what is still busy. To find the code that allocated it, any yuno reads two environment variables:
    - `YUNETA_TRACK_MEM_DUMP=1` prints the bytes of each leaked block.
    - `YUNETA_TRACK_MEM=<ref_min>-<ref_max>[:<size>,...]` logs a stack for every allocation inside that window. The procedure is in [`DEBUGGING.md`](../../../yunos/c/yuno_agent/DEBUGGING.md) §11.7.

## Key headers

| Header | What is in it |
|---|---|
| `src/gobj.h` | GClass definition macros, SData types, FSM API |
| `src/gobj.c` | Core GObj runtime (~12 kLoC) |
| `src/kwid.c/h` | `kw_*` JSON keyword helpers |
| `src/glogger.c/h` | Structured JSON logger |
| `src/gbuffer.c/h` | Growable byte buffer |
| `src/helpers.c/h` | Paths, timers, strings, static NSS replacements for `CONFIG_FULLY_STATIC` |
| `src/json_scan.c` | `json_scan_object_members()`: reads the members of a json object from its text, with the verdict of `json_loadb()`, and does not build the object (declared in `helpers.h`) |
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

For full real-world examples, see `kernel/c/root-linux/src/c_*.c` and any yuno. For the architecture, see the top-level `CLAUDE.md`.
