# Rotatory Functions

Source code in:

- [rotatory.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/rotatory.c)
- [rotatory.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/rotatory.h)

(rotatory_close)=
## `rotatory_close()`

Closes the given `hrotatory_h` instance, flushing and releasing all associated resources.

```C
void rotatory_close(
    hrotatory_h hr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance to be closed. |

**Returns**

This function does not return a value.

**Notes**

If `hr` is `NULL` or the rotatory system is not initialized, the function does nothing.

---

(rotatory_end)=
## `rotatory_end()`

`rotatory_end()` closes all active rotatory log instances and resets the internal state.

```C
void rotatory_end(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

This function iterates through all active rotatory log instances and closes them using [`rotatory_close()`](#rotatory_close).
After execution, the internal initialization flag is reset, preventing further operations until [`rotatory_start_up()`](#rotatory_start_up) is called again.

---

(rotatory_flush)=
## `rotatory_flush()`

`rotatory_flush()` flushes the buffered log data to the corresponding log file. If `hr` is `NULL`, it flushes all active log files.

```C
void rotatory_flush(
    hrotatory_h hr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance. If `NULL`, all log files are flushed. |

**Returns**

This function does not return a value.

**Notes**

Flushing ensures that all buffered log data is written to disk, reducing the risk of data loss in case of a crash.

---

(rotatory_fwrite)=
## `rotatory_fwrite()`

`rotatory_fwrite()` writes a formatted log message to the rotatory log file associated with the given handle.

```C
int rotatory_fwrite(
    hrotatory_h hr_,
    int priority,
    const char *format,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance. |
| `priority` | `int` | Logging priority level, determining the severity of the message. |
| `format` | `const char *` | Format string specifying how subsequent arguments are formatted. |
| `...` | `variadic` | Additional arguments corresponding to the format string. |

**Returns**

Returns the number of bytes written on success, or `-1` if an error occurs.

**Notes**

This function formats the log message using `vsnprintf()` and then writes it using [`rotatory_write()`](#rotatory_write).

---

(rotatory_open)=
## `rotatory_open()`

`rotatory_open()` initializes and opens a rotatory log file with the specified parameters, creating necessary directories if required.

```C
hrotatory_h rotatory_open(
    const char *path,
    size_t      bf_size,
    size_t      max_megas_rotatoryfile_size,
    size_t      min_free_disk_percentage,
    int         xpermission,
    int         rpermission,
    BOOL        exit_on_fail
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file path for the rotatory log. |
| `bf_size` | `size_t` | The buffer size for writing logs; `0` defaults to `64K`. |
| `max_megas_rotatoryfile_size` | `size_t` | The maximum size of a rotatory log file in megabytes; `0` defaults to `8MB`. |
| `min_free_disk_percentage` | `size_t` | The minimum free disk space percentage before stopping logging; `0` defaults to `10%`. |
| `xpermission` | `int` | The permission mode for directories and executable files. |
| `rpermission` | `int` | The permission mode for regular log files. |
| `exit_on_fail` | `BOOL` | If `TRUE`, the process exits on failure; otherwise, logs an error. |

**Returns**

Returns a handle to the rotatory log (`hrotatory_h`) on success, or `NULL` on failure.

**Notes**

If the specified log directory does not exist, `rotatory_open()` attempts to create it.
If the log file does not exist, `rotatory_open()` creates a new one with the specified permissions.
Use [`rotatory_close()`](#rotatory_close) to properly close the log handle.

---

(rotatory_path)=
## `rotatory_path()`

The `rotatory_path()` function retrieves the file path associated with the given rotatory log handle.

```C
const char *rotatory_path(
    hrotatory_h hr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance. |

**Returns**

Returns a pointer to the file path string associated with the given rotatory log handle.

**Notes**

The returned pointer is managed internally and should not be modified or freed by the caller.

---

(rotatory_start_up)=
## `rotatory_start_up()`

`rotatory_start_up()` initializes the rotatory logging system, ensuring it is only initialized once and registering cleanup functions.

```C
int rotatory_start_up(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns `0` on success, or `-1` if the rotatory system is already initialized.

**Notes**

This function registers [`rotatory_end()`](#rotatory_end) with `atexit()` to ensure proper cleanup.

---

(rotatory_write)=
## `rotatory_write()`

`rotatory_write()` writes a log message to the rotatory log file with the specified priority level.

```C
int rotatory_write(
    hrotatory_h  hr,
    int          priority,
    const char*  bf,
    size_t       len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance. |
| `priority` | `int` | Priority level of the log message, ranging from `LOG_EMERG` to `LOG_AUDIT`. |
| `bf` | `const char*` | Pointer to the buffer containing the log message. |
| `len` | `size_t` | Length of the log message in bytes. |

**Returns**

Returns the number of bytes written on success, or `-1` on error.

**Notes**

If `priority` is `LOG_AUDIT`, the message is written without a header.
If `priority` is outside the valid range, it defaults to `LOG_DEBUG`.
The function appends a newline character (`\n`) to the log message.
Internally calls `_rotatory()` to perform the actual writing.

---

(rotatory_subscribe2newfile)=
## `rotatory_subscribe2newfile()`

*Description pending — signature extracted from header.*

```C
int rotatory_subscribe2newfile(
    hrotatory_h hr,
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename),
    void *user_data
);
```

---

(rotatory_truncate)=
## `rotatory_truncate()`

*Description pending — signature extracted from header.*

```C
void rotatory_truncate(
    hrotatory_h hr
);
```

---

