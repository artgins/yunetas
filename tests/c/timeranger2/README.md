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
| `test_unreadable_at_open` | A store damaged before the tranger starts: the key is flagged at the cache build, and every load of it says `load_failed`. The damage is a md2 of mode 000 (skipped as root). A content that cannot be read ends the load. |
| `test_mark_tm_order` | `tranger2_mark_tm_order()` when it does not go all the way: a failure half way, and a file whose name has no room for a marker. |
| `test_uncommitted_append` | An append whose md2 row is never written. The cases: (1) a md2 of 0 rows beside a content file that is not empty is ignored with a warning; (2) the file takes the next append; (3) an append whose md2 cannot be created is refused and its content is cut back; (4) a flagged file that is readable again is counted by the next append; (5) the cut comes before the exit of `LOG_OPT_EXIT_ZERO` (in a child); (6) a write that stops part way (`RLIMIT_FSIZE`, in a child) is logged as a short write, with the bytes written and expected, and both files are cut back. |
| `test_torn_md2_tail` | A md2 whose last row is torn (its size is not a whole number of rows). (1) A master cuts it back to its whole rows with one warning, the key loads whole, and the next append goes into the file; (2) a replica writes nothing, logs nothing, and reads the whole rows; (3) a torn first row: the md2 is cut to 0 bytes and then ignored as an append that was never acknowledged. |

Case 6 of `test_uncommitted_append` sets a file size limit in a child process.
The limit applies to every file the child writes, stdout too. Run the test with
its output to a pipe (ctest does), not redirected to a file, or the child
cannot write its log.

See also `tr_msg` (message wrapper), `tr_queue` (queue semantics), `tr_msg2db`
(message database), `tr_treedb` (graph db) and `tr_treedb_load_failed` (a
treedb topic whose keys cannot all be read).
