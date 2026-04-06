# Stats Parser

Default parser for the control-plane `stats` verb. Resolves the target gobj and returns a JSON document with its statistics.

Source code:

- [`stats_parser.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/stats_parser.h)
- [`stats_parser.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/stats_parser.c)

(build_stats)=
## `build_stats()`

`build_stats()` constructs a JSON object containing statistical data extracted from the attributes of a given `hgobj` instance, including attributes marked with `SFD_STATS` flags.

```C
json_t *build_stats(
    hgobj      gobj,
    const char *stats,
    json_t     *kw,
    hgobj      src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which statistics are gathered. |
| `stats` | `const char *` | A string specifying which statistics to include. If `"__reset__"`, resets the statistics. |
| `kw` | `json_t *` | A JSON object containing additional parameters. This object is decremented after use. |
| `src` | `hgobj` | The source `hgobj` instance, used for context in the statistics gathering process. |

**Returns**

A JSON object containing the collected statistics, structured by the short names of the `hgobj` instances.

**Notes**

Internally, `_build_stats()` is used to extract statistics from the `hgobj` instance and its bottom-level objects.
The function iterates through the hierarchy of `hgobj` instances, aggregating statistics from each level.

---

(stats_parser)=
## `stats_parser()`

`stats_parser()` generates a JSON-formatted statistical report by extracting relevant attributes from the given `hgobj` instance and formatting them into a structured response.

```C
json_t *stats_parser(
    hgobj       gobj,
    const char *stats,
    json_t    *kw,
    hgobj       src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which statistics are extracted. |
| `stats` | `const char *` | A string specifying which statistics to retrieve. If `NULL`, all available statistics are included. |
| `kw` | `json_t *` | A JSON object containing additional parameters. Ownership is transferred to [`stats_parser()`](#stats_parser). |
| `src` | `hgobj` | The source `hgobj` that initiated the request. |

**Returns**

A JSON object containing the formatted statistical report. The caller assumes ownership of the returned object.

**Notes**

Internally, [`stats_parser()`](#stats_parser) calls [`build_stats()`](#build_stats) to generate the statistical data.
The function wraps the generated statistics in a standard command response format before returning.

---

(build_stats_response)=
## `build_stats_response()`

`build_stats_response()` builds a standardized JSON response object for statistics operations. It has the same structure and behavior as [`build_command_response()`](../parsers/command_parser.md#build_command_response), producing a response with `result`, `comment`, `schema`, and `data` fields.

```C
json_t *build_stats_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment,
    json_t *jn_schema,
    json_t *jn_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the response (currently unused in the response body). |
| `result` | `json_int_t` | Numeric result code. Use `0` for success and `-1` (or other negative values) for errors. |
| `jn_comment` | `json_t *` | **Owned.** A JSON string with a human-readable message. If `NULL`, defaults to an empty string `""`. |
| `jn_schema` | `json_t *` | **Owned.** A JSON value describing the schema of the returned data. If `NULL`, defaults to `json_null()`. |
| `jn_data` | `json_t *` | **Owned.** A JSON value containing the statistics payload. If `NULL`, defaults to `json_null()`. |

**Returns**

A new JSON object with the structure `{"result": <int>, "comment": <string>, "schema": <value>, "data": <value>}`. The caller owns the returned object.

**Notes**

All three owned parameters (`jn_comment`, `jn_schema`, `jn_data`) are consumed by the function and must not be used after the call. This function is typically called by [`stats_parser()`](#stats_parser) to wrap the output of [`build_stats()`](#build_stats).

---

