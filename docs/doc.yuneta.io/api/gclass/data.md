# Data GClasses

Time-series, graph database, and resource persistence.

**Source:** `kernel/c/root-linux/src/c_tranger.c`, `c_treedb.c`,
`c_node.c`, `c_resource2.c`

---

(gclass-c-tranger)=
## C_TRANGER

Time-range database manager — wraps **timeranger2** for CRUD operations
on time-series topics.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

**`master` answers what the tranger IS.** It is configured (`SDF_RD`) and read
at create, but read afterwards it answers the tranger's own flag: a master that
lost its lock while stopped (another process took the store) is a replica, and
`gobj_read_bool_attr(gobj_tranger, "master")` says `FALSE` (new after 7.25.4;
7.25.4 answered the configuration). `open-rt` and `open-list` follow it: a
replica's feed is an `rt_disk`, which the other master's appends reach through
the disk.
The flag moves when the tranger REVIVES, which happens at the first write or the
first topic open after `tranger2_stop()`. Between the stop and that moment it
still says what it was, so an `open-list` on a topic that is already open can
still get an `rt_mem` feed. Read `master` again after the restart has opened its
topics.

### Commands

| Command | Description |
|---------|-------------|
| `topics` | List all topics: their names, or with `expanded=1` a desc each (`{topic_name, system_flag, pkey, tkey, topic_version}`). `system_flag` is what tells whether the topic's `t`/`tm` are seconds or **milliseconds** (`sf_t_ms` / `sf_tm_ms`). |
| `create-topic` | Create a new topic. |
| `open-topic` | Open an existing topic. |
| `delete-topic` | Delete a topic. **Irrecoverable.** A topic that still holds records needs `force=1`, and the refusal says so: `command-yuno id=<id> service=<tranger> command=delete-topic topic_name=frames force=1`. Before 7.21.0 the guard never fired and a topic with records was deleted on the first call. |
| `delete-key` | Delete a whole key (primary key) of a topic and every record it holds. **Irrecoverable and master-only**; the delete propagates to the in-process subscribers and to the `rt_by_disk` followers. A key that still holds records needs `force=1` — the refusal names the record count. A key that is not there is an error, not a silent success. |
| `mark-tm-order` | Migrate a topic written before the tm markers -- every topic created by 7.25.4 or earlier -- to one that MARKS (new after 7.25.4): `tranger2_mark_tm_order()` reads every md2 file of every key once, writes `<file>.tm_unordered` / `<file>.unordered` where a `__tm__` / `__t__` goes back, and sets `marks_tm_unordered` in `topic_desc.json`. Until then a tm query (`from_tm` / `to_tm`) on that topic reads every md2 row of the key; marked, only the files its range meets. **Master-only** (a replica answers `-1` *"... is READ-ONLY, this yuno is not its master"* before the library is called), synchronous, idempotent: run it again after a rollback to a binary that appends without markers. Permission `write`: it rewrites what the store says about the topic's files, and adds or deletes no record. `command-yuno id=<id> service=<tranger> command=mark-tm-order topic_name=frames` answers `0: <role^name>: topic 'frames' marks tm order now: 1 key(s), 30 file(s), 2 tm and 0 t marker(s) written`, with `data: {"topic_name": "frames", "was_marking": false, "keys": 1, "files": 30, "rows": 600000, "t_unordered_marked": 0, "tm_unordered_marked": 2, "marks_tm_unordered": true}`; a topic that marked already says `re-marked` and `was_marking: true`. A failure answers `-1` *"cannot mark the tm order of topic 'frames', it is left as it was (see the log)"*: the markers already written stay. **`all=1` migrates every topic of the tranger** (every topic on disk: a directory of the store with its `topic_desc.json`; one without it, like the `saved_schemas/` that `C_TREEDB` keeps in the store of `__system__`, is not a topic and has no row; `topic_name` is not read), one row each `{topic_name, result, comment, data}` with the report above as `data`: `command-yuno id=<id> service=<tranger> command=mark-tm-order all=1` answers `0: <role^name>: mark-tm-order of every topic: 5 topic(s), 5 marked now, 0 re-marked`; a topic that fails does not stop the others, its row says `-1` and the answer is `-1`. **Synchronous: the yuno is blocked until the last topic is marked.** The cost is linear in rows and files (timeranger2's measure, warm page cache: 16 ms for 600000 rows in 30 files; 72-88 ms for 4 keys of 3650 daily files). The upgrade step and the rollback caution are in [Deploying yunos](#dy-mark-tm-order). |
| `open-list` / `close-list` | Open or close a record list (one-shot snapshot with `return_data=1`, else a live list collecting realtime appends). A live list opened by a remote session is that session's, like its iterators: it is closed when the session closes, and survives the session closing its last Live card. A **keyless** list accepts `rkey` (PCRE2 regex over the keys), and it governs both the disk load **and** the realtime feed. A list with no realtime feed (`to_rowid` set) is not its topic's, so it outlives it: `close-list` frees it after its topic closed, and a `delete-topic` frees the ones of that topic (after 7.25.3 they leaked, 10 KB each). |
| `get-list-data` | Retrieve an open list's data. |
| `list-keys` | List a topic's keys with their record counts **and their time span on both axes**: `[{key, records, fr_t, to_t, fr_tm, to_tm}]`. Lets a client bound a time picker to what the key really holds without reading a record. Filters, sorts and pages **in the server**: `rkey` (PCRE2 regex), `order=key\|records` + `desc`, and `from`/`limit` (with `limit>0` the answer is a page `{total_rows, pages, data}`, and `limit=0` keeps the plain full list). |
| `open-iterator` / `close-iterator` | Open/close a stateful per-key iterator (row index only, no upfront load) for cursor pagination. The handles a remote session opens are stamped with it and reaped when the session closes, whether or not it subscribed to anything (the service watches the session's `EV_ON_CLOSE`, once per session). Takes the match conditions below. A filtered iterator indexes the matching rows at open, so `total_rows` and the pages count only those. Give `key` for one key, or `rkey` (PCRE2 regex) for several keys: see *Several keys in one iterator* below. |
| `get-page` | Get a page `{total_rows, pages, data}` from an open iterator (`limit`, optional `backward`). `from_rowid` is 1-based and, on a **filtered** iterator, is a position among the MATCHING rows (a global rowid only when the iterator does not filter). **`backward` counts from the END and returns the newest rows first**, for every kind of iterator — one key filtered or not, several keys: `get-page iterator_id=it1 from_rowid=1 limit=100 backward=1` is the newest 100. A `get-page` that does not say takes the direction the iterator was OPENED with (`open-iterator … backward=1`). Before 7.25.0 an unfiltered one-key iterator kept the window counted from the start and only reversed it, and `open-iterator backward=1` did nothing. An iterator whose key was deleted since it opened (by `delete-key`, or by a treedb sharing the tranger) is closed at its next `get-page`, which answers `-1` with *"iterator 'it1' closed, its key 'D' was deleted: open it again"*; asked again, it is *"Iterator not found"*. A multi-key iterator closes when ANY of its keys goes. |
| `open-rt` / `close-rt` | Open/close a realtime feed on a topic key (no history load). New appends are published as `EV_TRANGER_RECORD_ADDED` to subscribers. An `rt_id` longer than `NAME_MAX` (255 bytes) is refused with `-1` (a replica names a directory after it), and so is any feed the tranger cannot open: `open-rt` answers `-1` and names the feed, it never answers "opened" for a feed that does not deliver. |
| `add-record` | Append a record. |
| `print-tranger` | Dump tranger state as bounded JSON (`expanded`, `lists_limit` and `dicts_limit`. Unexpanded containers answer as `[[size]]`). |
| `desc` | Describe topic schema. |

**`open-iterator` match conditions** (all optional, and `0` or empty means unset):
`from_t`/`to_t`, `from_tm`/`to_tm`, `from_rowid`/`to_rowid`, `backward`, and the
user_flag conditions (`user_flag`, `not_user_flag`, `user_flag_mask_set`,
`user_flag_mask_notset`). They are ANDed, and every one is honored **per
record**.

**Several keys in one iterator.** Give `rkey` instead of `key`. The iterator
lays the matching keys end to end, sorted by key, in the same order `tr2list`
prints a topic. `get-page` positions are positions in that concatenation. Each
record gives its key in `__md_tranger__.key`, because one page can hold more
than one key. The order is **not** a time order: a rowid counts inside one key
only. To see the records by time, sort the page on the client. The match
conditions apply to each key separately. `key` and `rkey` together is an
error.

```
command-yuno id=<id> service=<tranger> command=open-iterator iterator_id=all1 topic_name=binaries rkey=.*
command-yuno id=<id> service=<tranger> command=get-page iterator_id=all1 from_rowid=1 limit=100
command-yuno id=<id> service=<tranger> command=close-iterator iterator_id=all1
```

The answer of `open-iterator` also gives `keys`, the number of keys it found.

**A key deleted under it is a deleted key, born again or not.** A multi-key
iterator holds no tranger2 iterator between pages, so it keeps one realtime
feed over the topic (`only_md`, fed to nobody) for its `key_deleted` callback:
the next `get-page` after a `delete-key` of one of its keys answers -1 *"was
deleted"* and closes the iterator, like a one-key iterator does, even when the
key was created again in between. Open it again to page the reborn key.
**Only in the process that deleted the key** (the master): the callback fires
in the delete itself, so a REPLICA's feed never hears it. There the next
`get-page` reads files that are gone: it fails on the read, logs *"Key gone
from disk while its iterator was open: deleted (by the master?)"*, and the
iterator stays open. Close it and open it again.

**An id is free again the moment its handle is gone.** A handle goes with its
topic (a close and reopen, `delete-topic` + `create-topic`, a stop/start of the
service): the entry is dropped at the stop, after the `delete-topic`, when a
`get-page` answers *"closed with its topic"*, or when `open-iterator` /
`open-rt` / `open-list` meet the id again — so the same id opens a new handle,
with data, instead of *"already open"* and no data.

**The keys watch is not a client's feed.** It is named
`<iterator_id>^__keys__` and opened under a creator of its own, so a client's
`open-rt rt_id=all1^__keys__` is another feed: until after 7.25.3 the two
collided, `open-iterator` answered `-1`, and a `get-page` of a dead iterator
closed the client's feed. It records only the deletes of the keys its iterator
pages.

**What it costs.** The open counts the rows of each key once (for a filtered
iterator that is building and dropping each key's index) and keeps only a
number per key; a `get-page` opens the keys its page touches, reads, and closes
them. Until after 7.24.1 it kept one tranger2 iterator per key for its whole
life — 1000 keys with 60 daily files was +147 MB for one card. A key the
iterator does not filter is counted LIVE at every `get-page`, like a one-key
iterator: a backward page (newest first) shows the rows appended since the open.
A filtered key keeps the count of the rows its filter matched at open (after
7.25.3; before, every key kept the count of the open, so the whole-topic card
never showed a new row): a row appended to it after the open is never paged,
although each page indexes the key again -- counting it would mean indexing
every filtered key on every page. A key CREATED after the open is not in the
iterator: open it again to see it.

**Positions move under appends.** Because an unfiltered key is counted live, a
row appended to a key moves the positions after it by one. Paging forward after
an append to an EARLIER key (or backward after an append to a LATER key), the
next page repeats the last row of the page before; an append never makes a page
skip a row. Every record carries its key and rowid, so a client that must not
show a row twice drops a repeated `(key, rowid)`. For example, with key `I`
holding 3 rows and key `J` 2:

```
command-yuno id=<id> service=<tranger> command=open-iterator iterator_id=ij topic_name=t rkey=^[IJ]$
command-yuno id=<id> service=<tranger> command=get-page iterator_id=ij from_rowid=1 limit=4
#   I#1 I#2 I#3 J#1
#   (a row is appended to I)
command-yuno id=<id> service=<tranger> command=get-page iterator_id=ij from_rowid=5 limit=4
#   J#1 J#2         <- J#1 again
```

Bound the reading of a large topic anyway: a negative `from_rowid` reads the last
N records of EACH key, and is what gui_treedb's whole-topic card starts with:

```
command-yuno id=<id> service=<tranger> command=open-iterator iterator_id=all1 topic_name=binaries rkey=.* from_rowid=-100
```

A tranger record carries **two independent timestamps**, and a browser of raw
records needs both:

| Axis | Meaning |
|------|---------|
| `t`  | **Persistence** time — when the record was appended to the topic. |
| `tm` | **Message** time — when the event it carries happened (the record's `tkey` field, set by the producer). |

They diverge whenever data is backfilled or a device uploads a buffered batch
late. Both are expressed in the **topic's** unit — seconds, unless its
`system_flag` sets `sf_t_ms` / `sf_tm_ms` (ask `topics expanded=1`).

---

(gclass-c-treedb)=
## C_TREEDB

Hierarchical tree database manager — manages **TreeDB** instances on top
of timeranger with JSON schema support.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `path` | `string` | Storage path. |
| `filename_mask` | `string` | Filename pattern. |
| `master` | `bool` | `TRUE` for master, `FALSE` for read-only replica. |
| `exit_on_error` | `integer` | Log options for a critical error of a treedb, handed to its tranger as `on_critical_error`. Default `"2"` = `LOG_OPT_EXIT_ZERO`: a critical error EXITS the yuno. |
| `impose_c_schema` | `bool` | `SDF_RD`, default `1`, **not persistent**. Open every treedb with its schema from C, over a newer schema file on disk. `0`: open from the schema FILE, which `apply-schema` replaces. A literal is installed only when its `schema_version` is higher than the file's, and then WHOLE. It replaces the whole file, as in 7.25.4. `__system__` is projected from it whole, and that half is NEW after 7.25.4: a topic or column the literal does not declare is DELETED from `__system__` (7.25.4 only created and updated, so a removed topic stayed there and the next `save-schema` published it again). A delete that a **snapshot** of `__system__` holds is refused (*"cannot delete node, a snapshot still holds it"*); a write that fails (create, update, link; an ERROR) is the same case. The projection is then UNFINISHED and says so: a WARNING *"Schema projected into `__system__` only in part: its version is not recorded, and every open of the treedb retries it"* (`not_removed`, `not_written`, `how`), `c_schema_version` 0, and a RECORD, `saved_schemas/<treedb>.unfinished.json` under the `__system__` tranger, for example `{"schema_version": 3, "not_removed": ["treedb_x.departments"], "not_written": [], "leftovers": ["treedb_x.departments", "treedb_x.departments.id", "treedb_x.departments.name"], "draft_kinds": {}, "replaced_kinds": {}, "leftover_nodes": {"treedb_x.departments.name": {"value": "name", "order": 1, "header": "Name", "type": "string", "__parents__": ["treedb_x.departments"], ...}, ...}, "system_schema_version": 18}` (`draft_kinds`: the kind of each operator's draft the projection could not replace, for example `{"departments": "saved"}`; `replaced_kinds`: always written, `{}` when empty: the drafts a projection that DIED half way was replacing, carried forward by a retry that fails too, so the open that completes the projection reports them once, for example `{"users": "saved"}`; `leftover_nodes`: what the projection left at each leftover id, only the attributes a projection writes -- for a topic `value`, `order`, `pkey`, `pkey2s`, `system_flag`, `tkey`, `topic_version`, `system_topic`, `main_topic`; for a column `value`, `order` and the attributes of the column descriptor -- and its place, `__parents__` (the ids of the parents it hangs from, sorted), `null` where it left nothing, and nothing for an id in `not_written`: its write failed, and it is taken as left; `system_schema_version`: the meta-schema those nodes were kept under). The record is written whole (a `.new` file created `O_EXCL|O_NOFOLLOW`, flushed, renamed over the old one). The numbers of the treedb's node are written LAST, on full success only: the node of a new treedb is created with `schema_version` and `c_schema_version` 0 and stamped at the end, so a process that dies half way leaves no projection that says it is complete. And a projection RECORDS what it is about to write BEFORE its first write: the same record, with `"in_progress": true`, `planned` (every id it will write, link, unlink or delete), `target_nodes` (what it writes at each, `null` for a delete or an unlink) and `replaced_kinds` (the drafts it will replace). The projection that completes removes it after its WARNING; one that fails writes over it what it could not do. So a process that dies between two writes always leaves a record, and the next open knows what was under way: a node that is as it was or as the projection writes it is the projection's, never reported; only a node that is NEITHER can be the operator's work. That open completes the projection and reports `replaced_kinds` once, whether the dead process had replaced those drafts already or not. For example, killed after `treedb_x.users.email` is created and before it is linked, the next open links it, completes and answers `withdrawn_at_open: {}`. The record, not `c_schema_version` 0, says that the projection is unfinished (a projection seeded from a dynamic file has `c_schema_version` 0 too, and it is complete). While it is there: every open retries the projection -- from what runs when no newer literal is installed (INFO *"Completing the projection into __system__, left unfinished by an earlier open"*, `source` `schema from C` or `schema file in use`), from the literal when one is installed or imposed (*"Updating TreeDB schema in __system__"*); the ids in `leftovers` are nobody's draft on any path (not in `draft_changed`, never in `withdrawn_at_open`) as long as they stay as the projection left them (`leftover_nodes`), and such a leftover stays one at every retry; `unfinished_projection` in `treedbs` and `saved-schema` lists `not_removed` and `not_written` (and `planned` after a process died half way); `save-schema` refuses. A projection that succeeds removes the record. An operator's draft is reported ONCE, by the open that REPLACES it in `__system__`, made before the failure or after it: a part of a draft that a projection cannot replace (a refused delete, a failed write) is not a leftover, it stays a draft (`draft_changed` shows it), its topic is not reported at that open, and the record keeps its kind in `draft_kinds`: the open that replaces it reports that kind, so a SAVED draft is `"saved"` although the first open already withdrew the saved schema (that open reports `saved_schema_version` and no topic). For example, with `departments.budget` added by the operator, a literal that drops `departments` and a snapshot that holds it: the open answers `draft_changed: {"departments": true}` and `withdrawn_at_open: {}`, the record's `leftovers` name `departments`, `.id` and `.name` but not `.budget`, and the open after the snapshot is deleted reports `"topics": {"departments": "unsaved"}`. An EDIT of a leftover IS a draft: a node at a leftover id that is not what `leftover_nodes` kept (an attribute a projection writes changed, its place changed -- a column moved to another topic, linked to one more, or left in no topic --, the node deleted, or created where the projection left nothing) is the operator's work, wherever the node is now; a change of `_geometry` or of the metadata is not. A MOVE is a draft of both topics: the leftover column `treedb_x.departments.name` moved to `users` answers `draft_changed: {"departments": true, "users": true}`, and the open that removes `departments` reports `{"topics": {"departments": "unsaved", "users": "unsaved"}}`. `saved-schema` shows it in `draft_changed`, a retry that cannot finish keeps it a draft (not a leftover in the new record, its kind in `draft_kinds`), and the open that replaces it reports it (the WARNING and `withdrawn_at_open`, `"unsaved"`, or `"saved"` when a pending save carries it). For example, with `departments` a leftover, a new header `"Operator name"` on `treedb_x.departments.name` answers `draft_changed: {"departments": true}`, and the open that removes `departments` reports `{"topics": {"departments": "unsaved"}}`; the same for an attribute of the topic itself (`main_topic`), and for `treedb_x.departments.name` unlinked from `departments` (the open that removes the topic removes the column too). A record without `leftover_nodes` takes every leftover as left, and so does a record whose `system_schema_version` is not the running meta-schema, with a WARNING (*"Leftovers of an unfinished projection were kept under another meta-schema: every leftover is taken as left, an edit of one made meanwhile is not told apart"*). Which treedb a node of `__system__` belongs to is read from the node -- a topic's id is its treedb's name and its `value`, a column's id is its topic's id and its `value` -- never from the start of its id: the treedbs `m2` and `m2.b` never touch each other's nodes (`m2.b.c` is the topic `c` of `m2.b`, not a topic `b.c` of `m2`), and `delete-treedb` deletes only the nodes of its treedb. A column whose topic node is gone and that more than one treedb could own is owned by the one that SAYS it (the record of its unfinished projection names the id, or else its literal, schema file or saved schema declares the topic and the column), and by the treedb projected now when none says it; only the treedb that takes it logs, ONE WARNING per node, *"Node of __system__ that no tree reaches and more than one treedb could own: taken by this treedb"* (`how`: `record`, `schema` or `none says it`; `candidates`); the others say nothing. An open reads of `__system__` only the treedb nodes and the topics and columns whose id starts with the name of the treedb and a dot (a node of another treedb that it needs, for example a column the operator moved into one of its topics, is read when it is needed): the cost of an open does not grow with the other treedbs of the store (with 40 treedbs of 200 columns, the open of one reads its 220 topics and columns, not 8 800). **A schema whose elements get one id is refused**: a name with a dot can give two elements the same qualified id (the column `x.y` of the topic `u` and the column `y` of the topic `u.x` are both `<treedb>.u.x.y`; the topic `b.c` of `m2` and the topic `c` of `m2.b` are both `m2.b.c`), and such a treedb does not open (`-1` *"cannot open treedb '<name>': no valid treedb_schema (see the log)"*), a draft that collides is not saved and a saved schema that collides is not applied, each with ONE ERROR naming both, for example `{"msg": "Schema refused: two elements have the same qualified id in __system__ (a name with a dot), rename one of them", "id": "m2.b.c", "first": "topic 'c' of treedb 'm2.b'", "second": "a topic of treedb 'm2', a node of __system__"}` (in 7.25.4 the two were one node with no error, or the second treedb opened and logged *"Node already exists"* for its columns). No id changes, and no schema of the SDK or the projects has a dot in a name. What the operator linked differently is replaced in ONE open: a node the schema declares that is linked elsewhere (a column moved to another topic) is taken where the schema declares it, never created again (*"Node already exists"*), and a column linked to a topic that does not declare it is only UNLINKED from it when the schema declares it elsewhere or it belongs to another treedb (a topic of another treedb linked here is unlinked too, INFO *"Topic of another treedb, not declared by the schema from C: unlinked from the treedb in __system__"*); for example a column `departments.name` the operator moved to `users` is reported as `{"topics": {"departments": "unsaved", "users": "unsaved"}}`. A topic whose write or link fails leaves what it holds untouched: its columns are not written under a topic no tree reaches, and nodes the operator left for it are not deleted. A topic or column of the treedb that no tree reaches (unlinked by the operator, or by a link of a projection that failed) never blocks a projection: one the schema declares is TAKEN (written as the schema says and linked again), the rest is removed (INFO *"Node of the treedb that its tree does not reach: removed from __system__"*); when it is the operator's work its topic is reported as `"unsaved"` (no save carries a node in no topic), for example `{"topics": {"users": "unsaved"}}` for a column `treedb_x.users.email` the operator created and linked to nothing, taken by a literal that declares `users.email`. A record that cannot be WRITTEN (the disk refuses `saved_schemas/`) does not make the projection read as complete: the record is kept in memory and written again at every open (INFO *"Record of an unfinished projection written, the disk refused it before"*), and the node says it, `c_schema_version: -1`; after a restart the record is LOST, and says so, ONE WARNING at every read (*"Record of an unfinished projection is lost ..."*), `unfinished_projection: ["<treedb>"]`, `save-schema` refuses, the open retries, and what `__system__` holds over the file is taken for a draft (reported, never deleted in silence). A record that cannot be READ still means unfinished: ONE WARNING at every read (*"Record of an unfinished projection cannot be read: ..."*, the cause in `error`) and no other line, `unfinished_projection: ["<treedb>"]`, `save-schema` refuses, the next open retries and writes it again; its leftovers are unknown, so what `__system__` holds over the file is taken for a draft and reported by the open that replaces it, as `"unsaved"` (the kinds it kept are lost with it). A projection that was never STAMPED -- the treedb's node at `schema_version` 0, no record, a file in use at `schema_version` 1 or more -- is a seed that died half way: the next open completes it on every path. When the file runs, the INFO is *"Completing the projection into __system__, left unfinished by an earlier open"* with `why`, for example `{"treedb_name": "treedb_x", "why": "never stamped (schema_version 0), no record", "source": "schema from C", "schema_version": 1, "stored_version": 0}`. When a newer literal is installed or the literal is imposed, the INFO is *"Updating TreeDB schema in __system__"*, with no `why`, for example `{"treedb_name": "treedb_x", "schema_version": 2, "stored_version": 0, "in_use_version": 1}`. Nothing in it is a draft and nothing is reported. A node that says the literal (`schema_version` and `c_schema_version` equal to it) while `__system__` differs from the literal, with no file or a file behind it, with `impose_c_schema` or without it, is a projection STAMPED FIRST by 7.25.4 or earlier (which wrote the numbers, then each topic, then its columns) and a crash: the open completes it (INFO *"Completing the projection into __system__: it says it is of the schema from C, and part of it is not ..."*), and only a NODE (a topic or a column, never a topic with all its columns) that differs from both the old file and the literal is a draft: with the headers of `users.username` and `users.email` changed by the literal 2 and the process dead between the two writes, nothing is reported (before: `{"users": "unsaved"}`). A node that no tree reaches is the dead projection's when it is what the literal writes there and the old file does not declare it (a create whose link never happened: before, `{"roles": "unsaved"}`). An open that completes it and dies too keeps that literal in its record (`stamped_base`), and the open after it compares with both again. With no file, only a missing topic or column, or a topic whose `topic_version` is behind, counts: nothing then tells a draft from a gap (in 7.25.4 it was taken as done at every open). A store keyed by rowid (an older meta-schema) moves to qualified ids in a way that can die at any write: the next open completes the move (until this release a qualified copy left half way failed the next move, *"Node already exists"*). Delete the snapshot (`delete-node` of its row in `__snaps__` of `treedb_system_schema`), or fix the cause of the ERROR, and open again. A literal that is not higher is not installed: the file runs and `__system__` keeps what it holds. Either way `__system__` is not read at open: it is where a schema is edited, and the MASTER projects into it what runs; a replica writes nothing and reads what the master wrote. With it on, `__system__` is re-made from the literal (whole) only when it has no projection of that treedb or a lower `schema_version`. It is configuration and the DEFAULT of every treedb: set it in the yuno's `main.c` (`'global': {'treedbs.impose_c_schema': false}`) or in its config file; `dynamic_schema_treedbs` names the treedbs that open from their file whatever it says. No command changes it: it is read when a treedb opens, and a treedb opens when its yuno starts. See [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §3.11. |
| `dynamic_schema_treedbs` | `list` | `SDF_RD`, default `[]`, **not persistent**. The treedbs that open from their schema FILE whatever `impose_c_schema` says, so their schema can change dynamically (`save-schema` + `apply-schema`). Configuration: `'global': {'treedbs.dynamic_schema_treedbs': ['treedb_wattyzer']}` in the yuno's `main.c`. The yuno's code still wins (`open-treedb impose_c_schema=1`). |

### Commands

| Command | Description |
|---------|-------------|
| `open-treedb` / `close-treedb` | Open or close a treedb instance. `open-treedb impose_c_schema=1`, passed by the yuno's code, imposes the schema from C whatever the attribute says. `open-treedb` of a treedb that is already open here is refused BEFORE anything is touched: `-1` *"<role^name>: treedb '<name>' is already open here: close-treedb first, nothing was changed"* (new after 7.25.4; 7.25.4 reconciled `__system__` first, with creates and updates only, and then failed with *"Internal error, tranger client NULL"*). Every answer of `open-treedb`, `close-treedb` and `delete-treedb` names the yuno, the refusals of their parameters too (*"<role^name>: what treedb_name?"*, *"<role^name>: what filename_mask?"*; 7.25.4: *"What treedb_name?"*, *"Treedb closed!"*, *"Treedb_name not found: '<name>'"*). `open-treedb` answers `0` *"<role^name>: treedb opened: '<name>'"*; `-1` *"<role^name>: treedb '<name>' did not open, its schema was refused (see the log): close-treedb it before opening it again"* when `treedb_open_db()` refuses it (7.25.4 answered `0` *"Treedb opened!"*); `-1` *"<role^name>: cannot open treedb '<name>': no valid treedb_schema (see the log)"* when the schema is refused before (7.25.4: *"treedb_schema not found"*); and `-1` *"<role^name>: cannot open treedb '<name>': its tranger could not be created (see the log)"* (7.25.4: *"Internal error, tranger client NULL"*). After an open that did not open, its services stay until `close-treedb`, the next `open-treedb` answers `-1` *"<role^name>: treedb '<name>' did not open at its last open-treedb, its schema was refused (see the log): close-treedb it before opening it again, nothing was changed"*, and its `treedbs` row says `"opened": false`. The failed open logs its cause once (a schema file with no topics: ERROR *"No topics found"*), and the `close-treedb` that follows logs nothing: `C_NODE` neither sets the callback of a treedb that did not open nor closes it (7.25.4 logged *"TreeDB not found"* at the open and twice at the close, with stacks). `close-treedb` answers `0` *"<role^name>: treedb closed: '<name>'"*. A yuno that treats a `-1` of `open-treedb` as fatal stops there now: the agent logs it with `LOG_OPT_EXIT_ZERO`, exits `0`, and its `ydaemon` watcher does not relaunch it (7.25.4 answered `0` and the agent ran without its treedb). The treedb's tranger is created before the schema is decided: when another process holds its store, the treedb opens as a replica, runs its schema file, and `__system__` is not reconciled (INFO *"The store of the treedb is not written here..."*). `close-treedb` acts only on a treedb that THIS service opened, never on its own `__system__` treedb, whatever `force` says: `command-yuno id=<id> service=treedbs command=close-treedb treedb_name=<name> force=1`. |
| `delete-treedb` | Delete a treedb's SCHEMA: its projection in `__system__`, EVERY `treedbs` / `topics` / `cols` node OF that treedb -- a column the operator moved to another of its topics and a node of it that no tree reaches included (in 7.25.4 a node that no tree reaches stayed) -- while a node of another treedb that somebody linked into it is only unlinked. It never touches the treedb's own store on disk. `force=1` is required and means "yes, delete the schema". It refuses the system schema by name, and it refuses a treedb that is OPEN, and `force` does not lift that: an open treedb keeps answering from the copy in memory while its schema no longer exists anywhere, and the next `open-treedb` dies on the C_TRANGER service still alive under its name. Close it first (`close-treedb`, with `force=1` while the yuno plays, or `pause-yuno` + `play-yuno`): `command-yuno id=<id> service=treedbs command=delete-treedb treedb_name=<name> force=1`. A treedb whose last `open-treedb` did not open it is refused the same way, with `-1` *"<role^name>: cannot delete the schema of '<name>': its last open-treedb did not open it, and its services are still there. close-treedb it first, then delete-treedb"* (7.25.4 said *"while it is OPEN"*). It writes `__system__`, so it asks what `__system__`'s tranger IS: a master that lost its lock answers *"READ-ONLY"* (new after 7.25.4; 7.25.4 asked the `master` attribute and went on). It removes the treedb's saved schema from `saved_schemas/` too (new after 7.25.4; in 7.25.4 it stayed, and a treedb created again under the name found it), and the record of an apply not opened yet (`<treedb>.applied.json`), and the record of an unfinished projection (`<treedb>.unfinished.json`). It deletes the columns first, then their topics, then the nodes of the treedb that no tree reaches, and the `treedbs` node LAST, so a `delete-treedb` cut half way is finished by the next one: it answers `0` *"<role^name>: schema of '<name>' deleted, 5 nodes of __system__"* with the ids in `data.deleted`, for example `{"treedb_name": "treedb_foo", "deleted": ["treedb_foo.users.id", "treedb_foo.users", "treedb_foo"]}`. When the `treedbs` node is already gone (7.25.4 deleted it FIRST, and a delete cut after that answered `-1` *"not projected in __system__"* at every run), the nodes left in no tree are still the treedb's, read from the nodes, and they go. Nothing left answers `0` *"<role^name>: nothing of the schema of '<name>' was in __system__"*. A node that refuses the delete answers `-1`, keeps the `treedbs` node, and lists in `data.deleted` what went. |
| `create-topic` / `delete-topic` | Manage topics within a treedb that THIS service opened (not `__system__`): `command-yuno id=<id> service=treedbs command=delete-topic treedb_name=<name> topic_name=<topic>`. The answers name the yuno, the topic and the treedb: `0` *"<role^name>: topic 'frames' created in treedb 'treedb_x'"*, `0` *"<role^name>: topic 'frames' deleted from treedb 'treedb_x'"*, `-1` *"<role^name>: cannot create topic 'frames' in treedb 'treedb_x' (see the log)"*, and `-1` *"<role^name>: treedb 'treedb_x' not found"* (7.25.4: *"Topic created!"*, *"Topic deleted!"*, *"Treedb_name not found: 'treedb_x'"*). |
| `diff-schema` | What the `__system__` projection of a treedb says that its schema from C does not. |
| (every write) | Asks what the tranger it writes IS, not the `master` attribute. A replica, or a master that lost its lock, answers `-1` *"<role^name>: treedb '<name>' is READ-ONLY, this yuno is not the master of its tranger"*. Between a stop of the service and its next start the tranger holds no lock, and the answer is `-1` *"<role^name>: treedb '<name>' is STOPPED: its tranger holds no lock until its service starts again"* (new after 7.25.4; in 7.25.4 `save-schema` and `delete-treedb` read the stale `master` of the stop and went on to a `__system__` whose treedb was closed, and `save-schema` answered *"nothing to save"*). Applies to `save-schema`, `delete-treedb`, `create-topic`, `delete-topic` and `apply-schema`. |
| (every command) | Asks a permission, except `help` and `authzs`: `open-close` (`open-treedb`, `close-treedb`, `delete-treedb`), `create-delete` (`create-topic`, `delete-topic`, `apply-schema`), `write` (`save-schema`), `read` (`diff-schema`, `treedbs`, `saved-schema`). The test walks C_TREEDB's command table (`tests/c/c_treedb_system_schema`), so a command added without one fails it. On a replica every write answers *"READ-ONLY"*. Every answer of every command starts with the yuno, a refused permission too: `-403` *"<role^name>: No permission to 'read' in service 'treedbs'"* (7.25.4 had no yuno in `create-topic`, `delete-topic`, `treedbs`, `save-schema`, `saved-schema`, `apply-schema` and `diff-schema`). |
| `treedbs` | The treedbs this service opened (`treedb_system_schema` first), one row each: `opened` (`false` when the last `open-treedb` created its services but `treedb_open_db()` refused the schema: `close-treedb` it), `impose_c_schema` as it applies to that treedb, `decided_by` (`code`, `dynamic_schema_treedbs`, `impose_c_schema` or `system`), the `c_schema_version`, `in_use_schema_version` and `saved_schema_version`, and `master`: whether that treedb's tranger can write NOW (new after 7.25.4; a master that lost its lock reads `false`), and `stopped`: whether that tranger is stopped (new after 7.25.4), and `withdrawn_at_open`: what the last open of that treedb withdrew (`{}` when nothing; see `saved-schema`), and `unfinished_projection`: the ids of `__system__` that the projection could not remove or write, as its record says, and every id it was writing when its process died half way (`[]` when the projection is complete; see `impose_c_schema`). A STOPPED tranger -- the service was stopped and has not started again -- gave its lock back at the stop and holds none: it reads `master: false, stopped: true`, although its json still says the `master` it was until the next start revives it. Needs `read`. Example row: `{"treedb_name": "treedb_authzs", "opened": true, "impose_c_schema": true, "decided_by": "impose_c_schema", "c_schema_version": 19, "in_use_schema_version": 19, "saved_schema_version": 0, "master": true, "stopped": false, "withdrawn_at_open": {}, "unfinished_projection": []}`. |
| `save-schema` | Publish the draft of a schema edited in `__system__`: every topic that differs from the schema file IN USE gets its `topic_version` + 1, the treedb its `schema_version` + 1, and the schema is written to `saved_schemas/<treedb>.treedb_schema.json` under the `__system__` tranger, never over the file in use. `dry_run=1` answers it and writes nothing. Master only; permission `write`. `command-yuno id=<id> service=treedbs command=save-schema treedb_name=<name>`. With no `treedb_name` (this and the next two) it acts on every treedb opened there and lists the answers. The file in use is read whatever shape its `topics` have, a list or a dict keyed by name (what a node opened with `impose_c_schema` off wrote before 7.25.0): the same schema gives the same diff and publishes the same versions. It writes `__system__`, so it asks whether `__system__` is the master's (it asked the treedb's tranger). A column's `default: {}` is not written, on any column: it is the meta-schema's placeholder for "no default", and kept on a `required` column it filled the field, so `required` never refused a record (as in 7.25.4). The trade-off: a `required` column whose literal really declares `'default': {}` loses it through save + apply, and a record created without that field is refused with *"Field required: '<col>'"*. **A draft that is the file in use again withdraws the saved schema**: after "edit, save, undo the edit", a saved schema newer than the file in use is removed (logged *"Saved schema withdrawn, the draft is the schema in use"*), so `saved-schema` answers `can_apply: false` and Apply cannot install what was taken back (new after 7.25.4; in 7.25.4 the save answered "nothing to save" and left it). Both "nothing to save" answers carry `data: {treedb_name, withdrawn, schema_version, path, changes}`, for example `0: <role^name>: the draft of 'treedb_x' is the schema in use: the saved schema_version 13 is withdrawn` with `withdrawn: true`; `dry_run=1` says "would be withdrawn" and removes nothing. **Refused while the projection is unfinished** (`unfinished_projection` not empty): `-1` *"<role^name>: the projection of 'treedb_x' into __system__ is not complete, 2 node(s) could not be removed or written (see the log): a save would publish them"*, with `data: {"treedb_name": "treedb_x", "unfinished_projection": ["treedb_x.departments", "treedb_x.users.departments"]}`. **Refused for a draft whose elements get one id** in `__system__` (a name with a dot, see `impose_c_schema`): ONE ERROR naming both and `-1` *"<role^name>: cannot save the schema of 'treedb_x': topic 'u.x' of treedb 'treedb_x' and column 'x' of topic 'u' of treedb 'treedb_x' get the same id 'treedb_x.u.x' in __system__, rename one of them"*, with `data: {treedb_name, changes, collision}`. **Refused for a draft with no topics** (a treedb without topics does not open): a WARNING *"Draft of a treedb schema with no topics: not saved, a treedb without topics does not open"* and `-1` *"<role^name>: cannot save the schema of 'treedb_x': its draft in __system__ has no topics, and a treedb without topics does not open"*. |
| `saved-schema` | What `save-schema` wrote, what it changes against the file in use (`diff`: `added` / `removed` / `changed`, one row per `json2flat` leaf, the topics and columns keyed by name; their ORDER is a leaf of its own, `__topics_order__` and `topics`<topic>`__cols_order__`, the names joined by `, `, so a moved column reads `"topics`users`__cols_order__": {"from": "id, username, email, departments", "to": "id, username, departments, email"}` -- new after 7.25.4; in 7.25.4 a save that only moved a column showed its versions raised and nothing else; an order leaf is a difference only when the names both sides declare come in another order, so an added or removed column is an `added` / `removed` leaf and not a moved one), `impose_c_schema` for that treedb (the code's force included) and `can_apply`. `saved` is `true` only for a save still PENDING (newer than the file in use); a saved file that is not newer answers `stale: true`, no `diff` and `can_apply: false` (new after 7.25.4; 7.25.4 answered `saved: true` with the diff of a schema already in use), for example `{"saved": false, "stale": true, "can_apply": false, "in_use_schema_version": 14, "saved_schema_version": 13, "diff": {}}`. A saved file that cannot be READ answers `broken: true` (not `stale`), `can_apply: false`, and a comment *"the saved schema of '<treedb>' cannot be read (see the log): it is left out of apply-schema, save again to replace it"* (7.25.4 answered `saved: false`, with nothing to say why). A saved schema is withdrawn when an open writes the literal over the file it was saved against (no file in use, a newer literal, or an imposed one). **`withdrawn_at_open`** says what the last open of the treedb withdrew (new after 7.25.4; in 7.25.4 the log only): `{"schema_version": 22, "saved_schema_version": 21, "topics": {"users": "saved", "departments": "unsaved"}}` -- the literal that did it, the saved schema it withdrew (`0`: none), and each topic of the operator's work the literal replaced or removed: `"applied"` (an apply that never ran, as `apply-schema` recorded it), `"in_use"` (an apply that an open DID read: the dynamic schema the treedb was running; new after 7.25.4), `"saved"` or `"unsaved"` (a draft in `__system__`, a topic the operator DELETED from `__system__` included: `"saved"` when the pending saved schema does not declare it either, for example `{"topics": {"departments": "unsaved"}}` when a newer literal re-creates a topic the operator deleted and never saved; a topic the operator ADDED is `"saved"` only when the pending saved schema declares it, so `groups` added after a save of `users` is `{"users": "saved", "groups": "unsaved"}`); `{}` when nothing. Only the topics an apply changed are `"applied"` / `"in_use"`: a topic that only the developer changed is nobody's work. What an unfinished projection left (its record's `leftovers`), as it left it, is not a draft either, and is never withdrawn, on any path: it is in `unfinished_projection`. An edit of it is a draft (see `impose_c_schema`). The same open logs ONE warning with the same content, *"Schema from C withdrew work on the schema at open"* (`treedb_name`, `schema_version`, `in_use_version`, `saved_schema_version`, `topics`). A save taken back replaces nothing. Permission `read`. `draft_changed` names the topics whose draft in `__system__` is NOT SAVED: it is diffed against the saved schema when there is one newer than the file in use, and against the file in use otherwise, without what an unfinished projection left -- what the schema editor marks as unsaved, rebuilt from it after a reload. Until 7.25.3 it was always the file in use, so a topic saved a moment ago read as unsaved until an Apply. Example after a save: `{"draft_changed": {}, "can_apply": true}`. A pending saved schema with no topics answers `can_apply: false` and the comment *"the saved schema of '<treedb>' has no topics: apply-schema refuses it, a treedb without topics does not open"*. |
| `apply-schema` | Put the saved schema in place of the file in use: master only, only when C does not impose that treedb's schema, and only a saved `schema_version` higher than the one in use. It takes effect at the next open of the treedb (restart its yuno). Permission `create-delete`, like `create-topic` and `delete-topic`: it replaces the schema of a treedb whole (`save-schema` asks `write`; in 7.25.3 the two were the other way round). `command-yuno id=<id> service=treedbs command=apply-schema treedb_name=<name>`. The answer carries `data: {treedb_name, applied, saved_schema_version, in_use_schema_version}`; `applied` is `true` only when the file in use was replaced, and it is what says to restart. The file is written to a flushed temporary and renamed over the one in use (mode `rpermission`, 0660), never truncated in place. With no `treedb_name` it is **all or none up to the renames**: every treedb with something to apply is checked and written to its temporary first, and one that fails there leaves every file in use as it was (`-1`, every row `applied: false`). The renames that follow are one by one, and a rename that fails fails alone: its row `applied: false`, the others `true`, the answer `-1` with *"N of M treedb(s) applied, see each one"* -- read the rows, not the result. Each row is `{treedb_name, result, comment, data}` with that same `data`, and an apply with nothing to apply answers `0` and no row. A treedb whose saved file cannot be READ is not part of the all or none: it is left out, the others are applied, its row goes last with `applied: false, broken: true`, and the answer is `-1` with *"...; left out, their saved schema cannot be read: <treedb>"* (new after 7.25.4; 7.25.4 refused every treedb). The file is written without the derived `fkey` marks of `parse_schema()` (new after 7.25.4; in 7.25.4 it carried them). Once in place the saved schema is removed from `saved_schemas/`: it IS the file in use (new after 7.25.4; in 7.25.4 it stayed, and read as saved). **A newer literal withdraws an apply that never ran, and says so.** `apply-schema` then `upgrade-yunos` with a literal whose `schema_version` is higher than the applied file: the literal is written over the whole file, as any newer literal is, and every applied topic it says otherwise (or removes) is reported as `"applied"` in `withdrawn_at_open` and in the warning (until 7.25.4 it was lost with no word). A saved schema whose elements get one id in `__system__` (a name with a dot) is refused like one that does not parse: ONE ERROR naming both, and `-1` *"<role^name>: the saved schema of 'treedb_x' is refused: ... get the same id '<id>' in __system__, rename one of them"*, the file in use unchanged. The apply records what it put in use in `saved_schemas/<treedb>.applied.json` (`{"schema_version": 13, "topics": {"users": "applied"}}`: the topics whose `topic_version` it raised), written WHOLE (a `.new` file created `O_EXCL|O_NOFOLLOW`, flushed, renamed over the old one), BEFORE the file is renamed in place: a record that cannot be written refuses the apply with the file in use unchanged, and a rename that fails puts the previous record back. The record lives while that file is in use: the open that reads the apply marks its topics `"in_use"`, and a newer literal reports them and removes it -- both only once the open has OPENED (an open that fails leaves the record as it was). An applied topic is never guessed from the store: a topic whose store directory is gone is no apply. A saved schema with no topics is refused, like one that does not parse: a WARNING *"Saved treedb schema with no topics: not applied, a treedb without topics does not open"* and `-1` *"<role^name>: the saved schema of 'treedb_x' has no topics: a treedb without topics does not open"*, the file in use unchanged. To keep the operator's change, take it into the literal. See [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §3.11. |

---

(gclass-c-node)=
## C_NODE

Node resource interface for TreeDB — full CRUD and graph operations on
tree nodes with linking, snapshots, and import/export.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `tranger` | `pointer` | The tranger the treedb lives on. Set by the host. |
| `treedb_name` | `string` | Treedb name. |
| `treedb_schema` | `json` | The schema, projected into `__system__` and opened from there. |
| `initial_load` | `json` | Seed records, created if missing and marked immutable. |
| `with_link_events` | `bool` | A link/unlink publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED` instead of the parent's `EV_TREEDB_NODE_UPDATED`. Set by `C_TREEDB` at open; changed at run time with `set-link-events`. Written while the treedb is not open (before its open, or after an open that failed), it changes nothing then and logs nothing: the open reads it. |
| `impose_c_schema` | `bool` | Open with `treedb_schema` over a newer schema on disk too (`treedb_open_db()` option `"impose"`). Set by `C_TREEDB`. |
| `files_max_size` | `integer` | Largest file a `file` column accepts. Default 128 MB. A **memory** limit as much as a policy one — see *File columns* below. |
| `files_content_types` | `json` | Mime types a `file` column may hold, checked on the **bytes**. The default carries images, PDF, video and audio; `image/svg+xml` is **not** in it on purpose (an SVG served from the app's own origin runs script). A column narrows the list, never widens it. |
| `import_root` | `string` | Root that `import-assets` is confined to. Empty: `import-assets` is refused. |

### Commands

| Command | Description |
|---------|-------------|
| `create-node` / `update-node` / `delete-node` | CRUD operations on nodes. A record with `file` columns carries its bytes **beside** the record, in `__files__` — see below. `delete-node` takes two options that are NOT the same thing: `force` unlinks the children, and `ignore_snaps` deletes a node a snapshot still holds (and so breaks that snap's rollback). Since after 7.24.1 `force` no longer implies `ignore_snaps`. `ignore_snaps` erases records a snapshot froze, so it asks `create` (what `shoot-snap` asks) besides `delete` (after 7.25.3). Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=delete-node topic_name=items record={"id":"item1"} options={"force":1}'`. The comments name the yuno (new after 7.25.4): `<role^name>: Node update! 'item1' of topic 'items'`, `<role^name>: Node deleted, 'item1' of topic 'items'`. |
| `import-assets` | Turn a directory already on this node into N assets of `__assets__`: one command, no bytes on the wire. Confined to `import_root`. It creates index nodes, links nothing, and **answers the map `path -> id`** so the loader can link what it imported. `dry_run=1` says what it would take. The confinement is resolved, not only spelled: a `source_dir` with `..`, or one that resolves out of `import_root` through a symlink, is refused. |
| `gc-assets` | Delete the assets that **no live node and no snapshot** links — row and bytes. Never automatic: `delete-node force=1` unlinks children rather than deleting them, so an unlinked asset is a normal intermediate state of a bulk operation. `dry_run=1` lists what it would take. It sweeps too the blobs of the store that NO row names (what an interrupted write leaves). **Its `data` is the report of `treedb_gc_files2()`** (new after 7.25.4; BREAKING: 7.25.4 answered the list of ids taken): `{"dry_run": false, "assets": ["<id>", ...], "blobs": ["<id>", ...]}`, answered `0` with *"<role^name>: deleted 1 orphan asset(s) and 0 blob(s) no row names"*. **A refused gc still says what it swept**: while a snap is active in any treedb of the tranger (the nodes in memory are the snap's photo, not the live links), or when what the snapshots hold cannot be read whole, or when a topic that links assets (or `__assets__`) did not load whole, the asset rows are left untouched and the blobs no row names are swept all the same (no link nor snapshot can lead to them). The answer is `-1`, `data.refused` carries the reason, `data.assets` is `[]`, `data.blobs` lists what was taken: *"<role^name>: gc refused: a snap is active, the nodes in memory are its photo and not the live links (deactivate it first); the asset rows are untouched, deleted 1 blob(s) no row names"*. When the blobs cannot be swept either, `data.blobs_refused` says why. `command-yuno id=<id> service=<treedb> command=gc-assets dry_run=1` |
| `node` / `nodes` | Retrieve one node / list a topic's nodes (with filters). |
| `instances` | List node instances. |
| `link-nodes` / `unlink-nodes` | Manage parent-child relationships: `command-yuno id=<id> service=<treedb> command=link-nodes parent_ref=items^item00^children child_ref=items^item01` answers `0: <role^name>: Nodes linked, 'items^item01' to 'items^item00^children'`, and a refused one `-1: <role^name>: cannot link 'items^item01' to 'items^item00^children' (see the log)`. On a REPLICA both answer `-1` READ-ONLY, **also for a pair that is already linked** (new after 7.25.4; in 7.25.4 the treedb found the pair linked, wrote nothing and answered `0`): on a replica every write is refused, one that would change nothing included, because whether it would is read from the replica's memory, which lags the master's disk. A C caller of `gobj_link_nodes()` / `gobj_unlink_nodes()` on a replica gets `-1` and the log line *"Cannot link nodes on a READ-ONLY replica"*. |
| `parents` / `children` | Navigate the graph. |
| `hooks` / `links` | Inspect hook and fkey relationships. |
| `jtree` | Get a node's full subtree as JSON. |
| `shoot-snap` / `activate-snap` / `deactivate-snap` | Snapshot management. `deactivate-snap` answers `-1` when the save of the active snap fails: the snap is still ACTIVE on disk and the next start loads it (after 7.25.3; it answered *"Snap deactivated"*); its success is `<role^name>: Snap deactivated, treedb '<treedb>'`. `activate-snap` answers a snap that does not exist itself, `-1 <role^name>: snap not found: 's9'`, and any other failure as `cannot activate snap 's9' (see the log)` (new after 7.25.4; 7.25.4 quoted the process-wide last error message, whoever wrote it). When the new snap cannot be saved active, the old one is saved active again (*"Cannot activate snap, the one active before is active again"*). |
| `snaps` / `snap-content` | Inspect snapshots. |
| `import-db` / `export-db` | Bulk import/export. `import-db content64=<base64 of {"topic": [record, ...], ...}> if-resource-exists=abort\|skip\|overwrite` creates each record, then links them all (`update-node` with `autolink`). Its `data` counts what happened: `{"added": 1, "ignored": 2, "overwrite": 0, "failure": 0, "abort": 0, "link failure": 0, "errores": {"node exists": 2}}`. **`errores` counts the refused creates by their cause** (new after 7.25.4): `"node exists"`, or `"cannot create the node (see the log)"`. A create refused for another cause than an existing node is a `failure` in every mode, and stops the import in `abort` mode. 7.25.4 keyed `errores` on the process-wide last error message, which names the id (one key per record) or an older, unrelated error. |
| `treedbs` / `topics` | List the treedbs of the tranger / the topics of a treedb. |
| `desc` / `descs` | Describe one topic's schema / every topic's. |
| `set-link-events` | Show (no `set`) or change (`set=1` / `set=0`) which events a link and an unlink publish, on the open treedb and at once: `1` publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED` with the relationship (`hook_name`, `parent_topic_name`, `parent_id`, `child_topic_name`, `child_id`), `0` the parent's `EV_TREEDB_NODE_UPDATED` (what the v1 SPAs read). Either/or for every subscriber of the treedb. Needs the permission `update`. Not persistent: the next start takes the configured `with_link_events` again. Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=set-link-events set=1'`. |
| `print-tranger` | Dump the tranger the treedb lives on as bounded JSON (`kw_collapse()`-truncated: unexpanded containers answer as `[[size]]`, and `lists_limit` and `dicts_limit` bound the expansion). Pass `path=` (backtick-delimited, `kw_find_path` style, arrays by numeric index) to lazily drill into one subtree — this is what feeds the gui_treedb "Raw JSON" viewer. |

**Every comment starts with the yuno that answers** (`<role^name>: `), and a
failure says its own cause and points at the log: `create-node` answers
`<role^name>: Node created, 'item01' of topic 'items'` or `<role^name>: cannot
create the node of topic 'items' (see the log)`; `shoot-snap`, `snap-content`,
`nodes`, `instances`, `parents`, `children`, `jtree`, `desc`, `links`, `hooks`,
`topics` and `import-assets` the same (new after 7.25.4). In 7.25.4 they
answered `gobj_log_last_message()`, the process-global buffer of the last ERROR
of anybody -- a stale or unrelated cause -- and most answers carried no yuno.
The exception is the refusal of a permission, *"No permission to '<perm>' in
service '<treedb>'"*, which names its service. `instances` answers `-1` when it
cannot list (it answered `0`). The test walks C_NODE's command table
(`tests/c/c_node_authz`), so a command added with a bare comment fails it.
The log says its own cause too: an update of a node that does not exist, with
`create` and a create the library refuses, logs the library's error and then
*"Cannot update node: it does not exist and it cannot be created (see the
previous log)"* (new after 7.25.4; 7.25.4 logged the last message again as its
own).

### Permissions

Every command that reads or writes the treedb asks for a permission, inside
the command, with or without the global `enable_command_authz` gate. It
matters only in a yuno with an authz checker (`C_AUTHZ`).

| Permission | Commands |
|---|---|
| `read` | `nodes`, `node`, `instances`, `pkey2s`, `parents`, `children`, `jtree`, `hooks`, `links`, `snaps`, `snap-content`, `print-tranger`, `export-db`, `treedbs`, `treedb-info`, `topics`, `desc`, `descs`, `system-schema`, `schema-file`, `set-link-events` (shown, no `set`) |
| `create` | `create-node`, `import-assets`, `shoot-snap` |
| `update` | `update-node`, `link-nodes`, `unlink-nodes`, `set-link-events` (changed), `trace`, `activate-snap`, `deactivate-snap` |
| `delete` | `delete-node`, `gc-assets` |
| `create` and `update` | `import-db` |
| `delete` and `create` | `delete-node` with `options.ignore_snaps=1` |

Only `help` and `authzs` ask nothing. `system-schema`, `trace` and the
`set-link-events` display answered anyone until after 7.24.1; the test walks
C_NODE's command table (`tests/c/c_node_authz`), so a command added without a
permission fails it.

`update-node` with `options.create=1` also asks for `create` when the node
does not exist yet. A refusal answers `-403`, and nothing is written.

`options.create_only=1` makes a NEW node and refuses one that exists
(*"Node already exists"*); `options.create=1` alone is an upsert, and a taken
id there is an update of the existing record. A table's +New sends
`create_only` (gobj-ui 7.23.194). Both ask for `create` when the node does
not exist: `create_only` alone used to create under `update`.

```
command-yuno id=<id> service=<treedb> command=update-node topic_name=users record='{"id":"bob","username":"Bob"}' options='{"create_only":1}'
```

An `update-node` with `autolink` whose record names a link that cannot be
made saves the record and answers **-1**: *"node 'x' saved, but its links were
NOT changed"*. The fkey column keeps the links it had (see
`treedb_replace_links()`); it used to answer *"Node update!"*. The answer is
the update's own, even when a subscriber of its events updates the same service
before it answers (until 7.25.3 the nested update reset it to a success).

```
ycommand -c 'command-yuno id=<id> service=<treedb> command=delete-node topic_name=items record={"id":"item1"} options={"ignore_snaps":1}'
# -403 No permission to 'create' in service '<treedb>'   (a role with 'delete' only)
```

**A replica writes nothing, whoever asks.** The commands answer *"READ-ONLY"*
before anything is read. From C, every write method refuses before it moves
anything: `gobj_update_node()` returns `NULL` for every update but a `volatil`
one (memory only) -- the door C_AUTHZ uses for a user created from an event,
which no command guards; `gobj_link_nodes()`, `gobj_unlink_nodes()` and
`gobj_delete_node()` return `-1` and log *"Cannot link nodes / unlink nodes /
delete a node on a READ-ONLY replica"* (new after 7.25.4; 7.25.4 moved the
links in memory first and met the refused save last); `gobj_create_node()` is
refused by `treedb_create_node()`; and shooting or activating a snap by the
treedb (*"Only master can ..."*).

```C
/*  On a replica: -1, nothing moved  */
int ret = gobj_delete_node(gobj_node, "items",
    json_pack("{s:s}", "id", "item00"), json_pack("{s:b}", "force", 1), src);
```

**An autolink write that fails at its save changes nothing it wrote.**
`update-node` (or `gobj_update_node()`) with `autolink` writes the fields, the
links and the save as ONE write
([`treedb_update_node_and_links()`](#treedb_update_node_and_links)). When the
append fails (a full disk, a store gone), it answers `NULL` / `-1`. The fields
and the links go back in memory to what the disk has, and none of the events
of the write is told (`EV_TREEDB_NODE_LINKED`, `EV_TREEDB_NODE_UPDATED`):

- **create + autolink**: the create is on disk, with its ordinary fkeys EMPTY
  (a create stores none: only its `file` columns, which it links itself, and,
  for a secondary instance, the links it inherits from the primary). Its links
  are taken back, so memory and disk both have the node without them. The
  create was told (`EV_TREEDB_NODE_CREATED`), and an ERROR says what is left:
  *"Node created, but its links cannot be saved (autolink): the node stays
  without them"*.
- **update + autolink**: the node, its fields and its links are as the disk
  has them, in memory and on disk. One exception: the bytes of a `file`
  column of the record are stored BEFORE the write opens, as a write of their
  own. The blob and its `__assets__` node stay on disk, and their
  `EV_TREEDB_NODE_CREATED` (or `EV_TREEDB_NODE_UPDATED`, for a new name of
  bytes already stored) is told. The node does not link them.

In 7.25.4 this write was three calls, and a failed save took nothing back:
memory kept the fields and the links until the next reload, and
`EV_TREEDB_NODE_LINKED` had been told. Write it again once the cause is fixed:
the same `update-node` with `autolink` converges.

```C
/*  the store of `users` cannot be written (a full disk)  */
json_t *node = gobj_update_node(gobj_node, "users",
    json_pack("{s:s, s:s, s:[s]}",
        "id", "alice",
        "username", "ALICE-NEW",
        "departments", "departments^direction^users"
    ),
    json_pack("{s:b}", "autolink", 1),
    src
);
/*  node == NULL; alice keeps her old username and links, in memory as on
 *  disk; direction does not hook her; no event was published            */
```

`EV_TREEDB_UPDATE_NODE` is an input event for gobjs of the SAME yuno
(`gobj_send_event()`), with no permission asked. It is not a public event since
after 7.25.3: `C_IEVENT_CLI` delivered a public event from its peer to the
local service the event names, so the other end of an outbound session could
write the treedb.

Example: a user whose only role on `treedb_devices` has `"permission": "read"`
gets `nodes` and `node`, and is refused `link-nodes`:

```bash
ycommand -c 'command-yuno id=<yuno> service=treedb_devices command=link-nodes parent_ref=places^p1^devices child_ref=devices^d1'
# -403 No permission to 'update' in service 'treedb_devices'
```

(treedb-file-columns)=
### File columns: `__assets__`

A treedb node often owns something that is not JSON — a photo, a plan, a
signed pdf. Those bytes cannot go *in* the treedb: it is held in memory and
timeranger2 rewrites the whole record on every update, so a 40 KB photo would
be rewritten every time its node changed state and would ride along in every
page of `nodes`. Measured on one census: 12 134 blobs, 346 MB on disk, would
be ~460 MB of RAM for the life of the yuno.

So **you mark a column `file`, and treedb gives you a pseudo-filesystem**: you
hand it a file, you get it back; the *index* lives in memory and the *content*
on disk. Design note:
[`DESIGN-treedb-files.md`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/DESIGN-treedb-files.md).

```
'foto': {'header': 'Photo', 'type': 'string', 'flag': ['fkey', 'file']}
```

- **The column is an fkey into `__assets__`**, a system topic every treedb
  creates at open next to `__snaps__` and `__graphs__` (shown in system mode
  only). Its rows are the index entries: `id` (the **sha256 of the content**),
  `content_type`, `size`, `t`, `original_name` (the only writable one),
  `uploaded_by`. Its **hooks are derived**: for every column `C` of topic `T`
  flagged `file`, `__assets__` gains `as_<T>_<C> -> {T: C}` in memory, never
  written to `topic_cols.json`. The host declares nothing but the column, and
  nothing about `__assets__` is ever versioned. The derivation **follows the
  schema at run time**: `create-topic` and `delete-topic` are live commands,
  so a topic added while the yuno runs gets its hook, and one deleted takes
  its hook — and the children it held — away with it.
- **The bytes live under the treedb**, at `<treedb dir>/.blobs/ab/cd/<sha256>.<ext>`,
  so a `cp -a` of the treedb directory carries the nodes **and** their bytes.
- **The bytes ride beside the record**, never inside the column, in a
  `__files__` manifest keyed by column, consumed at the door. A browser sends
  `content64`; a C caller (command or `EV_TREEDB_UPDATE_NODE` alike) puts a
  real `gbuffer` in the kw and the manifest says
  which slice is whose (`offset`, `size`) — a kw holds ONE binary field, so one
  buffer carries every file of the record:

```json
{"topic_name": "devices",
 "record": {"id": "E22000041",
            "foto": "",
            "__files__": {"foto": {"content64": "...", "original_name": "E22000041.jpg",
                                   "content_type": "image/jpeg"}}},
 "options": {"create": 1, "autolink": 1}}
```

- **Treedb re-hashes what arrives.** A client may put the sha256 in the column
  up front, ask `node` whether that asset exists and skip the bytes if it does
  (a census reload then sends ~0 instead of 346 MB) — but the id is a claim,
  never an authority: a wrong id with good bytes is refused, and a bare id of
  an asset nobody stored is refused. A bare id of an existing asset links it.
  A column may also arrive holding the **full** reference, and then it must be
  exactly `__assets__^<id>^as_<T>_<C>` of that very column: the link is made
  by the hook the value names, so one naming another `file` column's hook
  would store the file into that other column.
- **Size and type are checked at the door, on the bytes.** The size is checked
  on the base64 before decoding; the type is *sniffed* from the first bytes
  and the declared one must agree — a png called `image/jpeg` is refused, and
  an svg called `image/png` is refused, which is the case the allowlist exists
  for. Two levels: the treedb's ceiling (`files_max_size`,
  `files_content_types`) and the column's policy, in its `properties`
  (`{'max_size': 4096, 'content_types': ['application/pdf']}`), which narrows
  the ceiling and never raises it. The ceiling itself sits **behind** the
  transport's: the message was accepted whole and parsed before treedb saw it,
  so keep `files_max_size` under the transport's `max_pkt_size`.
- **Three writes, in order**: the blob, the `__assets__` node, the host record
  with its link. Interrupted early it leaves an orphan blob or an orphan index
  node, which `gc-assets` takes; never a link to nothing. A `create-node`
  whose `file` column cannot be linked is **undone**, so the answer never
  says yes over a record with an empty column.
- **The write path links the `file` column itself, `autolink` or not.** An
  ordinary fkey moves only through `link-nodes` or an `autolink`; a `file`
  column is edited by handing over a file, and the link is part of it:
  `create-node` and `update-node` link what the column names, `""` unlinks,
  and a column the record does not carry is left alone. The `autolink` in
  the example above is for the OTHER fkeys of the record, not for `foto`.
- **A second arrival of the same bytes is an update of the asset node**, so
  the history of `__assets__` says every name a file arrived under (a manifest
  that carries no `original_name`, or the name already stored, says nothing
  new about the file and appends nothing). The blob
  is written once, and the **first** arrival names it for ever: the extension
  is part of the served path and the URL is cached for ever, so a later
  arrival that declares another member of the same container (`audio/mp4`
  for what was stored as `video/mp4`) keeps the stored `content_type` and
  says so in the log.
- **`gc-assets` reads the snapshots, exactly.** `shoot-snap` skips every `__`
  topic, so an asset node never carries a tag and the tag guard never protects
  it: the collector walks the instances on disk of every topic with a `file`
  column and keeps what an ACTIVATION would load — per key, the newest
  instance under each existing snap's tag, and only that one (a save is
  untagged since 7.24.0, so that instance is the record the snap SHOT, and a
  node that moves on after the shot does not drag the snap's hold along). A snapshot holds bytes alive; **deleting the snap frees them**, and
  the delete is `delete-node` on its `__snaps__` row (there is no `delete-snap`
  command). A treedb with no snapshot does not walk. It also takes the
  **bytes with no row** — what an interrupted write leaves, and the `.tmp` of
  one that never reached its rename — because every other reader of the store
  goes through the rows. `.blobs` is the tranger's, so what counts as named is
  the union of every treedb's `__assets__`.
- **Deleting an asset node deletes its bytes.** Refused while a node links it,
  like any parent with children — and refused, `force` included, while a
  **snapshot** links it: `delete-node` on an `__assets__` row runs the same
  walk `gc-assets` does, because the ordinary tag guard never fires for an
  asset. `force` means "unlink the children", never "ignore what a snapshot
  needs", and no key of `options` skips the walk: the gc skips it through a
  private entry, not through anything the wire can spell. On a tranger that
  hosts more than one treedb, `__assets__` and its bytes are shared: both
  `gc-assets` and `delete-node` read every treedb's links, and an asset
  another treedb of the tranger links is refused, `force` or not.
- **A `file` column must be `['fkey','file']` on a `string`**, and that is
  checked in three places: at open (fatal), by `create-topic` with the yuno
  running (the topic is refused, the answer says why), and by the write path
  (the write is refused). A column that is `file` and not `fkey` would store
  its bytes into a column nothing links, and `gc-assets` would take them.

- ⚠️ **A non-master replica gets the index and not the bytes.** The watcher
  replicates a topic's files, so a replica has every row of `__assets__` and
  none of the `.blobs` behind them: `get-asset` answers *"asset has no bytes
  on disk"* there unless a web server in front is serving a copy of the blob
  directory. Getting the bytes to a replica is not solved — plan for it, or
  serve the assets from the master. The same holds for `export-db` /
  `import-db`: they carry the rows of `__assets__` and not the `.blobs`.

Serving the bytes to a browser is [`C_ASSETS`](#gclass-c-assets)' job.

---

(gclass-c-resource2)=
## C_RESOURCE2

Simple resource persistence — stores each resource as a flat JSON file.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `strict` | `bool` | Enable schema validation. |
| `json_desc` | `json` | Resource schema descriptor. |
| `persistent` | `bool` | Persist to disk. |
| `service` | `string` | Service name. |
| `database` | `string` | Database name. |

---

(gclass-c-assets)=
## C_ASSETS

The way **out** of the bytes a treedb keeps for its `file` columns: a signed
URL a web server checks by itself, or the bytes inline when there is no web
server in front. Storing is treedb's ([File columns](#treedb-file-columns)):
`file` columns, `__assets__`, and the `import-assets` / `gc-assets` commands
of `C_NODE`. This gclass only publishes what is stored, and is one command.

The asset id is the **sha256 of its content**, so a served URL means the
same bytes for ever and can be cached for ever. `get-asset` takes the asset
id and nothing else — not a node plus a column: keyed by node, the same URL
would mean different bytes the moment the node was relinked.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `treedb` | `pointer` | The `C_NODE` that owns the treedb. Set by the host; takes precedence over `treedb_service`. |
| `treedb_service` | `string` | Its service name, when the host did not build it itself. |
| `public_url` | `string` | URL prefix a web server serves the blobs from. Empty: `get-asset` answers inline. |
| `sign_secret` | `string` | Shared secret of the web server's `secure_link_md5`. Empty: `get-asset` answers inline. |
| `url_ttl` | `integer` | Seconds a signed URL stays valid. Default `900`. |

### Commands

| Command | Description |
|---------|-------------|
| `get-asset` | Return a signed URL, or the bytes inline. See below. Takes `asset_id`, **not** `id`. |

Gated by the `read` authz of the service.

**`get-asset` takes `asset_id`, not `id`.** `command-yuno` hands its whole kw
to `gobj_list_nodes()` as the filter that picks the yuno, so a parameter named
like a field of the yuno record becomes a filter on that field: an `id` of a
sha256 matches no yuno and the answer is *"Yuno not found"* — which names the
yuno and never the parameter. The bare `id` still works for a caller that
never crosses the agent.

### Two ways out to a browser, and the service picks

`get-asset` answers in one of two shapes:

```json
{"mode": "url",    "url": "/media/ab/cd/<id>.jpg?e=<expires>&s=<token>"}
{"mode": "inline", "content_type": "image/jpeg", "content64": "..."}
```

It signs a URL when `public_url` **and** `sign_secret` are both configured,
and answers inline when they are not. So the caller has **one** code path,
and a node with no web server in front of it still shows its images instead
of showing nothing.

The signed form reproduces, byte for byte, what this nginx block hashes.
**Do not use `/assets/`**: a Vite SPA on the same vhost already owns that
prefix for its content-hashed bundles, and the two locations would fight
over it. The `alias` is the treedb's own blob directory.

```nginx
location /media/ {
    secure_link      $arg_s,$arg_e;
    secure_link_md5  "$secure_link_expires$uri <sign_secret>";
    if ($secure_link = "")  { return 403; }   # bad signature
    if ($secure_link = "0") { return 410; }   # expired
    alias <store>/<realm>/treedb_<name>/.blobs/;
    expires max;    # the name IS the hash: it can never go stale
    access_log off;
}
```

The client address is deliberately **not** in the signature: it would tie the
URL to one IP and break every phone that changes network mid-session. The
short lifetime is what limits a leaked URL.

`secure_link` needs nginx built `--with-http_secure_link_module`. Yuneta's
nginx and openresty are, but a node still running an older build must serve
inline until its web server is replaced.

`tests/c/c_assets` covers the whole round trip: a record with its bytes
beside it through `update-node`, both doors (`content64` and a `gbuffer` of
two slices), `get-asset` inline and signed, `import-assets` with hostile
paths (`..`, absolute, a symlink out of the root), and `gc-assets`.
