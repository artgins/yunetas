# timeranger2

**Append-only time-series storage** for Yuneta. Records are grouped into **topics**, keyed by a primary id, and indexed for fast access by time range or rowid. Several higher-level stores are built on top of it:

- **msg2db** — dict-style message store (one value per key)
- **msg2db queue** — message queue with at-least-once semantics (used by the MQTT broker)
- **TreeDB** — graph memory database with hook/fkey relationships (see `kernel/c/root-linux/src/c_treedb.c` and `CLAUDE.md`)

## Core API (selected)

```c
tranger2_startup(...) / tranger2_shutdown(...)
tranger2_open_topic(...) / tranger2_close_topic(...)
tranger2_append_record(...)
tranger2_open_iterator(...) / tranger2_iterator_get_page(...) / tranger2_close_iterator(...)
tranger2_topic_key_size(...) / tranger2_topic_key_range(...)
```

Every call to `tranger2_append_record()` writes one record to disk,
increments the per-key `g_rowid` (monotonic, never resets) and
`i_rowid` (row in the md2 file). TreeDB relies on this for its
link/unlink semantics — see `CLAUDE.md` for the full rules.

## Two time axes: `t` and `tm`

Every record carries **two** timestamps, and they are independent:

| Axis | Meaning | Source |
|------|---------|--------|
| `t`  | **Persistence** time — when the record was appended | the `__t__` argument of `tranger2_append_record()` (now, if 0) |
| `tm` | **Message** time — when the event it carries happened | the record's **tkey** field (usually `tm`), set by the producer |

They diverge whenever data is backfilled or a device uploads a buffered batch
late. Both are in the **topic's** unit: seconds, or **milliseconds** when the
topic sets `sf_t_ms` / `sf_tm_ms`.

An iterator's `match_cond` takes a range on each axis (`from_t`/`to_t`,
`from_tm`/`to_tm`) plus `from_rowid`/`to_rowid` and the `user_flag` conditions,
and **ANDs** them. Every condition is honored **per record**: a filtered paging
iterator builds its row index when it opens, so `tranger2_iterator_size()`,
`pages` and the pages themselves count only matching records (and `get_page`'s
`from_rowid` is then a position among the matching rows, not a global rowid). An
unfiltered iterator builds no index — its open stays cheap regardless of key
size. The index is a **snapshot** taken at open: records appended after a
filtered iterator opens never enter its count or its pages, while an unfiltered
one recounts the key on every call. `tranger2_topic_key_range()` reports a
key's span on both axes without reading a record.

The rows of a key are in `t` order except in a file that holds a late record
(marked `<file>.unordered`); a `t` scan reads such a file through instead of
stopping at the first row past its range.

`tm` is written by the producer, and the files are cut by `t`. A topic created
after 7.25.4 carries `"marks_tm_unordered": true` in its `topic_desc.json`, and
its master marks a file whose `tm` goes back (`<file>.tm_unordered`); a load
reads a marked file whole, so every file's `[fr_tm, to_tm]` is exact. A `tm`
condition then leaves out the files that do not meet it (the scan steps over
the hole), and in an unmarked file the first row past the range ends the scan
of that FILE, not of the key. In a marked file, and in every file of a topic
created by 7.25.4 or earlier (no file's `tm` range can be trusted there), a
`tm` condition skips rows and ends nothing.

Such a topic reads every md2 row of the key on a `tm` query, so its cost grows
with the files of the key: on one key of 30 files x 20000 rows, a query of 26
rows took 13.6 ms in 7.25.4, 408 ms now, and 0.09 ms once the topic is marked.
Mark it, once, with `tranger2_mark_tm_order()` (the `mark-tm-order` command of
`C_TRANGER`): it reads every md2 file once, writes the markers the files
need, and sets `"marks_tm_unordered": true`. Its cost is linear in the rows
and in the files, and it blocks the yuno while it runs: 16 ms for those 30
files x 20000 rows, 72-88 ms for 4 keys of 3650 daily files (warm page cache).
Run it again after a rollback to a binary that appends without markers.

```C
json_t *report = tranger2_mark_tm_order(tranger, "readings");  // master only
// {"files": 30, "rows": 600000, "tm_unordered_marked": 2, "marks_tm_unordered": true, ...}
JSON_DECREF(report)
```

The master writes a marker BEFORE the md2 row of the record that needs it, so
a crash leaves at worst a marker with no row (a whole read of that file), never
a row with no marker. A marker that cannot be written is logged, the master
still reads the file whole, and the next append to the file writes it.

```C
/*  E1 t=100, E2 t=50000, E3 t=200 (late) in one file: [150, 250] gives E3  */
json_t *it = tranger2_open_iterator(tranger, "readings", key,
    json_pack("{s:I, s:I}", "from_t", (json_int_t)150, "to_t", (json_int_t)250),
    NULL, "range", "", data, NULL);

/*  three day files, tm 100 / 5000 / 150: to_tm 300 gives D1 D3  */
json_t *it2 = tranger2_open_iterator(tranger, "readings", key,
    json_pack("{s:I}", "to_tm", (json_int_t)300),
    NULL, "range2", "", data2, NULL);
```

> **In the md2 record, the times carry flags.** On disk the 16 high bits of
> `__t__` hold the `user_flag` and those of `__tm__` the `system_flag`; only the
> low 44 bits are the time. Always read them through `get_time_t()` /
> `get_time_tm()` — the raw field yields a timestamp with the flags baked in.

> **A topic OWNS the handles opened on it** (iterators, `rt_mem`, `rt_disk`):
> `tranger2_close_topic()` closes them all and frees the topic. Anyone caching a
> handle must check `tranger2_topic_is_open()` first — it is freed memory once
> its topic is gone.

See [`yunos/c/yuno_agent/YUNO_TREEDB.md`](../../../yunos/c/yuno_agent/YUNO_TREEDB.md)
for the full timeranger2 + treedb walkthrough (mental model, on-disk
layout, master/non-master locking, snapshots, the cross-yuno
`rt_by_disk` pattern, sharp edges and recipes).

## Two delete granularities (record vs instance)

In timeranger2 the data model is **two-level**:

- A **record** = a primary `key`. Lives in its own directory
  `keys/<key>/` with its own `.md2` index.
- An **instance** = one entry in that key's time series. A row in
  the `.md2` file, addressed by `(key, __t__, rowid)`.

`tranger2_append_record(key, ..., __t__)` adds **one instance** of
the record `key`. The naming is historical; the contract operates on
instances.

| Granularity | API | What it does |
|---|---|---|
| Whole record (key + all instances) | **`tranger2_delete_key`** (was `tranger2_delete_record` before 2026-05-25; legacy alias kept in `timeranger2.h`) | `rmrdir` of `keys/<key>/` + drop from `topic_cache` + propagate to in-process subscribers + mirror to `disks/<rt_id>/<key>/` for `rt_by_disk` followers. Irrecoverable. |
| One instance | **`tranger2_delete_instance`** (2026-05-26) | Mutates the `.md2` row in place with `sf_deleted_instance = 0x0400` (inherited side, followers see the same tombstone). Optional `zero_payload` overwrites the matching bytes in the data `.json` for sensitive-data wipes. Read paths skip dead rows; rowids do NOT renumber. Master-only, idempotent. |

### Path-traversal hardening (since 7.6.0)

A string primary key or treedb node `id` becomes a filesystem path component
(`keys/<key>/…`). Since 7.6.0 the key/id is validated before any filesystem use
— a value containing `/` or beginning with `.` (so `..`, absolute paths, and
hidden traversal can't escape the topic directory) is rejected, on the create,
lookup, **and** `tranger2_delete_instance` paths, with the mirror predicate
aligned so followers reject the same inputs. Regression coverage in
`tests/c/timeranger2/test_pkey_path_traversal.c`.

The **topic name** got the same confinement later (since 7.25.0):
it was checked only for being empty, so a name such as `../other_db/users`
read, planted or deleted a topic of another database, and a name that is a
directory but not a topic (`..`, `<topic>/keys`, any stray directory) reached
`load_persistent_json()` as a critical — an `exit(0)` with
`on_critical_error=2`. Create, open, delete, backup, `tranger2_topic_path()` and
`tranger2_write_topic_var()` / `_cols()` now refuse an empty name, `.`, `..`,
and any name holding `/` or `` ` ``. A leading `.` stays legal (MQTT queues
are `<client_id>-IN/-OUT`, and the broker accepts `.foo` as a client id).
`tranger2_open_topic()` answers `NULL` for a directory without
`topic_desc.json`. `tranger2_delete_topic()` and `tranger2_backup_topic()` are
master-only, like every other destructive call. The **rt id** of a disk feed
(`<topic>/disks/<id>/`) follows the directory half of the rule, and is refused
longer than `NAME_MAX` too; an id already in use by a live feed of the topic is
refused whatever its creator (one id, one directory, one feed). The id may come
from a peer, so every refusal of it is a warning, an empty id included (*"Invalid
rt id (empty)"*). The one-feed guard is per process: two processes that follow
one store with the same id still take each other's directory. Regression
coverage in `tests/c/timeranger2/test_topic_path_traversal.c` and
`test_rt_disk_multi_feed.c`.

### `on_critical_error` is for writes, never for reads

A failed READ of records logs a critical and answers an error; it never exits
the process, whatever `on_critical_error` says (since 7.25.0). A
read has written nothing, and the default `2` turned one into an `exit(0)`
nothing relaunches — a key deleted under an open iterator was enough. A read
does not create files either: `tranger2_read_user_flag()` on a `(key, __t__)`
with no md2 answers *"Record metadata file not found"*. What keeps exiting on
purpose is the write side, above all a short write of an md2 row, which would
misalign every later append. Regression coverage in
`tests/c/timeranger2/test_read_never_exits.c`.

### A stopped master takes its lock again before it writes

`tranger2_stop()` gives the single-master lock back (C_TRANGER's `mt_stop`).
The next call that opens a topic or writes -- `tranger2_create_topic()` is the
restart path of C_TRANGER and C_TREEDB -- takes it again FIRST. When another
process took the store meanwhile (`flock()` answers `EWOULDBLOCK`), nothing is
written, and it is the startup's single-master conflict: a CRITICAL at
`on_critical_error`. With a yuno's default (`2`, exit(0)) the process exits and
stays down, as a second instance does at the startup. Any OTHER failure to
take the lock -- the lock file cannot be opened (`EMFILE`, `ENOENT`...), or
`flock()` fails with `ENOLCK`, `EINTR`... -- says nothing of another master:
it is an ERROR and the process goes on. A tranger that did not take its lock
back (and did not exit) goes on as a replica, reads, refuses every write, and
says so in its json:

```C
kw_get_bool(0, tranger, "master", 0, 0);        // FALSE: what it holds now
kw_get_bool(0, tranger, "master_lost", 0, 0);   // TRUE: it was the master, it lost the lock
```

A demoted tranger never takes the lock again, not even once the other process
let it go: its memory (the caches of its topics, the lists a treedb opened as
master) did not follow what the other master wrote, and appending on top of it
would number rows that exist. To be the master again, shut it down and start
it again (`tranger2_shutdown()` + `tranger2_startup()`).

**This holds for a transient failure too.** An `EMFILE`, `EINTR` or `ENOLCK`
at the revive is an ERROR, the process does not exit, and it stays a replica
for good: every later write is refused, until somebody restarts the yuno.
Monitor the log for *"Master lock NOT retaken after a stop"*, and the `master`
attribute of the `C_TRANGER` service (`view-attrs gobj=<service>` without
`attribute=`, which reads it through `mt_reading`): false on a yuno configured
as master means demoted.

Until 7.25.4 `tranger2_create_topic()` read the `master` of the stop and wrote
the topic directory, `topic_cols.json` and `topic_var.json` into the other
master's store before it noticed. Regression coverage in
`tests/c/timeranger2/test_lost_lock.c`.

### `topic_var.json` is replaced, never rewritten in place

It holds the `topic_version` and treedb's `last_rowid_id` counter, and it is
written on every create of a rowid-key node. Every write goes through
`topic_var.json.new` and a `rename()`: a process that dies half way leaves the
old file or the new one, never an empty one. The hot path does not `fsync()`
(safe against the death of the process, not against a power cut); a
`topic_version` change does. The `.new` is removed first and created
`O_EXCL|O_NOFOLLOW` with the tranger's `rpermission`, so a leftover lends the
file neither its mode nor, as a symlink, its target. `topic_cols.json`
(`tranger2_write_topic_cols()`) is replaced the same way, always fsync'ed with
its directory (it is written rarely, and a `topic_version` change writes it
before the version: the version must not survive a power cut the cols did
not), and the memory takes the new cols only when the file did (it returns -1
otherwise).
`tests/c/timeranger2/test_topic_var_replace.c`.

### A history that cannot be read whole says so

A load that meets a row whose metadata or whose content cannot be read stops
there and leaves `"load_failed": true` in the iterator; the rows before it, in
the load's direction, were handed to the callback (forward: the oldest,
backward: the newest). Up to 7.25.4 a content that could not be read reached
the callback as `NULL` and the load went on (treedb made a node of it with id
`""`). The same holds after a RESTART: a `.md2` file the topic's cache could
not count when it was built at the open -- one that cannot be opened or read
-- flags its key
(`"unreadable": [file_id, ...]` in its cache, logged once), every iterator of
the key says `load_failed`, and a load stops where the first such file is in
its direction. Up to 7.25.4 the cache build dropped such a file and nothing
failed. `tranger2_delete_key()` clears the flag with the key, and so does a
cell that counts the file again (an append into it that finds it readable; a
follower that reads it whole). `tests/c/timeranger2/test_unreadable_at_open.c`.

A `.md2` of 0 rows gets no cell and flags nothing. With a `.json` that is not
empty it is what an append never acknowledged leaves -- the content is written
first and the md2 row after -- and it is ignored with a warning naming the file
(*"md2 file of the key with no rows and a content file that is not empty: an
append that was never acknowledged, the file is ignored"*). 7.25.4 ignored its
rows too, without a warning. An append whose md2 fails does not leave it: it
answers -1 and cuts its content back, and the cut comes BEFORE the critical
that reports the failure. With the exit bit of `on_critical_error` (`2`, the
default of `C_TRANGER` and of `C_TREEDB`'s `exit_on_error`) that critical ends
the process inside the log call; 7.25.4 did not cut it back at all. A write that
stops part way (the file size limit, a full disk) is logged as a short write,
with the bytes written and expected. A kill or a power cut between the two writes
still leaves the shape, and the next open ignores it with the warning above.
An append into a file still flagged unreadable is refused; one into a flagged
file readable again counts it first, and a FILTERED iterator of the key takes
its segments and its index again (the rowids after the file moved). `tests/c/timeranger2/test_uncommitted_append.c`.

A `.md2` whose size is not a whole number of 32-byte rows is not damage
either. Its last row is torn: a power cut came during the write of the row,
and the md2 row is the commit point of an append, so that append was never
acknowledged. A MASTER cuts the `.md2` back to its whole rows
(`floor(size / 32) * 32`) when it first reads the file -- the cache build at
the open, the count of a flagged file, or the read of the key again after a
`tranger2_delete_key()` that could not remove the key's directory -- logs ONE
warning, and the key loads whole. The next append goes into the same file. The `.json` is left as
it is: its bytes after the last row belong to no row.

```text
WARNING: {"function": "load_first_and_last_record_md", "msgset": "Tranger",
    "msg": "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back",
    "topic": "alarms", "key": "dev1", "file_id": "2026",
    "path": "<store>/alarms/keys/dev1/2026.md2", "old_size": 109, "new_size": 96}
```

The cut removes fewer than 32 bytes, all after the last whole row. An append
writes its row on a row boundary: if the md2 of a running master ends in a torn
row (a row written in part whose cut back failed), the next append cuts it back
first, with the same warning, and a cut that fails refuses that append (the
next append tries again). A cut that fails at the open is damage (the file is
flagged).

7.25.4 did not cut a torn row back: the next appends wrote their rows after the
torn bytes, on no row boundary, and those rows were acknowledged. A cut of
such a file removes the end of its last acknowledged row. So every cut (open,
flagged count, delete-key read, append) first checks that the tail is a torn
row after good rows. A row is good when its content is WHOLE: inside the
`.json`, one json value and one NUL byte after it, as an append writes it (or
only zero bytes, for an instance deleted with its content zeroed). A NUL inside
a string is written as the escape `\u0000`: it is part of the value, not a NUL
byte. A content that the process that checks it has not the memory to parse
is not a content that is not whole: the largest memory block
(`gbmem_get_maximum_block()`) is set by `MEM_MAX_BLOCK` for each yuno, and the
yuno that wrote it can have a larger block. A content larger than that block
is read in parts: a NUL before its last byte says that it is not whole, with
no block of its size, and when its only NUL is its last byte the check cannot
tell. A content that fits in the block can still need more to parse (a long
string, a long array or object); when its parse fails for memory, the check
cannot tell either. The tail is not cut on a content that the check cannot
tell. The rules:

1. the last 32 bytes, read as a row, are NOT a row of their own: their content
   does not end exactly at the end of the `.json`, and it is not whole;
2. the last whole row is good, and so is the whole row before it, whose content
   ends at or before the start of the content of the last one.

Rule 1 stops the cut of every file 7.25.4 left whose last row still names its
content as 7.25.4 wrote it, also with content after the last row in the
`.json` (an append killed between its two writes, which 7.25.4 did not cut
back): the last row is a real row, and its content is whole. When the process
that checks has not the memory to parse that content, the check cannot tell,
and that is enough to not cut. The last 32 bytes of a torn row are bytes moved out of their fields,
and name a whole record to the byte only by a coincidence of 64-bit values: the
file is then flagged, not cut. The content is read only for a `.md2` that is
not a whole number of rows, at most three records.

If a rule fails, the file is not cut and its bytes do not change: a CRITICAL
names the shape and its `cause`, the file is flagged (every load of the key
says `load_failed`), and an append into it is refused. An append that finds
such a tail flags the file in memory the same way (*"md2 file of the key
flagged unreadable at an append: every load of the key says load_failed"*),
before the CRITICAL of its refusal. With the exit bit of `on_critical_error`
(the default) the process ends in that CRITICAL and the next open flags the
file again, so the flag in memory matters only when `on_critical_error` does
not exit. A check that cannot run refuses the append and does not flag the
file. The file is repaired by hand (the repair is in the treedb docs, *A topic that
did not load whole*):

```text
CRITICAL: {"function": "check_torn_md2_rows", "msgset": "Tranger",
    "msg": "md2 file of the key ends in a whole row that is not on a row boundary: written by 7.25.4 after a torn row; not cut, repair it by hand",
    "cause": "its content is a whole record of the content file",
    "topic": "alarms", "key": "dev1", "file_id": "2026",
    "path": "<store>/alarms/keys/dev1/2026.md2", "md2_size": 173,
    "content_size": 242, "row_at": 141, "__offset__": 160, "__size__": 32}
```

The other `cause`s of this shape are *"its content ends the content file"*
and *"its content has no NUL but the one at its end, and this process has not
the memory to parse it (MEM_MAX_BLOCK): it can be a record written by a yuno
with a larger block"*. The other shape is *"md2 file of the key ends in a part
of a row after a last whole row that is not valid: not cut, repair it by
hand"*, with the `cause` *"its content is not inside the content file"*, *"its
content is not a record"*, *"this process has not the memory to parse its
content (MEM_MAX_BLOCK): it cannot be checked"*, *"the whole row before it is
not a good row"*, *"this process has not the memory to parse the content of
the whole row before it (MEM_MAX_BLOCK): it cannot be checked"* or *"its
content starts before the end of the content of the row before it"*. A REPLICA never
writes: it reads the whole rows and logs nothing for a torn row, because it
also sees a torn row while a live master writes it. It makes the same check,
so it does not read the rows of a file that 7.25.4 wrote on no row boundary. The exception is a file with no whole row: for the
replica it is a md2 of 0 rows, and with its content not empty it gets the
0-rows warning above. Up to 7.25.4 the cache build logged a CRITICAL
(*"Cannot read last record, md2 file corrupted"*) and left the whole file out
of the key, on a master and on a replica.
`tests/c/timeranger2/test_torn_md2_tail.c`.

When the check cannot RUN, it logs one of three CRITICALs, and the file is not
cut: *"Cannot check the torn tail of a md2 file, its content file cannot be
read: not cut"* (the `.json` cannot be opened, `EMFILE`, `ENFILE`, or read;
with its `path`), *"Cannot check the torn tail of a md2 file, no memory to
read a content: not cut"* (with the `path` of the `.json` and the `size`), or
*"Cannot read a record of md2 file, read FAILED"* / *"..., short read"* (a row
of the `.md2` cannot be read; with its `path`, the `row` and its `offset`). At an open the file is flagged too: without
the check, the whole rows can be the moved rows of the shape 7.25.4 left; a
master's next append into the file counts it again, and the check runs then.
At an append it found nothing wrong: the append is refused with -1, its
content is cut back, the file is NOT flagged, and the next append checks again
(*"Cannot append record, the torn tail of its md2 file cannot be checked now:
the append is refused, the file is not flagged"*, with `topic`, `key`,
`file_id`, `md2_size`). With the default `on_critical_error` the refusal's
CRITICAL exits the yuno, and the next start checks the file at its open (and
flags it when the check still cannot run). `tests/c/timeranger2/test_torn_tail_check_fails.c`.

A string of a record can hold NUL characters: `json_dumps()` writes each one as
`\u0000`, and a read hands it back. A consumer that reads it with
`kw_get_str()` gets the C string up to the first NUL. A content that is not
json when it is read logs *"Bad data, the content of the record is not json"*,
with the jansson `error` and `position`. `tests/c/timeranger2/test_nul_escape_record.c`.

A content that this process has not the memory to parse logs *"Cannot read
the record, this process has not the memory to parse its content
(MEM_MAX_BLOCK)"*, and the record is not handed: a content smaller than the
largest memory block can need more to parse (jansson doubles the buffer of a
string, and the table of an array or an object), and a yuno with a larger
`MEM_MAX_BLOCK` reads it. Up to 7.25.4 a string longer than about half of the
block crashed the process: jansson wrote past its buffer (fixed by
`kernel/c/linux-ext-libs/patches/jansson/`). `tests/c/timeranger2/test_torn_tail_check_fails.c`.

`tranger2_open_list()` of ONE key answers `NULL` when the history of the key
did not load whole. A KEYLESS list loads every key it can read, opens its
feed, and names the others in the handle:

```C
json_t *list = tranger2_open_list(tranger, "items", match_cond, extra, "", FALSE, "");
// list: {"list_type": "rt_mem", "load_failed": true, "load_failed_keys": ["k2"], ...}
```

A caller that answers a question from the list reads `load_failed` (treedb
does: see "A topic that did not load whole" in the TreeDB doc page,
`docs/doc.yuneta.io/api/timeranger2/treedb.md`). Its one-shot iterators have a creator of
their own, so an iterator the caller keeps open on a key does not block the
load. `tests/c/timeranger2/test_open_list_history.c`.

### Subscriber propagation on `tranger2_delete_key`

In-process subscribers (rt_mem, rt_disk in the same yuno, open_iterator)
register interest with:

```c
tranger2_set_rt_key_deleted_callback(handle, cb, user_data);
```

When the master calls `tranger2_delete_key()`:

1. Every `topic/disks/<rt_id>/<key>/` subdirectory is removed before
   the live `keys/<key>/` directory. `rt_by_disk` followers
   recursive-watching `disks/<rt_id>/` pick this up as
   `FS_SUBDIR_DELETED_TYPE` and run the same callback fan-out on
   their side.
2. In-process subscribers whose `key` filter matches receive the
   `key_deleted_callback`. Handles that own an inotify watcher
   (`fs_event_client` set) are skipped here — the inotify branch
   delivers the same event.

Pre-2026-05-26 followers that polled their cache on a timer can drop
the timer. See `tests/c/timeranger2/test_delete_key_propagation.c`
for the regression coverage.

The deletion also drops the key from the **watermark** of every `rt_disk` feed
(below): a feed that outlives many keys must not carry a mark for each one, and
a key re-created afterwards must not inherit the dead one's.

And it drops what the key's open iterators took from its cache: an unfiltered
iterator takes its segments again at its next page (a key written again with
the same rows and files, spread another way, read the new files with the old
segments until 7.25.4), a filtered one pages an empty index.
`tests/c/timeranger2/test_key_reborn_pages.c`.

### What a realtime feed hands its callback

The rowid a feed delivers is the key's **global `g_rowid`** — the one that
never resets — not the row's position in the `.md2` file it happens to live in
(that one is `i_rowid`, and it restarts at 1 with every new file). A consumer
may therefore dedupe or page by it across a file rotation.

That distinction is also the feed's own bookkeeping: each `rt_disk` feed keeps
a watermark of the last row it was given **per key AND per file**, because the
shared key cache advances with the first wake-up of a batch and a feed served
by a later one would otherwise find "nothing new". A watermark that forgot
which file it counted in became a ceiling over the next one, and the first
record of every new file reached no feed at all (fixed 2026-07-14;
`tests/c/timeranger2/test_rt_disk_multi_feed.c` covers the rotation, the
several-feeds-per-key fan-out and the re-created key).

A feed opened with `only_md` receives the record's **metadata but not its
body**: its callback is handed a `NULL` `jn_record`, exactly like the historical
iterator. The realtime feeds honor `only_md` too (both the master `rt_mem`
fan-out and the follower `rt_disk` path), and `rt_disk` skips the per-record
disk read when no audience of the key needs the body (fixed 2026-07-15; the
`only_md` feeds in `test_rt_disk_multi_feed.c` cover it).

## `file` columns: bytes with an owner

A treedb is held in memory and timeranger2 rewrites the **whole record** on
every update, so a photo cannot be a field of a node. A column flagged
`['fkey','file']` is an fkey into `__assets__`, a system topic every treedb
creates at open: the **index** lives in memory and the **content** on disk,
under `<treedb dir>/.blobs/ab/cd/<sha256>.<ext>`. The id of an asset IS the
sha256 of its bytes, so the same content stored twice is one asset and a
served url can be cached for ever.

The hooks of `__assets__` are **derived** from the `file` columns themselves
and never persisted, so a host declares nothing but the column. The write
path takes the bytes BESIDE the record (a `__files__` manifest: `content64`
from a browser, or `offset`/`size` slices of the kw's one `gbuffer` from a C
caller), re-hashes them, checks the size before decoding and the type on the
BYTES, rewrites the column into the full fkey reference and **links it
itself**, `autolink` or not: a `file` column is edited by handing over a
file, and the link is part of the hand-over (`""` unlinks).

`treedb_store_files()`, `treedb_import_files()` and `treedb_gc_files()` are
the API; `import-assets` and `gc-assets` are the commands of `C_NODE` on top
of them. The gc keeps what a LIVE node of any treedb of the tranger links,
and what an activation of an existing snap would LOAD — per key, the newest
instance under the snap's tag, which (since 7.24.0) is the record
the snap SHOT: a save is always untagged -- only shoot-snap tags a record,
active snap or not, and a record is tagged once -- so a snap freezes what was live at the shot and holds it
until its `__snaps__` row is deleted; and it takes the bytes **no row names**,
which is what an interrupted write leaves behind. `delete-node` on an `__assets__` row runs the
same guards, and `force` does not override them.

Full account, including the defects the implementation and its review found
(§16): [`DESIGN-treedb-files.md`](DESIGN-treedb-files.md).

## Filesystem watcher

`src/fs_watcher.c` implements an inotify-based watcher (`fs_event_t`) used by `root-linux/C_FS` and by the stores themselves to react to on-disk changes.

## CLI companions

- `utils/c/tr2list` — list records in a topic
- `utils/c/tr2keys` — list keys in a topic
- `utils/c/tr2search` — search by filter
- `utils/c/tr2migrate` — migrate from legacy timeranger v1
- `utils/c/list_queue_msgs2` — list a msg2db queue
- `utils/c/msg2db_list` — list a msg2db topic
- `utils/c/treedb_list` — list TreeDB nodes

## Tests

`tests/c/timeranger2`, `tests/c/tr_msg`, `tests/c/tr_queue`, `tests/c/tr_treedb`, `tests/c/tr_treedb_link_events`, `tests/c/tr_treedb_files`.
