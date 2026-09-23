# Rotatory

Rotating file-based log handler with a size limit, a file name made from the date, and an optional retention. Creates the output directory if needed.

Source code:

- [`rotatory.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.h)
- [`rotatory.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c)

(rotatory-file-names)=
## File names and rotation

The last segment of the `path` given to [`rotatory_open()`](#rotatory_open) is a **mask**. The rotatory makes the file name from the mask and the current local date. These letters of the mask get digits; all other characters stay as they are:

| Letters | Replaced by | Example (Wednesday 23 September 2026) |
|---|---|---|
| `DD` | day of the month, `01`-`31` | `23` |
| `MM` | month, `01`-`12` | `09` |
| `CCYY` | year | `2026` |
| `W` | day of the week, `1` (Sunday) - `7` | `4` |
| `ZZZ` | day of the year, `001`-`366` | `266` |

So the file changes when the date changes. Two masks are in use:

| Mask | File of that day | What it keeps |
|---|---|---|
| `mqtt_broker-W.log` (the yuno logs) | `mqtt_broker-4.log` | 7 files. When a new day starts, the file of the same week day is opened with `"w"`, so the file of last week is emptied. The name is the retention. |
| `ZZZ-DD_MM_CCYY.log` (the agent audit) | `266-23_09_2026.log` | One new file each day, never used again. Nothing is removed unless the user calls [`rotatory_remove_old_files()`](#rotatory_remove_old_files). |

When the current file becomes larger than `max_megas_rotatoryfile_size`, the rotatory renames it to `<name>.OLD` (a previous `.OLD` is removed) and starts the file again. So one day keeps at most two files of that size.

A new file (a new day, or the size limit) calls the callback of [`rotatory_subscribe2newfile()`](#rotatory_subscribe2newfile). Nothing else happens on the write path.

**What one [`rotatory_write()`](#rotatory_write) costs.** The file is checked once for each record, before its first piece (the priority header, the text, the `"\n"`), so a record is never split between two files. The check is one `fstat()` of the open file: it gives the size (for the size limit) and the link count (a file removed from the directory is created again). The name is made again only when the time leaves the local day of the current name (midnight, or the clock set to another day), so `localtime()` does not run for each record. Measured on the agent's audit record (two writes of 300 bytes and 1 byte): 7.2 µs up to 7.25.4, 0.59 µs after.

Two small differences from 7.25.4: a file RENAMED by another program is not noticed (the record goes on to the renamed file, until the next new file), and the free-disk check (`min_free_disk_percentage`) runs every 100 records instead of every 100 pieces.

(rotatory_close)=
## [`rotatory_close()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L280)

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
## [`rotatory_end()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L109)

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
## [`rotatory_flush()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L392)

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

Flushing makes sure that all buffered log data is written to disk, reducing the risk of data loss in case of a crash.

---

(rotatory_fwrite)=
## [`rotatory_fwrite()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L351)

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
| `hr_` | `hrotatory_h` | Handle to the rotatory log instance. |
| `priority` | `int` | Logging priority level, determining the severity of the message. |
| `format` | `const char *` | Format string specifying how subsequent arguments are formatted. |
| `...` | `variadic` | Additional arguments corresponding to the format string. |

**Returns**

Returns the number of bytes written on success, or `-1` if an error occurs.

**Notes**

This function formats the log message using `vsnprintf()` and then writes it using [`rotatory_write()`](#rotatory_write).

---

(rotatory_open)=
## [`rotatory_open()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L122)

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
| `bf_size` | `size_t` | The buffer size for writing logs. `0` defaults to `64K`. |
| `max_megas_rotatoryfile_size` | `size_t` | The maximum size of a rotatory log file in megabytes. `0` defaults to `8MB`. |
| `min_free_disk_percentage` | `size_t` | The minimum free disk space percentage before stopping logging. `0` defaults to `10%`. |
| `xpermission` | `int` | The permission mode for directories and executable files. |
| `rpermission` | `int` | The permission mode for regular log files. |
| `exit_on_fail` | `BOOL` | If `TRUE`, the process exits on failure. Otherwise, logs an error. |

**Returns**

Returns a handle to the rotatory log (`hrotatory_h`) on success, or `NULL` on failure.

**Notes**

If the specified log directory does not exist, `rotatory_open()` attempts to create it.
If the log file does not exist, `rotatory_open()` creates a new one with the specified permissions.
Use [`rotatory_close()`](#rotatory_close) to properly close the log handle.

---

(rotatory_path)=
## [`rotatory_path()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L652)

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

The returned pointer is managed internally and must not be modified or freed by the caller.

---

(rotatory_remove_old_files)=
## [`rotatory_remove_old_files()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L711)

`rotatory_remove_old_files()` removes the files of this rotatory that are older than `keep_days`. It is the retention of a mask that makes a new name every day, such as `ZZZ-DD_MM_CCYY.log`.

```C
int rotatory_remove_old_files(
    hrotatory_h hr,
    unsigned    keep_days,
    json_t     *jn_removed,
    uint64_t   *removed_bytes
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance. |
| `keep_days` | `unsigned` | Files with an `mtime` older than this number of days are removed. `0` removes nothing. |
| `jn_removed` | `json_t *` | Optional, not owned. A list: the name of each removed file is appended to it. |
| `removed_bytes` | `uint64_t *` | Optional. Receives the total size of the removed files. |

**Returns**

Returns the number of files removed, or `-1` on error (logged).

**Notes**

Only the files of **this** rotatory are candidates. A file is removed only if all of these are true:

- It is in the directory of the rotatory.
- Its name has the shape of the mask: a digit in each position that the date fills, the other characters equal to the mask. A `.OLD` at the end is accepted.
- It is a regular file. A symbolic link, a directory or any other type stays. A link is never followed.
- It is not the current file or its `.OLD`.
- Its `mtime` is older than `keep_days` days.

A mask with no date letters matches only the current file, so the function removes nothing.

The function is not called on the write path. Call it after [`rotatory_open()`](#rotatory_open) and from the callback of [`rotatory_subscribe2newfile()`](#rotatory_subscribe2newfile). An unlink that fails is logged, and the sweep continues with the next file.

**Example**

Keep 7 days of a daily audit file (this is what `yuneta_agent` does with `audit_keep_days`):

```C
PRIVATE int remove_old_audit_files(hrotatory_h hr)
{
    json_t *jn_removed = json_array();
    uint64_t removed_bytes = 0;
    int removed = rotatory_remove_old_files(hr, 7, jn_removed, &removed_bytes);
    if(removed > 0) {
        gobj_log_info(0, 0,
            "msgset",   "%s", MSGSET_INFO,
            "msg",      "%s", "Old audit files removed",
            "removed",  "%d", removed,
            "files",    "%j", jn_removed,
            NULL
        );
    }
    JSON_DECREF(jn_removed);
    return removed;
}

PRIVATE int on_new_audit_file(void *user_data, const char *old_filename, const char *new_filename)
{
    remove_old_audit_files(user_data);
    return 0;
}

hrotatory_h hr = rotatory_open(
    "/yuneta/realms/agent/agent/audit/ZZZ-DD_MM_CCYY.log",
    0, 500, 20, 02775, 0660, TRUE
);
rotatory_subscribe2newfile(hr, on_new_audit_file, hr);
remove_old_audit_files(hr);     // at start: sweep what is already there
```

---

(rotatory_start_up)=
## [`rotatory_start_up()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L91)

`rotatory_start_up()` initializes the rotatory logging system. This makes sure of it is only initialized once and registering cleanup functions.

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

This function registers [`rotatory_end()`](#rotatory_end) with `atexit()` to make sure that proper cleanup.

---

(rotatory_write)=
## [`rotatory_write()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L318)

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

Returns `0`, also when the record could not be written (disk below `min_free_disk_percentage`, or the file cannot be opened: those are reported with `print_error()`). Returns `-1` only when `hr` or `bf` is `NULL`. The value `0` matters: the logger stops calling the next handlers when a handler returns a negative value.

**Notes**

If `priority` is `LOG_AUDIT`, the message is written without a header.
If `priority` is outside the valid range, it defaults to `LOG_DEBUG`.
The function appends a newline character (`\n`) to the log message.
The file is checked (new day, size limit, file removed) once, before the first piece of the record. See [File names and rotation](#rotatory-file-names).

**Example**

```C
const char *record = "{\"command\":\"list-yunos\"}";
rotatory_write(hr, LOG_AUDIT, record, strlen(record));     // no header
rotatory_write(hr, LOG_INFO, "started", strlen("started")); // "INFO: started\n"
```

---

(rotatory_subscribe2newfile)=
## [`rotatory_subscribe2newfile()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L303)

Registers a callback that is invoked whenever the rotatory log rotates to a new file.

```C
int rotatory_subscribe2newfile(
    hrotatory_h hr,
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename),
    void *user_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance. |
| `cb_newfile` | `int (*)(void *, const char *, const char *)` | Callback function invoked on file rotation. Receives the user data, the old filename, and the new filename. |
| `user_data` | `void *` | Opaque pointer passed to the callback on each invocation. |

**Returns**

Returns 0 on success.

**Notes**

Only one callback can be registered per rotatory log instance. Calling this function again replaces the previous callback and user data.

---

(rotatory_truncate)=
## [`rotatory_truncate()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L375)

Truncates the log file associated with a rotatory log instance, clearing all its contents. If `hr` is NULL, all rotatory log instances are truncated.

```C
void rotatory_truncate(
    hrotatory_h hr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance to truncate, or NULL to truncate all instances. |

**Returns**

This function does not return a value.

**Notes**

The function flushes the file before truncating. The file is reopened in write mode (`"w"`), which discards all previous content. If the file cannot be reopened, an error is printed to stderr.

---

