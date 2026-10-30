# Design: retention — deleting the old files of a key, with `g_rowid` intact

Status: **IMPLEMENTED** (2026-10-21, `bd3fdce51` the master, `83a771089` the
followers, `ffee7cd3f` the `C_TRANGER` command), released in 7.27.5. The archive
variant, `tranger2_archive_key_files()` (§12), was added on 2026-10-27, and the
yunos that ship the archive (§13) on 2026-10-28. Approved the
same day; the open questions are decided (§11), and two guards were added to
the proposal (§4.1). Where the implementation differs from the proposal, the
text says so (§4.5, §8). Line numbers are those of `timeranger2.c` at
`ff48d4f75`, before the change.

## 1. The problem

A timeranger2 store grows for ever. Nothing in the SDK can free the old part
of a topic that is in use, and no project does it today: the history stores of
the production yunos (`db_tracks`, `db_history`) keep every record they ever
wrote. A telemetry node that appends a few hundred records per second fills its
disk in days, and a retention period ("keep 30 days of readings") is a common
requirement of such systems.

What exists deletes either too much or too little:

| Primitive | What it frees | Why it is not retention |
|---|---|---|
| `tranger2_backup_topic()` (from timeranger v1) | the whole topic: renamed to `.bak`, created again empty | it drops everything at once, so it cannot keep "the last N days". Queues use it (`trq_check_backup`), and so do the `frames` topics of some gates |
| `tranger2_delete_key()` | one whole key | the key is the entity (a device, a user): it cannot go. `webstats` uses it because it makes one key per day |
| `tranger2_delete_instance()` | nothing: it tombstones one row | no disk is freed |

Retention needs a third granularity: **the old files of a key**.

## 2. Why it cannot be done today: `g_rowid` is a position

The global rowid of a record is not stored anywhere. It is computed as the rows
of the files before its file plus its row in that file
(`update_new_record_from_mem`, l. 12701; the followers, l. 10135). The files of
a key are kept in the order of their names (`cmp_file_ids`, l. 10563).
Removing the first files of a key therefore renumbers every record after them,
as soon as the cache is built again.

The SDK documents the opposite, and it is the right contract:

- *"increments the per-key `g_rowid` (monotonic, never resets)"* (README,
  l. 20);
- *"may therefore dedupe or page by it across a file rotation"* (README,
  l. 481);
- *"cumulative across all files, never reset"* (`YUNO_TREEDB.md` §2.3).

The contract holds today only because nothing deletes the first files of a
key. Retention must keep it.

What would break if `g_rowid` were renumbered:

- **Iterators.** They keep segments in global rowids, and a filtered
  iterator keeps an index of global rowids built at its open.
  `key_cache_stamp` (l. 13875) assumes that *"while the key lives its cells
  only grow"*.
- **Paging by `(key, rowid)`.** `c_tranger`'s multi-key pages promise that an
  append never makes a page skip a row, and a client dedups by that pair.
- **Lists that keep a `g_rowid`** in memory (`trmsg_data_tree` in
  `tr_msg.c`).
- **Followers in another process.** They are never told that a file went
  away: `client_fs_callback` ignores `FS_FILE_DELETED` (l. 8116). Their
  cache keeps the cells, so their rowids differ from the master's, and a read
  of a deleted file flags it unreadable (`load_failed`).

What does not depend on it:

- The rt_disk watermarks: they count inside each file (`i_rowid`, l.
  10033-10076).
- The flag, read and `delete_instance` calls: they take `(key, __t__,
  i_rowid)`.
- Negative rowids: they are resolved against the end, on every call.
- Conditions on `t` and `tm`.
- Queues and the MQTT store: they keep `first_rowid` in `topic_var.json`, but
  their mask is `"queue"`, one file.
- Treedb topics: they are not retention targets.

## 3. Goals and non-goals

Goals:

1. Free the disk of the old part of a key while the topic is open, on a master
   with live followers in other processes.
2. **`g_rowid` keeps its contract**: cumulative, never reused, never moved.
3. **Crash-safe**: a kill or a power cut at any point leaves the key either as
   it was or purged, never renumbered.
4. A store that was never purged is byte for byte what it is today, and is read
   exactly as today.

Non-goals:

- **Retention by `tm`.** Files are cut by `__t__`, and a `tm` condition reads
  every row (README, *Two time axes*). Retention is by the time of the append.
- **Policy.** How long each topic keeps, and when the purge runs, belong to
  the owner of the store (§7).
- **Compaction or downsampling.** That is the application's job: it writes its
  aggregates to another topic.
- **Queues.** They rotate whole, by `trq_check_backup`.

## 4. The design

### 4.1 What is deleted: whole files, chosen by their name

`tranger2_delete_key_files(tranger, topic, key, before_t)` deletes every file
of the key whose id sorts **before** the id of the file that `before_t` would
go to, `get_file_id(before_t)` (l. 3301). The order is `cmp_file_ids()`, the
same order that numbers the rows.

- **No row is read.** The file id says it all. The `to_t` of a cache cell
  would not: it comes from the first and last rows, and a file with a late
  record (`.unordered`) holds a `t` past its last row.
- **It is exact for late records.** A record goes to the file of its own
  `__t__`, so every record of a file before the cut has a `t` before the
  period of `before_t`.
- **The granularity is the file.** At least the period asked for is kept, and
  at most one file more. With `%Y-%m-%d`, "30 days" keeps 30 to 31 days.
- **It needs a mask whose ids sort as time.** The masks in use do:
  `%Y-%m-%d`, `%Y-%m`, `%Y`, and a fixed prefix in front of any of them. A
  mask like `%d-%m-%Y` sorts wrong already, and so do its rowids.

Two guards refuse a cut that would do damage. Each one returns -1 with an
error in the log, and deletes nothing:

- **A cut in the future is refused.** If the id of the cut sorts after
  `get_file_id(now)`, the cut is refused. Without this guard, a `before_t`
  later than now deletes the file that is being written. `upto` then names
  the current period, and §4.7 refuses every append with `t = now` until the
  period changes. Thus the file of the current period is never deleted.
- **A mask whose file ids do not sort as time is refused**
  (`tranger2_mask_sorts_as_time()`, since `8d404db485`). The ids are compared
  as strings, so every field must be numeric and of a fixed width, the year
  FIRST, and each field the next one down from the last. This refuses the
  `"queue"` mask of a tr_queue topic, a cyclic mask such as `%a`, `%s` (the
  epoch has no fixed width), a `%G` that ends the mask, `%Y-%d` (a field left
  out), `%d-%m-%Y`, `%Y-%b`, and the `%E`/`%O` modifiers. (The proposal only
  asked for an absolute year: `%Y`, `%G` or `%s`.) A call on such a topic is a
  mistake of the caller, so it is an error in the log, not a silent 0.

### 4.2 The base of a key: `keys/<key>/.purged`

The rows that a key has lost are recorded in a file of the key:

```json
{"upto": "tracks-2026-09-20", "rows": 1823411, "t": 1792573238}
```

- `upto` is the last file id deleted. A file whose id sorts at or before it
  is gone, or about to be.
- `rows` is how many rows those files held, as the cache counted them.
- `t` is when the last purge happened. It is informational.

In the cache, the key's totals get **`purged`**, the base. **`total.rows`
stays the live count**: the rows that can be read, which is what
`tranger2_topic_key_size()`, `tranger2_topic_size()`, `list-keys` and the
page counts already mean by it. The last `g_rowid` of the key is `purged +
rows`, and the live rowids are `purged+1 .. purged+rows`.

The name starts with `.`. No file id can collide with it (a key cannot start
with `.` either), the listing of a replica takes only `*.md2` (l. 11350), and
the master's listing skips anything that is not `.md2` (l. 11365).
`tranger2_delete_key()` removes it with the key, so a key that is written
again starts from 1, as it does today.

A key with no `.purged` has base 0: every store that exists today.

### 4.3 The operation, step by step (master only)

1. **Choose.** Apply the guards of §4.1. Walk the cells from the first while
   their id sorts before the cut. If there are none, return 0. Add up their
   rows.
2. **Decide.** Write the new base (`upto` = the last chosen id, `rows` = old
   base + the sum) to `.purged.new`, `fsync()` it, `rename()` it to
   `.purged`, and `fsync()` the directory. This is the same replacement as
   `topic_var.json` (README, *`topic_var.json` is replaced*). The barrier is
   per key (§6, §11). **From here the purge has happened.**
3. **Delete.** Close the open fds of those files (`close_fd_opened_files`
   does the key). For each chosen file, remove the hard links
   `disks/<rt_id>/<key>/<id>.md2` that a feed has not consumed (a dead
   follower leaves them, and they would keep the inode alive), THEN
   `<id>.json`, `<id>.md2` and any `<id>.unordered`. The open of the master
   finishes a removal cut half way from what it finds in `keys/`, so while a
   link cannot be removed the files of `keys/` stay too (as implemented
   after 7.27.6; the files went first, and a link left was never retried).
4. **Cache.** Drop the cells, set `total.purged`, recompute the totals,
   `retake_segments_of_key()` (l. 13926, the path that unreadable files
   already use), and drop the rt_disk watermarks of the deleted files.
5. **Tell the followers** (§4.5).

The function returns the rows it deleted (0 when there was nothing to do), or
-1, logged.

**Crash safety.** Nothing is deleted before step 2 is durable.

- A crash before the `rename()` leaves the key as it was. A left-over
  `.purged.new` is removed at the next open.
- A crash after it leaves a base that already counts files still on disk. The
  open finishes the job (§4.4).

The only state that could renumber the key — files gone, base not counting
them — needs an unlink to reach the disk before the rename, and the fsync
barrier is what forbids it.

### 4.4 The open reads the base

`load_key_cache_from_disk()` (l. 11320) reads `.purged` first.

- **Every md2 whose id sorts at or before `upto` is not counted.** A master
  removes it with its content, finishing a purge that a crash cut. A replica
  skips it: it never writes.
- **The base becomes `total.purged`.** `update_totals_of_key_cache()`
  (l. 12765) counts from it.
- **On a replica that already holds the cells of the key** (as implemented,
  after the reviews of 2026-10-21), a base it cannot read does NOT flag the
  key: the cells sum the rows it counted of the files purged, which is the
  master's base unless it missed a late row of them, so the numbering stays
  the master's. It keeps receiving the new records, drops the cells whose
  files are gone (as the purge would), and reads the base again at every
  read; a numbering that moves when it can (the rows it missed) is said with
  a WARNING. Flagging it lost the records whose links were consumed while
  flagged. What follows is the case of a cache being BUILT.
- **A `.purged` that cannot be read is damage, not a base of 0.** A base of 0
  would renumber the key in silence. The key is flagged **`"unlisted"`**
  (`flag_key_unlisted`, l. 10893), with its own error message, which names
  the `.purged`. The flag already does what is necessary: it marks the whole
  key, every load of the key says `load_failed`, and the key is listed again
  before each load (`retry_key_flags`) and before each append
  (`relist_key_before_append`, l. 11241). The relist reads `.purged` again,
  so a cause that went away (EIO, EMFILE) clears the flag, and a cause that
  stays makes the append fail. A load lists the key again when the directory
  or the `.purged` changed, or when the read failed with a passing errno
  (EMFILE, ENFILE, ENOMEM, EIO, EINTR, EAGAIN), which the flag keeps: until
  7.27.6 only a change was a retry, and a passing error kept the key flagged.

### 4.5 Followers: one signal, no sequence

A purge is **idempotent and convergent**. The state of a purged key is
entirely its `.purged`, and applying it means: drop every cell at or before
`upto`, and take `rows` as the base. Applied twice, or late, it gives the same
result. So unlike a key delete, it needs no delete sequence: no ordering, no
record of what was heard, no overflow bookkeeping.

- **The signal.** After step 4 the master creates and removes
  `disks/<rt_id>/.p.<key>` in every feed directory, as `mirror_key_delete_to_disks()`
  does with its delete signal. A key too long for the name uses the same
  `.h`-style hash form as the delete signal.
- **The follower** hears `FS_SUBDIR_DELETED_TYPE` of a `.p.` name, reads
  `.purged` of the key, drops its cells, sets the base, and retakes the
  segments of its iterators. Without that it would serve stale cells until it
  restarts.
- **The safety net** covers a lost signal: an overflow, a follower with no
  feed, or one that was down. **As implemented**, it runs BEFORE every load
  and page of a key on a replica (`check_purge_before_read()`), not on the
  `ENOENT` inside the read as proposed: applied in the middle of a scan, the
  purge would retake, and free, the segments the scan walks. A purge deletes
  a prefix of the files of a key, so one `stat()` of the md2 of the first
  cell tells whether one happened; when it is gone, the follower reads
  `.purged` and applies it, and the read goes on with the right cells.
- **The numbering of a follower is right even before it hears.** The base is
  exactly the sum of the rows of the cells the follower still holds, so its
  `file_base` for a new record is the master's.
- **A follower from before this change** ignores a dot name it cannot parse
  (*"no signal of ours"*, l. 7907). It works through the safety net only if it
  has it. Without it, it flags the deleted files unreadable. **Master and
  followers of a topic upgrade together**, as with the delete sequence of
  7.26.0.

### 4.6 Rowids in the readers

Every place that takes `1` as the first rowid takes `purged + 1`. Every place
that takes `total.rows` as the last rowid takes `purged + rows`.

| Where | Today | With a base |
|---|---|---|
| `find_cell_in_key_cache` (l. 10621): fast path | `file_base = total.rows - last.rows` | adds `purged` |
| `find_cell_in_key_cache`: walk from the first cell | `file_base = 0` | starts at `purged` |
| `get_segments` (l. 14064): `from_rowid` 0, or below the first | `1` | `purged + 1` |
| `get_segments`: negative bounds | `total + n + 1` | `purged + rows + n + 1`, clamped to `purged + 1` |
| `get_segments`: positive bound past the end | `> total` | `> purged + rows` |
| `get_segments`: forward walk (l. 14363) | `partial_rows2 = 1` | `purged + 1` |
| `get_segments`: backward walk (l. 14395) | `partial_rows2 = total` | `purged + rows` |
| `tranger2_match_metadata` (l. 14461) | the same resolution | the same change |
| `first_segment_row` (l. 14703) | the same resolution, twice | the same change |
| `tranger2_iterator_get_page`, unfiltered (l. 13727) | `from_rowid` is a global rowid in `1..total` | a **position**, read as `g_rowid = base + position` with the base the key had **at the open of the iterator** (as implemented, after the review of 2026-10-21: with the base of now, a purge between two pages moved every position and a client paging forward skipped live rows). The positions of the rows purged since the open come back empty. A filtered iterator keeps its index across a purge (g_rowids do not move) and skips the rows purged |
| `key_cache_stamp` (l. 13875) | rows, files, last file | unchanged (as implemented): a purge always changes the files, and calls the retake itself. Adding `purged` changed the stamp of every key never purged |
| `update_totals_of_key_cache` (l. 12765) | sums the cells | sums the cells. `purged` is kept apart |

The four resolutions marked `WARNING adjust REPEATED` (l. 14134, 14477, 14712,
14764) are the same code repeated. The change is the moment to make them
**one helper** that resolves `(from_rowid, to_rowid)` against
`(purged, rows)`, so they cannot drift apart again.

The pages keep their meaning for every caller. `c_tranger` counts positions
from 1 with `tranger2_topic_key_size()` (`c_tranger.c:1580`, 1747), and a key
never purged has `position == g_rowid`. Every record still carries its real
`g_rowid` in `__md_tranger__`, so the dedup by `(key, rowid)` holds.

`tranger2_topic_key_range()` adds `purged` to the copy of the totals it
returns. `rows` keeps meaning the live count.

### 4.7 Appends into a purged period are refused

An append whose `__t__` maps to a file id at or before `upto` would write a
file that the next open treats as purged and removes. Two choices:

- refuse it with -1 and an error (*"the record falls in a purged period"*);
- or accept it, which means lowering `upto` and moving the base back. That
  breaks the base, and so the contract.

**Refused.** Such a record would also be older than the retention, and the
next purge would take it. Appends with `__t__ = 0` (now), the common case,
never meet it.

### 4.8 A key purged whole

A key that stopped writing for longer than the retention loses every file. It
keeps its directory and its `.purged`: rows 0, base N. `list-keys` shows it
with 0 records, and a new append continues at `N + 1`. Removing such a key is
the owner's decision, with `tranger2_delete_key()`.

## 5. API

```c
/*
 *  Delete the files of a key whose records were all appended before the
 *  period of `before_t` (the topic's unit of t: ms with sf_t_ms).
 *  The g_rowid of every remaining record is kept. Master only.
 *  Return the rows deleted (0: nothing to do), -1 on error (logged).
 */
PUBLIC json_int_t tranger2_delete_key_files(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t before_t
);
```

The name follows `tranger2_delete_key()` and `tranger2_delete_instance()`:
it is the third delete granularity, and the README table gets its row.

`tranger2_topic_key_range()` gains `purged`. Nothing else changes signature.

`C_TRANGER` gains the operator's command, the low-level one beside
`delete-key`:

```
delete-key-files topic_name=<t> key=<k> before=<t|ISO date>
```

Master-only, under the same permission as `delete-key`. Without `key=` it
walks every key of the topic, for a one-shot by hand, and it says that it
blocks the yuno for as long as it takes.

## 6. Cost

- **The append path** gains one integer read (`purged`) in the fast path of
  `find_cell_in_key_cache`. A store with no base reads 0. It is a hot path,
  so the release carries an A/B against the last tag (rule of 2026-09-23).
- **A purge** is one `.purged` write with two `fsync()` calls, plus two or
  three unlinks per file. Once a day on a topic of a few thousand keys with
  daily files, that is a few thousand fsyncs. They are cheap one by one and
  not cheap together, on a yuno with one thread. So the owner purges **a slice
  of keys per tick** (§7).

Measured on 2026-10-21: a purge of about 3,000 keys (an application's store), one
`.purged` and three unlinks per key (small files):

| Node | No barrier | `fsync` per key | Batch of 100 (`syncfs`) |
|---|---|---|---|
| node A (Rocky 9, slow VM) | 1.0 s | **2.8 s** (0.9 ms/key) | 1.1 s |
| node B (Debian 13, VM) | 0.5 s | **5.6 s** (1.8 ms/key) | 0.7 s |
| dev machine (NVMe) | 0.2 s | 3.9 s | 0.4 s |

**The barrier is per key.** A slice of 100 keys blocks the yuno for at most
about 180 ms on the slowest node, and the whole census is purged in about 30
ticks, once a day. A batch is 3 to 8 times faster, but `syncfs()` flushes
every dirty page of the filesystem, not only the purge's. On a node that
appends hundreds of records per second, its time depends on what other
processes write, so it has no bound. If a batch is ever necessary, it needs
two barriers: write the `.new` files, `syncfs()`, rename them, `syncfs()`,
then unlink. With one barrier only, a crash can leave a `.purged` that is
empty (§4.4: the key is flagged, not renumbered, but it is damaged).

## 7. Policy belongs to the owner of the store

timeranger2 has no timer, and its owners already run their own housekeeping:
`webstats` purges past `keep_days` in its timer (`c_webstats.c:2204`), and the
gates check `backup_queue_size` in theirs. Retention follows them:

- The owner keeps one attribute per topic, the period to keep, in its config.
  A sensible default is 0: keep everything.
- On its timer it computes `before_t = now - period` and calls
  `tranger2_delete_key_files()` on a slice of keys, carrying the cursor of
  `tranger2_list_keys()` from tick to tick.

A generic helper in `C_TRANGER` (an attribute per topic, a timer, the slices)
is possible later, once two owners have written the same loop.

## 8. What changes, by layer

| Layer | Change |
|---|---|
| `timeranger2.c` | `tranger2_delete_key_files()`; `.purged` in `load_key_cache_from_disk()`; `purged` in the totals; the rowid resolutions (§4.6) in one helper; the append refusal (§4.7); the `.p.` signal and the follower's handler; the `ENOENT` safety net |
| `timeranger2.h` | the declaration and its contract |
| `c_tranger.c` | `delete-key-files` |
| `tr2keys`, `tr2list` | nothing (as implemented): `tr2keys` prints only the names of the keys, `tr2list` the records with their `g_rowid`, already right. `list-keys` of `C_TRANGER` gives `purged` |
| Docs | README (the third row of *Two delete granularities*, the base, the followers); `YUNO_TREEDB.md` §2.3 (the contract does not change; the range of rowids does); doc.yuneta.io API pages; CHANGELOG |

## 9. Tests (`tests/c/timeranger2/`)

1. **Purge and numbering.** Three day files, purge the first. The remaining
   records keep their `g_rowid`. After a close and an open they still do. The
   next append gets `last + 1`.
2. **Crash between the base and the unlinks.** Write `.purged` by hand over
   files still on disk. The master's open removes them and counts nothing
   twice. A replica's open skips them.
3. **`.purged.new` left over.** It is ignored and removed, and the key is as it
   was.
4. **A `.purged` that cannot be read.** It is not a base of 0: the key is
   flagged, and appends to it are refused.
5. **Iterators across a purge.** Open an unfiltered and a filtered iterator,
   purge, and page. No rowid moves, and no page reads a deleted file.
   Negative bounds still give the last N.
6. **Rowid bounds.** `from_rowid` 0, 1, `purged`, `purged + 1`, `-1`,
   `-(rows + 5)` and `to_rowid` past the end, before and after a purge.
7. **Pages.** `get_page` positions `1..rows` give `g_rowid = purged + position`.
   The multi-key pages of `c_tranger` neither skip nor repeat rows across a
   purge.
8. **Follower in another process.** It hears `.p.`, drops the cells, and keeps
   reading. With the signal lost (follower stopped during the purge), the
   `ENOENT` path applies it and flags nothing.
9. **rt_disk.** Watermarks of deleted files are dropped. A record appended
   after the purge reaches the feed with the master's `g_rowid`.
10. **Hard links left by a dead feed** are removed with the files.
11. **Late append into a purged period** is refused, and nothing is written.
12. **Key purged whole**: rows 0, its base kept, the next append continues.
13. **The guards of §4.1.** A cut in the future is refused, and the current
    file stays. The mask `"queue"` (a tr_queue topic) and a mask with no year
    are refused. In each case the function returns -1, logs an error, and
    deletes nothing.
14. **`delete_key` after a purge** removes `.purged`, and a key written again
    starts at 1.

## 10. Compatibility

- **A store never purged** has no `.purged`. It is read as today, with the
  same rowids and pages.
- **Upgrade.** Master and followers of a topic upgrade together (§4.5).
- **Downgrade** after a purge: an older SDK ignores `.purged` and numbers the
  key from 1. Nothing crashes, but every `g_rowid` of a purged key moves. The
  CHANGELOG says so.
- **Copying a store** with `cp -a` or rsync carries `.purged`, so its
  numbering travels with it.

## 11. Decisions (2026-10-21)

These were the open questions of the proposal.

1. **`fsync` per key, not a barrier per batch** (§6). Measured on both
   VMs: at most 1.8 ms per key, in slices of about 100 keys per
   tick. The two-barrier batch stays available as an optimization.
2. **An unreadable `.purged` flags the key `unlisted`** (§4.4), with its own
   message. The existing retry paths read the `.purged` again.
3. **An overflow does not read `.purged` of every cached key.** The lazy
   safety net (§4.5) is sufficient for correctness.
4. **No generic retention helper in `C_TRANGER` yet** (§7). It comes after two
   owners have written the same loop.
5. **Two guards were added** (§4.1): a cut in the future is refused, and so
   is a mask whose file ids do not sort as time
   (`tranger2_mask_sorts_as_time()`; first a mask with no absolute year).

## 12. Archive: keeping the old files instead of losing them

Added 2026-10-27, after 7.28.11. A retention period frees the disk, and some
installations must still keep what it takes: send it to a backup machine,
compress it, load it into something else. `tranger2_archive_key_files()` is
the purge of `tranger2_delete_key_files()` -- the same cut, the same guards,
the same base, the same answer -- and the files it takes out of the topic are
**kept in a batch of an archive directory** instead of lost. What is done
with a batch is the job of another process (another yuno), never of
timeranger2: it has no timer and no transport, and the policy is not its
own (§7).

```c
PUBLIC json_int_t tranger2_archive_key_files(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t before_t,
    const char *archive_directory
);
```

A separate function, not a parameter of the old one: the intention is in the
name, and no caller of `tranger2_delete_key_files()` changes.

### 12.1 The batch

Each purge of a key that takes at least one file makes one batch:

```
<archive_directory>/<database>/<topic>/<key>/<upto>@<t>/
    <file_id>.json  <file_id>.md2  [<file_id>.unordered] [<file_id>.tm_unordered]
    topic_desc.json  topic_cols.json
    manifest.json
```

```json
{
    "database": "tr_raw_tracks",
    "topic": "raw_tracks",
    "key": "dev1",
    "files": ["2026-09-19", "2026-09-20"],
    "rows": 5712,
    "first_g_rowid": 1817700,
    "last_g_rowid": 1823411,
    "upto": "2026-09-20",
    "t": 1792573238
}
```

- **Hard links, not copies.** The purge's `unlink()` of the topic's file then
  takes one name of the inode, and the data stays where the batch names it.
  The cost does not depend on the size of the files, and nothing is read or
  written but the manifest.
- **A batch describes itself.** `topic_desc.json` and `topic_cols.json` are
  linked as they are at the purge (both are replaced whole by a `rename()`,
  so the link is a snapshot of the schema the rows were written with), and
  `first_g_rowid` gives the numbering: row N of the k-th file of `files` is
  `first_g_rowid + (the rows of the files before it) + N - 1`. A reader needs
  nothing of the store.
- **`@<t>`** (the time of the purge, the `t` of the base) keeps two batches
  of the same `upto` apart: after a crash between the batch and the base the
  same files are archived again (§12.3).
- **A name that starts with `.` is not a batch.** The batch is built as
  `.new-<upto>@<t>` and renamed when it is whole and durable. A reader takes
  the names that do not start with `.`, as timeranger2 itself does with its
  signals.
- **The ready mark.** When the base is written (the purge happened), an empty
  file is created in `<archive_directory>/.ready/`. Its name is the path of
  the batch under the archive, `<database>/<topic>/<key>/<upto>@<t>`, with `%`
  written `%25` and `/` written `%2F`:

  ```
  <archive_directory>/.ready/tr_raw_tracks%2Fraw_tracks%2Fdev1%2F2026-09-20@1792573238
  ```

  A reader watches that ONE directory. Watching the tree would not do: a
  `rename()` gives no fs event (`fs_watcher` reports creations and deletions),
  and one watch per key of the archive does not scale. The mark is a hint,
  not a record: it is not fsync'ed, and it is written after the base, so a
  mark never names a batch whose purge did not happen. A batch without a mark
  (a crash between the base and the mark, a mark that could not be written:
  ERROR, the purge goes on) is found by the scan of the tree a reader makes
  when it starts. The reader removes the mark with the batch.

The archive is a spool, not a store: timeranger2 never reads it again, and a
batch put back into `keys/` would be removed by the next open of the master
(its files are at or before `upto`).

### 12.2 The order

1. **Choose** the files (§4.3, unchanged).
2. **Archive.** Make the levels `<database>/<topic>/<key>` that are missing
   (each one created is fsync'ed into its parent), remove a `.new-*` left by
   a crash, link the files into `.new-<upto>@<t>`, write `manifest.json`
   durable (an `fsync()` of the file and then of the batch directory, which
   covers the links), `rename()` the batch to its name, and `fsync()` the
   directory of the key.
3. **Decide**: the base (§4.3, unchanged). If it cannot be written the batch
   is removed -- its files are still live -- and the call returns -1 with
   nothing deleted.
4. **Mark** the batch ready (§12.1).
5. **Cache, delete, tell the followers** (§4.3, unchanged).

The batch is durable **before** the base, and that is the whole point of the
order: the open of the master finishes a purge cut by a crash by DELETING
what is at or before `upto` (§4.4), and it knows nothing of the archive. With
the batch first, every file that the open can delete is already archived.
The other order (base, then batch) would have needed the archive directory in
the topic's configuration, read by the open, to archive instead of deleting
there.

### 12.3 Crash safety

| A crash during… | Leaves | Then |
|---|---|---|
| step 2, before the `rename()` | a `.new-*` in the directory of the key; the key as it was | the next archive of the key removes it (INFO *"Batch of an archive that was not finished removed: its purge did not happen"*) |
| between steps 2 and 3 | a whole batch of files that are still live in the topic | the next purge archives them again, in another batch (another `@<t>`). Until then the batch holds hard links to live files: a late append, `tranger2_write_user_flag()` or `tranger2_delete_instance()` changes them in the batch too, after its `manifest.json`. Two batches of one file id can differ, and the older one may not match its manifest: a reader takes a file id from the batch with the latest `@<t>` |
| after step 3 | as in §4.3; the files are already in the batch | the open of the master removes them from the topic |

Inside the process there is no window at all: the purge is one synchronous
call on a single thread, so no append can come between the links and the
unlinks.

### 12.4 Guards

Each returns -1, logged, with nothing deleted. They are checked after the
guards of §4.1 and before anything is chosen:

- `archive_directory` empty, or not an absolute path;
- a directory that does not exist (it is configuration: the SDK creates the
  levels under it, never the directory itself), or that is not a directory;
- on another filesystem than the topic (`st_dev`): `link()` would fail with
  `EXDEV`, and a copy has no bounded time on a yuno with one thread;
- inside the directory of the tranger, or holding it: the batches would land
  in the store (with `archive_directory` = the `path` of the tranger, a batch
  would go into the directory of the topic).

A batch that already exists with the name of this one (a crash and a purge
in the same second) is refused too.

### 12.5 What does not change

The numbering, the base, the followers (to them an archive is a delete), the
feeds and their hard links (removed as in §4.3), the append path, and
`tranger2_delete_key_files()`.

### 12.6 The disk

**An archive frees no disk until its reader removes the batch.** The SDK puts
no limit on it. If the reader is down, the archive grows by what retention
takes, which is what retention was there to stop. The policy is the reader's:
`archive_sender` (§13) has `max_archive_bytes`, above which it removes the
oldest batches without shipping them, each one with a WARNING. That keeps the
node alive and loses the oldest part of the backup, not the newest.

### 12.7 Cost

About four `fsync()` more per key than a delete: the manifest, the batch
directory, the directory of the key after the `rename()`, and, the first time
a key is archived, its directory in the topic level. Measured on 2026-10-27
on the dev machine (NVMe), 3,000 keys, one file purged per key, three rounds:
**delete 11.8-12.3 ms per key, archive 35.5-37.2 ms per key: three times
more**. The absolute figures of that machine were about nine times worse that
day than on 2026-10-21 (§6: 3.9 s for about 3,000 keys), so the ratio is the figure
to keep; on the slowest node of §6 (1.8 ms per key for a delete) an archive
would cost about 5.4 ms per key. An owner that archives does a slice about a
third of the one it deletes in (§7): 30 to 40 keys per tick instead of 100.

### 12.8 `C_TRANGER`

`archive-key-files` takes the parameters of `delete-key-files`, under the same
permission (`delete`), master-only. The directory is the `archive_directory`
**attribute** of the `C_TRANGER` (configuration), never a parameter: a peer
that chose it could have the files of the store linked wherever the yuno can
write, a directory that a web server serves among them. With the attribute
empty the command is refused.

```
command-yuno id=<yuno> service=tranger command=archive-key-files topic_name=raw_tracks key=dev1 before=2026-09-21T00:00:00
```

### 12.9 Tests

`tests/c/timeranger2/test_archive_key_files.c`: a batch with the files, the
schema and the manifest, the topic numbered as after a delete; a second batch
that starts after the first; every guard of §12.4 (another filesystem when the
machine has one at `/dev/shm`); a base that cannot be written removes the
batch and deletes nothing; a `.new-*` left is removed; a batch name already
taken is refused; a replica and a reopened master read the numbering of the
base; the ready mark of each batch, and none for a purge that did not
happen. `tests/c/c_tranger/test_c_tranger.c`: the command refused with no
attribute, and a batch with one.

## 13. Shipping the archive: archive_sender and archive_keeper

Added 2026-10-28, after 7.28.11. Two SDK yunos read the archive of §12 and
take it to a backup machine. Their design is in their READMEs
(`yunos/c/archive_sender/README.md`, `yunos/c/archive_keeper/README.md`). In
short:

- `archive_sender` runs on the node, one per archive directory. It watches
  `.ready/` (C_FS), scans the tree when it plays, and ships the batches the
  oldest first, one at a time. It removes a batch, with its mark, only when
  the backup has all of it.
- Two transports. `native`: frames over `C_PROT_TCP4H` (TLS) to an
  `archive_keeper` on the backup machine, which writes the batch aside and
  renames it when every file is whole, has the sha256 the sender computed
  while reading it, and is fsync'ed. The link is mutual TLS: the keeper
  takes a batch only when its node is the name of the client certificate of
  the sender. `rsync`: two runs per
  batch, the files and then the manifest, so a batch at the destination is
  whole when its `manifest.json` is there. The node opens the connection in
  both cases: nothing connects to it.
- `max_archive_bytes` is the policy of §12.6.
- Test: `tests/c/archive_shipping/`, both transports in one process.
