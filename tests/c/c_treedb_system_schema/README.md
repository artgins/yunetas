# c_treedb_system_schema test

Tests the `__system__` meta-treedb that `C_TREEDB` keeps next to the treedbs it
manages: the place where a treedb schema is stored **as data** (topics
`treedbs` → `topics` → `cols`), which is what lets a schema be listed and
edited at runtime instead of only compiled into C.

The test drives `C_TREEDB` through its own commands and checks:

1. **Projection** — opening a treedb writes its schema into `__system__`: one
   `treedbs` node with its `schema_version`, one `topics` node per topic (with
   `system_topic` preserved), one `cols` node per column. Every node must be
   addressed by its **qualified** name — its parent's id, a dot, and its own —
   and carry the bare one in `value`: a name is unique only inside its parent,
   so a column stored without `value` is a column nobody can name, and one
   keyed by the bare name collides with the same column of another topic.
2. **Reconstruction** — the projection alone rebuilds the schema. The treedb is
   closed, its `<treedb_name>.treedb_schema.json` deleted so nothing on disk
   can supply the schema, and re-opened: the topics and columns that come back
   must be the ones that went in.

3. **Reconciliation** — a schema that moved forward (`schema_version` 1 → 2,
   one column added, another re-headered) updates the projection, and the
   columns that were already there **keep their id**: an update appends a
   version of the same node, a re-create would be a second column under the
   same name. The new column reaches the running treedb too, which is the
   whole chain: literal → `__system__` → schema file → topic.

4. **Fidelity** — every attribute a column MAY declare survives the round
   trip. The list is not written in the test: it is read from the descriptor
   a user column answers to, so an attribute added there without storage
   behind it fails here instead of disappearing from every schema in silence.
   That is how `enum`, `template` and `pkey2s` were being lost — a column kept
   its `enum` **flag** while its enumeration evaporated, so it declared an
   enumeration it no longer had, and every value passed.

5. **Refused writes** — a write that could not produce a working schema is
   refused where it is written, not at the next open: a column whose `type` is
   outside the enum the meta-schema declares (on create *and* on update, and
   the refused update leaves the node untouched), a change to the `pkey` of an
   existing topic, and a second column with a name the topic already has —
   which the qualified key refuses at the create, since same topic and same
   name means the same id, while the link still has to refuse a column born
   under another topic and hooked in here.

6. **Column order** — a schema comes back in the order it was DECLARED in,
   topics and columns alike, and a column added in the MIDDLE of a topic
   lands in the middle. The projection cannot supply an order by itself: its
   nodes are records, and records come back in the order the store holds
   them, so the position has to be stored with each node and read back. What
   is served and not declared — the treedb's own `__snaps__` / `__graphs__`,
   a topic the projection kept after C stopped declaring it — is not a
   mismatch; everything the schema declares must be there, in its order.
   What this does not reach is the RELOAD: the `__system__` treedb is built
   once per process, so the order its nodes come back from disk in is only
   exercised across a restart.

7. **The move to qualified ids** — a projection built by hand the way the old
   projector left it (numeric ids, the name in `value`) is re-projected, and
   what comes back is keyed by the qualified name with the legacy nodes gone.
   A column that is in **no** schema from C rides along: it is an operator's,
   the projector would never write it again, and moving it is the only way it
   survives its parent changing address.

8. **What an open does to the operator's work** — a literal newer than the
   file in use wins WHOLE (the user's decision of 2026-09-23): it replaces
   the file, `__system__` is projected from it whole, and what that withdraws
   of the operator's work is said in the API: `withdrawn_at_open` of
   `saved-schema` and `treedbs`, per topic `applied` (an apply that never
   ran), `in_use` (an apply that ran), `saved` or `unsaved`, and nothing for a save taken back. A literal
   that changes a topic without raising its `topic_version` leaves the store
   running its own columns, and a warning says so; after that, while the
   file runs, every open says that the store runs `departments` AHEAD of the
   file (*"Schema file in use declares other columns than the store runs, at
   a topic_version behind the store's ..."*). A saved file that cannot
   be read is `broken`, and the apply of every treedb leaves it out. The
   scenarios of that rule (a topic removed, renamed, added with a hook, a
   tie with an unopened apply) are in
   [`c_treedb_literal_wins`](../c_treedb_literal_wins/README.md).

9. **What a peer hands is refused before anything is made** (Tests 13d2 to
   13d7). Every case is sent as a peer's (`as_peer()` puts a `__username__`
   in the kw), and every refusal is asserted with its priority in the
   strict FIFO of `main.c` (`AS_WARNING` / `AS_ERROR`): a WARNING for a peer,
   an ERROR with a stack for C code.
   - **13d2**, the data a command names: a `treedb_name` of the wrong type
     (`save-schema` and `apply-schema` with `"treedb_name": 5` are refused by
     the command parser, with no answer per treedb: it saved EVERY treedb);
     `save-schema`, `saved-schema` and `apply-schema` of a treedb not open
     here, and of `treedb_system_schema`; `delete-topic` of an unknown topic
     and of `__snaps__`; `create-topic` of `a/b`, with no columns, and with a
     name longer than `NAME_MAX` -- up to 7.26.9 a CRITICAL of the tranger,
     an exit of the yuno; the test asserts now the WARNING *"Topic refused:
     bad name"* of the treedb's own check, and the answer `-1`;
     `open-treedb` of `../name`, with no schema, and with a
     `files_max_size` of the wrong type -- and no directory or service is
     left behind.
   - **13d3**, more of it: a topic named as a FILE of the database
     (`__timeranger2__.json`, `<treedb>.treedb_schema.json`: its `mkdir` was
     a CRITICAL); pkey2s that collide or break the column rule; names too
     long for the lists of the indexes (the 108-byte treedb name); a
     `filename_mask` that is a path or more than one file name; a treedb of
     the system's namespace (`__system__`), or named as what a store keeps
     (`.x`: its node in `__system__` was refused with an ERROR, and it ran
     with no projection; `x.json`); a schema whose `id` is another treedb's;
     `initial_load` of a wrong shape;
     a value of a wrong type from a user with no permission, refused by the
     parser before the command's own permission check; and the seeds of
     `initial_load`, a peer's data in an external `open-treedb` (WARNINGs,
     again at the next start of its `C_NODE`, which the test stops and
     starts) and a C caller's in an internal one (ERRORs).
   - **13d4**, the schema and the store of a peer's `open-treedb`: a topic
     with no columns, no `id` column or a bad `file` column (the open logged
     the CRITICAL *"Cannot create topic of the schema"*); a C caller's open of
     the same is an ERROR and opens as before, and its `create-topic` of a
     FILE of the database goes as it went, refused by the tranger itself
     (*"Not a directory..."* and its CRITICALs, with `exit_on_error` 0); `exit_on_error` and
     `import_root` from a peer; a column id too long for its id in
     `__system__`; a name with `.treedb_schema.json`; a seed id that is not a
     string; a topic named `.blobs` (in `create-topic` and in a schema); a
     store that is not that treedb's, one a master holds, one a tranger of
     this process has open; `delete-treedb` of `x/../<T>` (it removed the
     saved schema of the OPEN treedb `T`); a `save-schema` of a draft with a
     pkey2 `id`, refused at the save; an `apply-schema` of a saved schema
     (written by hand) whose topic the treedb refuses, with the file in use
     unchanged; and a dict of topics keyed by their names, still a schema.
   - **13d5**: a C caller's `open-treedb` goes as before 7.27.0 -- a store with a
     leftover topic directory and a seed with no string id opens, with the
     old ERRORs, while the same open from a peer is refused
     (`check_c_caller_open_as_before`); `close-treedb force=1` from a peer,
     the yuno playing, only for a treedb that same user opened (another user
     is refused, and a
     treedb the C code opened is refused to every peer, with *"close-treedb
     from outside of a treedb the code of the yuno opened, refused"*), and a
     peer re-opens its own valid store, the accepting branch of the store
     check (`check_peer_force_close`); `C_TRANGER`'s `add-record`, `delete-key`,
     `delete-topic` and `create-topic` of a peer into a treedb's topics or
     store are refused (`check_peer_writes_of_treedb_topics`); the shape of
     a peer's schema: topic names a store keeps, `impose_c_schema`, one name
     per topic, a hook column of type `string`, values of a wrong type, a
     version past 2147483647, two elements with one id in `__system__` (a
     WARNING) (`check_peer_schema_shapes`); a `create-topic` whose `file`
     column derives an `__assets__` hook the treedb has
     (`check_create_topic_file_hooks`); and versions past an `int` from a C
     caller's literal, installed and kept whole (`check_versions_past_an_int`).
   - **13d6**: a peer never makes the treedb adopt a topic of ANOTHER
     database of its store (`check_peer_cannot_adopt_foreign_topics`): with a
     persistent msg2db `msg2db_alarms` open on the treedb's tranger (as the
     `db_history` yunos and the broker open it), a peer's `create-topic` of
     `alarms` with `topic_version` 2 is refused and the topic keeps its
     version 1 and its 3 columns; a C caller adopts it, as it always could,
     and a peer's `delete-topic` of it is refused while the msg2db is open
     and after it is closed (its `msg2db_alarms.msg2db_schema.json` names it);
     with the msg2db closed, a peer's `open-treedb` of the store whose schema
     names `alarms` is refused and leaves no service, the store opens with
     its own schema, and a peer's `create-topic` of `alarms` is refused. A C
     caller's open of an EXISTING store whose literal now gives its topics
     bad pkey2s (`id`) or a pkey2 list id past `NAME_MAX` (a topic of 196
     bytes) opens them as they were, each fault an ERROR, and their nodes are
     there (`check_c_caller_topic_on_disk_as_before`); a C caller's
     `create-topic` with a hook column of type `string` is created, a peer's
     is refused (`check_c_caller_create_topic_as_before`); two topics marked
     `main_topic`, or the mark on a topic not hooked to itself, are refused
     to a peer and leave no store, one mark on a topic hooked to itself opens
     (`check_peer_main_topic`); a peer's treedb opened at `schema_version`,
     or a `topic_version`, 2147483647 cannot save an edit (*"save-schema
     would raise a version past 2147483647, refused"*), and nothing is saved
     (`check_peer_save_past_version_max`); and a treedb the C code opened is
     the code's, closed too (`check_peer_open_of_code_treedb`): with
     `treedb_code_own` closed, a peer's `open-treedb` of it is refused, by its
     name and in another case (a WARNING each; no service, and no
     `TREEDB_CODE_OWN` directory); a peer's `open-treedb` of
     `tranger_treedb_code_own`, whose `C_NODE` would take the service name
     of the code's tranger, is refused and makes no service or directory; a
     peer's `delete-treedb` `force=1` of it, by its name and in another case,
     is refused, and its projection in `__system__` and its saved schema
     stay; the code opens it again, and then a peer's open of
     `tranger_treedb_code_own` is answered by that service (*"exists
     already"*, no log). The other way round, with the code's
     `tranger_code_side` closed, a peer's `Code_Side`, whose own tranger
     would take that name, is refused and makes no service or directory, and
     the code opens `tranger_code_side` again.
   - **13d7**: a peer never closes a treedb the C code opened, playing or
     PAUSED, with `force` or without (`check_peer_close_of_code_treedb`). The
     test pauses the yuno for real (`gobj_pause(gobj_yuno())`, then
     `gobj_play()`), in a mode that keeps `C_TREEDB` and its treedbs through
     the pause, as `mqtt_broker` keeps its own (`pause_keeps_treedbs` of the
     test gclass); the four refusals leave the treedb open, and the C
     caller's close with no `force`, paused, closes it. The four refusals are
     repeated with the code's treedb named in another case
     (`TREEDB_CODE_CLOSE`), playing and paused, and leave it open too. A
     peer's own treedb named in another case (`TREEDB_PEER_CASE` for
     `treedb_peer_case`): another user's `force`, playing, is refused and the
     treedb stays; the opener's `force`, playing, closes it; opened again,
     its close with no `force`, paused, closes it; and each close takes its
     row out of `treedbs`. The case rule these checks follow: the name of an
     OPEN treedb is taken in any case, and the commands act under the name it
     was opened with (the answer names that name); the name of a treedb the
     C code opened is refused to a peer in any case, also while it is
     closed; any other name of a closed treedb is taken as it is written,
     because closed treedbs are directories and their names differ by case. A topic is another
     database's on POSITIVE evidence only (`check_peer_dropped_topic_comes_back`):
     a peer's treedb opened with `{items, x}`, closed, opened with `{items}`,
     closed, and opened with `{items, x}` again takes `x` back, with its node
     `x1`. And the `C_NODE` route of a tkey date past 44 bits
     (`check_tkey_date_past_max_c_node`): a peer's topic `events` with the
     string tkey `when`, where `create-node` and `update-node` of a peer with
     `"@99999999999999 +0000"` are refused (WARNINGs) and write nothing, a C
     caller's `gobj_create_node()` of it is refused (an ERROR), a date the
     row keeps (`"@1760000000 +0000"`) is created, and a C caller's
     `tranger2_append_record()` of the bad date straight to the tranger is
     appended, with its time cut and the ERROR *"Record time past what an md2
     row keeps (44 bits): appended CUT"*.

Steps 2 and 3 are the ones that matter for schema editing: they are the path an
edited schema takes to reach a running treedb. Step 4 is what stands between an
editor and a treedb that no longer opens.

## Run

```bash
ctest -R test_c_treedb_system_schema --output-on-failure --test-dir build
```
