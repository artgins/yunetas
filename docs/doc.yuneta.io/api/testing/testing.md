# Testing

Helpers used by Yuneta's C test suite: expected-log capture, JSON result comparison with key-ignoring, ordered/unordered matching, and human-readable diffs on failure.

Source code:

- [`testing.h`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.h)
- [`testing.c`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c)

(capture_log_write)=
## [`capture_log_write()`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c#L55)

`capture_log_write()` processes log messages, comparing them against expected log messages and categorizing them as expected or unexpected.

```C
int capture_log_write(
    void        *v,
    int         priority,
    const char  *bf,
    size_t      len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `v` | `void *` | Unused parameter, typically reserved for user-defined data. |
| `priority` | `int` | Log priority level, typically used for filtering log messages. |
| `bf` | `const char *` | Log message content in JSON format. |
| `len` | `size_t` | Length of the log message. |

**Returns**

Returns `-1` to indicate that the log message was processed internally.

**Notes**

If a log message matches an expected message, it is removed from the expected list. Otherwise, it is added to the unexpected log messages list.

---

(set_expected_results)=
## [`set_expected_results()`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c#L211)

`set_expected_results()` initializes the expected test results, including expected errors, expected JSON output, ignored keys, and verbosity settings.

```C
void set_expected_results(
    const char  *name,
    json_t      *errors_list,
    json_t      *expected,
    const char  **ignore_keys,
    BOOL        verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The name of the test case. |
| `errors_list` | `json_t *` | A JSON array containing expected error messages. |
| `expected` | `json_t *` | A JSON object representing the expected test output. |
| `ignore_keys` | `const char **` | An array of keys to be ignored during JSON comparison. |
| `verbose` | `BOOL` | Flag indicating whether verbose output must be enabled. |

**Returns**

This function does not return a value.

**Notes**

The function resets previously stored expected results before setting new ones.
If `verbose` is enabled, the function prints the test name to the console.
The function initializes `expected_log_messages`, `unexpected_log_messages`, and `expected` as JSON arrays if they are not provided.

**The list is a SCRIPT, and that is the point**

Every captured log must match the **head** of `errors_list`, so the list states
the exact messages, in the exact order, the exact number of times. That is the
right assertion for a test that drives one sequence, and it is what nearly
every test here wants. When the order is genuinely not yours to decide, see
[`set_expected_results_unordered()`](#set_expected_results_unordered) below —
and read what it gives up before reaching for it.

---

(set_expected_results_unordered)=
## [`set_expected_results_unordered()`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c#L227)

The same as [`set_expected_results()`](#set_expected_results), with
`errors_list` read as a **whitelist** instead of a script.

```C
void set_expected_results_unordered(
    const char  *name,
    json_t      *errors_list,
    json_t      *expected,
    const char  **ignore_keys,
    BOOL        verbose
);
```

**Parameters**

The same as [`set_expected_results()`](#set_expected_results).

**How the list is read**

| | `set_expected_results()` | `set_expected_results_unordered()` |
|---|---|---|
| A captured log matches | the HEAD of the list | ANY entry of the list |
| A match | consumes the entry | leaves it, so the message may repeat |
| Every entry must be matched | exactly as many times as listed | at least once |
| A log matching no entry | fails the test | fails the test |

So it gives up *"in this order, this many times"* and keeps *"these things
happened, and nothing else did"*.

**When to use it**

:::{warning}
Only when the order really is not ours to decide. A test whose sequence is
merely wrong gets its sequence fixed, not its assertion relaxed.
:::

The case it was written for is `c_tcp2/test2`: two `C_TCP` gobjs — the client
and the accepted server side — log `"Connected"` and `"Disconnected"`
independently, and the driver calls `set_yuno_must_die()` from inside one
side's close callback, which logs `"Exit to die"` synchronously and shuts the
yuno down. The other side's last log is swallowed, or is not, depending on
whether the two close completions land in the same io_uring batch. The
**count** moved and not only the order, so no ordered list could be right — and
the test failed on a busy box naming a message that was perfectly correct.

```C
set_expected_results_unordered(
    APP_NAME,
    errors_list,  // the messages the run may emit, in any order
    NULL,         // expected json, NULL to check only the logs
    NULL,         // ignore_keys
    1             // verbose
);
```

---

(test_directory_permission)=
## [`test_directory_permission()`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c#L315)

`test_directory_permission()` checks if a directory has the specified permission mode.

```C
int test_directory_permission(
    const char *path,
    mode_t      permission
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | Path to the directory to be checked. |
| `permission` | `mode_t` | Expected permission mode to compare against. |

**Returns**

Returns `0` if the directory has the expected permission, otherwise returns `-1`.

**Notes**

This function internally retrieves the directory's permission mode and compares it with the expected value.

---

(test_file_permission_and_size)=
## [`test_file_permission_and_size()`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c#L328)

`test_file_permission_and_size()` verifies if a file has the specified permissions and size.

```C
int test_file_permission_and_size(
    const char *path,    
    mode_t      permission,
    off_t       size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | Path to the file to be checked. |
| `permission` | `mode_t` | Expected file permission mode. |
| `size` | `off_t` | Expected file size in bytes. |

**Returns**

Returns `0` if the file matches the expected permissions and size, otherwise returns `-1`.

**Notes**

This function internally calls `file_permission()` and `file_size()` to retrieve the file's attributes.

---

(test_json)=
## [`test_json()`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c#L274)

`test_json()` compares a given JSON object with an expected JSON object and verifies if they match. It also checks for expected and unexpected log messages.

```C
int test_json(
    json_t *jn_found   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_found` | `json_t *` | The JSON object to be tested. It is owned and will be decremented after use. |

**Returns**

Returns `0` if the JSON object matches the expected JSON and all expected log messages are consumed. Returns `-1` if there is a mismatch or unexpected log messages are found.

**Notes**

If both `jn_found` and the expected JSON are `NULL`, only the log messages are checked.
Uses `match_record()` to compare JSON objects.
Calls `check_log_result()` to validate log messages.

⚠️ **`jn_found` is OWNED, so a BORROWED pointer must be increfed at the call.**
`json_array_get()`, `kw_get_dict()`, `treedb_get_node()` and friends answer a
pointer they still own; handing one straight to `test_json()` spends a reference
that was never given, and the next `json_decref()` of the container frees the
same block twice. Three bench tests died of exactly this — with heap corruption
surfacing somewhere else entirely — because they were written before this
function started freeing its argument:

```C
result += test_json(json_incref(record));   // record is borrowed
```

---

(test_json_file)=
## [`test_json_file()`](https://github.com/artgins/yunetas/blob/7.24.0/kernel/c/gobj-c/src/testing.c#L243)

`test_json_file()` compares the JSON content of a file with the expected JSON structure and validates log results.

```C
int test_json_file(
    const char *file
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `file` | `const char *` | Path to the JSON file to be tested. |

**Returns**

Returns `0` if the JSON content matches the expected structure and logs are as expected, otherwise returns `-1`.

**Notes**

Uses `match_record()` to compare the JSON structures.
Calls `check_log_result()` to validate log messages.
If `verbose` mode is enabled, additional debug information is printed.

---

(test_list)=
## `test_list()`

`test_list()` compares two JSON arrays element by element, verifying that each key present in the expected array also exists with an identical value in the found array. The sizes of both arrays must match.

```C
int test_list(
    json_t *found,
    json_t *expected,
    const char *msg,
    ...) JANSSON_ATTRS((format(printf, 3, 4))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `found` | `json_t *` | The JSON array of objects obtained from the operation being tested. |
| `expected` | `json_t *` | The JSON array of objects containing the expected values. Each object's keys are checked against the corresponding object in `found`. |
| `msg` | `const char *` | A `printf`-style format string used to identify the test in error output. |
| `...` | | Variable arguments for the format string. |

**Returns**

Returns `0` if all elements match. Returns a negative value (accumulated `-1` per mismatch) if the array sizes differ or if any key in an expected object does not match the corresponding value in `found`.

**Notes**

Only keys present in the `expected` objects are checked. Extra keys in `found` objects are ignored.
On mismatch, error details are printed to stdout with the formatted message, the mismatched key, index, and both expected and found values.

---

