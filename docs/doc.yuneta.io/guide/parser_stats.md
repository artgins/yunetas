(stats_parser_guide)=

# **Statistics Parser**

The statistics parser in Yuneta handles requests for retrieving and managing statistics from a GObj. It operates by collecting attributes marked with specific statistical flags (`SDF_STATS`, `SDF_RSTATS`, and `SDF_PSTATS`) and providing a structured way to query, reset, or persist these attributes.

The statistics parser integrates with the [`gobj_stats`](#gobj_stats) API, dynamically selecting between a global parser or a GClass-specific `mt_stats` method if defined.

```C
json_t *gobj_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src);
```

Source code in:

- [stats_parser.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/stats_parser.c)
- [stats_parser.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/stats_parser.h)

---

## How the Internal Statistics Parser Works

### 1. **Attribute Collection**
When `gobj_stats()` is called:
- Attributes marked with `SDF_STATS`, `SDF_RSTATS`, or `SDF_PSTATS` are collected.
- These attributes are treated as statistics and included in the response.

### 2. **Querying Statistics**
- If the `stats` parameter contains the name of an attribute, the value of that specific attribute is returned.
- If the `stats` parameter is empty, all attributes marked with `SDF_STATS` are returned.

### 3. **Resetting Statistics**
The internal global parser includes a special name, `__reset__`:
- When `__reset__` is passed as the `stats` parameter, all attributes marked with `SDF_RSTATS` are reset to their default values.

### 4. **Persistence of Statistics**
Attributes marked with `SDF_PSTATS` serve dual purposes:
- They are treated as persistent attributes and saved when `gobj_save_persistent_attrs()` is called.
- They are also included as part of the statistics queried via `gobj_stats()`.

### 5. **Custom GClass Handling**
- If a GClass defines an `mt_stats` method, `gobj_stats()` calls this method instead of the global parser.
- This allows GClasses to implement custom logic for managing and returning statistics.

---

## Flags Relevant to Statistics

| **Flag**        | **Description**                                                                 |
|------------------|---------------------------------------------------------------------------------|
| `SDF_STATS`      | Marks the attribute as a statistic to be retrieved by the statistics parser.    |
| `SDF_RSTATS`     | Indicates that the attribute is a resettable statistic. Can be reset via `__reset__`. |
| `SDF_PSTATS`     | Combines `SDF_STATS` with persistence. The attribute is saved as a persistent statistic. |

---

## API for Statistics

| **API**              | **Description**                                                                 |
|-----------------------|---------------------------------------------------------------------------------|
| `gobj_stats()`        | Retrieve or reset statistics for a GObj.                                       |

| **Parameter** | **Description**                                                                 |
|---------------|---------------------------------------------------------------------------------|
| `gobj`       | The GObj instance for which statistics are being requested.                     |
| `stats`      | The name of the statistic to retrieve, or `__reset__` to reset all resettable statistics. |
| `kw`         | Additional parameters for the statistics request.                                |
| `src`        | The source GObj sending the statistics request.                                  |

---

## Workflow for `gobj_stats()`

1. **Receiving a Statistics Request:**
    - A request is sent to a GObj using `gobj_stats()`.
    - The `stats` and `kw` parameters define the requested statistics and additional options.

2. **Attribute Collection:**
    - The parser collects attributes marked with `SDF_STATS`, `SDF_RSTATS`, and `SDF_PSTATS`.

3. **Parsing and Validation:**
    - If a specific statistic name is provided, its value is returned.
    - If no name is provided, all `SDF_STATS` attributes are returned.

4. **Custom or Global Handling:**
    - If the GClass defines `mt_stats`, it is invoked for custom processing.
    - Otherwise, the global parser processes the request.

5. **Resetting Statistics:**
    - If `__reset__` is passed, all attributes marked with `SDF_RSTATS` are reset.

---

## Benefits of the Statistics Parser

- **Dynamic Querying:** Supports both specific and bulk statistics requests.
- **Customizability:** GClasses can define their own `mt_stats` method for specialized handling.
- **Persistence Integration:** Attributes marked with `SDF_PSTATS` are automatically persisted.
- **Reset Capability:** The `__reset__` keyword simplifies resetting of statistics.

---

## When to override `mt_stats`

There are two ways for a GClass to expose statistics, and the right
choice depends on how often the counters are updated.

### Path A — `SDF_*STATS` attributes (default parser)

Declare counters as `SDF_RD|SDF_RSTATS` (or `SDF_PSTATS`) entries in
`attrs_table[]`. The framework's default `stats_parser` walks them
automatically and the standard `"__reset__"` stat name resets the
resettable ones.

This is the **convenient, zero-code** path. Use it for low-rate
metrics where the SDATA overhead is in the noise.

```C
PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_INTEGER, "txMsgs", SDF_RD|SDF_RSTATS, 0, "Messages transmitted"),
SDATA (DTP_INTEGER, "rxMsgs", SDF_RD|SDF_RSTATS, 0, "Messages received"),
SDATA_END()
};
```

Increment with `gobj_write_integer_attr()`. Each increment goes
through the SDATA system: name lookup, JSON read, JSON write.

### Path B — `mt_stats` gclass method with `PRIVATE_DATA` counters

Required for **high-traffic gobjs**. Every `SDF_*STATS` increment via
`gobj_write_integer_attr()` carries non-trivial cost (string lookup,
JSON read, JSON write). For a gobj that fires hundreds or thousands
of increments per second per instance — typical of protocol
processors, brokers, gateways, BFFs — that cost adds up and pollutes
the hot path with no functional benefit.

The fix is to keep the counters as plain `uint64_t` fields in
`PRIVATE_DATA`, increment them with a single `priv->stat++`, and
build the JSON dict **on demand** inside `mt_stats`. The override
takes preference over the default parser (see [`gobj_stats`](../api/gobj/stats.md#gobj_stats)) so the
SDATA path is bypassed entirely.

When you implement `mt_stats` you take full responsibility for the
two conventions the default parser handles for free:

- **`"__reset__"` semantics.** Zero the resettable counters yourself.
  Gauges (live state — current queue depth, current connection count,
  …) should be left alone or anchored to "now"; pure counters
  (lifetime totals, error counts, …) reset to `0`.
- **Prefix filter.** When `stats` is non-empty, return only the
  entries whose name appears in the filter string. Mirror the
  default parser's `strstr(stats, name)` convention so callers see
  no behavioural difference.

Return the result via [`build_stats_response`](#build_stats_response)
(`{result, comment, schema, data}`) so the envelope is identical to
what the default parser would return.

#### Example — `c_auth_bff.c`

`C_AUTH_BFF` is the per-channel BFF processor of the auth_bff yuno.
Each browser request crosses several increment points (`enqueue`,
`process_next`, `result_token_response`, `send_error_response`), and
there are up to 25 instances per yuno, so the counters live as raw
struct fields:

```C
typedef struct _PRIVATE_DATA {
    /* ... */
    int       q_count;            /* live gauge */
    uint64_t  st_requests_total;
    uint64_t  st_q_max_seen;
    uint64_t  st_q_full_drops;
    uint64_t  st_kc_calls;
    uint64_t  st_kc_ok;
    uint64_t  st_kc_errors;
    uint64_t  st_bff_errors;
} PRIVATE_DATA;
```

Hot path is just:

```C
priv->st_kc_calls++;
```

`mt_stats` builds the snapshot only when someone asks:

```C
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(stats && strcmp(stats, "__reset__") == 0) {
        priv->st_requests_total = 0;
        priv->st_q_max_seen     = priv->q_count;  /* anchor to "now" */
        priv->st_q_full_drops   = 0;
        priv->st_kc_calls       = 0;
        priv->st_kc_ok          = 0;
        priv->st_kc_errors      = 0;
        priv->st_bff_errors     = 0;
        stats = "";
    }

    json_t *jn_data = json_object();

    #define STAT_INT(name_, value_) do { \
        if(empty_string(stats) || strstr(stats, name_) != NULL) { \
            json_object_set_new(jn_data, name_, \
                json_integer((json_int_t)(value_))); \
        } \
    } while(0)

    STAT_INT("requests_total", priv->st_requests_total);
    STAT_INT("q_count",        priv->q_count);
    STAT_INT("q_max_seen",     priv->st_q_max_seen);
    STAT_INT("q_full_drops",   priv->st_q_full_drops);
    STAT_INT("kc_calls",       priv->st_kc_calls);
    STAT_INT("kc_ok",          priv->st_kc_ok);
    STAT_INT("kc_errors",      priv->st_kc_errors);
    STAT_INT("bff_errors",     priv->st_bff_errors);

    #undef STAT_INT

    KW_DECREF(kw)
    return build_stats_response(gobj, 0, 0, 0, jn_data);
}

PRIVATE const GMETHODS gmt = {
    /* ... */
    .mt_stats = mt_stats,
};
```

The full source is in `kernel/c/root-linux/src/c_auth_bff.c`.

### Quick rule of thumb

| Counter rate | Recommended path |
|---|---|
| A few updates per second or less, or once per state transition | Path A — `SDF_RSTATS` attribute |
| Per-request counters in a protocol processor / broker / gateway | Path B — `mt_stats` + `PRIVATE_DATA` `uint64_t` |

In both cases the public interface seen by callers is the same:
`gobj_stats(gobj, prefix, kw, src)` returns the same envelope.
Switching from path A to path B is a private optimisation that
doesn't break consumers.

---

## Use Cases

### 1. **Monitoring and Reporting**
Retrieve runtime statistics for performance monitoring or system diagnostics.

### 2. **Dynamic Configuration**
Integrate with external systems to query or modify statistical attributes dynamically.

### 3. **State Management**
Reset runtime statistics programmatically to maintain operational consistency.
