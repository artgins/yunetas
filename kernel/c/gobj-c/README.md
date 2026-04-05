# gobj-c

**Core GObject framework for Yuneta (C).** This is the foundation the rest of the C kernel and every yuno is built on.

## What it provides

- **GClass / GObj**: class + instance model with **finite-state machines** (`states`, `events`, action callbacks)
- **SData**: typed attribute schema (`SDATA()` macros) used for configuration, stats and authz
- **Event bus**: publish/subscribe between gobjs, with automatic cleanup on `gobj_destroy()`
- **Logging**: structured JSON logger (`gobj_log_info/warn/error`), trace levels, log handlers (stdout, file, UDP)
- **Helpers**: `kw_*` JSON helpers, `gbuffer_t` byte buffer, `dl_list_t` intrusive lists, strings, paths, regex, base64, hashes, …
- **Memory tracking**: `GBMEM_MALLOC` / `gbmem_malloc` — tracked allocations surface leaks at shutdown when `CONFIG_DEBUG_TRACK_MEMORY` is enabled

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
