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
| `test_torn_tail_check_fails` | A check of a torn md2 tail that could not RUN is not a check that found damage: (1) an append whose check cannot open the content file (`EMFILE`) is refused, its content is cut back, the file is NOT flagged, and the next append checks again, cuts and goes in; (2) an open whose check cannot run flags the file (it cannot tell a torn row from the 7.25.4 shape), and the next append counts it again, cuts and unflags it; (3) a candidate range larger than the largest memory block is not a record: the tail is cut with no memory error. The test links with `-Wl,--wrap=open`. |

Case 6 of `test_uncommitted_append` sets a file size limit in a child process.
The limit applies to every file the child writes, stdout too. Run the test with
its output to a pipe (ctest does), not redirected to a file, or the child
cannot write its log.

See also `tr_msg` (message wrapper), `tr_queue` (queue semantics), `tr_msg2db`
(message database), `tr_treedb` (graph db) and `tr_treedb_load_failed` (a
treedb topic whose keys cannot all be read).
