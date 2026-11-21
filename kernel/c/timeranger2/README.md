# timeranger2

timeranger2 is the append-only time-series storage of Yuneta. It groups records into topics. A topic is a named group of records. Each record has a primary id as its key, and an index gives fast access to it by time range or by rowid. Several higher-level stores are built on top of timeranger2:

- msg2db: dict-style message store (one value per key)
- msg2db queue: message queue with at-least-once semantics (used by the MQTT broker)
- TreeDB: graph memory database with hook/fkey relationships (see `kernel/c/root-linux/src/c_treedb.c` and `CLAUDE.md`)

## Core API (selected)

The main calls:

```c
tranger2_startup(...) / tranger2_shutdown(...)
tranger2_open_topic(...) / tranger2_close_topic(...)
tranger2_append_record(...) / tranger2_append_record_text(...)
tranger2_open_iterator(...) / tranger2_iterator_get_page(...) / tranger2_close_iterator(...)
tranger2_topic_key_size(...) / tranger2_topic_key_range(...)
```

Every call to `tranger2_append_record()` writes one record to disk,
increments the per-key `g_rowid` (monotonic, never resets) and
`i_rowid` (row in the md2 file). A purge of the old files of a key
(`tranger2_delete_key_files()`, below) keeps that promise: the records left
keep their `g_rowid`, and the live rowids of the key start after the rows it
took. TreeDB depends on this for its
link/unlink semantics. `CLAUDE.md` gives the full rules.

`tranger2_append_record_text()` is the same append for a record whose content
is already json TEXT (a frame as a peer sent it): the text is written as it is,
with no parse of the whole record and no dump. The store makes sure that the text
is one json object that a read parses (`json_scan_object_members()`), and takes
the pkey and the tkey from it. Both appends share the write half
(`append_content()`), so they write the same way.

## Two time axes: `t` and `tm`

Every record carries two timestamps, and they are independent:

| Axis | Meaning | Source |
|------|---------|--------|
| `t`  | Persistence time: when the record was appended | the `__t__` argument of `tranger2_append_record()` (now, if 0) |
| `tm` | Message time: when the event it carries happened | the tkey field of the record (usually `tm`), set by the producer |

The two times are different when data is backfilled, or when a device uploads
a buffered batch late. Both are in the unit of the topic: seconds, or
milliseconds when the topic sets `sf_t_ms` / `sf_tm_ms`.

The `match_cond` of an iterator takes a range on each axis (`from_t`/`to_t`,
`from_tm`/`to_tm`), plus `from_rowid`/`to_rowid` and the `user_flag`
conditions. It ANDs them. Every condition applies to each record. A filtered
paging iterator builds its row index when it opens. Thus
`tranger2_iterator_size()`, `pages` and the pages themselves count only
matching records. The `from_rowid` of `get_page` is then a position among the
matching rows, not a global rowid. An unfiltered iterator builds no index, so
its open stays cheap for any key size. The index is a snapshot taken at open.
Records appended after a filtered iterator opens never enter its count or its
pages. An unfiltered iterator counts the key again on every call.
`tranger2_topic_key_range()` reports the span of a key on both axes without
reading a record.

The rows of a key are in `t` order, except in a file that holds a late record
(marked `<file>.unordered`). A `t` scan reads such a file to the end, and does
not stop at the first row past its range.

The producer writes `tm`, and the files are cut by `t`, so nothing orders
`tm`. A `tm` condition is a FILTER on every row of the files of the key. It
leaves no file out and ends no scan. On one key of 30 files x 20000 rows, a
`tm` query of one minute reads the 600000 rows: ~15 ms
(`performance/c/perf_timeranger2`). The scans read the md2 rows in blocks of
1024, with one `pread()` for each block. They parse their match condition
once, not for each row.
If you need fast access by `tm`, keep it yourself, in a topic keyed by it or
in an index in memory. The store keeps the records as they came.

```C
/*  T1 tm=500, T2 tm=100, T3 tm=300 in one file: [50, 200] gives T2  */
json_t *it = tranger2_open_iterator(tranger, "readings", key,
    json_pack("{s:I, s:I}", "from_tm", (json_int_t)50, "to_tm", (json_int_t)200),
    NULL, "tm", "", data, NULL);
```

From 7.25.5 to 7.25.22, the master also marked a file whose `tm` went back
(`<file>.tm_unordered`, and `"marks_tm_unordered": true` in the
`topic_desc.json`). A scan trusted the `tm` range of the files not marked.
`tranger2_mark_tm_order()` / `mark-tm-order` migrated an older topic. All of
that is gone. A store written in that period keeps its markers, and the store
ignores them. The `tm` range that a file shows in `list-keys` (`fr_tm` /
`to_tm`) is its first and last rows, unless the file was read whole (a late
`__t__`). This range is approximate when the `tm` of the file goes back.

The master writes the `t` marker BEFORE the md2 row of the late record. Thus a
crash leaves at worst a marker with no row (a whole read of that file), never a
row with no marker. If the master cannot write a marker, it logs the fault and
still reads the file whole. The next append to the file writes the marker.

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

> In the md2 record, the times carry flags. On disk, the 16 high bits of
> `__t__` hold the `user_flag`, and those of `__tm__` hold the `system_flag`.
> Only the low 44 bits are the time. Always read them through `get_time_t()` /
> `get_time_tm()`. The raw field gives a timestamp with the flags in it.

> A topic OWNS the handles opened on it (iterators, `rt_mem`, `rt_disk`).
> `tranger2_close_topic()` closes them all and frees the topic. If you keep a
> handle, call `tranger2_topic_is_open()` before you use it. After its topic
> is closed, the handle is freed memory.

[`yunos/c/yuno_agent/YUNO_TREEDB.md`](../../../yunos/c/yuno_agent/YUNO_TREEDB.md)
gives the full timeranger2 + treedb walkthrough (mental model, on-disk
layout, master/non-master locking, snapshots, the cross-yuno
`rt_by_disk` pattern, sharp edges and recipes).

## Three delete granularities (record, old files, instance)

In timeranger2, the data model has two levels:

- A record is a primary `key`. It has its own directory
  `keys/<key>/` with its own `.md2` index.
- An instance is one entry in the time series of that key. It is a row in
  the `.md2` file, addressed by `(key, __t__, rowid)`.

`tranger2_append_record(key, ..., __t__)` adds one instance of
the record `key`. The name is historical. The contract works on
instances.

| Granularity | API | What it does |
|---|---|---|
| Whole record (key + all instances) | **`tranger2_delete_key`** (was `tranger2_delete_record` before 2026-05-25. The legacy alias stays in `timeranger2.h`) | `keys/<key>/` moved out of `keys/` whole (a `rename()`), then removed + drop from `topic_cache` + propagate to in-process subscribers + a delete signal `disks/<rt_id>/.d<seq>.<key>` (the topic's delete sequence) for `rt_by_disk` followers. Irrecoverable. |
| The old files of a key | **`tranger2_delete_key_files`** (7.27.5) | Retention: deletes every file of the key whose id sorts before the file of `before_t`, and keeps every `g_rowid` where it was. The rows deleted go to the base of the key, `keys/<key>/.purged`, made durable before any file goes. Followers hear `disks/<rt_id>/.p.<key>`. Master-only. See [Retention](#retention-the-old-files-of-a-key) below. |
| The old files of a key, kept | **`tranger2_archive_key_files`** (after 7.28.11) | The same purge, and the files taken out of the topic are hard-linked into a batch of an archive directory, for another process to send or read. See [Archive](#archive-keeping-the-old-files) below. |
| One instance | **`tranger2_delete_instance`** (2026-05-26) | Mutates the `.md2` row in place with `sf_deleted_instance = 0x0400` (inherited side, followers see the same tombstone). Optional `zero_payload` overwrites the matching bytes in the data `.json` for sensitive-data wipes. Read paths skip dead rows. Rowids do NOT renumber. Master-only, idempotent. |

### Retention: the old files of a key

`tranger2_delete_key_files(tranger, topic, key, before_t)` frees the disk of
the old part of a key while the topic is open, without renumbering it. A
retention period ("keep 30 days of readings") uses this function.
[`DESIGN-retention.md`](DESIGN-retention.md) gives the design and the reasons
for it.

```c
/*
 *  Keep 30 days of `raw_events` of one device: delete the files whose
 *  records were all appended before now - 30 days (a topic in seconds)
 */
json_int_t rows = tranger2_delete_key_files(
    tranger, "raw_events", device_id, time_in_seconds() - 30*86400
);
if(rows < 0) {
    // Error already logged: refused, nothing deleted -- or the purge applied
    // with its base not durable (its directory not fsync'ed): the files stay
    // until the next open of the master
}
```

- The cut is by file and by the time of the append. A file goes when its id
  sorts before the id of the file that `before_t` goes to. No row is read, and
  `tm` plays no part. The store keeps at least the period asked for, and at
  most one file more (with `%Y-%m-%d`, "30 days" keeps 30 to 31).
- `g_rowid` does not move. The rows deleted are counted in the base of the
  key, `keys/<key>/.purged` (`{"upto": "<last file id deleted>", "rows": N,
  "t": <when>}`). The live rowids of the key are `purged+1 .. purged+rows`.
  `tranger2_topic_key_size()` counts the live rows, and
  `tranger2_topic_key_range()` adds `"purged": N`. A rowid bound of a
  match_cond counts from the base. A negative bound counts from the end of the
  key AT THE OPEN: the store resolves it then, once. The positions of a page
  also count from the base. Position N of an unfiltered iterator is
  `g_rowid = purged + N`, with the base that the key had when the iterator
  OPENED. A purge after the open moves no position. The positions of the rows
  that it took return empty, so a client that pages forward skips no live
  row. A filtered iterator keeps its index (g_rowids, which a purge does not
  move), and its pages skip the rows purged. In `C_TRANGER`, a multi-key
  iterator can have a FILTERED part whose key was purged after the open. Such
  an iterator is closed at its next `get-page` (*"old rows of its key ... were
  purged since it opened: open it again"*). Its index is built again for each
  page, and nothing kept how far its positions moved.
- Crash-safe. The store writes the base with an `fsync()` of the file and of
  its directory BEFORE it deletes any file. A crash before the base leaves the
  key as it was (the next open removes a `.purged.new` that is left). A crash
  after the base leaves files that the base already counts, and the next open
  of the master removes them. A replica skips them. A dead feed can leave hard
  links of the files (`disks/<rt_id>/<key>/<id>.md2`). These links go BEFORE
  the files of `keys/`. While one of them cannot be removed, the files stay
  too. The next open finds the files, and removes both.
- A base that cannot be read is not a base of 0, because a base of 0 numbers
  the key from 1 again. When the cache of the key is built, the key is flagged
  `unlisted`, and its loads say `load_failed`. An append reads the base again
  first, or is refused. A load reads the base again when the directory or the
  base changed, or when the read failed for a passing cause (EMFILE, ENFILE,
  ENOMEM, EIO, EINTR, EAGAIN). A replica that already holds the cells of the
  key is not flagged. Its cells sum the rows that it counted of the files
  purged. That sum is the base of the master, unless the replica missed a late
  row of them. Thus the numbering of the replica stays that of the master, and
  its new records keep coming. In the meantime, the cells whose files are gone
  go to its base: the live rows are served, and the disk is freed. The replica
  reads the base again, quietly, at every read, until it can read it. If a
  numbering then moves (rows that it had missed, a file that it flagged
  unreadable), a WARNING says it. The records handed before had g_rowids below
  those of the master. A replica can build the cache of a key while the master
  purges it (a md2 listed and gone at its open). Then it reads the base again,
  and numbers the key as the master does.
- The function refuses these cases (-1, logged, nothing deleted):
    - A cut in the future, because it deletes the file that the store writes now.
    - A topic whose `filename_mask` gives ids that do not sort as time.
      `tranger2_mask_sorts_as_time()` accepts numeric fields of a fixed width,
      the year first, each the next field down. A tr_queue topic is `"queue"`,
      and `%d-%m-%Y`, `%Y-%d` or `%Y-%b` sort wrong.
    - A topic of a treedb or a msg2db open on the tranger (its keys are their
      nodes).
    - A key whose cells are not whole (flagged unlisted, or an unreadable file
      in the period).

  After a purge, an append whose `__t__` falls in a purged period is refused.
- A key purged whole (silent for longer than the retention) keeps its
  directory and its base, with 0 rows and a span of 0, and continues its
  numbering. `tranger2_delete_key()` removes every key in a way that no failure
  leaves some of its files without the others, or its files without its base.
  It first moves the directory out of `keys/`, whole (a `rename()`, atomic),
  and then removes it. A delete that fails keeps the key as it was. If a
  directory moved aside cannot be removed, the next open of the master sweeps
  it. Up to 7.27.5, a key without a base was removed in place, file by file,
  and a removal that failed half way renumbered it.
- Followers. The master makes and removes `disks/<rt_id>/.p.<key>`
  (`.q.<sha256>` for a key too long for the name) in every feed directory. A
  follower that hears it reads the base, drops the cells at or before `upto`,
  and closes their descriptors (they hold the disk). It also takes the
  segments of its iterators again. The flags of the files purged go too. A
  follower that listed the key during the purge flags the md2 that it found
  gone, and such a flag failed every load of the key. A follower that did not
  hear a signal detects the purge before its next load or page of the key. A
  purge deletes a prefix of the files, so one `stat()` of the first file (and
  of the first one flagged) tells. The exception is the time while the feeds
  of the key get its new records: a callback that reads the key then does not
  change the cells that they are counted in. A signal can stay in a feed
  directory (a master that died between its `mkdir()` and its `rmdir()`). The
  next purge signals it again, and the next open of the master removes it.
  Master and followers of a topic upgrade together.
- Cost. One small file and two `fsync()` per key purged, plus two or three
  unlinks per file: about 1-2 ms per key on the slowest node measured. The
  owner of a topic of thousands of keys purges a slice of keys per tick, on
  its own timer. timeranger2 has no timer, and the period to keep is in the
  configuration of the owner.

To run it by hand, `C_TRANGER` has the command (master-only, permission `delete`):

```
command-yuno id=<yuno> service=tranger command=delete-key-files topic_name=raw_events key=<device> before=2026-09-21T00:00:00
command-yuno id=<yuno> service=tranger command=delete-key-files topic_name=raw_events before=1790035200
```

Without `key=`, the command walks every key of the topic in one call. Tests:
`tests/c/timeranger2/test_delete_key_files.c` (the master: numbering, bounds,
pages, the guards, the crash cases) and `test_delete_key_files_follower.c`.

### Archive: keeping the old files

`tranger2_archive_key_files(tranger, topic, key, before_t, archive_directory)`
is the same purge, but the files that it takes out of the topic are KEPT. Each
purge of a key hard-links them into a new batch of `archive_directory`.
Another process takes and removes them, for example a yuno that sends them to
a backup machine. [`DESIGN-retention.md`](DESIGN-retention.md) §12 gives the
design and the crash table.

```c
/*
 *  Keep 30 days of `raw_events` in the topic, and what goes out of it in
 *  /yuneta/store/archive (it must exist, on the filesystem of the store)
 */
json_int_t rows = tranger2_archive_key_files(
    tranger, "raw_events", device_id, time_in_seconds() - 30*86400,
    "/yuneta/store/archive"
);
if(rows < 0) {
    // Error already logged: refused, nothing archived nor deleted
}
```

```
/yuneta/store/archive/tr_raw_events/raw_events/dev1/2026-09-20@1792573238/
    2026-09-19.json  2026-09-19.md2  2026-09-20.json  2026-09-20.md2
    topic_desc.json  topic_cols.json
    manifest.json    {"files": ["2026-09-19", "2026-09-20"], "rows": 5712,
                      "first_g_rowid": 1817700, "last_g_rowid": 1823411, ...}
```

- Hard links. The cost does not depend on the size of the files, and the
  `archive_directory` must be on the filesystem of the topic. It must exist
  and be absolute. It must not be inside the directory of the tranger, and it
  must not hold it.
- A batch is whole when its name does not start with `.`. The store builds it
  as `.new-<upto>@<t>` and renames it when it is durable. A crash between the
  batch and the base leaves a batch of files that are still live, and the next
  purge archives them again in another batch. Until then, a late append,
  `tranger2_write_user_flag()` or `tranger2_delete_instance()` changes them in
  the first batch too (hard links), after its manifest. Thus a reader takes a
  file id from the batch with the latest `@<t>`.
- The batch is durable before the base. The open of the master finishes a cut
  purge by deleting, and it never deletes a file that is not archived.
- A ready mark for each batch whose purge happened. The mark is an empty file
  in `<archive_directory>/.ready/`. Its name is the path of the batch, with
  `%` written `%25` and `/` written `%2F`. A reader watches that one
  directory, and scans the tree when it starts (the mark is a hint, not a
  record).
- The archive frees no disk until its reader removes a batch. The SDK puts no
  limit on it. The reader of the SDK is the `archive_sender` yuno. It ships
  the batches to an `archive_keeper` or with rsync. When the backup is not
  there, its `max_archive_bytes` drops the oldest batches
  ([`DESIGN-retention.md`](DESIGN-retention.md) §13).
- Cost: about three times a delete (three or four `fsync()` more per key).

To run it by hand, use `archive-key-files` of `C_TRANGER`, with the parameters
of `delete-key-files`. The directory is its `archive_directory` attribute,
never a parameter. Tests: `tests/c/timeranger2/test_archive_key_files.c`.

### Path-traversal hardening (since 7.6.0)

A string primary key or treedb node `id` becomes a filesystem path component
(`keys/<key>/…`). Since 7.6.0, the store validates the key/id before any
filesystem use. It rejects a value that contains `/` or begins with `.`, so
`..`, absolute paths, and hidden traversal cannot escape the topic directory.
The rejection applies on the create, lookup, and `tranger2_delete_instance`
paths. The mirror predicate is aligned, so followers reject the same inputs.
Regression coverage is in `tests/c/timeranger2/test_pkey_path_traversal.c`.

The topic name got the same confinement later (since 7.25.0). Before, the
store only made sure that the name was not empty. Thus a name such as
`../other_db/users` read, planted or deleted a topic of another database. A
name that is a directory but not a topic (`..`, `<topic>/keys`, any stray
directory) reached `load_persistent_json()` as a critical: an `exit(0)` with
`on_critical_error=2`. Create, open, delete, backup, `tranger2_topic_path()` and
`tranger2_write_topic_var()` / `_cols()` now refuse an empty name, `.`, `..`,
and any name that holds `/` or `` ` ``. A leading `.` stays legal (MQTT queues
are `<client_id>-IN/-OUT`, and the broker accepts `.foo` as a client id).

The rule has a name since 7.26.9, `tranger2_name_is_identifier()`. treedb
applies it to every treedb and topic name, and msg2db applies it to its own
names. A COLUMN name (a column id, a hook, an fkey) is only a key. It follows
the looser `treedb_col_name_is_identifier()` (not empty, no `` ` ``). If a
treedb opens with an old name that breaks a rule, it leaves that topic out
with an error, and opens the rest. Names are looked up by key
(`KWID_GET_BY_KEYS()`), never through a `kwid_get()` path, which splits at
`.` too. Thus a topic `par.v1` or a hook `kids.all` works.

`tranger2_open_topic()` answers `NULL` for a directory without
`topic_desc.json`. `tranger2_delete_topic()` and `tranger2_backup_topic()` are
master-only, like every other destructive call.

The rt id of a disk feed (`<topic>/disks/<id>/`) follows the directory half of
the rule. An id longer than `NAME_MAX` is refused too. An id already in use by
a live feed of the topic is refused, whatever its creator (one id, one
directory, one feed). A peer can send the id, so every refusal of it is a
warning, an empty id included (*"Invalid rt id (empty)"*). The one-feed guard
is per process. Two processes that follow one store with the same id still
take the directory of each other. Regression coverage is in
`tests/c/timeranger2/test_topic_path_traversal.c` and
`test_rt_disk_multi_feed.c`.

### `on_critical_error` is for writes, never for reads

A failed READ of records logs a critical and answers an error. Since 7.25.0,
it never exits the process, whatever `on_critical_error` says. A read wrote
nothing, but the default `2` turned a failed read into an `exit(0)` that
nothing relaunched. A key deleted under an open iterator was enough to cause
it. A read does not create files either: `tranger2_read_user_flag()` on a
`(key, __t__)` with no md2 answers *"Record metadata file not found"*. The
write side still exits on purpose. The main case is a short write of an md2
row, which misaligns every later append. Regression coverage is in
`tests/c/timeranger2/test_read_never_exits.c`.

### A stopped master takes its lock again before it writes

`tranger2_stop()` releases the single-master lock (the `mt_stop` of
C_TRANGER). The next call that opens a topic or writes takes the lock again
FIRST. `tranger2_create_topic()` is the restart path of C_TRANGER and
C_TREEDB. If another process took the store in the meantime (`flock()`
answers `EWOULDBLOCK`), nothing is written. It is the single-master conflict
of the startup: a CRITICAL at `on_critical_error`. With the default of a yuno
(`2`, exit(0)), the process exits and stays down, as a second instance does at
the startup. Any OTHER failure to take the lock says nothing of another
master. Examples are a lock file that cannot be opened (`EMFILE`,
`ENOENT`...), or `flock()` that fails with `ENOLCK`, `EINTR`... Such a failure
is an ERROR, and the process continues. A tranger that did not take its lock
again (and did not exit) continues as a replica. It reads, refuses every
write, and says so in its json:

```C
kw_get_bool(0, tranger, "master", 0, 0);        // FALSE: what it holds now
kw_get_bool(0, tranger, "master_lost", 0, 0);   // TRUE: it was the master, it lost the lock
```

A demoted tranger never takes the lock again, not even after the other
process releases it. Its memory (the caches of its topics, the lists a treedb
opened as master) did not follow what the other master wrote. An append on top
of that memory numbers rows that already exist, so the tranger does not
append. To make it the master again, stop it and start it again
(`tranger2_shutdown()` + `tranger2_startup()`).

This is also true for a transient failure. An `EMFILE`, `EINTR` or `ENOLCK` at
the revive is an ERROR. The process does not exit, and it stays a replica for
good. Every later write is refused, until somebody restarts the yuno. Monitor
the log for *"Master lock NOT retaken after a stop"*. Also monitor the
`master` attribute of the `C_TRANGER` service (`view-attrs gobj=<service>`
without `attribute=`, which reads it through `mt_reading`). If it is false on
a yuno configured as master, the yuno is demoted.

Until 7.25.4, `tranger2_create_topic()` read the `master` of the stop and wrote
the topic directory, `topic_cols.json` and `topic_var.json` into the store of
the other master before it noticed. Regression coverage in
`tests/c/timeranger2/test_lost_lock.c`.

### `topic_var.json` is replaced, never rewritten in place

`topic_var.json` holds the `topic_version` and the `last_rowid_id` counter of
treedb. The store writes it on every create of a rowid-key node. Every write
goes through `topic_var.json.new` and a `rename()`. A process that dies half
way leaves the old file or the new one, never an empty one. The hot path does
not `fsync()`: it is safe against the death of the process, not against a
power cut. A `topic_version` change does `fsync()`. The store removes the
`.new` first and creates it `O_EXCL|O_NOFOLLOW` with the `rpermission` of the
tranger. Thus a leftover gives the file neither its mode nor, as a symlink,
its target. `topic_cols.json` (`tranger2_write_topic_cols()`) is replaced the
same way, and always fsync'ed with its directory. It is written rarely, and a
`topic_version` change writes it before the version, because the version must
not survive a power cut that the cols did not survive. The memory takes the
new cols only when the file did (else the function returns -1).
`tests/c/timeranger2/test_topic_var_replace.c`.

### A history that cannot be read whole says so

A load that meets a row whose metadata or whose content cannot be read stops
there and leaves `"load_failed": true` in the iterator. The rows before it, in
the direction of the load, were handed to the callback (forward: the oldest,
backward: the newest). Up to 7.25.4, a content that was not readable reached
the callback as `NULL`, and the load continued (treedb made a node of it with
id `""`). The same is true after a RESTART. A `.md2` file that the cache of the
topic failed to count when it was built at the open (one that cannot be opened
or read) flags its key (`"unreadable": [file_id, ...]` in its cache, logged
once). Every iterator of the key says `load_failed`, and a load stops where
the first such file is in its direction. Up to 7.25.4, the cache build dropped
such a file and nothing failed. `tranger2_delete_key()` clears the flag with
the key. A cell that counts the file again also clears it: an append into it
that finds it readable, a follower that reads it whole, or a load of the key
once the file changed since it was flagged or opens again. A key whose
DIRECTORY the store failed to list is flagged whole (`"unlisted"`). It is
listed again by an append (master), by the notification of an append (replica,
whose feed then gets the new rows with their rowids in the whole key) and by a
load, once the directory opens. A flag whose cause looks unchanged is not
tried again: a load does not read and log a damaged file again.
`tests/c/timeranger2/test_unreadable_at_open.c`, `test_unlistable_dirs.c`.

A `.md2` of 0 rows gets no cell and flags nothing. If its `.json` is not
empty, it is what an append that was never acknowledged leaves, because the
content is written first and the md2 row after. The store ignores it with a
warning that names the file (*"md2 file of the key with no rows and a content
file that is not empty: an append that was never acknowledged, the file is
ignored"*). 7.25.4 ignored its rows too, without a warning. An append whose md2
fails does not leave it: it answers -1 and cuts its content back. The cut comes
BEFORE the critical that reports the failure. With the exit bit of
`on_critical_error` (`2`, the default of `C_TRANGER` and of the
`exit_on_error` of `C_TREEDB`), that critical ends the process inside the log
call. 7.25.4 did not cut it back at all. A write that stops part way (the file
size limit, a full disk) is logged as a short write, with the bytes written and
expected. A kill or a power cut between the two writes still leaves the shape,
and the next open ignores it with the warning above. An append into a file
still flagged unreadable is refused. An append into a flagged file that is
readable again counts it first, and a FILTERED iterator of the key takes its
segments and its index again (the rowids after the file moved).
`tests/c/timeranger2/test_uncommitted_append.c`.

A `.md2` whose size is not a whole number of 32-byte rows is not damage
either. Its last row is torn: a power cut came during the write of the row,
and the md2 row is the commit point of an append, so that append was never
acknowledged. A MASTER cuts the `.md2` back to its whole rows
(`floor(size / 32) * 32`) when it first reads the file. That first read is the
cache build at the open, the count of a flagged file, or the read of the key
again after a `tranger2_delete_key()` that failed to move the directory of the
key out of `keys/`. The master logs ONE warning, and the key loads whole. The next append goes into the same file. The `.json` is left as
it is: its bytes after the last row belong to no row.

```text
WARNING: {"function": "load_first_and_last_record_md", "msgset": "Tranger",
    "msg": "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back",
    "topic": "alarms", "key": "dev1", "file_id": "2026",
    "path": "<store>/alarms/keys/dev1/2026.md2", "old_size": 109, "new_size": 96}
```

The cut removes fewer than 32 bytes, all after the last whole row. An append
writes its row on a row boundary. If the md2 of a running master ends in a torn
row (a row written in part whose cut back failed), the next append cuts it back
first, with the same warning. A cut that fails refuses that append, and the
next append tries again. A cut that fails at the open is damage (the file is
flagged).

7.25.4 did not cut a torn row back. The next appends wrote their rows after the
torn bytes, on no row boundary, and those rows were acknowledged. A cut of
such a file removes the end of its last acknowledged row. So every cut (open,
flagged count, delete-key read, append) first makes sure that the tail is a
torn row after good rows. A row is good when its content is WHOLE: inside the
`.json`, one json value and one NUL byte after it, as an append writes it (or
only zero bytes, for an instance deleted with its content zeroed). A NUL inside
a string is written as the escape `\u0000`: it is part of the value, not a NUL
byte. If the process that checks a content does not have the memory to parse
it, that does not make the content not whole. `MEM_MAX_BLOCK` sets the largest
memory block (`gbmem_get_maximum_block()`) for each yuno, and the yuno that
wrote the content can have a larger block. A content larger than that block is
read in parts. A NUL before its last byte says that it is not whole, with no
block of its size. When its only NUL is its last byte, the check cannot tell. A
content that the block can hold can still need more to parse (a long string, a
long array or object). When its parse fails for memory, the check cannot tell
either. The tail is not cut on a content that the check cannot
tell. The rules:

1. The last 32 bytes, read as a row, are NOT a row of their own: their content
   does not end exactly at the end of the `.json`, and it is not whole.
2. The last whole row is good, and so is the whole row before it, whose content
   ends at or before the start of the content of the last one.

Rule 1 stops the cut of every file 7.25.4 left whose last row still names its
content as 7.25.4 wrote it, also with content after the last row in the
`.json` (an append killed between its two writes, which 7.25.4 did not cut
back): the last row is a real row, and its content is whole. When the process
that checks does not have the memory to parse that content, the check cannot
tell, and that is enough to not cut. The last 32 bytes of a torn row are bytes moved out of their fields,
and name a whole record to the byte only by a coincidence of 64-bit values: the
file is then flagged, not cut. The content is read only for a `.md2` that is
not a whole number of rows, at most three records.

If a rule fails, the file is not cut and its bytes do not change. A CRITICAL
names the shape and its `cause`. The file is flagged (every load of the key
says `load_failed`), and an append into it is refused. An append that finds
such a tail flags the file in memory the same way (*"md2 file of the key
flagged unreadable at an append: every load of the key says load_failed"*),
before the CRITICAL of its refusal. With the exit bit of `on_critical_error`
(the default), the process ends in that CRITICAL and the next open flags the
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
read: not cut"* (the `.json` cannot be opened, `EMFILE`, `ENFILE`, or read,
logged with its `path`), *"Cannot check the torn tail of a md2 file, no memory to
read a content: not cut"* (with the `path` of the `.json` and the `size`), or
*"Cannot read a record of md2 file, read FAILED"* / *"..., short read"* (a row
of the `.md2` cannot be read, logged with its `path`, the `row` and its
`offset`). At an open, the file is flagged too, because without the check the
whole rows can be the moved rows of the shape that 7.25.4 left. The next
append of a master into the file counts it again, and the check runs then.
At an append it found nothing wrong: the append is refused with -1, its
content is cut back, the file is NOT flagged, and the next append checks again
(*"Cannot append record, the torn tail of its md2 file cannot be checked now:
the append is refused, the file is not flagged"*, with `topic`, `key`,
`file_id`, `md2_size`). With the default `on_critical_error`, the CRITICAL of
the refusal exits the yuno, and the next start checks the file at its open (and
flags it when the check still cannot run). `tests/c/timeranger2/test_torn_tail_check_fails.c`.

A string of a record can hold NUL characters: `json_dumps()` writes each one as
`\u0000`, and a read hands it back. A treedb refuses one in an `id`, a pkey2
value or an fkey reference (7.27.0+, see `docs/doc.yuneta.io/api/timeranger2/treedb.md`). A consumer that reads it with
`kw_get_str()` gets the C string up to the first NUL. A content that is not
json when it is read logs *"Bad data, the content of the record is not json"*,
with the jansson `error` and `position`. `tests/c/timeranger2/test_nul_escape_record.c`.

A content that this process has not the memory to parse logs *"Cannot read
the record, this process has not the memory to parse its content
(MEM_MAX_BLOCK)"*, and the record is not handed. A content smaller than the
largest memory block can need more to parse (jansson doubles the buffer of a
string, and the table of an array or an object), and a yuno with a larger
`MEM_MAX_BLOCK` reads it. Up to 7.25.4, a string longer than about half of the
block crashed the process: jansson wrote past its buffer (fixed by
the lexer fix of the jansson of Yuneta, see `kernel/c/linux-ext-libs/HACKS.md`). `tests/c/timeranger2/test_torn_tail_check_fails.c`.

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

1. The live `keys/<key>/` directory is moved out of `keys/` FIRST, whole
   (a `rename()`, atomic), and then removed. The delete is announced only
   after the key left `keys/`. If the directory cannot be moved, the call
   answers `-1`, announces nothing, and reads the cache of the key again from
   the disk, where the key is as it was. If a moved directory cannot be
   removed, the key is deleted all the same (`0`, logged), and the next open
   of the master sweeps it. Then, in the directory of every feed,
   `topic/disks/<rt_id>/<key>/` is removed if it exists. The SIGNAL
   `disks/<rt_id>/.d<seq>.<key>` is created and removed at once
   (`.h<seq>.<sha256>` for a key too long for the name).

   `seq` is the delete sequence of the topic. It is recorded durable in
   `<topic>/delete_seq.json` BEFORE `keys/<key>/` is removed (when the topic
   has a feed). Thus a follower that lists `keys/` at an overflow and reads
   the record after bounds each missing key at or above its delete. The
   master makes the record (0) when it opens a topic that has none. If the
   master cannot read or write the record, it refuses the delete (`-1`,
   logged) before anything is removed, and leaves the record as it is. A
   sequence taken by a delete that failed to remove its key stays taken, as a
   gap. If the master gave it back, a follower that read it at an overflow
   can take the next delete for one that it was told.

   The master removes a signal that it failed to remove before (it died in
   between, or the `rmdir()` failed) at its next open of the topic. The feed
   hears it if it was the last delete. An older one (deletes followed it) is
   moved out of the directory of the feed first and removed unheard, with a
   warning. A follower that opens a feed on a topic with no record logs that
   it hears no delete (its master is older than 7.26.0, or did not open the
   topic since its upgrade). `rt_by_disk` followers recursive-watching
   `disks/<rt_id>/` hear the signal (`FS_SUBDIR_DELETED_TYPE` of the `.d`
   name). By its sequence, they know whether the delete is new or one that
   another feed heard first. They run the same callback fan-out on their
   side. Since 7.26.0 (up to 7.25.22 the signal was the directory of the key
   itself), master and followers of a topic upgrade together.
2. In-process subscribers whose `key` filter matches receive the
   `key_deleted_callback`. Handles that own an inotify watcher
   (`fs_event_client` set) are skipped here, because the inotify branch
   delivers the same event.

Followers from before 2026-05-26 that polled their cache on a timer can
remove the timer. `tests/c/timeranger2/test_delete_key_propagation.c` has
the regression coverage.

The deletion also drops the key from the watermark of every `rt_disk` feed
(below). A feed that outlives many keys must not carry a mark for each one. A
key created again afterwards must not inherit the mark of the dead key.

It also drops what the open iterators of the key took from its cache. An
unfiltered iterator takes its segments again at its next page. Until 7.25.4, a
key written again with the same rows and files, spread another way, read the
new files with the old segments. A filtered iterator pages an empty index.
`tests/c/timeranger2/test_key_reborn_pages.c`.

### What a realtime feed hands its callback

The rowid that a feed delivers is the global `g_rowid` of the key. That rowid
never resets, not even when the old files of the key are purged. It is not the
position of the row in its `.md2` file: that position is `i_rowid`, and it
restarts at 1 with every new file. Thus a consumer can dedupe or page by the
rowid across a file rotation.

The feed uses the same distinction for its own bookkeeping. Each `rt_disk`
feed keeps a watermark of the last row that it got, per key AND per file. The
reason is that the shared key cache advances with the first wake-up of a
batch. Without the watermark, a feed served by a later wake-up finds "nothing
new". A watermark that forgot which file it counted in became a ceiling over
the next file, and the first record of every new file reached no feed at all.
This was fixed on 2026-07-14. `tests/c/timeranger2/test_rt_disk_multi_feed.c`
covers the rotation, the several-feeds-per-key fan-out and the re-created key.

On a follower, the `rt_mem` lists get the records that the `rt_disk` feeds
read, one batch at a time. A callback can close a feed or open a list in the
middle of a batch. A feed or a list that a callback closes gets no more
records of the batch. A list that a callback opens gets the whole batch, after
the lists that were open: its history load read the cache of the key, and the
cache counts the batch only after the batch. Up to 7.31.29 such a list got
only the records after its open. Each record costs one look at each list.

A feed opened with `only_md` receives the metadata of the record, but not its
body. Its callback gets a `NULL` `jn_record`, exactly like the historical
iterator. The realtime feeds honor `only_md` too (both the master `rt_mem`
fan-out and the follower `rt_disk` path). `rt_disk` skips the per-record disk
read when no audience of the key needs the body. This was fixed on 2026-07-15,
and the `only_md` feeds in `test_rt_disk_multi_feed.c` cover it.

## `file` columns: bytes with an owner

A treedb is held in memory, and timeranger2 rewrites the whole record on every
update. Thus a photo cannot be a field of a node. A column flagged
`['fkey','file']` is an fkey into `__assets__`. `__assets__` is a system topic
that every treedb creates at open. Since 7.27.0, a NEW topic refuses a `file`
column whose id holds a `^`. The index of `__assets__` is in memory, and the
content is on disk, under `<treedb dir>/.blobs/ab/cd/<sha256>.<ext>`. The id of
an asset IS the sha256 of its bytes. Thus the same content stored twice is one
asset, and a served url can be cached for ever.

The hooks of `__assets__` are derived from the `file` columns themselves and
never persisted. Thus a host declares nothing but the column. The write path
takes the bytes BESIDE the record, in a `__files__` manifest: `content64` from
a browser, or `offset`/`size` slices of the one `gbuffer` of the kw from a C
caller. Then the write path does these steps:

- It hashes the bytes again.
- It examines the size before decoding, and the type on the BYTES.
- It rewrites the column into the full fkey reference.
- It links the column itself, with `autolink` or not.

You edit a `file` column when you hand over a file, and the link is part of
the hand-over (`""` unlinks).

`treedb_store_files()`, `treedb_import_files()` and `treedb_gc_files()` are
the API. `import-assets` and `gc-assets` are the commands of `C_NODE` on top
of them. The gc keeps what a LIVE node of any treedb of the tranger links. It
also keeps what an activation of an existing snap LOADS. For each key, that is
the newest instance under the tag of the snap. Since 7.24.0, it is the record
that the snap SHOT. A save is always untagged: only shoot-snap tags a record,
with an active snap or not, and a record is tagged once. Thus a snap freezes
what was live at the shot, and holds it until its `__snaps__` row is deleted.
The gc takes the bytes that no row names, which is what an interrupted write
leaves behind. `delete-node` on an `__assets__` row runs the same guards, and
`force` does not override them.

[`DESIGN-treedb-files.md`](DESIGN-treedb-files.md) gives the full account,
with the defects found after the implementation (§16).

## Filesystem watcher

`src/fs_watcher.c` implements an inotify-based watcher (`fs_event_t`) used by `root-linux/C_FS` and by the stores themselves to react to on-disk changes.

## CLI companions

- `utils/c/tr2list`: list records in a topic
- `utils/c/tr2keys`: list keys in a topic
- `utils/c/tr2check`: examine a topic that a load test filled
- `utils/c/tr2search`: search by filter
- `utils/c/tr2migrate`: migrate from legacy timeranger v1
- `utils/c/list_queue_msgs2`: list a msg2db queue
- `utils/c/msg2db_list`: list a msg2db topic
- `utils/c/treedb_list`: list TreeDB nodes

## Tests

`tests/c/timeranger2`, `tests/c/tr_dt_unknown`, `tests/c/tr_msg`,
`tests/c/tr_msg2db`, `tests/c/tr_queue`, `tests/c/tr_treedb` and the other
`tests/c/tr_treedb_*` directories (`tr_treedb_load_failed`,
`tr_treedb_failed_save`, `tr_treedb_link_events`, `tr_treedb_files`,
`tr_treedb_hook_hygiene`, ...), and, through `C_TREEDB`,
`tests/c/c_treedb_literal_wins`. `tests/c/README.md` lists them all.
