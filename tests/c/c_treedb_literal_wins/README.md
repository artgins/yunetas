# c_treedb_literal_wins test

Tests what `C_TREEDB` does when a dynamic-schema treedb (`impose_c_schema`
off) opens with a schema from C (the literal) that is newer than the schema
file in use. The rule is the user's decision of 2026-09-23:

- A literal whose `schema_version` is **higher** than the file in use (or
  with no file) **wins whole**. It replaces the whole file, `__system__` is
  projected from it whole, and a topic it does not declare disappears from
  both.
- The operator work that this discards is reported: one WARNING *"Schema
  from C withdrew work on the schema at open"* that names the treedb and the
  topics, and `withdrawn_at_open` in `treedbs` and `saved-schema`, per topic
  `applied` (an apply that never ran), `in_use` (an apply that ran),
  `saved` or `unsaved`.
- A literal that is **not** higher is not installed: the file runs, and
  `__system__` keeps what it holds.

Each scenario opens its own treedb. At the end, it checks that the three
places of the schema agree: what the store RUNS (the open topics, their
`topic_version` and columns), the schema FILE in use, and `__system__`
(`saved-schema` answers no draft that differs from the file).

| Scenario | Treedb | What it checks |
|---|---|---|
| RM | `tw_rm` | The literal removes a parent topic and the fkey to it. The treedb opens, twice. The parent is gone from the file, from what runs and from `__system__`. |
| TIE+HOOK | `tw_tie` | An apply of `users` (topic_version 2) is not opened yet. The literal gives `users` 2 with a new fkey, and adds a parent `groups` that hooks it. The literal runs. The apply is reported as `applied`. |
| EQ | `tw_eq` | The same, with a new plain column: a record that uses it is accepted. A second open with the same literal reports nothing. |
| SAVED | `tw_saved` | A saved draft of `users` and an unsaved draft of `departments` are both withdrawn and reported, with the withdrawn saved schema. |
| OLD | `tw_old` | A literal equal to the file (other content) and one older than it are not installed. The unopened apply runs at that open. |
| REN | `tw_ren` | A topic is renamed, with its hook and fkey. The old topic is gone. |
| NR | `tw_nr` | A topic is changed without a higher `topic_version`. The file and `__system__` show the literal. The store keeps its own columns, and a warning says so. |
| IMP | `tw_imp` | With `impose_c_schema` forced by the code, a newer literal also re-makes `__system__` whole. |
| L-2 | `tw_l2` | A topic whose store directory is gone is not reported as `applied`. An apply is recorded when it is made (`saved_schemas/<treedb>.applied.json`), not guessed from a missing `topic_var.json`. |
| SNAP | `tw_snap` | A snapshot of `__system__` holds the topic the literal removes. The projection is unfinished and says so: warning, `c_schema_version` 0, `unfinished_projection`, `save-schema` refused, retried at every open. Once the snapshot is deleted, the next open completes it. |
| TWICE | `tw_twice` | A second `open-treedb` of a treedb that is open is refused as "already open" before anything is reconciled: `__system__`, the saved schema and the file stay as they were. |
| RAN | `tw_ran` | An apply that ran (an open read it), then a newer literal: the running dynamic `users` is reported as `in_use`; `departments`, changed only by the developer, is not reported. |
| SEED | `tw_seed` | After `delete-treedb`, a projection seeded from a dynamic file has `c_schema_version` 0, and the tie with a literal of the same number is said at every open. |
| LOCK | `tw_lock` | The store of the treedb is locked (as by another process): it opens as a replica and `__system__` is not reconciled. When the lock is free, the literal is installed and projected. |
| REC | `tw_rec` | The record of an apply cannot be written: the apply is refused and the file in use does not change. |
| M1 | `tw_m1` | A projection is unfinished (a snapshot holds `departments`). `saved-schema` does not show the leftover as a draft. The operator adds a column to `users`: it is the only draft. The retry of the projection replaces that column and reports `users` as `unsaved`. A column added to the leftover topic makes that topic a draft: when the snapshot is gone, the projection completes and reports `departments` as `unsaved`. |
| M2 | `tw_m2i`, `tw_m2d` | An unedited leftover of an unfinished projection is not reported as withdrawn work when the projection completes: on the imposed retry, and when a newer literal arrives. |
| L1 | `tw_l1` | A projection seeded from a dynamic file (`c_schema_version` 0) is not unfinished. The file itself, as the literal, projects nothing: the operator's column and header draft stay, nothing is reported. |
| L3 | `tw_l3` | The process died between the record of an apply and its rename (the record is written by hand). The next apply keeps the topics of the apply that ran, from the record's `previous`: a newer literal reports `users` as `in_use` and `departments` as `applied`. |
| L4 | `tw_l4` | A projection seeded from the file fails (a column flag that the meta-schema refuses). It is recorded, retried at every open, its partial projection is no draft, and `save-schema` refuses. When the file is fixed, the next open completes it. |
| L6 | `tw_l6` | An open that fails answers `-1` and names the yuno: a literal that C_TREEDB refuses, and a schema file that `treedb_open_db()` refuses (no topics). The second answer tells to `close-treedb` first. |
| M1b | `tw_m1b` | A column the operator adds to the leftover topic stays a draft through a retry that cannot finish (the snapshot still holds the topic): `saved-schema` shows it, the record does not name it as a leftover, nothing is reported. The open that removes it reports `departments` as `unsaved`. |
| M2b | `tw_m2r`, `tw_m2c` | A draft that the projection cannot replace (its delete is refused) stays a draft and is reported once, by the open that replaces it: a column and a header edit on a topic the literal removes (`tw_m2r`), and a column added to a topic the literal keeps (`tw_m2c`). |
| L1b | `tw_l1b` | The record of an unfinished projection is torn. It still says "unfinished": `saved-schema` answers `unfinished_projection`, `save-schema` refuses, ONE WARNING is logged at every read (with the cause in `error`, and no CRITICAL). The next open retries and writes the record again. What the torn record left is unknown, so it is taken for a draft and reported when the projection completes. |
| L2b | `tw_l2b` | The node of a new treedb in `__system__` is created with `schema_version` and `c_schema_version` 0 (its first record on disk), and stamped last. |
| L3b, L4b | `tw_l3b` | After an open that failed, a second open says that the treedb did not open and tells to `close-treedb`; the `treedbs` row has `opened: false`. `delete-treedb` is refused: it says that the treedb did not open, and to `close-treedb` it first. `close-treedb` logs nothing (C_NODE does not close a treedb that never opened). Writing `with_link_events` on the C_NODE of that treedb logs nothing either (there is no callback to change). Every answer of `open-treedb` and `close-treedb` starts with the yuno. |
| N7 | `tw_n7u`, `tw_n7s` | The operator deletes the topic `departments` in `__system__` (a draft). A newer literal declares it: the topic is created again, and the deletion is reported as `unsaved` (`tw_n7u`), or as `saved` with the withdrawn saved schema when `save-schema` published it (`tw_n7s`). |
| N1 | `tw_n1` | After `delete-treedb`, a seed that died before its end is emulated: the `treedbs` node with `schema_version` and `c_schema_version` 0, and no record. The next open completes it (the file runs), reports nothing, and stamps `c_schema_version` 1. Then the operator deletes every topic: `save-schema` refuses a draft with no topics, and a saved schema with no topics (written by hand) is refused: `saved-schema` answers `can_apply` false and says why, and `apply-schema` refuses it; the file in use does not change. |
| N4 | `tw_n4` | A SAVED draft on the topic that the literal removes, and a snapshot refuses the delete. The first open reports only the withdrawn saved schema, and the record keeps `draft_kinds: {"departments": "saved"}`. When the snapshot is gone, the open that completes the projection reports `departments` as `saved`. |
| LE | `tw_lec`, `tw_let` | A snapshot holds `departments`, which the literal removes, so it is a leftover. The operator edits it: the header of `departments.name` (`tw_lec`), or the attribute `main_topic` of the topic (`tw_let`). The edit is a draft: `saved-schema` shows `departments` in `draft_changed`. A retry that cannot finish reports nothing, keeps the edit a draft (the edited column is not a leftover in the new record, `draft_kinds` keeps `"unsaved"`). The open that removes the topic reports `departments` as `unsaved`. |
| AD | `tw_adu`, `tw_adl`, `tw_adr`, `tw_dlu`, `tw_ads` | A save of `users` is pending, and the operator adds a topic `groups` that is not saved. A newer literal reports `groups` as `unsaved`, not `saved`: when the literal does not declare it (`tw_adu`), when it does (`tw_adl`), and through a projection that a snapshot leaves unfinished, where the record keeps `draft_kinds: {"groups": "unsaved"}` (`tw_adr`). The cases next to it do not change: an unsaved deletion with another save pending is `unsaved` (`tw_dlu`), and an added topic that was saved is `saved` (`tw_ads`). |
| FW | `tw_fw` | A write of the projection fails on disk (the files of `users.username` are read-only): the update is taken back in memory, and the record lists the column in `not_written`, with no node kept for it. After a restart of `__system__` and with the files writable, the open that completes the projection reports nothing. |
| FWS | `tw_fws` | The same failed write, retried by the same process (no restart). The failed update was taken back in memory, so the retry writes it: after a restart of `__system__` the disk says what the literal says, nothing is a draft, nothing is reported. |
| UL | `tw_ul` | A snapshot holds `departments`, which the literal removes. The operator unlinks the leftover column `departments.name` from its topic. It is a draft: `saved-schema` shows `departments` in `draft_changed`. The open that completes the projection removes the topic and the column, which no topic holds any more, and reports `departments` as `unsaved`. A later literal that declares the column again creates it and completes. |
| OA | `tw_oa` | The operator creates a column node that no topic holds, with the id that a newer literal declares. It changes no schema (`draft_changed` is empty). The newer literal takes the node for the column (written as the literal says and linked to `users`) instead of failing on it at every open, and reports `users` as `unsaved`. |
| OT | `tw_ot` | The operator unlinks the topic `departments` from its treedb node. A newer literal that declares the topic takes the topic node and its columns, and reports `departments` as `unsaved`. |
| MS | `tw_ms` | The leftovers were kept under an older meta-schema (the record has a lower `system_schema_version`, and the kept columns have no `description`, as if the newer meta-schema added it). Every leftover is taken as left, a WARNING says it, and the open that completes the projection reports nothing. |
| LF | `tw_lf` | The link of a new topic to its treedb fails (every write of the topic node fails once it is created). No column is written under the topic that no tree reaches, and the record names the topic in `not_written`. After a restart, the open that completes the projection takes the topic, writes its columns and reports nothing. |
| FT, FC | `tw_ft`, `tw_fc` | The write that TAKES a topic the operator unlinked fails (its files are read-only), and the create of a new topic fails while the operator's column for it is in no topic (the keys directory of `topics` is read-only). No node is deleted, nothing is reported, and the record keeps `draft_kinds: {"departments": "unsaved"}` (`tw_ft`). The open that succeeds takes the nodes and reports `departments` once, as `unsaved`. |
| DN | `tw_dn`, `tw_dn.b` | Two treedbs whose names start the same. A newer literal of `tw_dn` leaves the nodes of `tw_dn.b` alone, linked or not, and reports nothing. A column of `tw_dn.b` that the operator linked into `tw_dn.users` is only unlinked (and `users` is reported). `delete-treedb` of `tw_dn` deletes its own topic and no node of `tw_dn.b`. A newer literal of `tw_dn.b` takes its own orphan column and reports `c`. |
| MV, NP | `tw_mv`, `tw_np` | The operator moves `departments.name` to `users` (`tw_mv`), or links it to `users` too (`tw_np`). A newer literal puts it back where it declares it in ONE open, with no record left, and reports it once: `departments` and `users` (`tw_mv`), `users` (`tw_np`). The node is never deleted. |
| CR | `tw_crn_*`, `tw_crd_*` | The process DIES half way through a projection. A child process opens a newer literal (a header changed, a column and a topic added, a topic removed) and is killed (SIGKILL) at the k-th write of `__system__`, for every k (23 writes, 26 with drafts). It always leaves a record with `in_progress: true`. The next open completes the projection with no error, reports nothing that nobody did, and reports the operator's drafts (a header edited in `users`, a column added to `groups`, in `tw_crd_*`) exactly once. The open after that reports nothing. The log of this scenario is not compared line by line (where the process dies decides what the retry logs); each retry counts its errors and warnings. |
| LM | `tw_lm` | A snapshot holds `departments` (a leftover). The operator MOVES its column `name` to `users`: the place of a leftover is part of "as left", so the move is a draft of both topics (`draft_changed`). The open after the snapshot is deleted removes the column and reports `departments` and `users` as `unsaved`. |
| OE | `tw_oe0_*`, `tw_oe1_*` | A process dies between the delete of `departments` and the delete of its columns: `departments.name` is in NO topic. The operator edits its header (`tw_oe0_*`) or links it to `users` (`tw_oe1_*`). A node in no topic is still compared: the edit is a draft, and the open that completes the projection deletes the column and reports it (`departments`; and `users` for the link). |
| AMB | `tw_am`, `tw_am.b`, `tw_zz` | The operator deletes the topic node `tw_am.b.departments` with force; its columns could be of `tw_am` or `tw_am.b`. The schema file of `tw_am.b` declares them: an unrelated treedb and `tw_am` say nothing and leave them; the newer literal of `tw_am.b` takes them (ONE WARNING each), deletes them and reports `departments`. A later open says nothing. |
| AC | `tw_cx`, `tw_cx.b*` | The same ambiguity left by a process that dies between the delete of `departments` and of its columns: the record of the projection in progress names them, so the retry takes them (one WARNING each), deletes them and reports nothing. |
| DTM | `tw_dtm` | `delete-treedb` deletes every node of its treedb: a column the operator moved to another of its topics, and a column the operator left in no topic. |
| RU, RL | `tw_ru`, `tw_rl` | The record of an unfinished projection cannot be written (`saved_schemas/` read-only) and a snapshot refuses a delete. The node says `c_schema_version: -1`, the record is kept in memory (no draft, `save-schema` refuses) and written at the next open; the open that completes reports nothing (`tw_ru`). When the process that kept it is gone (a child), the record is LOST: one WARNING, `save-schema` refuses, and the open that completes reports what `__system__` held over the file, as `unsaved` (`tw_rl`). |
| DD | `tw_dd`, `tw_dx`, `tw_dx.b`, `tw_ds` | Names with a dot give two elements one id. A literal whose columns collide (`tw_dd`), and one whose topic collides with another treedb's (`tw_dx.b` against `tw_dx`), are refused: the treedb does not open, one ERROR, nothing projected. A draft that collides is not saved, and a saved schema that collides is not applied (`tw_ds`). |
| L8 | `tw_l8` | The literal removes `departments`, which holds a column the operator added. The topic is deleted and the deletes of its columns fail: the draft is still in `__system__`, so it is not reported, and the record keeps its kind. The open that removes the columns reports it. |
| SE | `tw_se` | A node stamped with the literal 2 and no topic and no schema file, as a crash of 7.25.4 left it (it wrote the numbers first). The open with the literal 2 completes the projection and reports nothing. |
| LG | `tw_lg*` | A projection keyed by rowid (an older meta-schema) moves to qualified ids, and the process is killed at each write of the move. The next open completes it: no legacy id is left, every node is at its qualified id, linked, with its content (an operator's column included), and no error is logged. |

An unfinished projection is RECORDED in
`saved_schemas/<treedb>.unfinished.json` under the `__system__` tranger. The
record says which ids of `__system__` the projection left. Only these, while
they stay as the projection left them, are not drafts, on every path. An
operator's draft that the projection cannot replace is not a leftover: it
stays a draft, the record keeps its kind (`draft_kinds`), and the open that
replaces it reports it with that kind.
The record also keeps what the projection left at each of those ids
(`leftover_nodes`): only the attributes that a projection writes (for a
column: `value`, `order` and the attributes of the column descriptor; for a
topic: `value`, `order`, `pkey`, `pkey2s`, `system_flag`, `tkey`,
`topic_version`, `system_topic`, `main_topic`), and the version of the
meta-schema (`system_schema_version`). A leftover that the operator EDITS
afterwards in one of those attributes is no longer as the projection left
it: the edit is a draft like any other, and the open that replaces it
reports it (see YUNO_TREEDB.md §3.11). A link and the editor geometry are
not compared, but a leftover column that is unlinked from its topic, or
deleted, is an edit too. No node is kept for an id that the projection failed to write
(`not_written`): it is taken as left. When the meta-schema version changed,
every leftover is taken as left, with a WARNING.

A projection records what it is about to write BEFORE its first write (the
same record, with `in_progress: true`, `planned`, `target_nodes` and
`replaced_kinds`). A node at a planned id that is as it was, or as the
projection writes it, is the projection's: only a node that is neither can
be the operator's work (scenario CR).

The expected log list in `src/main.c` is strict FIFO: every line from INFO
up, in order. The scenarios from LM on are not in it: each runs in its own
timeout (the event loop runs between two, and completes what the treedbs
a step closed cancelled), and counts the errors and warnings of each open
instead. A forked child takes an io_uring ring of its own before it does
anything: the rings of the parent are mapped shared, and a child that
submits on them breaks every later submission of the parent.

## Run

```bash
ctest -R test_c_treedb_literal_wins --output-on-failure --test-dir build
```
