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

So the file changes when the date changes. Two masks are in use, and a third kind is allowed:

| Mask | File of that day | What it keeps |
|---|---|---|
| `mqtt_broker-W.log` (the yuno logs) | `mqtt_broker-4.log` | 7 files. When a new day starts, the file of the same week day is emptied, because it was last written before this day: it is the file of last week. The name is the retention. |
| `ZZZ-DD_MM_CCYY.log` (the agent audit) | `266-23_09_2026.log` | One new file each day. Nothing is removed unless the user calls [`rotatory_remove_old_files()`](#rotatory_remove_old_files). |
| `logcenter.log` (no date letter: a fixed name) | `logcenter.log` | One file for ever. It is never emptied by a date, only its size rotation (`.OLD`) bounds it. |

When the current file becomes larger than `max_megas_rotatoryfile_size`, the rotatory renames it to `<name>.OLD` (a previous `.OLD` is removed) and starts the file again. So one day keeps at most two files of that size: this is what bounds the yuno logs. The size is checked at the next record, and never at the first record of a new name: at a new day the file of the day before is left as it is, even when its last record took it over the limit. (In 7.25.4, when the last piece of a day took its file over the limit, the first record of the next day renamed the file of the day before to `.OLD`, removing the `.OLD` of that day, so the day kept one piece of two.) A handle set with [`rotatory_keep_all_old_files()`](#rotatory_keep_all_old_files) renames to `<name>.OLD.1`, `<name>.OLD.2`, … instead and removes nothing: the agent audit does this, with a retention by days.

A new file (a new day, or the size limit) calls the callback of [`rotatory_subscribe2newfile()`](#rotatory_subscribe2newfile). The callback runs inside the [`rotatory_write()`](#rotatory_write) of the first record of the new file, before that record is written. So what the callback does (the agent audit applies its retention there) is on the write path of that one record, once a day or once for each size rotation. **The same file opened again is not a new file**, and the callback does not run for it: after a failed write, after the file was removed from the directory, after a truncate whose reopen failed. So a file that refuses writes (a quota, no inodes, `EFBIG`, `EIO`), and is opened again at the next record, does not send a summary email of the logcenter for each log line.

(rotatory-newfile-pending)=
**A new file that cannot be opened.** When the open of a new file fails (no file descriptors, a quota of inodes, a directory that refuses writes for a moment), the callback is kept pending, and it runs once, at the next open that works, with the `old_filename` of before the failure. The name and the day have already moved when the open fails, so the next record opens the same name again, which is not a new file: the pending callback is what keeps it from being lost. In 7.25.4 a failed open of a new name stopped the file for the rest of the day, and the callback of that day (the retention of the agent audit, the summary email of the logcenter) never ran. On a full disk the open is tried again every 100 records (with the free space check), because only the callback can free the space.

(rotatory-exit-on-fail)=
**`exit_on_fail` is for the open.** The last parameter of [`rotatory_open()`](#rotatory_open) applies to that open only. The agent opens its audit with `TRUE`, and every yuno opens its file log with `TRUE`: the process does not start if the file cannot be opened. After the open, a file that cannot be opened again never exits the process. This applies to a new day, a size rotation, a file removed from the directory, the open after a failed write, and a truncate. The rotatory prints one line (stdout and syslog). That record is not written. The next record tries the open again, with no line while it fails. When the open works, one more line is printed, the pending callback runs (see [a new file that cannot be opened](#rotatory-newfile-pending)), and the record is written. Up to 7.25.4 the first of these failures exited a handle opened with `TRUE`: the agent stopped at the first command after midnight if it had no free file descriptor, and the retention of the audit never ran. For example, the audit of a new day, when the process has no free descriptor:

```C
hrotatory_h hr = rotatory_open("/yuneta/realms/agent/agent/audit/ZZZ-DD_MM_CCYY.log",
    0, 500, 20, 02775, 0660, TRUE);     // exits here if the audit cannot be opened now
// ... after midnight, no free file descriptor
rotatory_write(hr, LOG_AUDIT, record, len);   // cannot create the file: one line, 0
rotatory_write(hr, LOG_AUDIT, record, len);   // tried again, no line
// ... a descriptor is free again
rotatory_write(hr, LOG_AUDIT, record, len);   // open: one line, the callback, the record
```

```
_rotatory(): Cannot create '/yuneta/realms/agent/agent/audit/269-26_09_2026.log' file, Too many open files
_rotatory(): '/yuneta/realms/agent/agent/audit/269-26_09_2026.log' is open again
```

(rotatory-rename-fails)=
**A size rotation whose rename fails** (a directory with `chattr +a`, a read-only bind, a MAC denial). Without keep_all the file is emptied, so its size stays bounded. With [`rotatory_keep_all_old_files()`](#rotatory_keep_all_old_files) nothing may be removed: the file is kept and grows over the limit, one line is printed, and the rename is tried again after 60 seconds of the monotonic clock ([`start_msectimer()`](#start_msectimer): a wall clock set back or forward does not move the retry), or at the next name (the next day), not at every record. One line is printed when a rename works again:

```
_rotatory(): Cannot rename '/yuneta/realms/agent/agent/audit/267-24_09_2026.log' to '/yuneta/realms/agent/agent/audit/267-24_09_2026.log.OLD.1', Permission denied, the file is kept and grows, the rename is tried again every minute
_rotatory(): the size rotation of '/yuneta/realms/agent/agent/audit/267-24_09_2026.log' works again
```

(rotatory-clock-set-back)=
**An existing file is emptied only when it is old.** At a new name, and at the first record after the handle is opened, an existing file of the name is emptied only if its name is used again, and it was last written (its `mtime`) before the period that the name stands for now. A name is used again when the mask has a day letter (`DD`, `W`, `ZZZ`) or the month (`MM`), and no year (`CCYY`). The period is the local day, or the month for a mask with `MM` only. That is the file of last week of a `W` mask. A file written in this period or later is appended to. These never empty a file: a mask with the year (the agent audit), a fixed name (a mask with no date letter), and a handle set with [`rotatory_keep_all_old_files()`](#rotatory_keep_all_old_files) right after the open.

| Mask | Last written | Opened on | Emptied? |
|---|---|---|---|
| `log-W.log` | Thursday of last week | Thursday | yes: the file of last week |
| `log-W.log` | this morning | the same day | no |
| `month-MM.log` | the 12th | the 15th of the same month | no: the file of this month |
| `month-MM.log` | a day of an earlier month | any day | yes: the file of last year |
| `app.log` | two days ago | today | no: a fixed name |
| `ZZZ-DD_MM_CCYY.log` | (a clock set back) | any day | no: the name has the year |
 Up to 7.25.4 every new name was opened with `"w"`. A clock set back across midnight (for example 7 seconds at 00:00:05) opened the file of the day before again and emptied it. When the clock went forward again, the file of today was emptied too, with its first records. For example, with the audit mask:

```
00:00:05  record "today 1"  -> 267-24_09_2026.log
23:59:58  (clock set back)  -> 266-23_09_2026.log is opened again: now appended to, not emptied
00:00:10  (clock forward)   -> 267-24_09_2026.log is opened again: "today 1" is kept
```

Up to 7.25.4 [`rotatory_open()`](#rotatory_open) always appended. A yuno that started on a Monday went on with the file of last Monday, and that one file held the records of 8 days.

**What one [`rotatory_write()`](#rotatory_write) costs.** The file is checked once for each record, before its first piece (the priority header, the text, the `"\n"`), so a record is never split between two files. The check is one `fstat()` of the open file: it gives the size (for the size limit) and the link count (a file removed from the directory is created again). The name is made again only when the time leaves the local day of the current name (midnight, or the clock set to another day), so `localtime()` does not run for each record. Measured on the agent's audit record (two writes of 300 bytes and 1 byte, with the benchmark `performance/c/perf_rotatory`): 6.2 µs up to 7.25.4, 0.55 µs in 7.25.5 (the figures of `performance/c/README.md`).

**A piece of 0 bytes, and a write that fails.** A piece of 0 bytes writes nothing. Up to 7.25.4 `fwrite()` of 0 bytes was taken as a failure, and the file was closed until the next name, the next day. A write that really fails (the disk, a file size limit) closes the file, and the next record opens it again. The rotatory prints one line when the writes fail, and one when a record reaches the file again (the first record after a failure is flushed at once to know it). The file opened again is the same file: the callback of [`rotatory_subscribe2newfile()`](#rotatory_subscribe2newfile) does not run.

Two small differences from 7.25.4: a file RENAMED by another program is not noticed (the record goes on to the renamed file, until the next new file), and the free-disk check (`min_free_disk_percentage`) runs every 100 records instead of every 100 pieces.

(rotatory-disk-full)=
**A full disk.** Every 100 records, each handle checks the free space of its own disk. Below `min_free_disk_percentage` that handle stops writing: its records are dropped. It goes on checking every 100 records, and when the free space is back at or above the limit it writes again. Other handles, on other disks, are not affected. It prints one line when it stops and one when it writes again, to stdout and syslog (the rotatory is the sink of the log, it cannot log through itself):

```
rotatory(): stop logging to '/yuneta/realms/agent/agent/logs/yuneta_agent-4.log' because full disk: 9% free (<10%)
rotatory(): logging to '/yuneta/realms/agent/agent/logs/yuneta_agent-4.log' again: 23% free (>=10%), 5210 records were dropped
```

Up to 7.25.4 the state was one flag for every handle of the process, and nothing cleared it: once one disk went below the limit, all the file logs of the process stopped until it was restarted.

A new day is taken also while the disk is full: the file of the new name is opened and the callback of [`rotatory_subscribe2newfile()`](#rotatory_subscribe2newfile) runs. That callback is where the agent audit applies its retention, and the retention is what frees the space. After it, the free space is checked again: if it is back, the record of that moment is written. Without it, the retention would never run while the disk is full, and the audit would not write again by itself. If the file of the new day cannot be opened, the open is tried again every 100 records while the disk is full, and the callback runs at the first open that works (see [a new file that cannot be opened](#rotatory-newfile-pending)).

(rotatory-closed-handle)=
**A closed handle.** Every public function checks that the handle is open before it touches it. After [`rotatory_close()`](#rotatory_close) or [`rotatory_end()`](#rotatory_end), a write, a flush, a truncate or a second close through the old handle does nothing, and [`rotatory_write()`](#rotatory_write) answers `0`. This matters because others keep the handle: the file log handler of the logger keeps it, and the process logs after `rotatory_end()`. The check compares pointers with the list of open handles (a few), and it costs nothing measurable: 551 ns per audit record with it, 586-620 ns without.

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

If `hr` is `NULL`, is already closed, or the rotatory system is not initialized, the function does nothing. So a second close is harmless, and so is the close that the logger does when its handler is deleted after [`rotatory_end()`](#rotatory_end). See [a closed handle](#rotatory-closed-handle).

**Example**

```C
hrotatory_h hr = rotatory_open("/yuneta/realms/agent/agent/logs/W.log", 0, 0, 0, 0, 0, FALSE);
rotatory_write(hr, LOG_INFO, "hello", 5);
rotatory_close(hr);
rotatory_close(hr);     // does nothing
```

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

A handle kept by someone else after this call is harmless: a write through it does nothing (see [a closed handle](#rotatory-closed-handle)). `yuneta_entry_point()` calls it as the very last step, after the memory leak report, so that the report reaches the log file.

**Example**

```C
gobj_end();
print_track_mem();      // still written to the log files
rotatory_end();         // the last call: the log files are closed
```

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

Returns `0`, also when nothing was written (as [`rotatory_write()`](#rotatory_write)). Returns `-1` only when `hr` is `NULL`. It does not return the number of bytes written. A text longer than the `bf_size` of [`rotatory_open()`](#rotatory_open) is cut to that size.

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
| `min_free_disk_percentage` | `size_t` | The minimum free disk space percentage. Below it this handle drops its records, and it writes again when the space is back (see [a full disk](#rotatory-disk-full)). `0` defaults to `10%`. |
| `xpermission` | `int` | The permission mode for directories and executable files. |
| `rpermission` | `int` | The permission mode for regular log files. |
| `exit_on_fail` | `BOOL` | For this open only. If `TRUE`, the process exits (`exit(-1)`) when the directory or the file cannot be created or opened now. If `FALSE`, one line is printed and the function returns `NULL`. A later failure never exits (see [exit_on_fail is for the open](#rotatory-exit-on-fail)). |

**Returns**

Returns a handle to the rotatory log (`hrotatory_h`) on success, or `NULL` on failure.

**Notes**

If the specified log directory does not exist, `rotatory_open()` attempts to create it.
`exit_on_fail` applies to this open only: a file that cannot be opened later is printed and tried again at the next record, and never exits the process (see [exit_on_fail is for the open](#rotatory-exit-on-fail)).
If the log file does not exist, `rotatory_open()` creates a new one with the specified permissions.
If it exists, it is opened to append. It is emptied at the first record only if it is old: a name that is used again (a day letter or the month, no year) whose file was last written before the period of the name (see [File names and rotation](#rotatory-clock-set-back)). The decision waits for the first record, so [`rotatory_keep_all_old_files()`](#rotatory_keep_all_old_files) called right after the open applies to it. A fixed name (`logcenter.log`) is never emptied by a date.

**Example**

```C
// The yuno logs: last week's "Thursday" file is emptied at the first record
hrotatory_h hr = rotatory_open("/yuneta/realms/agent/agent/logs/yuneta_agent-W.log",
    0, 0, 0, 0, 0, TRUE);

// A fixed name: appended to, whatever day it was last written
hrotatory_h hr2 = rotatory_open("/yuneta/realms/utils/logcenter/logs/logcenter.log",
    0, 0, 0, 0, 0, FALSE);
```

Use [`rotatory_close()`](#rotatory_close) to properly close the log handle.

---

(rotatory_keep_all_old_files)=
## [`rotatory_keep_all_old_files()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/rotatory.c#L371)

`rotatory_keep_all_old_files()` makes a size rotation keep every piece of the day: the file is renamed to the first free `<name>.OLD.<n>` (`n` = 1, 2, …) and nothing is removed.

```C
int rotatory_keep_all_old_files(
    hrotatory_h hr,
    BOOL        keep_all
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hr` | `hrotatory_h` | Handle to the rotatory log instance. |
| `keep_all` | `BOOL` | `TRUE`: numbered `.OLD.<n>` pieces, none removed. `FALSE` (the default): one `.OLD`, the previous one removed. |

**Returns**

Returns `0`, or `-1` if the handle is not open.

**Notes**

It is off by default, and the yuno logs keep it off: their `W` mask makes 7 files that are emptied each week, and one `.OLD` per file keeps them bounded (7 × 2 × 8 MB). With `keep_all`, a noisy yuno (traces on) would leave any number of `.OLD.<n>` pieces that the weekly reuse of the name never empties.

Use it with a mask that makes a new name every day and a retention by days ([`rotatory_remove_old_files()`](#rotatory_remove_old_files)), which removes the `.OLD.<n>` pieces with the rest. The agent audit does this, because a piece of audit must never be removed by the size of the day: up to 7.25.4 a day that crossed the limit twice lost its first part.

A handle with `keep_all` never empties an existing file when its name comes back (for example, a clock set back across midnight): the records are appended. Call it right after [`rotatory_open()`](#rotatory_open), before the first record: the file that the open found is judged at that first record. See [An existing file is emptied only when it is old](#rotatory-clock-set-back).

A rename that fails with `keep_all` keeps the file and tries again after 60 seconds of the monotonic clock, not at every record: see [a size rotation whose rename fails](#rotatory-rename-fails).

After 9999 pieces in one day, the last one is replaced (and a line is printed).

**Example**

```C
hrotatory_h hr = rotatory_open(
    "/yuneta/realms/agent/agent/audit/ZZZ-DD_MM_CCYY.log",
    0, 500, 20, 02775, 0660, TRUE
);
rotatory_keep_all_old_files(hr, TRUE);
// a big day: 266-23_09_2026.log.OLD.1, 266-23_09_2026.log.OLD.2, 266-23_09_2026.log
```

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

Returns a pointer to the path of the current file of the handle. Returns `""` (an empty string, never `NULL`) when the handle is not open: `NULL`, or closed by [`rotatory_close()`](#rotatory_close) or [`rotatory_end()`](#rotatory_end).

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
- Its name has the shape of the mask: a digit in each position that the date fills, the other characters equal to the mask. A `.OLD` or a `.OLD.<n>` at the end is accepted.
- It is a regular file. A symbolic link, a directory or any other type stays. A link is never followed.
- It is not the current file or one of its pieces (`.OLD`, `.OLD.<n>`).
- Its `mtime` is older than `keep_days` days.

A mask with no date letters matches only the current file, so the function removes nothing.

The rotatory never calls this function by itself. Call it after [`rotatory_open()`](#rotatory_open) and from the callback of [`rotatory_subscribe2newfile()`](#rotatory_subscribe2newfile). That callback runs inside the [`rotatory_write()`](#rotatory_write) of the first record of a new file, so the sweep is on the write path of that one record (once a day, or at a size rotation). It reads the directory once. An unlink that fails is logged, and the sweep continues with the next file.

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
The function appends a newline character (`\n`) to the log message. A `len` of `0` writes the newline only (and the header).
A write that fails closes the file, and the next record opens it again.
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

Returns `0`, or `-1` if the handle is not open (`NULL`, or closed). In that case one line is printed to syslog with `print_error()`, and no callback is registered.

**Notes**

Only one callback can be registered per rotatory log instance. Calling this function again replaces the previous callback and user data.

The callback runs inside the [`rotatory_write()`](#rotatory_write) of the first record of the new file, before that record is written. A new file is a new name (a new day) or a size rotation; at a size rotation `old_filename` and `new_filename` are the same name. The same file opened again is not a new file, and the callback does not run: after a failed write, after the file was removed, after a truncate whose reopen failed. When the open of a new file fails, the callback runs once at the next open that works, with the `old_filename` of before the failure (see [a new file that cannot be opened](#rotatory-newfile-pending)).

**Example**

```C
PRIVATE int on_new_log_file(void *user_data, const char *old_filename, const char *new_filename)
{
    // once a day, or once for each size rotation; never for each record
    send_daily_summary(user_data);
    return 0;
}

rotatory_subscribe2newfile(hr, on_new_log_file, gobj);
```

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

The function flushes the file before truncating. The file is reopened in write mode (`"w"`), which discards all previous content. If the file cannot be reopened, one line is printed (stdout and syslog), and the next record opens the file again (appended to). The process is never exited, also for a handle opened with `exit_on_fail` (see [exit_on_fail is for the open](#rotatory-exit-on-fail)).

---

