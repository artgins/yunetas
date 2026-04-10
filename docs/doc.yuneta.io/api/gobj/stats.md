# Stats

Runtime statistics exposed by gobjs. The global stats parser dispatches `stats` commands to the matching gobj and returns a JSON response. See the [Statistics Parser guide](../../guide/parser_stats.md) for the full picture, including when to use `SDF_*STATS` attributes vs. overriding `mt_stats`.

```{note}
**Two distinct stat stores live inside every gobj — do not confuse them.**

1. **`SDF_STATS` / `SDF_RSTATS` / `SDF_PSTATS` attributes** declared in `attrs_table[]`. These are typed, schema-defined and read/written via `gobj_read_*_attr()` / `gobj_write_*_attr()`. The default `stats_parser` walks them when [`gobj_stats()`](#gobj_stats) is called.

2. **A free-form integer dict `jn_stats`** owned by every gobj. The helpers on this page — [`gobj_set_stat()`](#gobj_set_stat), [`gobj_incr_stat()`](#gobj_incr_stat), [`gobj_decr_stat()`](#gobj_decr_stat), [`gobj_get_stat()`](#gobj_get_stat), [`gobj_jn_stats()`](#gobj_jn_stats) — operate **only on this dict**, never on `SDF_*STATS` attributes. The default `stats_parser` includes the contents of `jn_stats` in the response after the `SDF_*STATS` attributes.

Pick one store per metric and stick with it: mixing them in the same gobj makes the stats output ambiguous. For high-traffic counters, prefer the third path described in the [Statistics Parser guide](../../guide/parser_stats.md#when-to-override-mt_stats) — plain `uint64_t` fields in `PRIVATE_DATA` exposed via a custom [`mt_stats`](../../guide/parser_stats.md#when-to-override-mt_stats) — which avoids both stores entirely on the hot path.
```

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gobj_stats)=
## `gobj_stats()`

Public entry point for collecting the runtime statistics of a gobj. Builds a webix-style JSON envelope (`{result, comment, schema, data}`) where `data` contains every metric the gobj exposes.

```C
json_t *gobj_stats(
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj`  | `hgobj`        | The gobj being queried. |
| `stats` | `const char *` | Filter / control string. `NULL` or `""` returns every metric. A non-empty string filters by `strstr` match against each metric name. The reserved value `"__reset__"` resets all resettable counters and then returns the post-reset snapshot. |
| `kw`    | `json_t *`     | Owned. Optional parameters; passed through to the gclass's `mt_stats` if any. |
| `src`   | `hgobj`        | The gobj making the call (used for tracing and authorization). |

**Returns**

A webix-style response (owned by the caller) — `{result, comment, schema, data}` — where `data` is a flat dict of metric names to values.

**Dispatch**

`gobj_stats()` chooses the implementation in this order:

1. If the gclass defines [`mt_stats`](../../guide/parser_stats.md#when-to-override-mt_stats), it is called and its return value is used as-is. The default parser is bypassed.
2. Otherwise the global stats parser (default: [`stats_parser`](../../guide/parser_stats.md#stats_parser_guide) in `stats_parser.c`) walks the gobj's `SDF_STATS` / `SDF_RSTATS` / `SDF_PSTATS` attributes plus the contents of the free-form `jn_stats` dict and builds the response automatically.

See the [Statistics Parser guide](../../guide/parser_stats.md) for the full picture, including the trade-off between attribute-backed stats (default parser) and `PRIVATE_DATA`-backed stats (`mt_stats` override) for high-traffic gobjs.

---

(gobj_decr_stat)=
## `gobj_decr_stat()`

Decrements the named entry in the gobj's free-form `jn_stats` dict by a given value and returns the new value. **Operates on `jn_stats`, not on `SDF_*STATS` attributes** — see the note at the top of this page.

```C
json_int_t gobj_decr_stat(
    hgobj gobj,
    const char *path,
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose `jn_stats` entry is to be decremented. |
| `path` | `const char *` | The key in `jn_stats` to decrement. |
| `value` | `json_int_t` | The amount to subtract. |

**Returns**

The new value after decrementing.

**Notes**

If the key does not exist, it is initialized to zero before decrementing.

---

(gobj_get_stat)=
## `gobj_get_stat()`

Reads the named entry from the gobj's free-form `jn_stats` dict. **Operates on `jn_stats`, not on `SDF_*STATS` attributes** — see the note at the top of this page.

```C
json_int_t gobj_get_stat(
    hgobj gobj,
    const char *path
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which the entry is retrieved. |
| `path` | `const char *` | The key under which the value lives in `jn_stats`. |

**Returns**

Returns the integer value stored under `path`. If the key does not exist, returns `0`.

---

(gobj_incr_stat)=
## `gobj_incr_stat()`

Increments the named entry in the gobj's free-form `jn_stats` dict by a given value and returns the new value. **Operates on `jn_stats`, not on `SDF_*STATS` attributes** — see the note at the top of this page.

```C
json_int_t gobj_incr_stat(
    hgobj gobj,
    const char *path,
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose `jn_stats` entry is to be incremented. |
| `path` | `const char *` | The key in `jn_stats` to increment. |
| `value` | `json_int_t` | The amount to add. |

**Returns**

The new value after incrementing.

**Notes**

If the key does not exist, it is initialized to zero before incrementing.

---

(gobj_jn_stats)=
## `gobj_jn_stats()`

Returns the gobj's free-form `jn_stats` dict (the same dict the helpers on this page read and write). **Does not include `SDF_*STATS` attributes** — for the full snapshot used by the agent, call [`gobj_stats()`](#gobj_stats) instead.

```C
json_t *gobj_jn_stats(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose `jn_stats` is to be retrieved. |

**Returns**

The internal `jn_stats` dict. The returned JSON object is **not owned** by the caller and must not be modified or freed.

**Notes**

The returned JSON object is the live internal `jn_stats`. Modifying it directly may lead to undefined behaviour. Use [`gobj_set_stat()`](#gobj_set_stat) / [`gobj_incr_stat()`](#gobj_incr_stat) / [`gobj_decr_stat()`](#gobj_decr_stat) to mutate it safely.

---

(gobj_set_stat)=
## `gobj_set_stat()`

Sets the named entry in the gobj's free-form `jn_stats` dict and returns the previous value. **Operates on `jn_stats`, not on `SDF_*STATS` attributes** — see the note at the top of this page.

```C
json_int_t gobj_set_stat(
    hgobj gobj, 
    const char *path, 
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose `jn_stats` entry is to be set. |
| `path` | `const char *` | The key in `jn_stats` to set. |
| `value` | `json_int_t` | The new value. |

**Returns**

The previous value before the assignment.

**Notes**

If the key does not exist, it is created with the specified value.

---
