# Debug / Backtrace Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(init_backtrace_with_backtrace)=
## `init_backtrace_with_backtrace()`

Initializes the backtrace system for capturing stack traces using the `backtrace` library. This function sets up the backtrace state for later use in debugging.

```C
int init_backtrace_with_backtrace(
    const char *program
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `program` | `const char *` | The name of the program, used to initialize the backtrace state. |

**Returns**

Returns 0 on success, or -1 if the backtrace state could not be created.

**Notes**

This function should be called early in the program execution to enable stack trace capturing. It is only available when `CONFIG_DEBUG_WITH_BACKTRACE` is enabled.

---

(show_backtrace_with_backtrace)=
## `show_backtrace_with_backtrace()`

Generates and logs a backtrace using the `backtrace` library, providing detailed function call information.

```C
void show_backtrace_with_backtrace(
    loghandler_fwrite_fn_t fwrite_fn,
    void *h
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fwrite_fn` | `loghandler_fwrite_fn_t` | Function pointer for writing log output. |
| `h` | `void *` | Handle to be passed to the log function. |

**Returns**

This function does not return a value.

**Notes**

This function is only available when `CONFIG_DEBUG_WITH_BACKTRACE` is enabled. It utilizes the `backtrace` library to capture and log stack traces.

---

(tdump)=
## `tdump()`

`tdump` prints a formatted hexadecimal and ASCII dump of a given byte array.

```C
void tdump(
    const char *prefix,
    const uint8_t *s,
    size_t len,
    view_fn_t view,
    int nivel
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `prefix` | `const char *` | A string prefix to prepend to each line of the dump. |
| `s` | `const uint8_t *` | Pointer to the byte array to be dumped. |
| `len` | `size_t` | The number of bytes to dump. |
| `view` | `view_fn_t` | A function pointer for output formatting, typically `printf`. |
| `nivel` | `int` | The verbosity level: 1 for hex only, 2 for hex and ASCII, 3 for hex, ASCII, and byte index. |

**Returns**

This function does not return a value.

**Notes**

If `nivel` is 0, it defaults to 3. The function formats the output in a structured manner, displaying hexadecimal values alongside their ASCII representation.

---

(tdump2json)=
## `tdump2json()`

`tdump2json` converts a binary buffer into a JSON object, representing the data in a structured hexadecimal and ASCII format.

```C
json_t *tdump2json(
    const uint8_t *s,
    size_t len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const uint8_t *` | Pointer to the binary data buffer. |
| `len` | `size_t` | Length of the binary data buffer. |

**Returns**

A JSON object where each key represents an address offset and the value is a string containing the hexadecimal and ASCII representation of the corresponding data.

**Notes**

This function is useful for debugging and inspecting binary data in a human-readable format.

---
