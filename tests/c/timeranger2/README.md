# timeranger2 test

Low-level tests for the `timeranger2` store: topic create/open/close, record append, iterator (forward / reverse, by time range, by rowid), and on-disk md2 file consistency.

## Run

```bash
ctest -R '^timeranger2/' --output-on-failure --test-dir build
```

One test:

```bash
ctest -R '^timeranger2/test_uncommitted_append$' --output-on-failure --test-dir build
```

## Tests of 7.25.4 and earlier

| Test | What it checks |
|---|---|
| `test_tranger_startup` | `tranger2_startup()` of a new store, a topic created in it, and `tranger2_shutdown()`, with the files the store and the topic must have on disk. |
| `test_topic_pkey_integer` | A topic with an integer primary key: open as master, check the main files, open rt lists and append records; then change the topic version and its columns. Its appends/s are the timing figure followed from release to release. |
| `test_topic_pkey_integer_iterator` | An iterator with no callback over the store `test_topic_pkey_integer` leaves. |
| `test_topic_pkey_integer_iterator2` | An iterator for each of two keys, with an empty `match_cond` (all records) and a callback; an iterator that matches by `tm`. |
| `test_topic_pkey_integer_iterator3` | ABSOLUTE searches of an iterator. |
| `test_topic_pkey_integer_iterator4` | RELATIVE searches of an iterator. |
| `test_topic_pkey_integer_iterator5` | Paged searches of an iterator. |
| `test_topic_pkey_integer_iterator6` | An iterator by memory (master) or by disk (non-master) while records are appended in realtime. |
| `test_delete_instance` | `tranger2_delete_instance()`: the history and paged iterators skip a dead row (a filtered iterator counts only the live rows, also after a cold reload), and the zero wipe of the content when it is asked for. |
| `test_delete_key_propagation` | `tranger2_delete_key()` and who hears it: rt_mem feeds of the key, of every key and of another key; an rt_disk feed in the same process; a real follower (once per feed). In this release also: the iterators of a deleted key drop their segments, the delete is announced only once its directory is gone, and after a directory that cannot be removed (case `rmrdir_fails_filtered`, the key directory of mode 0550, SKIPPED as root) a FILTERED paging iterator of the key keeps its 3 rows. A key directory whose `stat()` fails with `EIO` (case `key_dir_unstatable`) is not "not found": the delete answers `-1` and nothing is deleted, dropped or announced (up to this fix: `0`, the key dropped from the cache and announced, its files left on disk). A `disks/` whose `opendir()` or `readdir()` fails (case `mirror_fails`) is logged; the delete is done. The test links with `--wrap=stat,opendir,readdir`. |
| `test_rt_disk_multi_feed` | Several rt_disk feeds alive on the same key of a follower (a per-key card and a whole-topic card): each wake-up serves only the feed whose directory fired. |
| `test_out_of_order_append` | An append whose `__t__` belongs to an EARLIER file of the key goes into the cell of that file, and every record is served once. |
| `test_pkey_path_traversal` | A string primary key is one `keys/<key>/` directory component: a key with `/` or a leading `.` is refused, and no directory is made outside `keys/`. |
| `test_topic_path_traversal` | A topic name is one directory component of the store: an empty name, `.`, `..`, or a name with `/` or `` ` `` is refused by every topic call, and a replica cannot delete or back up a topic. |
| `test_read_never_exits` | A failed READ never makes timeranger2 leave the process, whatever `on_critical_error` says: it logs, answers an error, and the process goes on. |
| `test_iterator_index` | The id index of a topic's iterators (`iterators_by_id`): every open iterator is found by `(id, creator)`, a closed one is not, and a duplicate is refused. |
| `test_late_record` | A LATE record, whose `__t__` is below the times already in its md2 file: the file's time range is read from all its rows, so a time-range query serves it. |
| `test_stop_reopen` | A tranger stopped with `tranger2_stop()` and used again: the stop never closes the same fd numbers twice, and a topic opened again revives the tranger. |
| `test_append_md_contract` | What `tranger2_append_record()` does to the record it is handed: a `__md_tranger__` it carries in is dropped before the content is written, and the new metadata is returned in the out-param. |
| `test_str2system_flag` | `tranger2_str2system_flag()`: every name maps to its own bit of `system_flag2_t`. |
| `test_testing` | The helpers of gobj-c `testing.c` that the other tests use (`test_json()`). |

## Tests added after 7.25.4

| Test | What it checks |
|---|---|
| `test_tm_order` | A `__tm__` that does not grow with `__t__`: a key's segments with holes, a file's tm range taken from all its rows, and the tm order markers. |
| `test_lost_lock` | A master that lost its single-master lock while stopped writes nothing: every write path takes the lock again first, and the tranger goes on as a replica (`master_lost`). |
| `test_topic_var_replace` | `topic_var.json` is written to `topic_var.json.new` and renamed: the file is the old one or the new one, never a truncated one. |
| `test_key_reborn_pages` | An iterator open while its key is deleted and written again: the delete forgets the segments (and the index) of every iterator of that key. |
| `test_open_list_history` | `tranger2_open_list()` of every key: a key whose history does not load is named in `load_failed_keys`, and the list goes on with the next key. |
| `test_unreadable_at_open` | A store damaged before the tranger starts: the key is flagged at the cache build, and every load of it says `load_failed`. The damage is a md2 of mode 000 (SKIPPED as root, and it says so). A content that cannot be read ends the load. |
| `test_mark_tm_order` | `tranger2_mark_tm_order()` when it does not go all the way: a failure half way, and a file whose name has no room for a marker. |
| `test_uncommitted_append` | An append whose md2 row is never written. The cases: (1) a md2 of 0 rows beside a content file that is not empty is ignored with a warning; (2) the file takes the next append; (3) an append whose md2 cannot be created is refused and its content is cut back; (4) a flagged file that is readable again is counted by the next append; (5) the cut comes before the exit of `LOG_OPT_EXIT_ZERO` (in a child); (6) a write that stops part way (`RLIMIT_FSIZE`, in a child) is logged as a short write, with the bytes written and expected, and both files are cut back. |
| `test_torn_md2_tail` | A md2 whose last row is torn (its size is not a whole number of rows). (1) A master cuts it back to its whole rows with one warning, the key loads whole, and the next append goes into the file; (2) a replica writes nothing, logs nothing, and reads the whole rows; (3) a torn first row: the md2 is cut to 0 bytes and then ignored as an append that was never acknowledged; (4) a running master whose md2 ends torn cuts it back at the next append and writes the row on a row boundary; (5) a flagged file (mode 000) that also ends torn is cut back and unflagged by the append that counts it again; (6) a delete that cannot remove the key directory reads the key again, and the cut happens there too; (7) a replica's realtime disk feed on a torn store hands each new row of the master once; (8) a replica feed that sees a row half written (in a counted file, and as the first row of a new file) hands nothing, logs nothing, and hands each row once at the next notification; (9) a replica with a md2 of no whole row logs the 0-rows warning and does not cut; (10) a md2 as 7.25.4 left it after a torn row (acknowledged rows after the torn bytes, on no row boundary): a master does not cut it, logs a CRITICAL that names the shape, flags the file, refuses an append into it, and the md2 bytes do not change; (11) a replica does the same, and does not read the misaligned rows; (12) a torn md2 whose last whole row is not valid is not cut, with its own CRITICAL, and is flagged; (13) a running master that finds the 7.25.4 shape at an append refuses the append, cuts its content back and flags the file in memory; (14, 15) the 7.25.4 shape for every torn size, with and without content after the last row, at the open and at an append: not cut, flagged, the list of the same process says `load_failed`; (16) a torn row with content after it is still cut; (17, 18) a last whole row that names no record, or content before the row before it: not cut, flagged; (19) a zeroed deleted instance as last row is a good row: cut. Cases 5 and 6 are SKIPPED as root, and say so. |
| `test_md2_short_write` | The md2 row of an append is written short (13 bytes) and its cut back fails: the append is refused with an ERROR and a CRITICAL, its content is cut back, and the next append cuts the torn bytes back and writes its row on a row boundary (also after a restart). Then the cut of the next append fails too: that append is refused, its content is cut back, and the append after it goes in. The test links with `-Wl,--wrap=write,--wrap=ftruncate` and fails them in its own wrappers, for md2 files only. It runs as root too. |
| `test_md2_read_error` | A read of the first row (EIO) or of the last row (short) of a md2 fails while the cache is built: the file is flagged, the forward load says `load_failed`, and once the read works an append counts the file again and the key loads whole. The test links with `-Wl,--wrap=pread` and fails the read in its own `__wrap_pread()`: no file mode makes a read fail after an open. It runs as root too. |
| `test_nul_escape_record` | A record with a string that holds NUL characters (`json_dumps()` writes each as the escape `\u0000`): (1) a master appends such records and the list hands each back with the same string, also after a restart; (2) a torn md2 after such records is cut back with the one warning of a torn row, and the key loads. Up to 7.25.4 the append took such a record and no read could read it back. |
| `test_torn_tail_check_fails` | A check of a torn md2 tail that could not RUN is not a check that found damage: (1) an append whose check cannot open the content file (`EMFILE`) is refused, its content is cut back, the file is NOT flagged, and the next append checks again, cuts and goes in; (2) an open whose check cannot run flags the file (it cannot tell a torn row from the 7.25.4 shape), and the next append counts it again, cuts and unflags it; (3) a candidate range larger than the largest memory block that crosses the NUL of a record is not a record: the check reads it in parts, and the tail is cut with no memory error; (4) a master whose memory block is smaller than the writer's (`MEM_MAX_BLOCK` is set per yuno) opens the shape 7.25.4 left, whose last acknowledged row names a record larger than that block, and a torn row after a last whole row that large: both are flagged, not cut; (5) the same master, with records that fit in its block and whose parse does not (a string of 40 000 bytes, an array in 20 000 bytes), each in a child process, which must not die by a signal: (a, b) the shape 7.25.4 left with such a last row is flagged, not cut, and the md2 does not change; (c) the read of such a record fails with a CRITICAL that says why, and the list says `load_failed`. Up to 7.25.4 the string crashed the process (jansson wrote past its buffer). The test links with `-Wl,--wrap=open`. |
| `test_unlistable_dirs` | A directory of the store that cannot be LISTED (mode 0, SKIPPED as root): (1) `tranger2_mark_tm_order()` with a key directory that cannot be listed fails and does not mark the topic, and the tm query of a legacy file whose tm goes back (100, 300, 200; query [250,350]) still finds its row -- up to 7.25.4 the topic was marked and the row was lost to tm queries; (2) a key directory that cannot be listed at the open flags the key: its iterator says `load_failed`, a keyless list names it in `load_failed_keys`, an append into it is refused while it cannot be listed, and once it can the append lists the key again and every row is there -- up to 7.25.4 it loaded EMPTY, and the append left the old rows out; (3) `keys/` that cannot be listed: the topic does not open, and the next open reads every key; (4) a REPLICA that opened the key unlistable, with a keyless feed: once the mode is back, the notification of the master's append lists the key again, hands the feed the new row with rowid 4 (HEAD: 1), and the replica reads the whole key; (5) a replica with A unlistable and a md2 of B unreadable reads both keys whole once the modes are back, with no append, and while the cause is there a load logs nothing more than its load_failed. |
| `test_unlisted_relist_once` | A key directory that OPENS and cannot be READ (`readdir()` fails with `EIO`, by `--wrap=readdir`) at the open flags the key `unlisted`, logged once; three loads say `load_failed` and do not list it again; with `readdir()` working and the directory unchanged it stays flagged; once the directory changes (a `chmod`, its ctime) the next load lists it and reads every row. |
| `test_cmp_file_ids` | The order of two md2 file ids is the order of their names (`strcmp("<a>.md2", "<b>.md2")`), as the load sorts the files of a key. `cmp_file_ids()` compares in place, without building the names, and must give the same sign for every pair: a list of cases where the suffix decides (one id is the start of the other, the next char is below or above `.`, bytes above 127), then 2 000 000 pseudo-random pairs. A unit test: it `#include`s `timeranger2.c` to reach the PRIVATE function. |

Case 6 of `test_uncommitted_append` sets a file size limit in a child process.
The limit applies to every file the child writes, stdout too. Run the test with
its output to a pipe (ctest does), not redirected to a file, or the child
cannot write its log.

See also `tr_msg` (message wrapper), `tr_queue` (queue semantics), `tr_msg2db`
(message database), `tr_treedb` (graph db) and `tr_treedb_load_failed` (a
treedb topic whose keys cannot all be read).
