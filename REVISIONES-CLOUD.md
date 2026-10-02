# Cloud review of main

Reviewed up to `e6ace430a` (2026-10-02). What is resolved is removed from
this file. Last check: clean build with no warning, suite 278/278 as user
`yuneta` under `ulimit -Sn 1024`, `timeranger2/test_delete_key_propagation`
20 in 20 (kernel 6.18).

## Open

### 1. timeranger2: use-after-free when the process runs out of descriptors

`kernel/c/timeranger2/src/timeranger2.c`, `keys_are_the_life_of()` (from
`b2f972382`). Found by reading, not reproduced.

`life` is borrowed from `rd_fd_life[key]`. Then
`get_topic_rd_fd(..., TRUE)` opens the content file. On `EMFILE` it calls
`close_fd_rd_files(gobj, topic, "")`, which runs
`json_object_clear(rd_fd_life)` and frees `life`. The function then runs
`json_object_set_new(life, file_id, ...)` on freed memory.

The same recovery also closes `md2_fd`, which was checked with `fstat()`
just before, and the function still returns it in `*md2_fd_`. The reopen
of the content file normally takes that same number, so
`load_first_and_last_record_md()` reads md2 rows from the `.json` file.

Before this commit md2 was opened inside `load_first_and_last_record_md()`,
so the recovery could not leave a stale descriptor behind.

Fix: read `life` again after `get_topic_rd_fd()`, and do not return an
`md2_fd` taken before a call that can close every read descriptor (open
the content file first, or check with `fstat()` again).

### 2. timeranger2: one descriptor per key directory for the life of a feed (risk)

`kernel/c/timeranger2/src/fs_watcher.c`, `add_watch()` with
`FS_FLAG_DIR_FDS`.

A follower keeps one descriptor for each key directory under
`disks/<rt_id>/`, and the count grows with every distinct key written
since the feed opened. These descriptors are outside the `EMFILE` recovery
of `get_topic_rd_fd()`. When they fill the limit:

- every record read fails;
- each new key directory logs "Cannot open a directory to watch it through
  its descriptor";
- that key goes back to the watch by path, which brings back the reborn-key
  race for it.

The packages set `nofile unlimited` for the `yuneta` user, and
`tr2list`/`tr2search` raise their soft limit. Still exposed: a host with a
low hard limit, and any follower process that does not raise its own limit.

### 3. timeranger2: a read by path is left in `consume_link_by_fd()` (risk)

When `openat()` of the link answers `ENOENT` and the directory is still
there, the link is taken as already consumed. The code then calls
`update_new_records_from_disk(..., pin=NULL, -1)`, which reads
`keys/<key>/` by path. If the master deletes and writes the key again in
that window, the new life is read against the old cache, which is the race
that `b2f972382` fixes. The window is a few microseconds.

Fix: skip that read (the scan that consumed the link already read it), or
pin it like the other reads.

### 4. Nits of `b2f972382`

- **A key directory that cannot be watched is treated as gone.** When
  `add_watch()` fails (`ENOSPC` at `max_user_watches`, `ENOMEM`),
  `place_new_key_dir_scans` marks it not alive, and the links already in it
  are never read. The failure is logged.
- **Misleading log.** `fs_watcher.c`: any failure of `inotify_add_watch()`
  through `/proc/self/fd/N`, `ENOSPC` included, is logged as "/proc
  missing", and only once per process.
- **`build_path()` not used.** New paths are built with `snprintf()` and
  their truncation is not checked: `fs_watcher.c` (the `/proc/self/fd`
  path), `timeranger2.c` `keys_are_the_life_of()` (`md2_path`,
  `data_path`).
- **Indentation.** In `load_first_and_last_record_md()` the new
  `if(own_fd)` bodies are indented wrongly.
- **Redundant code.** `fs_watcher.c` sets `event_wd` twice.
- **No descriptor count in the test.** The "dir fds" test does not count
  open descriptors (`/proc/self/fd`) after the stop, so a leak would not be
  caught.

## Checked and correct

- **`a89a6992e` (`yev_loop`, kernel too old):** the probe frees itself. A
  kernel before 5.19 answers `-EINVAL` to the cancel flags, and a newer one
  answers 0 or `-ENOENT`, so the check is sound.
- **`23bd58aec` (`gclass_create()`):** it refuses a required secret before
  another required parameter. The check stops at the first parameter that
  is not required, as the parser does.
- **`b2f972382` (key directories through descriptors):**
  - Every directory descriptor is closed on destroy, stop, GONE, a dropped
    watch and a stale watch after an overflow.
  - A directory removed and made again cannot be reached through the old
    descriptor.
  - `unlinkat()` goes through the watched directory's descriptor.
  - The master side is unchanged.
  - The hot path costs about the same calls as before.
