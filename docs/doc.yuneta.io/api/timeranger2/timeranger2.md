# timeranger2

**Append-only time-series storage** for Yuneta. Records are grouped into
**topics**, keyed by a primary id, and indexed for fast access by time
range or row id.

## Higher-level stores built on top

- **`tr_msg2db`** — dict-style message store (one value per key).
- **`tr_queue`** — message queue with at-least-once semantics (used by
  the MQTT broker).
- **TreeDB** — graph memory database with hook/fkey relationships. See
  the [`tr_treedb` API](treedb.md) and the
  [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) for
  the full rules.

## Core API (selected)

```c
tranger2_startup(...) / tranger2_shutdown(...)
tranger2_open_topic(...) / tranger2_close_topic(...)
tranger2_append_record(...)
tranger2_iterator_open(...) / tranger2_iterator_next(...) / _close(...)
```

## Persistence semantics

Every call to `tranger2_append_record()` writes one record to disk,
increments the per-key `g_rowid` (monotonic, never resets) and `i_rowid`
(row position in the `.md2` index file).

:::{important}
TreeDB relies on these semantics for its `link` / `unlink` behavior:
**link/unlink saves only the child node** (the one carrying the fkey
field), never the parent. See the
[TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) for the
full rules and `g_rowid` tracing examples.
:::

## Filesystem watcher

`src/fs_watcher.c` implements an inotify-based watcher (`fs_event_t`)
used by `root-linux/C_FS` and by the stores themselves to react to
on-disk changes. See the **fs_watcher** page in the sidebar.

## CLI companions

| Tool | Purpose |
|---|---|
| `utils/c/tr2list` | List records in a topic |
| `utils/c/tr2keys` | List keys in a topic |
| `utils/c/tr2search` | Search by filter |
| `utils/c/tr2migrate` | Migrate from legacy timeranger v1 |
| `utils/c/list_queue_msgs2` | List a `tr_queue` queue |
| `utils/c/msg2db_list` | List a `tr_msg2db` topic |
| `utils/c/treedb_list` | List TreeDB nodes |

## Tests

`tests/c/timeranger2`, `tests/c/tr_msg`, `tests/c/tr_queue`,
`tests/c/tr_treedb`, `tests/c/tr_treedb_link_events`.

## Source code

- [`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h)
- [`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c)

## Function reference

(tranger2_append_record)=
## [`tranger2_append_record()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L2885)

Appends a new record to a topic in the TimeRanger database. **Master-only.** If
`__t__` is zero a timestamp is assigned (milliseconds when the topic is `sf_t_ms`,
else seconds). The new record's metadata is returned through the required
`md_record_ex` **out-parameter** — the return value is a status code, not the
metadata. `jn_record` is owned (consumed, even on error).

```C
int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,
    uint16_t user_flag,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic where the record will be appended. |
| `__t__` | `uint64_t` | Timestamp of the record. If set to 0, the current time is used. |
| `user_flag` | `uint16_t` | User-defined flag associated with the record. |
| `md_record_ex` | `md2_record_ex_t *` | Pointer to the metadata structure of the record. This field is required. |
| `jn_record` | `json_t *` | JSON object containing the record data. Ownership is transferred to the function. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

The function makes sure that the record is appended to the specified topic in [`tranger2_startup()`](<#tranger2_startup>). If the topic does not exist, it must be created using [`tranger2_create_topic()`](<#tranger2_create_topic>) before calling this function.

**Reading the metadata back.** The metadata of the new record is returned in
`md_record_ex`, and that is where a caller reads it: `md.g_rowid` is the
key's global rowid (the one that never resets), `md.rowid` the row's position
inside its `.md2` file (`i_rowid`), then `__t__`, `__tm__`, `__offset__`,
`__size__`, `system_flag`, `user_flag`. Hand the function your only reference:

```C
md2_record_ex_t md;
if(tranger2_append_record(tranger, "topic", 0, 0, &md, record) == 0) {
    json_int_t g_rowid = (json_int_t)md.g_rowid;
}
```

The function does not add a `__md_tranger__` to the record for the caller.
It adds one (`g_rowid`, `i_rowid`, `t`, `tm`, `offset`, `size`,
`system_flag`, `user_flag`, the same dict a load attaches) only to the record
it hands to a realtime list that takes it: one that wants the key and is not
an `only_md` feed (an `only_md` feed is handed `NULL`). A list open on another
key does not make the append build it. Nor is a `__md_tranger__` ever stored:
one the record carries in, from an earlier append or from a load, is the
metadata of THAT record and is dropped before the content is written.

The record is **owned** by the function, and these changes are made to the
object itself, not to a copy: a list hands it on without copying it, on the
hot path of every append. So a caller that keeps a reference of its own sees
them, and reads the metadata from `md_record_ex` anyway:

```C
json_incref(record);                    // the caller keeps one
tranger2_append_record(tranger, "topic", 0, 0, &md, record);
// No list takes the key:   record has no __md_tranger__ (a carried one is gone)
// A list takes the record: record HAS the list's fresh __md_tranger__
// On an sf_rowid_key topic: record also has "__rowid__"
json_decref(record);
```

**A `__t__` that belongs to an earlier file.** The record goes to the file that
its `__t__` selects (through the topic's `filename_mask`), even if that file is
not the newest one. A key's global rowid counts the records in the order of its
files, so the new record takes its place in that order, and **every record of
the later files moves one place up**:

```C
/*  filename_mask "%Y-%m-%d"; key 1 has A (2000-01-01), B (2000-01-02)  */
tranger2_append_record(tranger, "topic", t_2000_01_01 + 10, 0, &md, jn_c);
/*  C gets g_rowid 2, and B is now 3: an iterator serves A, C, B  */
```

The `g_rowid` in `md_record_ex` is its place, the same number a reload gives
(and stores in the loaded record's `__md_tranger__`). If you keep global rowids, for example as a page position, a
record written into an earlier file makes the rowids of the later records
stale. Records written with `__t__ = 0` (now) always go to the last file, and
move nothing. Until 2026-09-15 the cache in memory disagreed with the disk
until the next reload: it served the wrong record, and a follower re-published
the whole file.

**An append that fails is not half written.** The content (`.json`) is written
first and the md2 row after. When the md2 cannot be opened or created, sought
or written, the function returns `-1` and cuts the content file back to where
the record began (a row written in part is cut from the `.md2` too): the file
keeps no record that no row names. If the cut fails as well, it logs *"Cannot
cut back the content of an append whose md2 row was not written: the content
file keeps a record no row names"*. A kill or a power cut between the two
writes still leaves that shape; the next open ignores it with a warning (see
*After a restart too*, under [`tranger2_open_iterator()`](#tranger2_open_iterator)).
Up to 7.25.4 the content was left behind.

The cut comes BEFORE the critical that reports the failure. That matters with
the exit bit of the tranger's `on_critical_error` (`LOG_OPT_EXIT_ZERO`, `2`,
the default of `C_TRANGER`, and what `C_TREEDB` passes with its
`exit_on_error`): `gobj_log_critical()` then ends the process inside the log
call, and nothing after it runs. When the md2 cannot be opened or created, the cause is
logged without leaving, the content is cut back, and then one more critical
exits:

```text
CRITICAL create_file: Cannot create json file  path=<store>/items/keys/A/2000-01-06.md2
CRITICAL get_topic_wr_fd: Cannot open file to write  path=<store>/items/keys/A/2000-01-06.md2
CRITICAL tranger2_append_record: Cannot append record, its md2 file cannot be opened: its content
         was cut back  topic=items key=A file_id=2000-01-06          <- exit(0) here, with 2
```

The same order holds for a content write that fails part way: its part is cut
back before the critical.

**An append into a file flagged unreadable** (a `.md2` the cache build could
not count, see the same section) counts the file again first. If it can be read
now, it gets its cell, the flag of that file goes (*"md2 file of the key
readable again: it is counted, and the key is not flagged for it"*), and the
append goes on. If not, the append is refused with `-1` and writes nothing
(*"Cannot append record, its file is flagged unreadable: its row would follow
rows no cell counts"*): its row would have been counted as the file's first,
and a load read an OLD row of the file in its place.

```C
/*  key A: the md2 of day 2 could not be opened at the open (mode 0000)     */
tranger2_append_record(tranger, "topic", t_day2, 0, &md, rec1);   // -1, nothing written
chmod(path_md2_day2, 0660);
tranger2_append_record(tranger, "topic", t_day2, 0, &md, rec2);   // 0: day 2 counted,
                                                                  // A loads whole again
```

The file counted again gets its cell in the MIDDLE of the key, and every
global rowid after it moves up by the rows of the file. The iterators open on
the key follow: an unfiltered one takes its segments again at its next page; a
FILTERED one (a paging iterator with an index, see
[`tranger2_iterator_get_page()`](#tranger2_iterator_get_page)) takes its
segments and its index again at once, as its open would build them now. So
its index gains the rows of the file, with their new rowids. Like every append
after the open of a filtered iterator, the append that caused the count is not
in its index. For example:

```C
/*  key A: v1 (day 1), v2 (day 2, flagged), v3 (day 3), v4 (day 4)          */
it = tranger2_open_iterator(tranger, "topic", "A",
        json_pack("{s:I}", "from_t", (json_int_t)t_day1), NULL, "pager", "me", NULL, NULL);
/*  its page:  total_rows 3, v1 v3 v4                                         */
chmod(path_md2_day2, 0660);
tranger2_append_record(tranger, "topic", t_day2, 0, &md, rec_v9); // counts day 2
/*  its page:  total_rows 4, v1 v2 v3 v4                                     */
```

---

(tranger2_backup_topic)=
## [`tranger2_backup_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1889)

Creates a backup of a topic in the TimeRanger database. If `backup_path` is empty, the topic path is used. If `backup_name` is empty, the backup file is named `topic_name.bak`. If `overwrite_backup` is true and the backup exists, it is overwritten unless `tranger_backup_deleting_callback` returns true.

```C
json_t *tranger2_backup_topic(
    json_t *tranger,
    const char *topic_name,
    const char *backup_path,
    const char *backup_name,
    BOOL overwrite_backup,
    tranger_backup_deleting_callback_t tranger_backup_deleting_callback
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to back up. |
| `backup_path` | `const char *` | The directory where the backup will be stored. If empty, the topic path is used. |
| `backup_name` | `const char *` | The name of the backup file. If empty, `topic_name.bak` is used. |
| `overwrite_backup` | `BOOL` | If true, an existing backup is overwritten unless `tranger_backup_deleting_callback` prevents it. |
| `tranger_backup_deleting_callback` | `tranger_backup_deleting_callback_t` | A callback function that determines whether an existing backup must be deleted before overwriting. |

**Returns**

A JSON object representing the new topic backup.

**Notes**

If `overwrite_backup` is true and the backup exists, `tranger_backup_deleting_callback` is called. If it returns true, the existing backup is not removed.

Only a master can back up a topic (the backup moves its directory): on a replica it returns `NULL` with *"Only master can back up"*. The topic name follows the [topic name rule](<#timeranger2-topic-name-rule>).

---

(tranger2_close_all_lists)=
## [`tranger2_close_all_lists()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L10065)

Closes the iterators, `rt_mem` and `rt_disk` lists of a topic that belong to a
given `creator`. An empty `creator` closes **all** of them. A non-empty `creator`
with an empty `rt_id` closes all of that creator's. With both set it narrows to a
single `rt_id`.

```C
int tranger2_close_all_lists(
    json_t *tranger,
    const char *topic_name,
    const char *creator,    // if empty, remove all
    const char *rt_id       // if empty, remove all lists of creator
);
```

:::{note}
The parameter order is `(creator, rt_id)` — matching the implementation and every
caller. Earlier headers listed them swapped as `(rt_id, creator)`. A caller that
trusted that order filtered by the wrong field.
:::

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic whose lists must be closed. |
| `creator` | `const char *` | Creator identifier. If empty, all lists are removed. |
| `rt_id` | `const char *` | Realtime list identifier. If empty, all lists of the `creator` are removed. |

**Returns**

Returns `0` on success, or a negative value on failure. No-op (`0`) if the topic
is not opened.

**Notes**

This closes the iterators, `rt_mem` and `rt_disk` lists associated with a specific
`creator`, preventing resource leaks. If `creator` is empty, every list of the
topic is closed regardless of `rt_id`.

---

(tranger2_close_iterator)=
## [`tranger2_close_iterator()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L7684)

Closes an iterator in the TimeRanger 2 database, releasing associated resources.

```C
int tranger2_close_iterator(
    json_t *tranger,
    json_t *iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger 2 database instance. |
| `iterator` | `json_t *` | A pointer to the iterator object to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

Closing an iterator makes sure that any allocated memory or resources are properly released.

---

(tranger2_close_list)=
## [`tranger2_close_list()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L10037)

The function `tranger2_close_list()` closes a previously opened list, which can be a real-time memory list (`rt_mem`), a real-time disk list (`rt_disk`), or a non-real-time list.

```C
int tranger2_close_list(
    json_t *tranger,
    json_t *list
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `list` | `json_t *` | A pointer to the list object to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function makes sure that resources associated with the list are properly released. If the list is a real-time list, it stops receiving updates.

---

(tranger2_close_rt_disk)=
## [`tranger2_close_rt_disk()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4842)

The `tranger2_close_rt_disk()` function closes a previously opened real-time disk stream in the TimeRanger database, releasing associated resources.

```C
int tranger2_close_rt_disk(
    json_t *tranger,
    json_t *disk
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `disk` | `json_t *` | A pointer to the real-time disk stream to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function must be called when a real-time disk stream is no longer needed to free resources.

---

(tranger2_close_rt_mem)=
## [`tranger2_close_rt_mem()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4525)

The `tranger2_close_rt_mem()` function closes a real-time memory stream associated with a given TimeRanger instance.

```C
int tranger2_close_rt_mem(
    json_t *tranger,
    json_t *mem
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger instance managing the real-time memory stream. |
| `mem` | `json_t *` | A pointer to the real-time memory stream to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

Closing a real-time memory stream using [`tranger2_close_rt_mem()`](#tranger2_close_rt_mem) makes sure that resources are properly released.

---

(tranger2_close_topic)=
## [`tranger2_close_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1742)

The `tranger2_close_topic()` function closes an open topic in the TimeRanger database, releasing associated resources.

```C
int tranger2_close_topic(
    json_t  *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

Closing a topic does not delete its data. It only releases resources associated with the open topic.

---

(tranger2_create_topic)=
## [`tranger2_create_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L911)

The `tranger2_create_topic()` function creates a new topic in the TimeRanger database if it does not already exist. If the topic exists, it returns the existing topic metadata. The function makes sure that the topic is properly initialized with the specified primary key, time key, system flags, and additional metadata.

```C
json_t *tranger2_create_topic(
    json_t              *tranger,
    const char          *topic_name,
    const char          *pkey,
    const char          *tkey,
    json_t              *jn_topic_ext,
    system_flag2_t      system_flag,
    json_t              *jn_cols,
    json_t              *jn_var
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. If the topic exists, only `tranger` and `topic_name` are required. |
| `topic_name` | `const char *` | Name of the topic to be created. |
| `pkey` | `const char *` | Primary key field of the topic. If not specified, the key type defaults to `sf_int_key`. |
| `tkey` | `const char *` | Time key field of the topic. |
| `jn_topic_ext` | `json_t *` | JSON object containing additional topic parameters. See [`topic_json_desc`](#topic_json_desc) for details. This parameter is owned by the function. |
| `system_flag` | `system_flag2_t` | System flags for the topic, defining its behavior and properties. |
| `jn_cols` | `json_t *` | JSON object defining the topic's columns. This parameter is owned by the function. |
| `jn_var` | `json_t *` | JSON object containing variable metadata for the topic. This parameter is owned by the function. |

**Returns**

Returns a JSON object representing the topic metadata. The returned JSON object is not owned by the caller and must not be modified or freed.

`NULL` (logged) for a refused name, a topic that does not exist on a replica,
an empty `pkey` or no key type.

**Notes**

This function is idempotent. This means that if the topic already exists, it will return the existing topic metadata instead of creating a new one. If the primary key (`pkey`) is not specified, the function defaults to `sf_string_key` if `pkey` is defined, otherwise it defaults to `sf_int_key`.

**Only the master writes, and it asks first.** On a replica, or on a master
that lost its lock while it was stopped (see [`tranger2_stop()`](#tranger2_stop)),
an existing topic is opened read-only and nothing is written, and a new one is
refused with *"Cannot open TimeRanger topic. Not found and no master"*.

**A new topic marks the files whose `tm` goes back.** Its `topic_desc.json`
carries `"marks_tm_unordered": true`, and the master leaves
`<file>.tm_unordered` beside an md2 file when a record's `tm` is below the
file's highest one. What a `tm` condition does with it is in
[`tranger2_open_iterator()`](#tranger2_open_iterator). A topic created by
7.25.4 or earlier has no such key; it gets it from
[`tranger2_mark_tm_order()`](<#tranger2_mark_tm_order>), the operator's
migration.

```json
{
    "topic_name": "readings",
    "pkey": "id",
    "tkey": "tm",
    "system_flag": 1,
    "marks_tm_unordered": true
}
```

**A new `topic_version` replaces `topic_cols.json`, then `topic_var.json`**,
each through a temporary file and a `rename()`, both fsync'ed with their
directory, so the new version never survives a power cut its cols did not; the
`last_rowid_id` counter of treedb is carried over. The cols go first because
the version is what says they moved. A file that cannot be written refuses the
topic: the call answers `NULL`, logs *"Cannot re-create topic_cols.json for a
new topic_version: the topic keeps its version, and is not opened"* (or the
error of `topic_var.json`), and the next create tries again -- the version on
disk has not moved. *"Re-Creating topic_var.json"* and *"Re-Creating
topic_cols.json"* are logged only for a file written. Up to 7.25.4
`topic_cols.json` was REMOVED first and a failed write went unnoticed: the
topic opened with no cols file under the new version.

(timeranger2-topic-name-rule)=
**Topic names.** A topic name is ONE directory under the database, and a segment of the backtick kw paths (`` topics`<name>`cols ``). Every call that takes a topic name — create, open, delete, backup, `tranger2_topic_path()`, `tranger2_write_topic_var()` / `_cols()` — refuses, with the error *"Invalid topic name (path metacharacters not allowed)"*:

- an empty name, `.` and `..`;
- any name that holds `/` or `` ` ``.

A leading `.` is accepted, unlike a key: MQTT queues are named `<client_id>-IN` / `-OUT`, and the broker accepts a `client_id` such as `.foo`.

**The id of a disk feed follows the directory half of the rule.** The `id`
of [`tranger2_open_rt_disk()`](<#tranger2_open_rt_disk>) (the `rt_id` of a
follower's `open-rt` / `open-list`) becomes the directory
`<topic>/disks/<id>/`, and the follower removes that directory before creating
it. An empty id is refused with *"Invalid rt id (empty)"*; `.`, `..`, or an id
holding `/` with *"Invalid rt id (path metacharacters not allowed)"*, and an id
longer than `NAME_MAX` (255 bytes on Linux) with *"Invalid rt id (longer than
NAME_MAX)"* (its directory cannot exist). A backtick is accepted, since an rt
id is not a segment of any kw path (treedb names its own feeds
`` <treedb>`<topic>`<id> ``). The id may come from a peer, so a refused id is
logged as a **warning**, without a stack.

**One id, one feed.** The directory is keyed by the id alone, so an id already
in use by a live feed of the topic is refused whatever the `creator`, with a
warning: *"rt disk id already in use by another creator, refused"*, or *"rt
disk id already in use by the same creator, refused"*. A second feed used to
take the first one's directory over, and its close removed it. The guard is
**per process**: it reads the feeds of this tranger, so two processes that
follow one store with the same id still take each other's directory.

```C
tranger2_open_rt_disk(tranger, "users", "", 0, cb, "gui-42", "gui", 0);       // OK
tranger2_open_rt_disk(tranger, "users", "", 0, cb, "../../etc", "", 0);       // refused
tranger2_open_rt_disk(tranger, "users", "", 0, cb, "", "", 0);                // refused: empty
tranger2_open_rt_disk(tranger, "users", "", 0, cb, "gui-42", "other", 0);     // refused: in use
tranger2_open_rt_disk(tranger, "users", "", 0, cb, "gui-42", "gui", 0);       // refused: in use

char long_id[NAME_MAX + 2];
memset(long_id, 'x', sizeof(long_id) - 1);
long_id[sizeof(long_id) - 1] = 0;                                              // 256 bytes
tranger2_open_rt_disk(tranger, "users", "", 0, cb, long_id, "gui", 0);        // refused: > NAME_MAX
```

```C
tranger2_create_topic(tranger, "users", "id", "tm", 0, sf_string_key, 0, 0);   // OK
tranger2_create_topic(tranger, ".foo-IN", "", "", 0, sf_rowid_key, 0, 0);      // OK
tranger2_create_topic(tranger, "../other_db/users", "id", "tm", 0, 0, 0, 0);   // refused
tranger2_create_topic(tranger, "users/keys", "id", "tm", 0, 0, 0, 0);          // refused
```

---

(tranger2_delete_key)=
## [`tranger2_delete_key()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L3544)

The `tranger2_delete_key()` function deletes a whole record (= a
primary key, with every instance stored under it) from the specified
topic in the TimeRanger database. It removes the `keys/<key>/`
directory and drops the key from the topic cache. Irrecoverable. Only
the master can delete.

The deletion is **propagated to subscribers**:

- The master signals the delete in the directory of **every** rt_disk feed,
  before it removes the live `keys/<key>/` directory. Where
  `topic/disks/<rt_id>/<key>/` exists (the feed received records of the
  key), it is removed. Where it does not exist, it is created and removed
  at once: a key directory that appears and vanishes means "deleted". A
  feed that received no record of the key since it opened therefore hears
  the delete too.
- Each follower watches its own `disks/<rt_id>/`. It clears the key from its
  topic cache and fires the callback of **that feed only**, once.
- In-process `rt_mem`, `open_iterator`, and `rt_disk` subscribers without a
  watcher (no loop) whose `key` filter matches receive their
  [`tranger2_key_deleted_callback_t`](#tranger2_set_rt_key_deleted_callback)
  directly, if registered.

For per-instance delete (one row of a key's `.md2` index —
irrecoverable, no resurrection), see
[`tranger2_delete_instance()`](#tranger2_delete_instance).

```C
int tranger2_delete_key(
    json_t  *tranger,
    const char  *topic_name,
    const char  *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic from which the record will be deleted. |
| `key` | `const char *` | The key identifying the record to be deleted. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

A `key` with no directory on disk is logged (*"key directory not found"*, an
error) and the call still answers `0`: the key is removed from memory all the
same. A directory that cannot be removed answers `-1` and leaves the memory as
it was.

The iterators of the key, in this process, lose what they took from it: see
[`tranger2_iterator_get_page()`](#tranger2_iterator_get_page).

The legacy name `tranger2_delete_record()` is kept as a source-level
alias in `timeranger2.h` (`#define tranger2_delete_record tranger2_delete_key`)
so existing callers keep compiling unchanged. New code must use
`tranger2_delete_key()`.

---

(tranger2_delete_instance)=
## [`tranger2_delete_instance()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4117)

The `tranger2_delete_instance()` function deletes a single instance
(one row of a key's `.md2` index) without touching the surrounding
rows. The row's metadata is mutated in place: `sf_deleted_instance`
(bit `0x0400` of `system_flag2_t`, on the inherited side of the mask
so `rt_by_disk` followers see the same tombstone) is ORed into the
system flag bits. The `.json` data log stays append-only.

Read paths (`tranger2_open_iterator` history loop,
`tranger2_iterator_get_page`, and the `rt_by_disk` follower) skip rows
whose tombstone bit is set. Treedb is downstream of those paths and
inherits the skip with no `tr_treedb` change. `tranger2_read_record_content()`
and `tranger2_read_user_flag()` deliberately do **not** filter — the
caller has already located the row — so they remain available for
audit / wipe-verification tooling.

Irrecoverable. Master-only. Second delete of the same row is a silent
no-op (returns `0`).

```C
int tranger2_delete_instance(
    json_t      *tranger,
    const char  *topic_name,
    const char  *key,
    uint64_t    __t__,
    uint64_t    rowid,          // per-key i_rowid, slot in .md2, based 1
    BOOL        zero_payload
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The topic that owns the key. |
| `key` | `const char *` | The key whose instance is to be deleted. |
| `__t__` | `uint64_t` | Time of the instance (matches the value passed to `tranger2_append_record`). |
| `rowid` | `uint64_t` | Per-key slot index in the `.md2` file, based 1. |
| `zero_payload` | `BOOL` | If `TRUE`, also overwrites the matching `__size__` bytes at `__offset__` in the `.json` data file with zeros. Opt-in: after the wipe the JSON is no longer a parseable concatenation — only the `.md2` index makes sense of it. Use for sensitive-data wipes. |

**Returns**

Returns `0` on success, a negative value on failure.

**Notes**

Side effects to be aware of:

- `rowid`s do **not** renumber. Slot N stays slot N (dark).
- `iterator_size()`, `total_rows`, `pages` count slots, not live
  rows. A page returning `data.length=2` with `total_rows=3` is the
  contract for "1 dead in this segment".
- `topic_cache` cells (`fr_t`/`to_t`/`fr_tm`/`to_tm`) are not
  refreshed. If the deleted instance was the min/max t/tm of its file,
  the cell rollup can lie. Cheap to fix on next cold reload. Expensive
 to fix incrementally. Deferred until a consumer needs it.

---

(tranger2_set_rt_key_deleted_callback)=
## [`tranger2_set_rt_key_deleted_callback()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L5008)

Registers a key-delete callback on a handle returned by
`tranger2_open_rt_mem()`, `tranger2_open_rt_disk()` or
`tranger2_open_iterator()`. The callback fires when
[`tranger2_delete_key()`](#tranger2_delete_key) runs against a key
this subscriber tracks:

- On the master, directly from `tranger2_delete_key()`.
- On `rt_by_disk` followers, from the inotify watcher on
  `disks/<rt_id>/` when the master mirrors the deletion.

The subscriber's `key` filter is honoured: an empty filter (`""`)
matches every deletion. A specific key only matches that one.

Additive: existing handles default to "no callback", behavior
unchanged. Passing `cb=NULL` clears any previously registered callback.

```C
typedef int (*tranger2_key_deleted_callback_t)(
    json_t      *tranger,
    json_t      *topic,
    const char  *key,           // the deleted key
    json_t      *list,          // iterator / rt_mem / rt_disk entry, don't own
    void        *user_data
);

int tranger2_set_rt_key_deleted_callback(
    json_t                              *list,
    tranger2_key_deleted_callback_t     cb,
    void                                *user_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | A handle previously returned by `tranger2_open_rt_mem`, `tranger2_open_rt_disk` or `tranger2_open_iterator`. |
| `cb` | `tranger2_key_deleted_callback_t` | Function to invoke, or `NULL` to clear. |
| `user_data` | `void *` | Opaque pointer echoed back to the callback. |

**Returns**

Returns `0` on success, `-1` if `list` is `NULL`.

**Example**

A follower feed that closes itself when the key it follows is deleted:

```C
PRIVATE int on_key_deleted(
    json_t *tranger, json_t *topic, const char *key, json_t *list, void *user_data)
{
    tranger2_close_rt_disk(tranger, list);  // allowed: see the notes
    return 0;
}

/*  The loop is the PARAMETER: a "yev_loop" key in the config is overwritten  */
json_t *tranger = tranger2_startup(0, jn_tranger, yev_loop);
json_t *rt = tranger2_open_rt_disk(
    tranger, "devices", "DVES_000000", NULL, on_record, "live-card", "", NULL
);
tranger2_set_rt_key_deleted_callback(rt, on_key_deleted, NULL);
```

**Notes**

The callback fires **once per feed** for each delete. An rt_disk feed that
owns an inotify watcher hears the delete only from its own directory, and
the master-side fan-out skips it. Until 2026-09-15 a feed was fired once
for every inotify event of every feed of the topic (six times in a
three-feed follower), and not at all for a key that had no record since
the feed opened.

The callback may close its own feed (`tranger2_close_rt_disk()`). The
watcher is stopped at the end of the batch it is handling, and the rest
of that batch is dropped.

The feed's watcher exists only when the tranger was started with a loop:
pass it as the third argument of `tranger2_startup()`. Without a loop an
rt_disk has no watcher, and it is fired directly by the master's delete
path, like an rt_mem.

---

(tranger2_delete_topic)=
## [`tranger2_delete_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1798)

The `tranger2_delete_topic()` function deletes a topic from the TimeRanger database, effectively removing all associated records and metadata.

```C
int tranger2_delete_topic(
    json_t  *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to be deleted. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

Deleting a topic is irreversible. Make sure that the topic is no longer needed before calling [`tranger2_delete_topic()`](<#tranger2_delete_topic>).

Only a master can delete: on a replica (`master=0`) it returns `-1` with *"Only master can delete"*, and the directory stays. The topic name follows the [topic name rule](<#timeranger2-topic-name-rule>): `tranger2_delete_topic(tranger, "../other_db/users")` returns `-1` and deletes nothing.

---

(tranger2_dict_topic_desc_cols)=
## [`tranger2_dict_topic_desc_cols()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L2328)

`tranger2_dict_topic_desc_cols()` retrieves the column descriptions of a specified topic in dictionary format.

```C
json_t *tranger2_dict_topic_desc_cols(
    json_t      *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose column descriptions are to be retrieved. |

**Returns**

A JSON object containing the column descriptions of the specified topic. The caller must decrement the reference count of the returned JSON object.

**Notes**

This function is similar to [`tranger2_list_topic_desc_cols()`](<#tranger2_list_topic_desc_cols>), but returns the data in dictionary format instead of a list.

---

(tranger2_get_iterator_by_id)=
## [`tranger2_get_iterator_by_id()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L7747)

Retrieve an iterator by its identifier. If the iterator exists, it is returned. Otherwise, NULL is returned.

```C
json_t *tranger2_get_iterator_by_id(
    json_t      *tranger,
    const char  *topic_name,
    const char  *iterator_id,
    const char  *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic associated with the iterator. |
| `iterator_id` | `const char *` | Unique identifier of the iterator to retrieve. |
| `creator` | `const char *` | Identifier of the entity that created the iterator. NULL or empty matches only creatorless iterators. To reach one registered with a creator, pass that same creator. |

**Returns**

Returns a pointer to the iterator JSON object if found. Otherwise, returns NULL.

**Notes**

This function does not produce any error messages if the iterator is not found.
The `creator` filters the match: pass the same creator used at open. An empty
`creator` matches only entries that were themselves opened without a creator.

It is one hash lookup (since 7.25.0): the topic keeps
`iterators_by_id` — `{creator: {id: iterator}}` — beside its `iterators`
array. It used to walk the array, and [`tranger2_open_iterator()`](<#tranger2_open_iterator>)
calls it to refuse a duplicate, so opening N iterators on a topic was O(N²).

It also **opens the topic** when it is closed (it goes through `tranger2_topic()`).
To ask whether a handle you registered is still alive, check
`tranger2_topic_is_open()` first, then look the handle up by its id: a topic
closed and opened again has the same name and none of the old handles.

```C
// Is my iterator "it1" still there? Never trust a pointer kept from the open.
json_t *it = tranger2_topic_is_open(tranger, "frames")?
    tranger2_get_iterator_by_id(tranger, "frames", "it1", "my_service") : NULL;
```

---

(tranger2_get_rt_disk_by_id)=
## [`tranger2_get_rt_disk_by_id()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4954)

Retrieve a real-time disk instance by its identifier. If the specified real-time disk exists, it returns the corresponding JSON object. Otherwise, it returns NULL.

```C
json_t *tranger2_get_rt_disk_by_id(
    json_t      *tranger,
    const char  *topic_name,
    const char  *rt_id,
    const char  *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic associated with the real-time disk. |
| `rt_id` | `const char *` | Identifier of the real-time disk to retrieve. |
| `creator` | `const char *` | Creator identifier used to filter the real-time disk instances. NULL or empty matches only creatorless feeds. To reach one registered with a creator, pass that same creator. |

**Returns**

Returns a JSON object representing the real-time disk instance if found. Otherwise, returns NULL.

**Notes**

This function does not produce any error messages if the real-time disk is not found.
The `creator` filters the match: pass the same creator used at open. An empty
`creator` matches only entries that were themselves opened without a creator.

---

(tranger2_get_rt_mem_by_id)=
## [`tranger2_get_rt_mem_by_id()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4594)

Retrieve a real-time memory instance by its identifier. If the specified real-time memory instance exists, it is returned. Otherwise, NULL is returned.

```C
json_t *tranger2_get_rt_mem_by_id(
    json_t      *tranger,
    const char  *topic_name,
    const char  *rt_id,
    const char  *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic associated with the real-time memory instance. |
| `rt_id` | `const char *` | Identifier of the real-time memory instance to retrieve. |
| `creator` | `const char *` | Creator identifier used to filter the real-time memory instance. NULL or empty matches only creatorless feeds. To reach one registered with a creator, pass that same creator. |

**Returns**

Returns a pointer to the real-time memory instance as a `json_t *` if found, otherwise returns NULL.

**Notes**

This function does not produce any internal logging or error messages if the requested real-time memory instance is not found.
The `creator` filters the match: pass the same creator used at open. An empty
`creator` matches only entries that were themselves opened without a creator.

---

(tranger2_iterator_get_page)=
## [`tranger2_iterator_get_page()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L7959)

Retrieves a page of records from an iterator in the TimeRanger database. The function returns a JSON object containing the total number of rows, the number of pages based on the specified limit, and the list of retrieved records.

```C
json_t *tranger2_iterator_get_page(
    json_t *tranger,
    json_t *iterator,
    json_int_t from_rowid,    // based 1
    size_t limit,
    BOOL backward
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `iterator` | `json_t *` | A pointer to the iterator from which records will be retrieved. |
| `from_rowid` | `json_int_t` | The starting position of the page, 1-based. See the note below: it is a position among the rows the iterator RETURNS, which is a global rowid only when the iterator does not filter. |
| `limit` | `size_t` | The maximum number of records to retrieve in the page. |
| `backward` | `BOOL` | If `TRUE`, records are retrieved in reverse order. Otherwise, they are retrieved in forward order. |

**Returns**

A JSON object containing the retrieved records, total row count, and pagination details. The caller owns the returned JSON object and must manage its memory.

**Notes**

This function is useful for paginating through records in an iterator. The `from_rowid` parameter determines the starting point, and the `limit` parameter controls the number of records retrieved per page.

A **filtered** iterator (one opened with any `match_cond` condition) pages over
its row **index**, so `total_rows`, `pages` and the page contents count only the
records that MATCH — and `from_rowid` is a position among those, not a global
rowid. An **unfiltered** iterator has no index, and its positions are the global
rowids. See [`tranger2_open_iterator()`](#tranger2_open_iterator).

An **unfiltered** iterator pages the key **as it is**: its segments are taken
from the cache again when the key's cache moved since they were taken (its
rows, its number of files, its last file), so the rows appended since the open
are readable and `total_rows` / `pages` (the live count) can be paged to the
end. A **filtered** iterator keeps the index and the segments of its open.

A **delete of the key** drops them: an unfiltered iterator takes its segments
again at its next page (a key written again with the same number of rows and
files, spread another way, is not the same key), and a filtered one pages an
empty index -- the rows it indexed are gone. A page that finds the key gone
from disk behind its back (a replica whose master deleted it) logs *"Key gone
from disk while its iterator was open"* and stops.

```C
json_t *it = tranger2_open_iterator(tranger, "topic", "K", NULL, NULL, "pager", "", NULL, NULL);
// K: 2 rows on day 1, 1 on day 2
tranger2_delete_key(tranger, "topic", "K");
// K written again: 1 row on day 1, 2 on day 2
json_t *page = tranger2_iterator_get_page(tranger, it, 1, 100, FALSE);
// page["data"]: the 3 new rows (7.25.4: 1 row, and a CRITICAL)
```

```C
json_t *it = tranger2_open_iterator(tranger, "topic", "key", NULL, NULL, "it1", "", NULL, NULL);
/*  ... 3 records appended to "key" since ...  */
json_t *page = tranger2_iterator_get_page(tranger, it, 1, 100, FALSE);
/*  page["data"] holds the 3 new rows too; page["total_rows"] counts them  */
```

---

(tranger2_iterator_size)=
## [`tranger2_iterator_size()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L7902)

`tranger2_iterator_size()` returns the number of records in the specified iterator.

```C
size_t tranger2_iterator_size(
    json_t *iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `iterator` | `json_t *` | The iterator whose size is to be determined. |

**Returns**

The number of records the iterator will return.

**Notes**

A **filtered** iterator answers with its index: the exact number of records
matching its `match_cond`. An **unfiltered** one sums its segments — the same
thing when nothing is filtered out.

:::{warning}
An unfiltered iterator counts its rows from the segment totals, so a record
deleted with [`tranger2_delete_instance()`](#tranger2_delete_instance) still adds
to its size while its pages skip it. A filtered iterator never indexes a dead
row, so its count and its pages agree.
:::

---

(tranger2_list_keys)=
## [`tranger2_list_keys()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1621)

Returns a JSON array with the key names of a topic, read from its in-memory
`cache`.

```C
json_t *tranger2_list_keys(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic from which to retrieve the keys. |

**Returns**

A JSON array of key-name strings. The caller owns the returned array and must free
it when no longer needed. Never NULL — a missing topic yields an **empty array**.

**Notes**

The keys are from the topic's in-memory `cache`, not a disk scan. Slow for
thousands of keys (allocates one string per key).

---

(tranger2_list_topic_desc_cols)=
## [`tranger2_list_topic_desc_cols()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L2310)

Returns a JSON array containing the column descriptions of a topic in the TimeRanger database. The returned JSON object must be decremented after use.

```C
json_t *tranger2_list_topic_desc_cols(
    json_t      *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A JSON object representing the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose column descriptions are to be retrieved. |

**Returns**

A JSON array containing the column descriptions of the specified topic. The caller must decrement the reference count of the returned JSON object after use.

**Notes**

This function was previously known as `tranger_list_topic_desc()`.

---

(tranger2_list_topics)=
## [`tranger2_list_topics()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1559)

Returns a JSON array with the names of the topics currently **opened in memory**
(the `tranger["topics"]` registry) — not a disk scan.

```C
json_t *tranger2_list_topics(
    json_t *tranger
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The `tranger` database instance. |

**Returns**

A JSON array with the names of the currently-opened topics. The caller owns the
returned array and must free it. Never NULL — an empty array when no topic is
opened.

**Notes**

This lists only topics already opened in memory. To list every topic present on
disk — including ones not yet opened — use
[`tranger2_list_topic_names()`](#tranger2_list_topic_names).

---

(tranger2_mark_tm_order)=
## [`tranger2_mark_tm_order()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L8348)

Marks the md2 files of a topic whose `__t__` or `__tm__` goes back, and makes
the topic one that marks (`"marks_tm_unordered": true`). The migration of a
topic created by 7.25.4 or earlier, asked by an operator: see the cost of such
a topic in [`tranger2_open_iterator()`](<#tranger2_open_iterator>).

```C
json_t *tranger2_mark_tm_order(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The TimeRanger database instance. It must be the master. |
| `topic_name` | `const char *` | The topic to mark. |

**Returns**

A dict, yours:

```json
{
    "topic_name": "readings",
    "was_marking": false,
    "keys": 1,
    "files": 30,
    "rows": 600000,
    "t_unordered_marked": 0,
    "tm_unordered_marked": 2,
    "marks_tm_unordered": true
}
```

`t_unordered_marked` / `tm_unordered_marked` count the markers THIS call
wrote. `NULL` (logged, and in `gobj_log_last_message()`) when the handle is
not the master (*"Only master can write"*), the topic does not exist, a md2
file cannot be read, or a marker or `topic_desc.json` cannot be written. The
keys are walked in the order of the topic's cache and the call stops at the
first failure. What it leaves:

- the topic is NOT marked: `"marks_tm_unordered"` is unchanged, in
  `topic_desc.json` and in memory, so no file's `tm` range is trusted, as
  before the call;
- the markers written before the failure stay on disk;
- the cells of the files read before the failure keep, in memory, the flags
  and the whole-file ranges the call gave them, and the totals of their keys
  follow them (the key that failed too). Those ranges are what the disk
  holds: they only make the answers exact.

Run it again once the cause is fixed. A file that needs a marker and whose
name leaves no room for one (`<file_id>.tm_unordered` longer than `NAME_MAX`)
is not a failure: it is skipped and logged (*"Cannot mark a md2 file, its name
leaves no room for a marker: skipped, every load reads it whole"*), since
every load reads such a file whole already.

**Notes**

For every key of the topic and every md2 file of the key, it reads the file
whole once, writes `<file>.tm_unordered` where a `__tm__` goes back and
`<file>.unordered` where a `__t__` does (unless the marker is there), and gives
the file's cell in memory its whole ranges and those flags. Then, if the topic
did not mark, it sets `"marks_tm_unordered": true` in `topic_desc.json`
(through a temporary file, fsync'ed, renamed, the directory fsync'ed; mode
0440 as before) and in memory. The next page of an open iterator takes its
segments again.

It is synchronous -- the yuno's event loop is blocked while it runs -- and
costs a listing of each key's directory and one sequential read of every md2
file, 32 bytes a row: linear in the rows and in the files. Measured with a
warm page cache:

| Store | Time |
|---|---|
| 1 key, 30 files x 20000 rows (600000 rows) | 16 ms |
| 4 keys, 365 daily files of one row each | 21-32 ms |
| 4 keys, 3650 daily files of one row each (14600 files) | 72-88 ms |

Run it on a quiet node.

Run it again on a topic that marks to re-mark it after a rollback to a binary
that appends without markers (every release up to 7.25.4), or after a crash
that lost one. It is idempotent, and it never removes a marker (a marker on a
file in order costs a whole read of that file, nothing more).

A replica that has the topic open reads it as before (no file's `tm` range
trusted) until it opens the topic again.

From a yuno, the `mark-tm-order` command of `C_TRANGER`:

```bash
ycommand -c 'command-yuno id=<id> service=<tranger service> command=mark-tm-order topic_name=readings'
```

From C, every topic of a store. [`tranger2_list_topic_names()`](#tranger2_list_topic_names)
lists every DIRECTORY of the store, and not every directory is a topic:
`C_TREEDB` keeps `saved_schemas/` in the store of `__system__`. Skip a
directory without its `topic_desc.json`, as the command does, or the loop
stops there, before the topics listed after it:

```C
const char *directory = json_string_value(json_object_get(tranger, "directory"));
json_t *names = tranger2_list_topic_names(tranger);
size_t i; json_t *jn_name;
json_array_foreach(names, i, jn_name) {
    const char *name = json_string_value(jn_name);
    char topic_dir[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), directory, name, NULL);
    if(!file_exists(topic_dir, "topic_desc.json")) {
        continue;   // not a topic (saved_schemas/ in __system__)
    }
    json_t *report = tranger2_mark_tm_order(tranger, name);
    if(!report) {
        break;  // logged; the topics already marked stay marked
    }
    JSON_DECREF(report)
}
JSON_DECREF(names)
```

---

(tranger2_open_iterator)=
## [`tranger2_open_iterator()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L7382)

Opens an iterator for traversing records in a topic within the TimeRanger database. The iterator allows filtering records based on specified conditions and supports real-time data loading.

```C
json_t *tranger2_open_iterator(
    json_t *tranger,
    const char *topic_name,
    const char *key,    // required
    json_t *match_cond, // owned
    tranger2_load_record_callback_t load_record_callback, // called on LOADING and APPENDING, optional
    const char *iterator_id,     // iterator id, optional, if empty will be the key
    const char *creator,     // creator
    json_t *data,       // JSON array, if not empty, fills it with the LOADING data, not owned
    json_t *extra       // owned, user data, this json will be added to the return iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic to iterate over. |
| `key` | `const char *` | Specific key to filter records. Required. |
| `match_cond` | `json_t *` | JSON object containing filtering conditions. Owned by the function. |
| `load_record_callback` | `tranger2_load_record_callback_t` | Callback function invoked during data loading and appending. Optional. |
| `iterator_id` | `const char *` | Unique identifier for the iterator. If empty, the key is used. |
| `creator` | `const char *` | Identifier of the entity creating the iterator. |
| `data` | `json_t *` | JSON array to store loaded data. Not owned by the function. |
| `extra` | `json_t *` | Additional user data attached to the iterator. Owned by the function. |

**Returns**

Returns a JSON object representing the iterator. The topic owns it: close it
with [`tranger2_close_iterator()`](<#tranger2_close_iterator>).

`NULL` (logged; `match_cond` and `extra` are consumed all the same) when the
topic cannot be opened, `key` is empty, an iterator with this `iterator_id`
and `creator` is already open, or the index of a filtered paging iterator
cannot be built.

**Notes**

The iterator supports real-time data loading and filtering based on various conditions. Use [`tranger2_close_iterator()`](<#tranger2_close_iterator>) to release resources when done.

**A load that stops half way says so.** When the LOADING (a callback, or
`data`) meets a row whose metadata or whose CONTENT cannot be read, it stops
there, logs it, and leaves `"load_failed": true` in the iterator. The rows
before it, in the load's direction, were handed (forward: the oldest,
backward: the newest). What the callback saw, and what `data` holds, is then
NOT the whole history: a caller that asks the history a question (is any
record of this key frozen by a snapshot?) must read the flag before it takes a
"no". Up to 7.25.4 a record whose content could not be read was handed to the
callback as `NULL` and the load went on; an `only_md` load reads no content,
and no content fails it.

**After a restart too.** The cache of a topic is built from disk when it is
opened. A `.md2` file of a key that it cannot count flags the key: one that
cannot be opened or read. It logs *"md2 file of the key unreadable when its cache was built: every load of
the key says load_failed"* once, at the open, and then every iterator of the
key -- paging ones too -- logs *"The history of the key is not whole: a md2
file of it could not be read when its cache was built"* and says
`load_failed`. A loading stops where the first flagged file is in its
direction, the place a running tranger stops when the damage happens behind
its back. Key `A` with the files of days 1, 2 and 3, and the `.md2` of day 2
made unreadable (mode `0000`) while the yuno was down:

```text
forward load   -> the rows of day 1, then load_failed
backward load  -> the rows of day 3, then load_failed
```

A `.md2` of 0 rows gets no cell and flags nothing. With an EMPTY `.json` it
loses nothing. With a `.json` that is NOT empty it is what an append that was
never acknowledged leaves (the content is written first, the md2 row after,
and the row was never written: the md2 failed, or the process died between
the two). That is not damage: the file is ignored, as until 7.25.4, with a
warning that names it, and the key loads from its other files:

```text
WARNING: md2 file of the key with no rows and a content file that is not empty:
         an append that was never acknowledged, the file is ignored
         topic_directory=<store>/devices key=A file_id=2000-01-02 content_size=32
forward load   -> the rows of days 1 and 3, load_failed false
```

The next
append into that day's file names its content with a row of its own, and the
warning goes. A `.md2` cut to 0 bytes behind the yuno's back has the same
shape, and loses the rows of its file the same way it did until 7.25.4: a
`.json` much larger than one record is the sign; put the pair back from a
backup.

**A `.md2` whose last row is torn is not damage either.** Its size is not a
whole number of 32-byte rows: a power cut came during the write of a row. The
md2 row is the commit point of an append, and an append is acknowledged only
after the whole row is written, so a torn row is an append that was never
acknowledged. A MASTER cuts the `.md2` back to its whole rows (to
`floor(size / 32) * 32` bytes) when it first reads the file, logs one warning,
and the key loads whole. The next append goes into the same file:

```text
WARNING: md2 file of the key ends in a part of a row: an append that was never
         acknowledged was cut back
         topic=devices key=A file_id=2000-01-02
         path=<store>/devices/keys/A/2000-01-02.md2 old_size=109 new_size=96
forward load   -> the rows of days 1, 2 (3 rows) and 3, load_failed false
```

The cut removes fewer than 32 bytes, all after the last whole row, so it
never removes a row that was acknowledged. The `.json` is left as it is: its
bytes after the last row belong to no row, and the next append writes at its
end. If the cut itself fails, that is damage: the file is flagged as above
(*"Cannot cut back a md2 file that ends in a part of a row: the file is
damaged"*). A torn first row (a `.md2` of fewer than 32 bytes) is cut to 0
bytes, and the file is then a `.md2` of 0 rows, ignored with the warning
above.

A REPLICA never writes. It reads only the whole rows of such a file, and logs
nothing: a replica also sees a torn row while a live master is writing it, and
counts the row when the master's next notification of the file comes. When the
master opens the store, it cuts the file back. In the unreleased work after
7.25.4 a torn row flagged the key, on a master and on a replica: every load
said `load_failed` and every append into the file was refused until the md2
was cut by hand or the period changed.

The flag of a file goes once a cell counts it again: an append into the file
that finds it readable (see [`tranger2_append_record()`](#tranger2_append_record)),
or, on a follower, the file read whole from the disk after the master wrote it.
[`tranger2_delete_key()`](#tranger2_delete_key) clears every flag with the key.
Up to 7.25.4 the cache build dropped an unreadable file and nothing failed: the
key read as a shorter key.

```C
json_t *data = json_array();
json_t *it = tranger2_open_iterator(
    tranger, "devices", "dev-1", NULL, NULL, "", "me", data, NULL
);
if(!it) {
    // not opened, the cause is in the log
} else if(json_is_true(json_object_get(it, "load_failed"))) {
    // data holds only a part of the history: do not decide on it
}
if(it) {
    tranger2_close_iterator(tranger, it);
}
JSON_DECREF(data)
```

**A deleted key.** A delete of the key
([`tranger2_delete_key()`](#tranger2_delete_key), or a replica's key-deleted
notice) drops what its iterators took from it: an unfiltered iterator takes its
segments again at its next page, a filtered one keeps an empty index (its rows
are gone, and an index is built only at the open).

**`match_cond` — the conditions an iterator honors**

```
backward
only_md                 (don't load jn_record on calling callbacks; honored by
                        the historical load AND the realtime rt_mem/rt_disk
                        feeds, which hand the callback NULL jn_record)

rkey                    (str) PCRE2 regex over keys ("" == ".*"), for a KEYLESS
                        list (no `key`) = the whole topic, narrowed to the keys
                        matching it. Honored by BOTH halves: the disk load only
                        visits matching keys, and the realtime feed only
                        dispatches appends of matching keys.

from_rowid / to_rowid
from_t   / to_t         t:  PERSISTENCE time (when the record was appended)
from_tm  / to_tm        tm: MESSAGE time (the record's tkey field)

user_flag / not_user_flag / user_flag_mask_set / user_flag_mask_notset
```

`t` and `tm` are two **independent** axes and their ranges combine (AND). Both
are expressed in the **topic's** own unit: seconds, or milliseconds when the
topic sets `sf_t_ms` / `sf_tm_ms`.

The two axes are not ordered the same way, and the scan knows it:

- **Rows are in `t` order**, except in a file that holds a late record (a
  `__t__` below the times the file already had). The master marks that file
  `<file>.unordered`, and inside a marked file the scan reads every row of the
  selected files instead of stopping at the first row past the `t` range.
- **Rows are in `tm` order only where the master says so.** `tm` is written
  by the producer (a device that sends what it buffered writes it out of
  order), and the files are cut by `t`. In a topic created after 7.25.4
  (`"marks_tm_unordered": true` in its `topic_desc.json`) the master marks a
  file whose `tm` goes back `<file>.tm_unordered`, and a load reads a marked
  file whole, so the `[fr_tm, to_tm]` of every file is exact. There a file
  that does not meet the `tm` condition is not read, and in an unmarked file
  the first row past the range ends the scan of THAT FILE; the scan goes on in
  the next file (a later file may hold a lower `tm`). In a marked file a `tm`
  condition skips rows and ends nothing.
- **A topic created by 7.25.4 or earlier** cannot tell which of its files are
  in `tm` order, and no file's `tm` range is trusted: a `tm` condition leaves
  no file out and ends no scan, it skips rows. Correct, and it reads every
  md2 row of the key, so the cost grows with the files of the key. Measured on
  one key of 30 day files x 20000 rows, a `from_tm`..`to_tm` query of 26 rows
  (best of 5, warm page cache): 13.6 ms in 7.25.4 (which trusted the first and
  the last row of each file, and so could miss rows), 408 ms on such a topic
  now, 0.09 ms once the topic is marked. Mark it with
  [`tranger2_mark_tm_order()`](<#tranger2_mark_tm_order>) (the `mark-tm-order`
  command of `C_TRANGER`): 16 ms for those 600000 rows in 30 files; its
  cost is linear in the rows and in the files, and it blocks the yuno while
  it runs.
- **The marker goes down before the row.** The master writes the marker of a
  record whose `t` or `tm` goes back BEFORE its md2 row, so a process that dies
  between the two leaves a marker with no row behind it (one whole read of the
  file at the next load), never a row with no marker. A marker that cannot be
  written is logged (*"Cannot mark md2 file, a reload will misread its time
  range"*); the master still reads that file whole, and writes the marker at
  the next append to the file (*"md2 file marked, the marker missed earlier is
  written"*). Neither is fsync'ed, like the append itself. A marker lost all
  the same (a crash, a power cut, a rollback to a binary that appends without
  markers) is written again by `tranger2_mark_tm_order()`.
- A file left out by `tm` is a **hole** in the rowids the scan walks: the
  scan steps over it, and a `from_rowid` / `to_rowid` that falls in it begins
  at the next row the scan can read.

Both hold in both directions, and for a paged iterator too. With the rows
`t=100 E1`, `t=50000 E2`, `t=200 E3` (late) in one file:

```C
json_t *data = json_array();
json_t *it = tranger2_open_iterator(
    tranger, "readings", "0000000000000000001",
    json_pack("{s:I, s:I, s:b}",
        "from_t", (json_int_t)150,
        "to_t", (json_int_t)250,
        "backward", 0
    ),
    NULL, "range", "", data, NULL
);
// data holds E3 (up to 7.25.3: nothing, the scan stopped at E2)
tranger2_close_iterator(tranger, it);
JSON_DECREF(data)
```

And with three day files of one key, `tm` 100, 5000 and 150, the middle one is
out of `to_tm = 300` and the scan steps over it:

```C
json_t *data = json_array();
json_t *it = tranger2_open_iterator(
    tranger, "readings", "K",
    json_pack("{s:I}", "to_tm", (json_int_t)300),
    NULL, "range", "", data, NULL
);
// data holds D1 D3 (7.25.4: D1, and "next rowids not consecutive")
tranger2_close_iterator(tranger, it);
JSON_DECREF(data)
```

Every condition is honored **per record**, in both modes of the iterator:

- **LOADING** (a `load_record_callback` or both `data`): each record is matched as
  the history is walked.
- **PAGING** (neither): a filtered iterator builds its row **index** when it
  opens, so [`tranger2_iterator_size()`](#tranger2_iterator_size) and
  [`tranger2_iterator_get_page()`](#tranger2_iterator_get_page) count and return
  exactly the matching records, and `get_page`'s `from_rowid` is a position among
  THOSE rows. An **unfiltered** iterator builds no index — its open stays cheap
  regardless of the key size, and its positions are the global rowids.

:::{warning}
The index is a **snapshot** taken at open: records appended after a filtered
iterator opens never enter its `total_rows` or its pages, while an unfiltered
iterator recounts the key on every call and keeps growing. Reopen the iterator
(or pair it with a realtime feed) to see new appends.
:::

:::{note}
A topic **owns** the iterators opened on it:
[`tranger2_close_topic()`](#tranger2_close_topic) closes them all and frees the
topic. Anyone caching an iterator handle must check the topic is still open
(see [`tranger2_topic_is_open()`](#tranger2_topic_is_open)) — the handle is dead
memory once it is not.
:::

---

(tranger2_open_list)=
## [`tranger2_open_list()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L9808)

`tranger2_open_list()` opens a list of records in memory, optionally enabling real-time updates via memory or disk.

```C
json_t *tranger2_open_list(
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond, // owned
    json_t *extra,      // owned, will be added to the returned rt
    const char *rt_id,
    BOOL rt_by_disk,
    const char *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to open. |
| `match_cond` | `json_t *` | A JSON object specifying filtering conditions for records. Owned by the function. |
| `extra` | `json_t *` | Additional user data to be added to the returned real-time object. Owned by the function. |
| `rt_id` | `const char *` | The real-time list identifier. |
| `rt_by_disk` | `BOOL` | If `TRUE`, enables real-time updates via disk. Otherwise, uses memory. |
| `creator` | `const char *` | The identifier of the entity creating the list. |

**Returns**

Returns a JSON object representing the real-time list (`rt_mem` or `rt_disk`),
or, when real-time is disabled (`to_rowid` set), the `extra` given, tagged
`"list_type": "no_rt"`. Close either with
[`tranger2_close_list()`](<#tranger2_close_list>).

`NULL` (logged; `match_cond` and `extra` are consumed) on error: no topic, no
`load_record_callback`, a bad `rkey`, a real-time feed that cannot be opened,
no `extra` for a `no_rt` list -- and when the history of THE key of a list of
one key (`key` set) could not be loaded whole (the `load_failed` of
[`tranger2_open_iterator()`](#tranger2_open_iterator)).

**A keyless list with a key that cannot be loaded** logs the key (*"Cannot
load the whole history of a key of the list: the records read before the
failure were handed, the list goes on with the next key"*), loads every other
key, opens its real-time feed, and says what it lacks in the handle it
returns:

```json
{
    "list_type": "rt_mem",
    "load_failed": true,
    "load_failed_keys": ["B"]
}
```

The records already handed to the callback stay handed: of a failed key, the
ones read before the failure (forward: its oldest, backward: its newest). A
caller that answers a question from the list must read the flag, or it takes
"not in the list" for "not on disk":

```C
json_t *list = tranger2_open_list(tranger, "devices", match_cond, extra, "", FALSE, "");
if(list && json_is_true(json_object_get(list, "load_failed"))) {
    json_t *keys = json_object_get(list, "load_failed_keys");
    // these keys are on disk and not in what the callback saw
}
```

What the callers in the SDK do with it:

| Caller | With `load_failed` |
|---|---|
| treedb (every topic, `__snaps__`, `__graphs__`) | remembers the keys; refuses a create of such an id, refuses shoot/activate of a snap when `__snaps__` has any, and its asset guards fail closed (see [TreeDB](treedb.md)). |
| treedb's snapshot guard of the assets | a walk that did not load everything fails closed: the gc and the delete of an asset refuse. |
| `tr_queue` / `tr2q_mqtt` (`trq_load()`, `tr2q_load()`) | the pending messages read are in memory; those of a row that cannot be read are not, and are not delivered. The load returns -1 and does NOT move nor save the queue's `first_rowid`, so the next load, once the store is repaired, finds them (up to 7.25.4 it saved the size of the topic, and they were skipped for ever). |
| msg2db | nothing more than the library's log: the messages of that key are not in the index. |

Until 7.25.4 such a list was handed over silently, with the key missing.

**Notes**

Loading all records can introduce delays in application startup. Use filtering conditions in `match_cond` to optimize performance.

The history is loaded with one-shot iterators of their own creator
(`__tranger2_open_list__`): an iterator the caller keeps open on a key does not
make the load "already exist".

```C
json_t *list = tranger2_open_list(
    tranger, "devices",
    json_pack("{s:I, s:I}",
        "to_rowid", (json_int_t)1000000,    // one-shot, no realtime
        "load_record_callback", (json_int_t)(uintptr_t)load_cb
    ),
    json_object(),      // extra: IS the returned handle of a no_rt list
    "", FALSE, "me"
);
if(!list) {
    // refused, or some key's history could not be read: what load_cb got
    // is not the whole topic
} else {
    tranger2_close_list(tranger, list);
}
```

---

(tranger2_open_rt_disk)=
## [`tranger2_open_rt_disk()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4649)

Opens a real-time disk-based iterator for monitoring changes in a topic. The function allows tracking new records appended to the topic by monitoring disk events.

```C
json_t *tranger2_open_rt_disk(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on disk
    const char *rt_id,      // rt disk id, REQUIRED
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic to monitor. |
| `key` | `const char *` | Specific key to monitor. If empty, all keys are monitored. |
| `match_cond` | `json_t *` | JSON object containing conditions for filtering records. Owned by the function. |
| `load_record_callback` | `tranger2_load_record_callback_t` | Callback function invoked when a new record is appended to the disk. |
| `rt_id` | `const char *` | Unique identifier for the real-time disk iterator. Required. |
| `creator` | `const char *` | Identifier of the entity creating the iterator. |
| `extra` | `json_t *` | Additional user data to be associated with the iterator. Owned by the function. |

**Returns**

Returns a JSON object representing the real-time disk iterator. The caller does not own the returned object.

**Notes**

This function is used when monitoring real-time changes in a topic via disk events. The iterator remains active until explicitly closed using [`tranger2_close_rt_disk()`](<#tranger2_close_rt_disk>).

The rowid the callback receives is the key's **global `g_rowid`** — the one that
never resets — not the row's position inside the `.md2` file it lives in (that
one is `i_rowid`, and it restarts at 1 with every new file). A consumer can
therefore dedupe or page by it across a file rotation.

Several feeds can be open on the same key (a per-key one and a whole-topic one,
say). Each is served exactly once per record: the feed keeps a watermark of the
last row it was given, per key and per file -- one mark per FILE, so a batch
that touches two files of a key (a late record and a current one, or a rotation
while the follower is busy) serves each feed both (before 7.25.0, the second feed lost both) -- and the master hard-links each new
`.md2` into the directory of every feed that wants the key. A feed's watermark
dies with its key, so a key re-created later does not inherit it.

---

(tranger2_open_rt_mem)=
## [`tranger2_open_rt_mem()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4401)

Opens a real-time memory stream for a given topic in `tranger`. This function enables real-time message processing for the specified `key` and applies filtering conditions from `match_cond`. The callback [`tranger2_load_record_callback_t`](#tranger2_load_record_callback_t) is invoked when new records are appended.

```C
json_t *tranger2_open_rt_mem(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on mem
    const char *list_id,    // rt list id, optional (internally will use the pointer of rt)
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic for which real-time memory streaming is enabled. |
| `key` | `const char *` | Specific key to monitor. If empty, all keys are monitored. |
| `match_cond` | `json_t *` | JSON object containing filtering conditions. Owned by the function. |
| `load_record_callback` | `tranger2_load_record_callback_t` | Callback function invoked when a new record is appended in memory. |
| `list_id` | `const char *` | Optional identifier for the real-time list. If empty, the internal pointer is used. |
| `creator` | `const char *` | Identifier of the entity creating the real-time memory stream. |
| `extra` | `json_t *` | Additional user data stored in the returned iterator. Owned by the function. |

**Returns**

Returns a JSON object representing the real-time memory stream. The caller does not own the returned object.

**Notes**

This function is valid when the Yuno instance is the master writing real-time messages from [`tranger2_append_record()`](<#tranger2_append_record>).

---

(tranger2_open_topic)=
## [`tranger2_open_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1324)

The `tranger2_open_topic()` function opens a topic in the TimeRanger database. If the topic is already open, it returns the existing topic JSON object.

```C
json_t *tranger2_open_topic(
    json_t  *tranger,
    const char  *topic_name,
    BOOL  verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A JSON object representing the TimeRanger database. |
| `topic_name` | `const char *` | The name of the topic to open. |
| `verbose` | `BOOL` | If `TRUE`, additional debug information is printed. |

**Returns**

A JSON object representing the opened topic. The returned object is not owned by the caller and must not be modified or freed.

**Notes**

This function is idempotent. This means that calling it multiple times with the same `topic_name` will return the same JSON object without creating a new instance.

It returns `NULL`, never a critical, when the name is refused by the [topic name rule](<#timeranger2-topic-name-rule>), and when the directory exists but is not a topic (it has no `topic_desc.json`): *"Not a topic: topic_desc.json not found"*, logged only with `verbose`. A peer can send any name, and with `on_critical_error=2` a critical is an `exit(0)` of the yuno.

---

(tranger2_print_md0_record)=
## [`tranger2_print_md0_record()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L10205)

Prints metadata of a record, including row ID, time, message time, and key, into a buffer.

```C
void tranger2_print_md0_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the formatted metadata string. |
| `bfsize` | `int` | Size of the buffer to prevent overflow. |
| `key` | `const char *` | Key associated with the record. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the record metadata structure. |
| `print_local_time` | `BOOL` | Flag indicating whether to print time in local format. |

**Returns**

This function does not return a value.

**Notes**

The function formats the metadata into the provided buffer. This makes sure of it does not exceed `bfsize`. It prints row ID, time, message time, and key.

---

(tranger2_print_md1_record)=
## [`tranger2_print_md1_record()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L10269)

Prints metadata information of a record, including row ID, user flag, system flag, timestamps, and key, into a buffer.

```C
void tranger2_print_md1_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer where the formatted metadata string will be stored. |
| `bfsize` | `int` | Size of the buffer to make sure that safe writing. |
| `key` | `const char *` | Key associated with the record. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the record metadata structure containing the information to be printed. |
| `print_local_time` | `BOOL` | Flag indicating whether to print timestamps in local time (if `TRUE`) or UTC (if `FALSE`). |

**Returns**

This function does not return a value.

**Notes**

The function formats and writes metadata details into the provided buffer. This makes sure that the output does not exceed `bfsize` bytes.

---

(tranger2_print_md2_record)=
## [`tranger2_print_md2_record()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L10337)

Prints detailed metadata of a record, including row ID, offset, size, timestamp, and file path, into the provided buffer.

```C
void tranger2_print_md2_record(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer where the formatted metadata string will be stored. |
| `bfsize` | `int` | Size of the buffer to make sure that safe writing. |
| `tranger` | `json_t *` | Reference to the TimeRanger database instance. |
| `topic` | `json_t *` | JSON object representing the topic associated with the record. |
| `key` | `const char *` | Key identifying the record within the topic. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the extended metadata structure of the record. |
| `print_local_time` | `BOOL` | Flag indicating whether to print timestamps in local time. |

**Returns**

This function does not return a value.

**Notes**

The function formats metadata details into the provided buffer. This makes sure that the output remains within the specified buffer size.

---

(tranger2_print_record_filename)=
## [`tranger2_print_record_filename()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L10389)

Formats and stores the filename of a record in a buffer, using metadata from [`tranger2_print_record_filename()`](#tranger2_print_record_filename).

```C
void tranger2_print_record_filename(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Pointer to the buffer where the formatted filename will be stored. |
| `bfsize` | `int` | Size of the buffer in bytes. |
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic` | `json_t *` | Pointer to the topic associated with the record. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the record metadata structure. |
| `print_local_time` | `BOOL` | Flag indicating whether to format timestamps in local time. |

**Returns**

This function does not return a value.

**Notes**

The buffer `bf` must be large enough to store the formatted filename. The function retrieves metadata from [`tranger2_print_record_filename()`](#tranger2_print_record_filename) to construct the filename.

---

(tranger2_read_record_content)=
## [`tranger2_read_record_content()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L9516)

Reads the content of a record from a given topic in the TimeRanger database. The function retrieves the record's data based on its metadata.

```C
json_t *tranger2_read_record_content(
    json_t            *tranger,
    json_t            *topic,
    const char        *key,
    md2_record_ex_t   *md_record_ex
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic` | `json_t *` | Pointer to the topic from which the record will be read. |
| `key` | `const char *` | The key identifying the record to be read. |
| `md_record_ex` | `md2_record_ex_t *` | Pointer to the metadata structure of the record to be retrieved. |

**Returns**

Returns a `json_t *` object containing the record's content. The caller owns the returned JSON object and must free it when no longer needed.

**Notes**

This function is useful when only metadata was loaded and the full record content needs to be retrieved.

---

(tranger2_read_user_flag)=
## [`tranger2_read_user_flag()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4350)

The `tranger2_read_user_flag()` function retrieves the user flag associated with a specific record in a given topic.

```C
uint16_t tranger2_read_user_flag(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the record. |
| `rowid` | `uint64_t` | Unique identifier of the record whose user flag is to be retrieved. |

**Returns**

Returns the `uint16_t` user flag associated with the specified record.

**Notes**

The function is used in writing mode to check the user flag of a record before modifying it.

---

(tranger2_set_trace_level)=
## [`tranger2_set_trace_level()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L10415)

Sets the trace level of the `tranger` instance, controlling the verbosity of logging and debugging output.

```C
void tranger2_set_trace_level(
    json_t *tranger,
    int      trace_level
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the `tranger` instance whose trace level is to be modified. |
| `trace_level` | `int` | The new trace level to be set, determining the verbosity of logging. |

**Returns**

This function does not return a value.

**Notes**

Higher trace levels typically enable more detailed logging, which can be useful for debugging.

---

(tranger2_set_system_flag)=
## [`tranger2_set_system_flag()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L4016)

Sets or clears specific bits in the **system** flag of a record — the metadata
band reserved for the framework (for example the immutable-record bit), distinct from
the user flag set by [`tranger2_set_user_flag()`](<#tranger2_set_user_flag>).

```C
int tranger2_set_system_flag(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid,
    uint16_t mask,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the record. |
| `key` | `const char *` | Record key — with `__t__` and `rowid` it identifies the record metadata. |
| `__t__` | `uint64_t` | Record time. |
| `rowid` | `uint64_t` | Real rowid in the file (not the topic-global rowid). |
| `mask` | `uint16_t` | Bitmask specifying which bits in the system flag to modify. |
| `set` | `BOOL` | If `TRUE`, the bits in `mask` are set. If `FALSE`, they are cleared. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

Only the bits in `mask` are touched. The system flag is framework-owned. User
code normally goes through higher-level treedb helpers rather than setting system
bits directly.

Master only: on a replica, or on a master that lost its lock (see
[`tranger2_stop()`](<#tranger2_stop>)), it writes nothing, logs *"Only master
can write"* and returns `-1`. Up to 7.25.4 it asked nothing: the write of the read-only md2 fd failed,
which is a CRITICAL, and with the default `on_critical_error` the process
exited(0).

---

(tranger2_set_user_flag)=
## [`tranger2_set_user_flag()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L3931)

Sets or clears specific bits in the user flag of a record in a topic within the TimeRanger database.

```C
int tranger2_set_user_flag(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid,
    uint16_t mask,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the record. |
| `rowid` | `uint64_t` | Unique identifier of the record whose user flag is modified. |
| `mask` | `uint32_t` | Bitmask specifying which bits in the user flag must be modified. |
| `set` | `BOOL` | If `TRUE`, the bits specified in `mask` are set. If `FALSE`, they are cleared. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

This function modifies only the bits specified in `mask`. This leaves other bits in the user flag unchanged.

```C
// Mark row 3 of key "dev-1" (its __t__ is 1731601280) as pending, keep the other bits
if(tranger2_set_user_flag(tranger, "messages", "dev-1", 1731601280, 3, 0x0001, TRUE) < 0) {
    // not the master, or the row is not there: logged
}
```

Master only: on a replica, or on a master that lost its lock (see
[`tranger2_stop()`](<#tranger2_stop>)), it writes nothing, logs *"Only master
can write"* and returns `-1`. Up to 7.25.4 it asked nothing: the write of the read-only md2 fd failed,
which is a CRITICAL, and with the default `on_critical_error` the process
exited(0).

---

(tranger2_shutdown)=
## [`tranger2_shutdown()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L672)

The `tranger2_shutdown()` function stops the TimeRanger database, releasing all allocated memory.

```C
int tranger2_shutdown(
    json_t *tranger
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance to be stop. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

This function must be called when the database is no longer needed to free resources.

---

(tranger2_startup)=
## [`tranger2_startup()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L384)

Initializes the TimeRanger 2 database, setting up its internal structures and preparing it for use. The function requires a `hgobj` instance and a JSON configuration object that defines database parameters.

```C
json_t *tranger2_startup(
    hgobj      gobj,
    json_t    *jn_tranger,
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj framework instance managing the database. |
| `jn_tranger` | `json_t *` | A JSON object containing database configuration parameters. See [`tranger2_json_desc`](#tranger2_json_desc) for details. |
| `yev_loop` | `yev_loop_h` | An event loop handle for asynchronous operations. |

**Returns**

A JSON object representing the initialized TimeRanger 2 database instance.

**Notes**

The returned JSON object must be properly managed and eventually passed to [`tranger2_stop()`](#tranger2_stop) or [`tranger2_shutdown()`](#tranger2_shutdown) to release resources.

---

(tranger2_stop)=
## [`tranger2_stop()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L597)

The `tranger2_stop()` function closes the TimeRanger database. This makes sure that all topics and file descriptors are properly closed.

```C
int tranger2_stop(
    json_t *tranger
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance to be closed. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

[`tranger2_shutdown()`](<#tranger2_shutdown>) calls it when nobody did. Call it
yourself when the event loop needs a turn between the two, to finish the
watchers the topics close.

What the stop does:

- It closes every topic, and the fds of `fd_opened_files`: the
  `__timeranger2__.json` lock of a master is one of them, so **the stop gives
  the single-master lock back**. Each closed fd is marked `-1`: a second stop,
  or the shutdown, never closes that number again (by then it can belong to
  somebody else).
- A tranger stays usable after the stop. The first call that opens a topic
  again, or that WRITES (`tranger2_create_topic()`, `_delete_topic()`,
  `_backup_topic()`, `_write_topic_var()`, `_write_topic_cols()`,
  `_append_record()`, `_delete_key()`, `_delete_instance()`,
  `_write_user_flag()`, `_set_user_flag()`, `_set_system_flag()`), **revives** it:
  the shutdown then closes what the revival opened, and a master takes its
  lock again BEFORE anything is written. If another process took the store in
  the meantime (`flock()` answers `EWOULDBLOCK`), it is the startup's
  single-master conflict: the revival logs a CRITICAL at `on_critical_error`,
  *"Master lock NOT retaken after a stop: another process holds it, go on as
  not master"*. With a yuno's default (`"on_critical_error": 2`, exit(0)) the
  process exits there and stays down, as a second instance does at the
  startup. Any OTHER failure to take the lock says nothing of another master
  and is an ERROR, not a critical, and the process goes on: *"Master lock NOT
  retaken after a stop: cannot open the lock file, go on as not master"*
  (`EMFILE`, `ENOENT`...) or *"... flock() FAILED, go on as not master"*
  (`ENOLCK`, `EINTR`...). A tranger that did not take its lock back (and did
  not exit) goes on as a replica: it reads, every write is refused, and its
  json says `"master": false, "master_lost": true`.
  It never takes the lock again, not even once it is free -- its memory did
  not follow what the other master wrote; shut it down and start it again to
  be the master. Read `master` again after a restart: it is what the tranger
  holds now. A lookup of a topic that is already open revives nothing.
- **A TRANSIENT failure demotes for good too.** An `EMFILE` (too many open
  files for a moment), an `EINTR` or an `ENOLCK` at the revive is logged as an
  ERROR, not a critical: the process does NOT exit, it goes on as a replica,
  and it stays one after the cause is gone. Every write it is asked for is
  refused from then on (each with its own error), and nothing else says so.
  Nothing restarts it: the operator does. Watch for it:
  - the log line *"Master lock NOT retaken after a stop"* (any of its three
    endings), at ERROR or CRITICAL;
  - the `master` attribute of the `C_TRANGER` service, which answers what the
    tranger IS (false once demoted). Read it with `view-attrs` WITHOUT
    `attribute=`: that form goes through the gclass' `mt_reading`; the
    one-attribute form answers the stored configuration.

    ```bash
    ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-attrs gobj=<tranger service>'
    # "master": false on a yuno configured as the master = demoted: restart it
    ```
  - in C, `master_lost` true in the tranger's json.

  Restart the yuno (`kill-yuno` + `run-yuno`) once the cause is found: the new
  process takes the lock at its startup.

This is the life of the tranger of a `C_TRANGER` that is stopped and started
again: the tranger is built in `mt_create`, stopped in `mt_stop`, and shut down
only in `mt_destroy`.

```C
json_t *tranger = tranger2_startup(0, jn_tranger, yev_loop);   // master, lock taken
tranger2_open_topic(tranger, "devices", TRUE);

tranger2_stop(tranger);         // topics closed, lock given back
                                // (C_TRANGER mt_stop)

tranger2_topic(tranger, "devices");  // opens it again: revived, lock retaken
                                     // (C_TRANGER started again)

tranger2_shutdown(tranger);     // closes what the revival opened
```

And when another process took the store while it was stopped:

```C
tranger2_stop(a);                                   // a gives the lock back
json_t *b = tranger2_startup(0, jn_tranger_b, 0);   // b takes the store

tranger2_create_topic(a, "devices", "id", "tm", 0, 0, 0, 0);
// a cannot take the lock: nothing written, "devices" opened read-only
kw_get_bool(0, a, "master", 0, 0);        // FALSE
kw_get_bool(0, a, "master_lost", 0, 0);   // TRUE
```

---

(tranger2_str2system_flag)=
## [`tranger2_str2system_flag()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L687)

Converts a formatted string containing system flag representations into a `system_flag2_t` integer. The input string can use delimiters such as '|', ' ', or ','.

```C
system_flag2_t tranger2_str2system_flag(
    const char *system_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `system_flag` | `const char *` | A string containing system flag values separated by '\|', ' ', or ','. |

**Returns**

Returns the corresponding `system_flag2_t` integer representing the parsed system flags.

**Notes**

This function is useful for converting human-readable flag representations into their corresponding bitmask values.

---

(tranger2_topic)=
## [`tranger2_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1509)

Retrieve a topic by its name from the TimeRanger database. If the topic is not already opened, [`tranger2_open_topic()`](<#tranger2_open_topic>) is called to open it.

```C
json_t *tranger2_topic(
    json_t      *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic to retrieve. |

**Returns**

A JSON object representing the topic. The returned object is not owned by the caller and must not be modified or freed.

**Notes**

If the topic exists on disk but has not been opened yet, [`tranger2_open_topic()`](<#tranger2_open_topic>) is automatically invoked.

---

(tranger2_topic_desc)=
## [`tranger2_topic_desc()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L2258)

`tranger2_topic_desc()` retrieves the description of a specified topic from the TimeRanger database.

```C
json_t *tranger2_topic_desc(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose description is to be retrieved. |

**Returns**

A JSON object containing the topic description. The caller must decrement the reference count of the returned JSON object.

The description is a projection of the topic, not the topic itself:

| Key | Description |
|---|---|
| `topic_name` | The name of the topic. |
| `pkey` | The column that is the primary key. |
| `pkey2s` | The columns that are secondary keys, if the topic declares any. |
| `tkey` | The column that carries the time, if the topic declares one. |
| `system_flag` | The system flag the topic was created with. |
| `topic_version` | The version of the topic schema. |
| `system_topic` | `true` when the topic cannot be deleted. |
| `main_topic` | `true` on the topic the tree of a treedb hangs from, as the schema marks it (a topic hooked to itself, one per treedb; stamped by `treedb_open_db()`). |
| `cols` | The column descriptors, as a list. |

A key the topic does not carry is omitted rather than returned empty, so a
topic with no secondary key has no `pkey2s` at all.

`pkey2s` matters to any reader that must NAME a record and not only identify
it. A topic whose id column is flagged `rowid`, `uuid` or `qualified` keys its
records by a value that is not the name: a rowid and a UUID mean nothing to a
person, and a qualified id (`<parent id>.<name>`) names every ancestor together
with the record. `treedb_system_schema` keys `topics` and `cols` with qualified
ids. In all three the name lives in the secondary key. A viewer given only
`pkey` can do nothing but print the address.

**Notes**

The returned JSON object must be properly decremented using `json_decref()` to avoid memory leaks.

---

(tranger2_topic_key_size)=
## [`tranger2_topic_key_size()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1671)

Retrieves the number of records associated with a specific key in a given topic
within the TimeRanger database. If `key` is empty, the **whole-topic** size is
returned instead.

```C
uint64_t tranger2_topic_key_size(
    json_t  *tranger,
    const char *topic_name,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the key. |
| `key` | `const char *` | The key whose record count is to be retrieved. If empty, the whole-topic size is returned (sum over every key). |

**Returns**

Returns the number of records associated with the specified `key`, or the
whole-topic total when `key` is empty.

**Notes**

Counts are from the in-memory cache. A missing topic returns `0`. When `key` is
empty the call delegates to [`tranger2_topic_size()`](#tranger2_topic_size).

---

(tranger2_topic_key_range)=
## [`tranger2_topic_key_range()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1694)

Returns the **time span** of one key of a topic, on both axes, read from the
in-memory cache totals (maintained on load and on every append) — so a client can
bound a time picker to what the key holds without reading a single
record.

```C
json_t *tranger2_topic_key_range( // return is yours
    json_t *tranger,
    const char *topic_name,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the key. |
| `key` | `const char *` | The key whose span is to be retrieved. |

**Returns**

A JSON object, **yours** to decref:

```json
{"fr_t": t, "to_t": t, "fr_tm": tm, "to_tm": tm, "rows": n}
```

`NULL` (silent) if the topic or the key is unknown.

**Notes**

`t` (persistence) and `tm` (message time) are independent, so the two spans
differ whenever data was backfilled. Both are in the topic's own unit: seconds,
or milliseconds when the topic sets `sf_t_ms` / `sf_tm_ms` (read `system_flag`
from the topic desc).

---

(tranger2_topic_is_open)=
## [`tranger2_topic_is_open()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1716)

`TRUE` if the topic is currently open in this tranger. Silent — a closed topic is
a legitimate answer, not an error.

```C
BOOL tranger2_topic_is_open(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic to check. |

**Returns**

`TRUE` if the topic is open, `FALSE` otherwise.

**Notes**

:::{warning}
A topic **owns** the iterators, `rt_mem` and `rt_disk` handles opened on it:
[`tranger2_close_topic()`](#tranger2_close_topic) closes them all
(`tranger2_close_all_lists`) and frees the topic. Anyone caching such a handle
must call this **before touching it** — the handle is freed memory once its topic
is gone, and dereferencing it is a use-after-free.
:::

---

(tranger2_topic_name)=
## [`tranger2_topic_name()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1732)

Retrieves the topic name from the given `json_t *` topic object.

```C
const char *tranger2_topic_name(
    json_t *topic
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `topic` | `json_t *` | A JSON object representing the topic. |

**Returns**

A pointer to a string containing the topic name. The returned string must not be modified or freed by the caller.

**Notes**

If the `topic` parameter is `NULL` or invalid, the behavior is undefined.

---

(tranger2_topic_size)=
## [`tranger2_topic_size()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1646)

`tranger2_topic_size()` retrieves the total number of records present in a specified topic within the TimeRanger database.

```C
uint64_t tranger2_topic_size(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic whose record count is to be retrieved. |

**Returns**

Returns the total number of records in the specified topic as a `uint64_t` value.

**Notes**

If the topic does not exist, the function can return `0`.

---

(tranger2_write_topic_cols)=
## [`tranger2_write_topic_cols()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L2179)

The `tranger2_write_topic_cols()` function updates the column definitions of a specified topic in the TimeRanger database.

```C
int tranger2_write_topic_cols(
    json_t  *tranger,
    const char  *topic_name,
    json_t  *jn_cols  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose column definitions are updated. |
| `jn_cols` | `json_t *` | A JSON object containing the new column definitions. The ownership of this object is transferred to [`tranger2_write_topic_cols()`](<#tranger2_write_topic_cols>). |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

**Master-only.** Replaces the topic's columns **wholesale** — both the in-memory
`topic["cols"]` and the on-disk `topic_cols.json` (not a merge). A columns change
must bump the topic's `topic_version` (and `schema_version` for structural
changes), otherwise the persisted `topic_cols.json` masks the new schema on the
next reload.

The file is replaced, never written in place: `topic_cols.json.new` is removed
if a process left it, created `O_EXCL|O_NOFOLLOW` with the tranger's
`rpermission`, written, fsync'ed, renamed over the old file, and the directory
fsync'ed: it survives a power cut too (it is written when a topic is created,
re-versioned or re-ordered, never per record; up to 7.25.4 it was not
fsync'ed, and a `topic_version` change could leave the new
version on disk over cols lost with the power). The memory takes the new
columns only when the file did; otherwise the call logs (*"Cannot replace
topic_cols.json, ..."*) and returns `-1` with the old file and the old columns in
place. Up to 7.25.4 it wrote in place,
put the columns in memory before writing, and returned `0` whatever happened.
`tranger2_write_topic_var()` replaces `topic_var.json` the same way.

```C
json_t *cols = json_pack("{s:s, s:s, s:s}", "id", "", "content", "", "extra", "");
if(tranger2_write_topic_cols(tranger, "readings", cols) < 0) {  // cols consumed either way
    // not written: topic_cols.json and topic["cols"] are the old ones
}
```

One change does not wait for the bump. When the incoming schema holds the same
columns saying the same things and only their **order** differs,
[`tranger2_create_topic()`](<#tranger2_create_topic>) rewrites the file: the
freeze is there so that a change to what a column declares cannot arrive
unannounced, and an order announces nothing new.

---

(tranger2_write_topic_var)=
## [`tranger2_write_topic_var()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L2092)

The `tranger2_write_topic_var()` function updates the variable metadata of a specified topic in the TimeRanger database.

```C
int tranger2_write_topic_var(
    json_t  *tranger,
    const char  *topic_name,
    json_t  *jn_topic_var
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose variable metadata is to be updated. |
| `jn_topic_var` | `json_t *` | A JSON object containing the new variable metadata for the topic. The ownership of this object is transferred to [`tranger2_write_topic_var()`](<#tranger2_write_topic_var>). |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

**Master-only.** Merges `jn_topic_var` into the existing `topic_var.json` (update,
not replace) and persists it. The in-memory topic is updated too, except the
immutable descriptor fields, and only when the file was written.

The file is never rewritten in place: the merge is written to
`topic_var.json.new` and renamed over `topic_var.json`, so a process that dies
half way leaves the old file or the new one, never an empty one. treedb calls
this on every create of a rowid-key node (the `last_rowid_id` counter lives
here), so it does not `fsync()`: it is safe against the death of the process,
not against a power cut. A `topic_version` change
([`tranger2_create_topic()`](#tranger2_create_topic)) does fsync.

```C
tranger2_write_topic_var(tranger, "items", json_pack("{s:I}", "last_rowid_id", (json_int_t)42));
// topic_var.json: {"topic_version": 3, "last_rowid_id": 42}  (a new inode)
```

---

(tranger2_write_user_flag)=
## [`tranger2_write_user_flag()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L3863)

The `tranger2_write_user_flag()` function updates the user flag of a specific record identified by `rowid` in the given `topic_name` within the TimeRanger database.

```C
int tranger2_write_user_flag(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid,
    uint16_t user_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic containing the record. |
| `rowid` | `uint64_t` | The unique row identifier of the record to update. |
| `user_flag` | `uint32_t` | The new user flag value to be assigned to the record. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function modifies the user flag of an existing record but does not alter other record attributes. Use [`tranger2_set_user_flag()`](<#tranger2_set_user_flag>) if you need to update the flag using a mask.

Master only: on a replica, or on a master that lost its lock (see
[`tranger2_stop()`](<#tranger2_stop>)), it writes nothing, logs *"Only master
can write"* and returns `-1`. Up to 7.25.4 it asked nothing: the write of the read-only md2 fd failed,
which is a CRITICAL, and with the default `on_critical_error` the process
exited(0).

---

(tranger2_list_topic_names)=
## [`tranger2_list_topic_names()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1580)

`tranger2_list_topic_names()` returns a JSON array of topic names by scanning the tranger database directory on disk. Unlike [`tranger2_list_topics()`](#tranger2_list_topics), which reads from the in-memory topic registry, this function reads subdirectory names from the filesystem.

```C
json_t *tranger2_list_topic_names(
    json_t *tranger
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The tranger database handle (JSON object). |

**Returns**

A new JSON array of strings, each being a topic name found as a subdirectory in the tranger database directory. The caller owns the returned array and must call `json_decref()` on it. Hidden entries (names starting with `.`) are excluded.

**Notes**

This function operates on disk, not in memory. It can return topic names that are not currently open, or miss topics that exist only in memory. Use [`tranger2_list_topics()`](#tranger2_list_topics) to get the in-memory list instead.

---

(tranger2_topic_path)=
## [`tranger2_topic_path()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L1536)

`tranger2_topic_path()` writes the filesystem path of a topic into the provided buffer. The path is constructed by appending the topic name to the tranger database directory.

```C
int tranger2_topic_path(
    char *bf,
    size_t bfsize,
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer where the topic path will be written. |
| `bfsize` | `size_t` | Size of the output buffer in bytes. |
| `tranger` | `json_t *` | The tranger database handle (JSON object). |
| `topic_name` | `const char *` | Name of the topic. |

**Returns**

Returns `0` on success.

**Notes**

The resulting path has the format `<directory>/<topic_name>`, where `<directory>` is the tranger database directory stored in the tranger handle.

---

