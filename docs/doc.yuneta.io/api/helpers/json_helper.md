# JSON

Jansson helpers: load and dump JSON, copy/merge objects, traverse trees, and convert between JSON values and common C types.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c)

(anystring2json)=
## [`anystring2json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5413)

Converts a given string into a JSON object, supporting various formats including arrays and objects. Returns NULL if parsing fails.

```C
json_t *anystring2json(
    const char *bf,
    size_t len,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `const char *` | The input string to be converted into a JSON object. |
| `len` | `size_t` | The length of the input string. |
| `verbose` | `BOOL` | If TRUE, logs errors when parsing fails. |

**Returns**

Returns a `json_t *` representing the parsed JSON object, or NULL if parsing fails.

**Notes**

Uses `json_loadb()` with `JSON_DECODE_ANY` to support multiple JSON formats. If `verbose` is TRUE, a failure is an ERROR with a stack trace: use it for JSON that YOUR code wrote. The bytes are never logged as they came (7.27.0+): the error is jansson's text without the bytes it quotes near the error ([`json_error_for_log()`](#json_error_for_log)), with its `line`, `column` and `position`, and the dump of the `len` bytes is masked ([`mask_secrets_in_text()`](#mask_secrets_in_text)). Up to 7.26.9 the log carried the whole text in a `"bf"` field, a password among it. For bytes received from a peer (an HTTP body) use [`anystring2json_from_peer()`](#anystring2json_from_peer): a WARNING, no stack.

**Example**

```C
json_t *jn = anystring2json(text, strlen(text), TRUE);
if(!jn) {
    // Error already logged: where it breaks, the bytes masked
    return -1;
}
```

---

(bits2gbuffer)=
## [`bits2gbuffer()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4866)

Converts a bitmask into a gbuffer_t structure containing a string representation of the set bits, separated by '|'.

```C
gbuffer_t *bits2gbuffer(
    const char **strings_table,
    uint64_t bits
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `strings_table` | `const char **` | An array of strings representing bit names, terminated by a NULL pointer. |
| `bits` | `uint64_t` | A bitmask where each set bit corresponds to an index in `strings_table`. |

**Returns**

A newly allocated `gbuffer_t *` containing the string representation of the set bits, or NULL if memory allocation fails.

**Notes**

The caller is responsible for managing the memory of the returned `gbuffer_t *` using `gbuffer_decref()`.

---

(bits2jn_strlist)=
## [`bits2jn_strlist()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4847)

Converts a bitmask into a JSON list of corresponding string representations based on a provided lookup table.

```C
json_t *bits2jn_strlist(
    const char **strings_table,
    uint64_t bits
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `strings_table` | `const char **` | An array of string literals representing bit positions, terminated by a NULL pointer. |
| `bits` | `uint64_t` | A bitmask where each set bit corresponds to an index in `strings_table`. |

**Returns**

A JSON array containing the string representations of the set bits in `bits`. The caller is responsible for managing the returned JSON object.

**Notes**

The function iterates through the `strings_table` and checks each bit in `bits`. If a bit is set, the corresponding string is added to the JSON array.

---

(cmp_two_simple_json)=
## [`cmp_two_simple_json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5315)

`cmp_two_simple_json()` compares two JSON values that are either strings, integers, reals, or booleans. It returns a value indicating their relative order.

```C
int cmp_two_simple_json(
    json_t *jn_var1,  // not owned
    json_t *jn_var2   // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_var1` | `json_t *` | The first JSON value to compare. Must be a string, integer, real, or boolean. |
| `jn_var2` | `json_t *` | The second JSON value to compare. Must be a string, integer, real, or boolean. |

**Returns**

Returns -1 if `jn_var1` is less than `jn_var2`, 0 if they are equal, and 1 if `jn_var1` is greater than `jn_var2`. If either value is a complex JSON type (object or array), they are considered equal.

**Notes**

This function does not compare JSON objects or arrays. It first attempts to compare as real numbers, then as integers, then as booleans, and finally as strings if necessary.

---

(create_json_record)=
## [`create_json_record()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4675)

`create_json_record()` initializes a JSON object based on a predefined schema, setting default values for each field.

```C
json_t *create_json_record(
    hgobj gobj,
    const json_desc_t *json_desc
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging errors. |
| `json_desc` | `const json_desc_t *` | A pointer to an array of `json_desc_t` structures defining the JSON schema. |

**Returns**

A newly allocated `json_t *` object containing the initialized JSON structure, or `NULL` on failure.

**Notes**

['The `json_desc_t` structure must be properly terminated with a NULL entry.', 'The function supports various JSON types including `string`, `integer`, `real`, `boolean`, `null`, `object`, and `array`.', 'If an unknown type is encountered, an error is logged using [`gobj_log_error()`](<#gobj_log_error>).']

---

(debug_json)=
## [`debug_json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L8885)

Prints a JSON object with its reference counts, providing a structured view of its contents.

```C
int debug_json(
    const char *label,
    json_t *jn,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `label` | `const char *` | A label to prefix the output, used for identification. |
| `jn` | `json_t *` | The JSON object to be printed, must not be NULL. |
| `verbose` | `BOOL` | If TRUE, prints detailed information including indentation. |

**Returns**

Returns 0 on success, or -1 if the JSON object is NULL or has an invalid reference count.

**Notes**

This function is useful for debugging JSON structures by displaying their contents and reference counts.

---

(get_real_precision)=
## [`get_real_precision()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5617)

Retrieves the current precision setting for real number representation in JSON encoding.

```C
int get_real_precision(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns the current precision value used for encoding real numbers in JSON.

**Notes**

This function is useful for checking the precision setting before modifying it with [`set_real_precision()`](#set_real_precision).

---

(jn2bool)=
## [`jn2bool()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5290)

The function `jn2bool` converts a JSON value to a boolean representation, interpreting various JSON types accordingly.

```C
BOOL jn2bool(json_t *jn_var);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_var` | `json_t *` | A pointer to a JSON value to be converted to a boolean. |

**Returns**

Returns `TRUE` if the JSON value represents a truthy value, otherwise returns `FALSE`. Specifically, `true` JSON values return `TRUE`, `false` and `null` return `FALSE`, numbers return `TRUE` if nonzero, and strings return `TRUE` if non-empty.

**Notes**

This function is useful for safely interpreting JSON values as boolean flags. It makes sure that various JSON types are correctly mapped to boolean values.

---

(jn2integer)=
## [`jn2integer()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5236)

`jn2integer()` converts a JSON value to an integer, handling different JSON types such as strings, booleans, and numbers.

```C
json_int_t jn2integer(json_t *jn_var);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_var` | `json_t *` | The JSON value to be converted to an integer. |

**Returns**

Returns the integer representation of the JSON value. If the value is a string, it is parsed as an integer. If it is a boolean, `true` is converted to `1` and `false` to `0`. If the value is `null`, it returns `0`.

**Notes**

This function supports parsing integers from strings with decimal, octal (prefix `0`), and hexadecimal (prefix `x` or `X`) formats.

---

(jn2real)=
## [`jn2real()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5213)

The function `jn2real` converts a JSON numeric value to a double-precision floating-point number. It supports JSON integers, real numbers, strings representing numbers, and boolean values.

```C
double jn2real(json_t *jn_var);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_var` | `json_t *` | A pointer to a JSON value that will be converted to a double. It can be an integer, real, string, boolean, or null. |

**Returns**

Returns the double representation of the JSON value. If the input is null, it returns 0.0.

**Notes**

If `jn_var` is a string, it is converted using `atof()`. Boolean values are mapped to 1.0 (true) and 0.0 (false).

---

(jn2string)=
## [`jn2string()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5265)

`jn2string()` converts a JSON value into a dynamically allocated string representation. This makes sure of proper memory management.

```C
char *jn2string(json_t *jn_var);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_var` | `json_t *` | The JSON value to be converted into a string. |

**Returns**

A dynamically allocated string representation of the JSON value. The caller must free the returned string using `GBMEM_FREE()`.

**Notes**

If `jn_var` is a string, it returns a duplicate of the string. If it is an integer or real, it converts the value to a string. If it is a boolean, it returns "1" for true and "0" for false.

---

(json2str)=
## [`json2str()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5626)

`json2str` converts a JSON object into a formatted string with indentation.

```C
char *json2str(
    const json_t *jn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `const json_t *` | The JSON object to be converted into a string. |

**Returns**

A newly allocated string containing the formatted JSON representation. The caller must free the returned string using `gbmem_free()`.

**Notes**

The function uses `JSON_INDENT(4)` for formatting and `JSON_REAL_PRECISION(get_real_precision())` to control floating-point precision.

---

(json2uglystr)=
## [`json2uglystr()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5639)

`json2uglystr` converts a JSON object into a compact, non-tabular string representation.

```C
char *json2uglystr(const json_t *jn);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `const json_t *` | A pointer to the JSON object to be converted. |

**Returns**

A dynamically allocated string containing the compact JSON representation. The caller must free the returned string using `gbmem_free()`.

**Notes**

This function uses `JSON_COMPACT` and `JSON_ENCODE_ANY` flags to generate a minimal JSON string without indentation.

---

(json_check_refcounts)=
## [`json_check_refcounts()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5652)

Checks the reference counts of a JSON object and its nested elements. This makes sure of they do not exceed a specified limit.

```C
int json_check_refcounts(
    json_t *kw,
    int max_refcount,
    int *result
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | The JSON object to check. It is not owned by the function. |
| `max_refcount` | `int` | The maximum allowed reference count for any JSON element. If set to a positive value, elements exceeding this limit will be reported. |
| `result` | `int *` | Pointer to an integer that stores the result of the check. It must be initialized to 0 before calling the function. |

**Returns**

Returns 0 if all reference counts are within the allowed limit. Returns a negative value if any reference count is invalid or exceeds the specified limit.

**Notes**

This function recursively checks all elements in the JSON object, including arrays and nested objects. If `max_refcount` is greater than 0, elements exceeding this limit will be logged as errors.

A NULL `kw` is reported and the function **returns**: it used to log the NULL and then fall through to `json_typeof()` on it, crashing on the very thing it exists to detect.

It recurses, and does not guard against a CYCLE (two nodes of a treedb in each other's hooks), which it walks round for ever; and it logs an ERROR for every fault, and a json trace of each container over the maximum. To ask what a json held by others looks like -- a tranger, as the `check-json` commands do -- use [`json_refcounts_report()`](#json_refcounts_report), which answers and logs nothing.

---

(json_refcounts_report)=
## [`json_refcounts_report()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L6171)

What the reference counts of a json say, as a REPORT, logging nothing: the jsons held more than `max_refcount` times, and the corrupted ones, with the [`kw_find_path()`](<#kw_find_path>) path of the first `max_shown`. Corrupted is a type jansson does not have, a refcount of 0, or a refcount BELOW the truth: every json is counted each time the walk reaches it, and one reached more times than its refcount says it is held is reported, at the path where it was reached once too many -- a cycle whose refcounts are corrupted, a json `json_object_set_new()`'d into two places. The root is reached once by the caller, who holds it. Each json is walked ONCE, in a loop: a json held in several places -- a node of a treedb is in its indexes and in the hooks of its parents -- is reported once, and a cycle does not loop the walk. A corrupted json is not walked into.

```C
json_t *json_refcounts_report(
    json_t *jn,
    json_int_t max_refcount,
    int max_shown
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `json_t *` | The json to walk. Not owned. |
| `max_refcount` | `json_int_t` | Report the jsons held more than this many times; `0` (or less) asks only for the corrupted ones. |
| `max_shown` | `int` | How many faults the report lists with their path; the counts count them all. |

**Returns**

A NEW json, `NULL` (and an ERROR) if `jn` is `NULL`:

```json
{
    "max_refcount": 1,
    "jsons": 431,
    "corrupted": 0,
    "over_max": 27,
    "truncated": false,
    "shown": [
        {"path": "treedbs`treedb_x`items`id`a1", "refcount": 3, "fault": "over_max"}
    ]
}
```

`jsons` is how many jsons were walked, each one once (`true`, `false` and `null` are not counted: they are singletons); `fault` is `"over_max"` or `"corrupted"`.

**What it costs.** The walk remembers every json it meets and how many times it reached it -- a set of pointers and counts, kept half empty and doubled as it fills: 24 to 48 bytes per json, and up to 72 while it doubles, the old set and the new one held together -- and a frame of 32 bytes per dict or list for its place in the walk (a scalar takes none). Both are kept in chunks (768 KB and 512 KB), so no block grows with the json: `MAX_BLOCK` bounds none of them, the memory of the yuno does. Measured at its peak (7.27.0, the allocators of gbmem wrapped with `gbmem_set_allocators()`, the bytes each caller asks counted, the peak above what was held before the call):

| The json | jsons | Peak | Per json |
|---|---|---|---|
| a list of 1 000 integers | 1 001 | 39 KB | 39 bytes |
| a list of 10 000 integers | 10 001 | 592 KB | 59 bytes |
| a list of 1 000 000 integers | 1 000 001 | 37.8 MB | 38 bytes |
| a dict of 1 250 records of 8 jsons | 10 001 | 721 KB | 72 bytes |
| a dict of 125 000 records of 8 jsons | 1 000 001 | 44.6 MB | 45 bytes |
| 250 000 nodes held in two indexes | 1 000 003 | 46.1 MB | 46 bytes |

`truncated` is `true` when the walk stopped short, with no memory left for what it remembers -- the gbmem ERROR says why --, and the counts are then of what was walked. Up to 7.26.9 a frame was reserved for EVERY child of a container walked, scalars too, in one block: a list of a million integers asked 32 MB for its frames, more than the default `MAX_BLOCK` (16 MB), and the walk stopped at its root, `truncated` with one json counted (a flat list took 70 to 111 bytes per json below that, measured the same way); the set was one block too, past `MAX_BLOCK` at about half a million jsons. And earlier, only the jsons held more than once were remembered: a cycle held once, its refcounts corrupted below the truth, was walked round until its frames passed `MAX_BLOCK`, and the report did not say it was cut; and a json held by more places than its refcount, once the walk ended, was not reported at all.

```C
json_t *c = json_object();
json_t *d = json_object();
json_object_set_new(c, "hook", d);
json_object_set_new(d, "hook", c);      // c is d's now: refcount 1
json_t *root = json_object();
json_object_set_new(root, "c", c);      // and the root's too: held twice, refcount 1
json_t *jn_report = json_refcounts_report(root, 0, 20);
// {"max_refcount": 0, "jsons": 3, "corrupted": 1, "over_max": 0, "truncated": false,
//  "shown": [{"path": "c`hook`hook", "refcount": 1, "fault": "corrupted"}]}
```

**Example**

```C
json_t *jn_report = json_refcounts_report(tranger, 2, 20);
if(kw_get_int(gobj, jn_report, "corrupted", 0, 0) > 0) {
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "Corrupted jsons in the tranger",
        NULL
    );
}
if(kw_get_bool(gobj, jn_report, "truncated", 0, 0)) {
    /*
     *  The walk stopped short (no memory: the gbmem ERROR says why): its
     *  counts are of what was walked, and "no corrupted json" says nothing
     *  of the rest
     */
}
JSON_DECREF(jn_report)
```

**Notes**

`check-json` of `C_TRANGER` and of the agent answer with it (since 7.27.0): `json_check_refcounts()` logged an ERROR for every json held more than `max_refcount` times, and with its default `1` that was every node of a treedb -- a reader filled the log. Their `-1` is a FAULT, a corrupted json or a walk cut short; the jsons held more than `max_refcount` times are the answer to what was asked, a `0`.

---

(json_nul_fault)=
## [`json_nul_fault()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L6281)

What is wrong with a json that C reads by its strings, as a short phrase, or `NULL` when nothing is. A json decoded with `JSON_ALLOW_NUL` -- every ievent frame is -- carries a string with a NUL in it, and C reads a string up to its first NUL: the id `"dx\u0000evil"` is `"dx"`. Walked at any depth, at most `JSON_PARSER_MAX_DEPTH` (2048) levels, the depth a parser builds and the one jansson dumps or copies; the json of a peer, built by a parser, is walked once.

```C
const char *json_nul_fault(
    json_t *jn      // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `json_t *` | Any json. Not owned, not modified. |

**Returns**

`NULL`, or what is wrong: *"a string with a NUL in it"*, *"a key with a NUL in it"* (a key set by code with `json_object_setn()`; a parser refuses one), or *"a json nested deeper than a parser builds"* (a cycle built with `json_object_set()` is one).

**Example**

```C
json_t *record = json_pack("{s:s%, s:s}", "id", "dx\0evil", (size_t)7, "name", "Dx");
const char *fault = json_nul_fault(record);    // "a string with a NUL in it"
if(fault) {
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER,
        "msg",          "%s", "record of a wrong shape, refused",
        "why",          "%s", fault,
        NULL
    );
}
JSON_DECREF(record)
```

**Notes**

`C_NODE` (create, update, delete, import) and `C_TRANGER` (`add-record`) refuse a peer's record with it (7.27.0+): an id `"dx\u0000evil"` took the key `"dx"`, and an update of `"g1"` with the id `"g1\u0000x"` saved that id under the key `"g1"`, with no log. The treedb asks it of the id and the pkey2 values of a create or an update, for a C caller too.

---

(json_config)=
## [`json_config()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/json_config.c#L349)

The `json_config` function merges multiple JSON configuration sources into a single JSON string. This allows for variable substitution and expansion. It processes fixed, variable, file-based, and parameter-based configurations in a structured order.

```C
json_t *json_config(
    BOOL print_verbose_config,
    BOOL print_final_config,
    const char *fixed_config,
    const char *variable_config,
    const char *config_json_file,
    const char *parameter_config,
    pe_flag_t quit
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `print_verbose_config` | `BOOL` | If `TRUE`, prints the verbose configuration and exits. |
| `print_final_config` | `BOOL` | If `TRUE`, prints the final merged configuration and exits. |
| `fixed_config` | `const char *` | A fixed JSON configuration string that cannot be modified. |
| `variable_config` | `const char *` | A JSON configuration string that can be modified. |
| `config_json_file` | `const char *` | A file path containing JSON configuration data. |
| `parameter_config` | `const char *` | A JSON string containing additional configuration parameters. |
| `quit` | `pe_flag_t` | Determines the behavior on error, such as exiting or continuing execution. |

**Returns**

Returns a dynamically allocated JSON string containing the merged configuration. The caller must free the returned string using `jsonp_free()`.

**Notes**

['The function processes configurations in the following order: `fixed_config`, `variable_config`, `config_json_file`, and `parameter_config`.', 'If `print_verbose_config` or `print_final_config` is `TRUE`, the function prints the configuration and exits.', 'The function supports variable substitution using the `__json_config_variables__` key.', 'The JSON string can contain one-line comments using `##^`.', 'If an error occurs in JSON parsing, the function can exit based on the `quit` parameter.']

---

(json_desc_to_schema)=
## [`json_desc_to_schema()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4815)

Converts a JSON record descriptor into a topic schema, mapping field definitions to a structured format.

```C
json_t *json_desc_to_schema(
    const json_desc_t *json_desc
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `json_desc` | `const json_desc_t *` | Pointer to an array of JSON field descriptors, defining the structure of the record. |

**Returns**

A new JSON array containing schema definitions for each field in the input descriptor.

**Notes**

Each field in the schema includes an 'id', 'header', 'type', and 'fillspace' attribute. The caller is responsible for managing the returned JSON object.

---

(json_is_identical)=
## [`json_is_identical()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5389)

The function `json_is_identical()` compares two JSON objects and returns `TRUE` if they are identical, otherwise `FALSE`.

```C
BOOL json_is_identical(
    json_t *kw1,
    json_t *kw2
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw1` | `json_t *` | The first JSON object to compare. It is not owned by the function. |
| `kw2` | `json_t *` | The second JSON object to compare. It is not owned by the function. |

**Returns**

Returns `TRUE` if `kw1` and `kw2` are identical, otherwise returns `FALSE`.

**Notes**

The function converts both JSON objects to their string representations and compares them. It makes sure of deep comparison of JSON structures.

---

(json_list_str_index)=
## [`json_list_str_index()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4925)

The function `json_list_str_index()` searches for a string in a JSON array and returns its index if found, or -1 if not found.

```C
int json_list_str_index(
    json_t *jn_list,
    const char *str,
    BOOL ignore_case
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_list` | `json_t *` | A JSON array to search within. |
| `str` | `const char *` | The string to search for in the JSON array. |
| `ignore_case` | `BOOL` | If `TRUE`, the search is case-insensitive. Otherwise, it is case-sensitive. |

**Returns**

Returns the index of the string in the JSON array if found, or -1 if not found.

**Notes**

The function expects `jn_list` to be a JSON array. If `jn_list` is not an array, an error is logged.

---

(json_print_refcounts)=
## [`json_print_refcounts()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L6291)

Prints the reference counts of a JSON object and its nested elements recursively.

```C
int json_print_refcounts(
    json_t *jn,
    int level
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `json_t *` | The JSON object whose reference counts will be printed. It is not owned by the function. |
| `level` | `int` | The indentation level for formatting the output. |

**Returns**

Returns 0 on success.

**Notes**

This function is useful for debugging memory management issues related to JSON reference counts.

---

(json_scan_object_members)=
## [`json_scan_object_members()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/json_scan.c#L570)

Reads the TEXT of a json object without building it. It answers if the text is one json object, with the same verdict as `json_loadb()`. For each key you ask for, it says if the object has that key at its top level and where the key is in the text. It builds no json node and allocates no memory.

Use it when you need two or three members of a large object and the rest of the object only as text. For example, a store that writes the text of a record as it arrived, and that needs only the key and the time of the record. Then the parse, the free and the dump of the whole object are not necessary.

```C
typedef struct {
    const char *key;        // in: the key of the member to find (keys must differ)
    int count;              // out: times the key is in the object, 0 if absent
    json_type type;         // out: type of its value (when count > 0)
    size_t value_offset;    // out: its value is text + value_offset, value_length bytes
    size_t value_length;
    size_t cut_offset;      // out: remove these bytes and the object is valid without it
    size_t cut_length;      //      (only when count is 1)
} json_member_span_t;

int json_scan_object_members(
    const char *text,
    size_t len,
    size_t flags,
    json_member_span_t *members,    // in/out, NULL when n_members is 0
    int n_members
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `text` | `const char *` | The text. It does not need a NUL at the end. |
| `len` | `size_t` | The number of bytes of `text`. |
| `flags` | `size_t` | `0` or `JSON_ALLOW_NUL`: the flags that `json_loadb()` gets. Any other flag is an error. |
| `members` | `json_member_span_t *` | The members to find. You set `key`, the function sets the other fields. `NULL` when `n_members` is `0`. |
| `n_members` | `int` | The number of entries in `members`. |

**Returns**

`0` if `text` is one json object that `json_loadb(text, len, flags)` accepts. `-1` if it is not, or if a parameter is wrong. When the function returns `-1`, do not read `members`.

For each member:

- `count` is the number of times that the key is in the object. `0` means that the object does not have the key. A key can be written with escapes in the text: `"id"` is the key `id`.
- When the key occurs more than one time, the other fields give the LAST occurrence. jansson also keeps the last one.
- `type` is the jansson type of the value: `JSON_OBJECT`, `JSON_ARRAY`, `JSON_STRING`, `JSON_INTEGER`, `JSON_REAL`, `JSON_TRUE`, `JSON_FALSE` or `JSON_NULL`.
- `value_offset` and `value_length` give the text of the value. Decode a scalar from it with `json_loadb()` and `JSON_DECODE_ANY`.
- `cut_offset` and `cut_length` give the bytes to remove so that the object stays valid without the member: the key, its value and one comma. Use them only when `count` is `1`.

**Example**

```C
/*
 *  Get the key and the time of a record, and remove its queue metadata,
 *  without a parse of the record
 */
char *text = gbuffer_cur_rd_pointer(gbuf);
size_t len = gbuffer_leftbytes(gbuf);

json_member_span_t m[] = {
    {.key = "id"},
    {.key = "tm"},
    {.key = "__md_trq__"},
};
if(json_scan_object_members(text, len, JSON_ALLOW_NUL, m, 3) < 0 ||
        m[0].count != 1 || m[0].type != JSON_STRING ||
        m[1].count != 1 || m[1].type != JSON_INTEGER ||
        m[2].count != 1 || m[2].type != JSON_OBJECT) {
    return process_record_parsed(gobj, gbuf);   // the usual path logs what is wrong
}

json_t *jn_md = json_loadb(text + m[2].value_offset, m[2].value_length, 0, 0);
json_t *jn_tm = json_loadb(text + m[1].value_offset, m[1].value_length, JSON_DECODE_ANY, 0);

memmove(text + m[2].cut_offset,
    text + m[2].cut_offset + m[2].cut_length,
    len - m[2].cut_offset - m[2].cut_length
);
len -= m[2].cut_length;     // text is now the record without __md_trq__
```

**Notes**

The function accepts a text only if jansson 2.15 accepts it as an object. It uses the same grammar, the same UTF-8 check, the same escapes and surrogate pairs, the same `\u0000` rule (always refused in a key), the same ranges for integers (`json_int_t`) and reals (a real that `strtod()` changes to an infinite is refused, an underflow is accepted), and the same depth limit (`JSON_PARSER_MAX_DEPTH`). The test `tests/c/helpers/test_json_scan.c` compares the two verdicts on the jansson test corpus and on a fuzz of mutated objects.

One difference is known, and it is on the safe side. `json_loadb()` skips a raw NUL byte that comes after a number or a literal: it accepts `{"a":12\0}` as `{"a":12}`. This function refuses all raw NUL bytes. `json_loads()` also refuses that text, because it stops at the NUL.

A text that is not json is an answer, not an error: the function writes no log for it. It writes an ERROR log only for a wrong parameter.

---

(json_str_in_list)=
## [`json_str_in_list()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L6385)

Checks if a given string exists within a JSON array of strings.

```C
BOOL json_str_in_list(
    hgobj gobj,
    json_t *jn_list,
    const char *str,
    BOOL ignore_case
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj instance, used for logging errors. |
| `jn_list` | `json_t *` | A JSON array containing string elements to be searched. |
| `str` | `const char *` | The string to search for within the JSON array. |
| `ignore_case` | `BOOL` | If TRUE, the comparison is case-insensitive. Otherwise, it is case-sensitive. |

**Returns**

Returns TRUE if the string is found in the JSON array, otherwise returns FALSE.

**Notes**

If `jn_list` is not a JSON array, an error is logged using [`gobj_log_error()`](<#gobj_log_error>).

---

(load_json_from_file)=
## [`load_json_from_file()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4492)

The function `load_json_from_file()` loads a JSON object from a file located in the specified directory. It returns a parsed JSON object if successful, or NULL if the file does not exist or an error occurs.

```C
json_t *load_json_from_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to a GObj, used for logging errors. |
| `directory` | `const char *` | The directory where the JSON file is located. |
| `filename` | `const char *` | The name of the JSON file to be loaded. |
| `on_critical_error` | `log_opt_t` | Logging options to handle critical errors. |

**Returns**

Returns a `json_t *` object containing the parsed JSON data if successful, or NULL if the file does not exist or an error occurs.

**Notes**

The function reads the whole file into one buffer and then parses it (`json_loadb()`). If the file does not exist, it returns NULL without logging an error. If an error occurs while opening, reading or parsing the file, it logs an error message using [`gobj_log_critical()`](<#gobj_log_critical>): *"Cannot load json file, bad json"* carries `path` and the parser's `error` and `line`.

In 7.25.4 and earlier the file was parsed with `json_loadfd()`, which reads ONE byte per `read()`: a schema file of 60 KB was 60 000 system calls (about 15 ms), and every open of a treedb reads several such files. Now it is a few `read()` calls.

**Example**

```C
json_t *jn_schema = load_json_from_file(gobj, "/yuneta/store/treedb_x", "treedb_x.treedb_schema.json", 0);
if(!jn_schema) {
    // No file (nothing logged), or it cannot be read or parsed (logged)
}
JSON_DECREF(jn_schema)
```

---

(load_persistent_json)=
## [`load_persistent_json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4418)

The function `load_persistent_json()` loads a JSON object from a file in a specified directory. It supports exclusive access by keeping the file open if requested.

```C
json_t *load_persistent_json(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error,
    int *pfd,
    BOOL exclusive,
    BOOL silence
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj (generic object) that is requesting the JSON load. |
| `directory` | `const char *` | The directory where the JSON file is located. |
| `filename` | `const char *` | The name of the JSON file to be loaded. |
| `on_critical_error` | `log_opt_t` | Logging options to handle critical errors during file operations. |
| `pfd` | `int *` | Pointer to an integer where the file descriptor will be stored if `exclusive` is `TRUE`. If `exclusive` is `FALSE`, the file is closed after reading. |
| `exclusive` | `BOOL` | If `TRUE`, the file remains open and its descriptor is stored in `pfd`. If `FALSE`, the file is closed after reading. |
| `silence` | `BOOL` | If `TRUE`, suppresses error logging when the file does not exist. |

**Returns**

Returns a `json_t *` representing the loaded JSON object. Returns `NULL` if the file does not exist or if an error occurs.

**Notes**

If `exclusive` is `TRUE`, the caller is responsible for closing the file descriptor stored in `pfd`. The function logs errors unless `silence` is `TRUE` and `on_critical_error` is set to `LOG_NONE`. The file is read whole and then parsed, as in [`load_json_from_file()`](<#load_json_from_file>).

---

(print_json)=
## [`print_json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L8731)

Prints a JSON object to stdout with indentation and a label.

```C
int print_json(
    const char *label,
    json_t *jn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `label` | `const char *` | A label to print before the JSON output. |
| `jn` | `json_t *` | The JSON object to print. |

**Returns**

Returns 0 on success, or -1 if the JSON object is NULL or has an invalid reference count.

**Notes**

Uses `json_dumpf()` to print the JSON object with indentation.

---

(save_json_to_file)=
## [`save_json_to_file()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4543)

The function `save_json_to_file()` saves a JSON object to a file, creating the directory if necessary and applying the specified permissions.

```C
int save_json_to_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,
    BOOL only_read,
    json_t *jn_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj context, used for logging and error handling. |
| `directory` | `const char *` | The directory where the JSON file will be saved. |
| `filename` | `const char *` | The name of the JSON file to be created or overwritten. |
| `xpermission` | `int` | The permission mode for the directory (for example 02770). |
| `rpermission` | `int` | The permission mode for the file (for example 0660). |
| `on_critical_error` | `log_opt_t` | Logging options for handling critical errors. |
| `create` | `BOOL` | If `TRUE`, the function will create the directory and file if they do not exist. |
| `only_read` | `BOOL` | If `TRUE`, the file will be set to read-only mode after writing. |
| `jn_data` | `json_t *` | The JSON object to be saved to the file. This parameter is owned by the function. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

The file is written in place (truncated and rewritten), not atomically. Every
failure is logged: a missing directory with `create` `FALSE` as an ERROR
(*"Cannot save json file, directory not found and not to be created"*); a
failure to create the directory, or to create, write or **close** the file, as
a CRITICAL at `on_critical_error` (with the default it exits). The close is
checked because a delayed write fails there (`EIO`, `ENOSPC`): since 7.25.5 a
failed close returns `-1` with *"Cannot close json file, what was written may
be lost"* (7.25.4 reported the file saved). If `only_read` is
`TRUE`, the file is set to `0440` after writing.

**Example**

```C
if(save_json_to_file(
        gobj,
        "/yuneta/store/x",
        "state.json",
        02770,              // directory
        0660,               // file
        LOG_OPT_EXIT_ZERO,  // on_critical_error
        TRUE,               // create the directory if missing
        FALSE,              // only_read
        json_pack("{s:i}", "n", 1)  // owned
    ) < 0) {
    // Error already logged
}
```

---

(set_real_precision)=
## [`set_real_precision()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5611)

Sets the precision for real number formatting in JSON encoding and returns the previous precision value.

```C
int set_real_precision(int precision);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `precision` | `int` | The new precision value for real number formatting. |

**Returns**

Returns the previous precision value before the update.

**Notes**

This function affects the precision of floating-point numbers when converting JSON objects to strings.

---

(string2json)=
## [`string2json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5483)

`string2json` converts a legal JSON string into a `json_t` object. The input string must be a valid JSON array (`[]`) or object (`{}`).

```C
json_t *string2json(
    const char *str,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | A null-terminated string containing a valid JSON object or array. |
| `verbose` | `BOOL` | If `TRUE`, logs errors when parsing fails. |

**Returns**

Returns a `json_t *` object if parsing is successful. Returns `NULL` if parsing fails.

**Notes**

- This function only accepts JSON objects (`{}`) or arrays (`[]`). For more flexible JSON parsing, use [`anystring2json()`](#anystring2json).
- With `verbose`, a failure is logged as [`anystring2json()`](#anystring2json) logs it (7.27.0+): an ERROR without the text, which is dumped masked. Up to 7.26.9 the log carried the whole text in a `"str"` field. Pass `FALSE` when the caller says the failure itself, or when the text is not yours (a value a user wrote).
- The returned `json_t *` must be freed using `json_decref()` to avoid memory leaks.

**Example**

```C
json_t *jn_schema = string2json(schema_text, TRUE);   // our own literal: a failure is a bug
if(!jn_schema) {
    // Error already logged
    return -1;
}
```

---

(strings2bits)=
## [`strings2bits()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4898)

Converts a delimited string of names into a bitmask based on a predefined string table.

```C
uint64_t strings2bits(
    const char **strings_table,
    const char *str,
    const char *separators
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `strings_table` | `const char **` | A null-terminated array of strings representing bit positions. |
| `str` | `const char *` | A string containing names separated by the specified delimiters. |
| `separators` | `const char *` | A string containing delimiter characters used to split `str`. Defaults to "\|, " if NULL or empty. |

**Returns**

A 64-bit bitmask where each bit corresponds to a matched string in `strings_table`.

**Notes**

['Each string in `str` is matched against `strings_table`, and the corresponding bit is set in the returned bitmask.', 'If `separators` is NULL or empty, the default separators "|, " are used.', 'The function assumes that `strings_table` contains at most 64 entries, as it maps each entry to a bit in a `uint64_t`.']

---

(anyfile2json)=
## [`anyfile2json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5452)

Loads a JSON object from a file at the specified path.

```C
json_t *anyfile2json(
    const char *path,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file path to load the JSON data from. |
| `verbose` | `BOOL` | If TRUE, logs errors when the file cannot be opened or parsed. The error is jansson's without the bytes it quotes from the file ([`json_error_for_log()`](#json_error_for_log), 7.27.0+), with its `line`, `column` and `position`. |

**Returns**

Returns a `json_t *` representing the parsed JSON object, or NULL if the file cannot be opened or parsing fails.

**Example**

```C
json_t *jn = anyfile2json("/yuneta/agent/yuneta_agent.json", TRUE);
if(!jn) {
    // Error already logged
    return -1;
}
```

---

(debug_json2)=
## `debug_json2()`

Prints a JSON object with a formatted prefix label, along with its reference counts.

```C
int debug_json2(
    json_t *jn,
    const char *format,
    ...)JANSSON_ATTRS((format(printf, 2, 3))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `json_t *` | The JSON object to be printed. |
| `format` | `const char *` | A printf-style format string used to build the prefix label. |
| `...` | variadic | Additional arguments for the format string. |

**Returns**

Returns 0 on success, or -1 if the JSON object is NULL.

---

(is_metadata_key)=
## [`is_metadata_key()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L9526)

Checks if a key is a metadata key by verifying it has exactly two leading underscores.

```C
BOOL is_metadata_key(
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `key` | `const char *` | The key string to check. |

**Returns**

Returns TRUE if the key starts with `__`, otherwise returns FALSE.

---

(is_private_key)=
## [`is_private_key()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L9547)

Checks if a key is a private key by verifying it has exactly one leading underscore.

```C
BOOL is_private_key(
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `key` | `const char *` | The key string to check. |

**Returns**

Returns TRUE if the key starts with a single `_`, otherwise returns FALSE.

---

(json_config_string2json)=
## [`json_config_string2json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5514)

Converts a JSON configuration string into a JSON object, with support for inline comment removal (using `##^` delimiter).

```C
json_t *json_config_string2json(
    const char *bf,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `const char *` | The JSON configuration string to parse. It can contain inline comments delimited by `##^`. |
| `verbose` | `BOOL` | If TRUE, logs errors when parsing fails, as [`anystring2json()`](#anystring2json) logs them: the text dumped masked, comments removed. |

**Returns**

Returns a `json_t *` representing the parsed JSON object, or NULL if parsing fails, or if `bf` is NULL (an ERROR; up to 7.26.9 a SIGSEGV -- the agent's `create-config` of a `content64` that is not base64 reached it).

**Example**

```C
json_t *jn_config = json_config_string2json(
    "{\"port\": 8080}   ##^ the port of the gate\n",
    TRUE
);
```

---

(json_dict_recursive_update)=
## [`json_dict_recursive_update()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5167)

Recursively updates a JSON object with key-value pairs from another object.

```C
int json_dict_recursive_update(
    json_t *object,
    json_t *other,
    BOOL overwrite
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `object` | `json_t *` | The target JSON object to be updated. |
| `other` | `json_t *` | The source JSON object whose key-value pairs will be merged into `object`. |
| `overwrite` | `BOOL` | If TRUE, existing keys in `object` are overwritten with values from `other`. If FALSE, existing keys are preserved. |

**Returns**

Returns 0 on success, or -1 if either parameter is not a JSON object.

---

(json_is_range)=
## [`json_is_range()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5067)

Checks if a JSON array represents a valid integer range (exactly two integers where first <= second).

```C
BOOL json_is_range(
    json_t *list,
    json_int_t *pfirst,
    json_int_t *psecond
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The JSON array to check. Must contain exactly two integer elements. |
| `pfirst` | `json_int_t *` | If not NULL, receives the first (lower) value of the range. |
| `psecond` | `json_int_t *` | If not NULL, receives the second (upper) value of the range. |

**Returns**

Returns TRUE if the array contains exactly two integers where the first is less than or equal to the second, otherwise returns FALSE.

---

(json_list_find)=
## [`json_list_find()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5023)

Searches for a JSON value in a JSON array and returns its index.

```C
int json_list_find(
    json_t *list,
    json_t *value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The JSON array to search within. |
| `value` | `json_t *` | The JSON value to search for. |

**Returns**

Returns the index of the value in the array if found, or -1 if not found.

---

(json_list_int)=
## [`json_list_int()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4961)

Retrieves an integer value from a JSON array at the specified index.

```C
json_int_t json_list_int(
    json_t *jn_list,
    size_t idx
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_list` | `json_t *` | The JSON array to retrieve the value from. |
| `idx` | `size_t` | The index of the element to retrieve. |

**Returns**

Returns the integer value at the specified index, or 0 if the index is out of bounds or the element is not an integer.

---

(json_list_int_index)=
## [`json_list_int_index()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L4993)

Searches for an integer value in a JSON array and returns its index.

```C
int json_list_int_index(
    json_t *jn_list,
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_list` | `json_t *` | The JSON array to search within. |
| `value` | `json_int_t` | The integer value to search for. |

**Returns**

Returns the index of the integer value in the array if found, or -1 if not found.

---

(json_list_update)=
## [`json_list_update()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5042)

Extends a JSON array with values from another array, optionally treating it as a set.

```C
int json_list_update(
    json_t *list,
    json_t *other,
    BOOL as_set_type
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The target JSON array to extend. |
| `other` | `json_t *` | The source JSON array whose values will be appended to `list`. |
| `as_set_type` | `BOOL` | If TRUE, only adds values from `other` that are not already present in `list`, treating the array as a set. |

**Returns**

Returns 0 on success, or -1 on error.

---

(json_listsrange2set)=
## [`json_listsrange2set()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5127)

Converts a mixed list of integers and integer ranges into a single flattened set of unique integers.

```C
json_t *json_listsrange2set(
    json_t *listsrange
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `listsrange` | `json_t *` | A JSON array containing integers or both two-element integer range arrays. Not owned by the function. |

**Returns**

Returns a new `json_t *` array containing the flattened set of unique integers, or NULL on error.

---

(json_range_list)=
## [`json_range_list()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.c#L5098)

Expands an integer range array into a full JSON array of all integers in that range.

```C
json_t *json_range_list(
    json_t *list
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | A two-element JSON array representing an integer range `[first, second]`. |

**Returns**

Returns a new `json_t *` array containing all integers from first to second (inclusive), or NULL on error.

**Notes**

This function can be slow for large ranges, as it allocates an element for every integer in the range.

---

(json_replace_var)=
## `json_replace_var()`

Replaces variable placeholders using default delimiters `(^^` and `^^)`.

```C
json_t *json_replace_var(
    json_t *jn_dict,
    json_t *jn_vars
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_dict` | `json_t *` | The JSON object containing string values with variable placeholders. Owned by the function. |
| `jn_vars` | `json_t *` | A JSON object mapping variable names to their replacement values. Owned by the function. |

**Returns**

Returns a new `json_t *` with all placeholders replaced, or NULL on error.

---

(json_replace_var_custom)=
## `json_replace_var_custom()`

Replaces variable placeholders with custom delimiters.

```C
json_t *json_replace_var_custom(
    json_t *jn_dict,
    json_t *jn_vars,
    const char *open,
    const char *close
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_dict` | `json_t *` | The JSON object containing string values with variable placeholders. Owned by the function. |
| `jn_vars` | `json_t *` | A JSON object mapping variable names to their replacement values. Owned by the function. |
| `open` | `const char *` | The opening delimiter string that marks the start of a variable placeholder. |
| `close` | `const char *` | The closing delimiter string that marks the end of a variable placeholder. |

**Returns**

Returns a new `json_t *` with all placeholders replaced, or NULL on error.

---

(json_size)=
## [`json_size()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.h#L138)

Returns the size of a JSON value based on its type. It is a `static inline` in
the header, so it costs no call.

```C
static inline size_t json_size(
    const json_t *jn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `const json_t *` | The JSON value whose size is to be determined. May be NULL. |

**Returns**

Returns the number of elements for arrays and objects, 1 for non-empty strings, and 0 for all other types (including a C NULL, empty strings, integers, reals, booleans, and null).

---

(empty_json)=
## [`empty_json()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.h#L161)

Tells whether there is anything usable in a JSON value. **This is the guard to
write over a `json_t *`, never `if(!jn)`**: an attribute declared `DTP_JSON`
with a null default holds `json_null()` — a valid pointer — so the pointer
test is a branch that can never be taken. The same applies to every optional
key of a subscription (`__config__`, `__global__`, `__local__`, `__filter__`).

It also answers TRUE for an empty container, which a null test does not: an
empty filter is as much "no filter" as an absent one.

```C
static inline BOOL empty_json(
    const json_t *jn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `const json_t *` | The JSON value to test. May be NULL. |

**Returns**

TRUE for a C NULL, a json null, `{}`, `[]`, `""` and any scalar. FALSE only when there is content. It is `json_size(jn)==0`.

---

(json_absent)=
## [`json_absent()`](https://github.com/artgins/yunetas/blob/7.27.15/kernel/c/gobj-c/src/helpers.h#L172)

Tells whether there is no value at all. Use it **only** where a null and an
empty container mean different things — a field written null on purpose against
a field never written. Everywhere else the question is
[`empty_json()`](#empty_json).

Its twin in `gobj-js` is `is_null()`: JavaScript has no second null to tell
apart.

```C
static inline BOOL json_absent(
    const json_t *jn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `const json_t *` | The JSON value to test. May be NULL. |

**Returns**

TRUE for a C NULL and for an explicit json null, FALSE for everything else.

---

