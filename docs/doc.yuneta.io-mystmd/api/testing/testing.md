# Testing

Helpers used by Yuneta's C test suite: expected-log capture, JSON result comparison with key-ignoring, ordered/unordered matching, and human-readable diffs on failure.

Source code:

- [`testing.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/testing.h)
- [`testing.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/testing.c)

(capture_log_write)=
## `capture_log_write()`

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

Returns `-1` to indicate that the log message has been processed internally.

**Notes**

If a log message matches an expected message, it is removed from the expected list. Otherwise, it is added to the unexpected log messages list.

---

(set_expected_results)=
## `set_expected_results()`

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
| `verbose` | `BOOL` | Flag indicating whether verbose output should be enabled. |

**Returns**

This function does not return a value.

**Notes**

The function resets previously stored expected results before setting new ones.
If `verbose` is enabled, the function prints the test name to the console.
The function initializes `expected_log_messages`, `unexpected_log_messages`, and `expected` as JSON arrays if they are not provided.

---

(test_directory_permission)=
## `test_directory_permission()`

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
## `test_file_permission_and_size()`

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
## `test_json()`

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

---

(test_json_file)=
## `test_json_file()`

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

*Description pending — signature extracted from header.*

```C
int test_list(
    json_t *found,
    json_t *expected,
    const char *msg,
    ...) JANSSON_ATTRS((format(printf, 3, 4))
);
```

---

