# JSON Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(anystring2json)=
## `anystring2json()`

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

Uses `json_loadb()` with `JSON_DECODE_ANY` to support multiple JSON formats. If `verbose` is TRUE, logs errors when parsing fails.

---

(bits2gbuffer)=
## `bits2gbuffer()`

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

A newly allocated [`gbuffer_t *`](#gbuffer_t) containing the string representation of the set bits, or NULL if memory allocation fails.

**Notes**

The caller is responsible for managing the memory of the returned [`gbuffer_t *`](#gbuffer_t) using [`gbuffer_decref()`](<#gbuffer_decref>).

---

(bits2jn_strlist)=
## `bits2jn_strlist()`

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
## `cmp_two_simple_json()`

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
## `create_json_record()`

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
## `debug_json()`

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
## `get_real_precision()`

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
## `jn2bool()`

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

This function is useful for safely interpreting JSON values as boolean flags. It ensures that various JSON types are correctly mapped to boolean values.

---

(jn2integer)=
## `jn2integer()`

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
## `jn2real()`

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
## `jn2string()`

`jn2string()` converts a JSON value into a dynamically allocated string representation, ensuring proper memory management.

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
## `json2str()`

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
## `json2uglystr()`

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
## `json_check_refcounts()`

Checks the reference counts of a JSON object and its nested elements, ensuring they do not exceed a specified limit.

```C
int json_check_refcounts(
    json_t *jn, // not owned
    int max_refcount,
    int *result // firstly initalize to 0
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn` | `json_t *` | The JSON object to check. It is not owned by the function. |
| `max_refcount` | `int` | The maximum allowed reference count for any JSON element. If set to a positive value, elements exceeding this limit will be reported. |
| `result` | `int *` | Pointer to an integer that stores the result of the check. It must be initialized to 0 before calling the function. |

**Returns**

Returns 0 if all reference counts are within the allowed limit. Returns a negative value if any reference count is invalid or exceeds the specified limit.

**Notes**

This function recursively checks all elements in the JSON object, including arrays and nested objects. If `max_refcount` is greater than 0, elements exceeding this limit will be logged as errors.

---

(json_config)=
## `json_config()`

The `json_config` function merges multiple JSON configuration sources into a single JSON string, allowing for variable substitution and expansion. It processes fixed, variable, file-based, and parameter-based configurations in a structured order.

```C
char *json_config(
    BOOL        print_verbose_config,
    BOOL        print_final_config,
    const char  *fixed_config,
    const char  *variable_config,
    const char  *config_json_file,
    const char  *parameter_config,
    pe_flag_t   quit
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

['The function processes configurations in the following order: `fixed_config`, `variable_config`, `config_json_file`, and `parameter_config`.', 'If `print_verbose_config` or `print_final_config` is `TRUE`, the function prints the configuration and exits.', 'The function supports variable substitution using the `__json_config_variables__` key.', 'The JSON string can contain one-line comments using `##^`.', 'If an error occurs in JSON parsing, the function may exit based on the `quit` parameter.']

---

(json_desc_to_schema)=
## `json_desc_to_schema()`

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
## `json_is_identical()`

The function `json_is_identical()` compares two JSON objects and returns `TRUE` if they are identical, otherwise `FALSE`.

```C
PUBLIC BOOL json_is_identical(
    json_t *kw1,    /* NOT owned */
    json_t *kw2     /* NOT owned */
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

The function converts both JSON objects to their string representations and compares them. It ensures deep comparison of JSON structures.

---

(json_list_str_index)=
## `json_list_str_index()`

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
| `ignore_case` | `BOOL` | If `TRUE`, the search is case-insensitive; otherwise, it is case-sensitive. |

**Returns**

Returns the index of the string in the JSON array if found, or -1 if not found.

**Notes**

The function expects `jn_list` to be a JSON array. If `jn_list` is not an array, an error is logged.

---

(json_print_refcounts)=
## `json_print_refcounts()`

Prints the reference counts of a JSON object and its nested elements recursively.

```C
PUBLIC int json_print_refcounts(
    json_t *jn,  // not owned
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

(json_str_in_list)=
## `json_str_in_list()`

Checks if a given string exists within a JSON array of strings.

```C
PUBLIC BOOL json_str_in_list(
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
| `ignore_case` | `BOOL` | If TRUE, the comparison is case-insensitive; otherwise, it is case-sensitive. |

**Returns**

Returns TRUE if the string is found in the JSON array, otherwise returns FALSE.

**Notes**

If `jn_list` is not a JSON array, an error is logged using [`gobj_log_error()`](<#gobj_log_error>).

---

(legalstring2json)=
## `legalstring2json()`

The function `legalstring2json` converts a legal JSON string into a JSON object. A legal JSON string must be either an array (`[]`) or an object (`{}`).

```C
json_t *legalstring2json(
    const char *str,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | A null-terminated string containing a legal JSON representation (either an array or an object). |
| `verbose` | `BOOL` | If `TRUE`, logs errors when parsing fails. |

**Returns**

Returns a `json_t *` representing the parsed JSON object. Returns `NULL` if parsing fails.

**Notes**

This function is an alias for [`string2json()`](#string2json).

---

(load_json_from_file)=
## `load_json_from_file()`

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

The function uses `json_loadfd()` to parse the JSON file. If the file does not exist, it returns NULL without logging an error. If an error occurs while opening or parsing the file, it logs an error message using [`gobj_log_critical()`](<#gobj_log_critical>).

---

(load_persistent_json)=
## `load_persistent_json()`

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

If `exclusive` is `TRUE`, the caller is responsible for closing the file descriptor stored in `pfd`. The function logs errors unless `silence` is `TRUE` and `on_critical_error` is set to `LOG_NONE`.

---

(print_json)=
## `print_json()`

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
## `save_json_to_file()`

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
| `xpermission` | `int` | The permission mode for the directory (e.g., 02770). |
| `rpermission` | `int` | The permission mode for the file (e.g., 0660). |
| `on_critical_error` | `log_opt_t` | Logging options for handling critical errors. |
| `create` | `BOOL` | If `TRUE`, the function will create the directory and file if they do not exist. |
| `only_read` | `BOOL` | If `TRUE`, the file will be set to read-only mode after writing. |
| `jn_data` | `json_t *` | The JSON object to be saved to the file. This parameter is owned by the function. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

The function ensures that the directory exists before saving the file. If `only_read` is `TRUE`, the file permissions are set to read-only after writing.

---

(set_real_precision)=
## `set_real_precision()`

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

(str2json)=
## `str2json()`

The function `str2json` converts a legal JSON string into a JSON object. The input string must be a valid JSON array `[]` or object `{}`.

```C
json_t *str2json(
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

Returns a `json_t *` representing the parsed JSON object. Returns `NULL` if parsing fails.

**Notes**

['This function is an alias for [`string2json()`](#string2json).', 'Use [`anystring2json()`](#anystring2json) if the input may contain other JSON types such as numbers or strings.']

---

(string2json)=
## `string2json()`

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

['This function only accepts JSON objects (`{}`) or arrays (`[]`).', 'For more flexible JSON parsing, use [`anystring2json()`](#anystring2json).', 'The returned `json_t *` must be freed using `json_decref()` to avoid memory leaks.']

---

(strings2bits)=
## `strings2bits()`

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
