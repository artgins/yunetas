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
`gobj_read_bool_attr(gobj_tranger, "master")` says `FALSE` (after 7.25.4; it
answered the configuration). `open-rt` and `open-list` follow it: a replica's
feed is an `rt_disk`, which the other master's appends reach through the disk.
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
| `delete-topic` | Delete a topic. **Irrecoverable.** A topic that still holds records needs `force=1`, and the refusal says so: `command-yuno id=<id> service=<tranger> command=delete-topic topic_name=frames force=1`. Before 7.20.x (Unreleased) the guard never fired and a topic with records was deleted on the first call. |
| `delete-key` | Delete a whole key (primary key) of a topic and every record it holds. **Irrecoverable and master-only**; the delete propagates to the in-process subscribers and to the `rt_by_disk` followers. A key that still holds records needs `force=1` — the refusal names the record count. A key that is not there is an error, not a silent success. |
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
| `impose_c_schema` | `bool` | `SDF_RD`, default `1`, **not persistent**. Open every treedb with its schema from C, over a newer schema file on disk. `0`: open from the schema FILE (the literal is installed only when it is newer), which `apply-schema` replaces. Either way `__system__` is not read at open: it is where a schema is edited, and the MASTER projects into it when it has no projection of that treedb or a lower `schema_version`; a replica writes nothing and reads what the master wrote. It is configuration and the DEFAULT of every treedb: set it in the yuno's `main.c` (`'global': {'treedbs.impose_c_schema': false}`) or in its config file; `dynamic_schema_treedbs` names the treedbs that open from their file whatever it says. No command changes it: it is read when a treedb opens, and a treedb opens when its yuno starts. See [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §3.11. |
| `dynamic_schema_treedbs` | `list` | `SDF_RD`, default `[]`, **not persistent**. The treedbs that open from their schema FILE whatever `impose_c_schema` says, so their schema can change dynamically (`save-schema` + `apply-schema`). Configuration: `'global': {'treedbs.dynamic_schema_treedbs': ['treedb_wattyzer']}` in the yuno's `main.c`. The yuno's code still wins (`open-treedb impose_c_schema=1`). |

### Commands

| Command | Description |
|---------|-------------|
| `open-treedb` / `close-treedb` | Open or close a treedb instance. `open-treedb impose_c_schema=1`, passed by the yuno's code, imposes the schema from C whatever the attribute says. `close-treedb` acts only on a treedb that THIS service opened, never on its own `__system__` treedb, whatever `force` says: `command-yuno id=<id> service=treedbs command=close-treedb treedb_name=<name> force=1`. |
| `delete-treedb` | Delete a treedb's SCHEMA: its projection in `__system__` (the `treedbs` / `topics` / `cols` nodes). It never touches the treedb's own store on disk. `force=1` is required and means "yes, delete the schema". It refuses the system schema by name, and it refuses a treedb that is OPEN, and `force` does not lift that: an open treedb keeps answering from the copy in memory while its schema no longer exists anywhere, and the next `open-treedb` dies on the C_TRANGER service still alive under its name. Close it first (`close-treedb`, with `force=1` while the yuno plays, or `pause-yuno` + `play-yuno`): `command-yuno id=<id> service=treedbs command=delete-treedb treedb_name=<name> force=1`. It writes `__system__`, so it asks what `__system__`'s tranger IS: a master that lost its lock answers *"READ-ONLY"* (after 7.25.4 it asked the `master` attribute and went on). It removes the treedb's saved schema from `saved_schemas/` too (after 7.25.4 it stayed, and a treedb created again under the name found it). |
| `create-topic` / `delete-topic` | Manage topics within a treedb that THIS service opened (not `__system__`): `command-yuno id=<id> service=treedbs command=delete-topic treedb_name=<name> topic_name=<topic>`. |
| `diff-schema` | What the `__system__` projection of a treedb says that its schema from C does not. |
| (every write) | Asks what the tranger it writes IS, not the `master` attribute. A replica, or a master that lost its lock, answers `-1` *"<role^name>: treedb '<name>' is READ-ONLY, this yuno is not the master of its tranger"*. Between a stop of the service and its next start the tranger holds no lock, and the answer is `-1` *"<role^name>: treedb '<name>' is STOPPED: its tranger holds no lock until its service starts again"* (after 7.25.4 `save-schema` and `delete-treedb` read the stale `master` of the stop and went on to a `__system__` whose treedb was closed: `save-schema` answered *"nothing to save"*). Applies to `save-schema`, `delete-treedb`, `create-topic`, `delete-topic` and `apply-schema`. |
| (every command) | Asks a permission, except `help` and `authzs`: `open-close` (`open-treedb`, `close-treedb`, `delete-treedb`), `create-delete` (`create-topic`, `delete-topic`, `apply-schema`), `write` (`save-schema`), `read` (`diff-schema`, `treedbs`, `saved-schema`). The test walks C_TREEDB's command table (`tests/c/c_treedb_system_schema`), so a command added without one fails it. On a replica every write answers *"READ-ONLY"*. |
| `treedbs` | The treedbs this service opened (`treedb_system_schema` first), one row each: `impose_c_schema` as it applies to that treedb, `decided_by` (`code`, `dynamic_schema_treedbs`, `impose_c_schema` or `system`), the `c_schema_version`, `in_use_schema_version` and `saved_schema_version`, and `master`: whether that treedb's tranger can write NOW (after 7.25.4; a master that lost its lock reads `false`), and `stopped`: whether that tranger is stopped (after 7.25.4). A STOPPED tranger -- the service was stopped and has not started again -- gave its lock back at the stop and holds none: it reads `master: false, stopped: true`, although its json still says the `master` it was until the next start revives it. Needs `read`. Example row: `{"treedb_name": "treedb_authzs", "impose_c_schema": true, "decided_by": "impose_c_schema", "c_schema_version": 19, "in_use_schema_version": 19, "saved_schema_version": 0, "master": true, "stopped": false}`. |
| `save-schema` | Publish the draft of a schema edited in `__system__`: every topic that differs from the schema file IN USE gets its `topic_version` + 1, the treedb its `schema_version` + 1, and the schema is written to `saved_schemas/<treedb>.treedb_schema.json` under the `__system__` tranger, never over the file in use. `dry_run=1` answers it and writes nothing. Master only; permission `write`. `command-yuno id=<id> service=treedbs command=save-schema treedb_name=<name>`. With no `treedb_name` (this and the next two) it acts on every treedb opened there and lists the answers. The file in use is read whatever shape its `topics` have, a list or a dict keyed by name (what a node opened with `impose_c_schema` off wrote before 7.25.0): the same schema gives the same diff and publishes the same versions. It writes `__system__`, so it asks whether `__system__` is the master's (it asked the treedb's tranger). A column's `default: {}` is not written, on any column: it is the meta-schema's placeholder for "no default", and kept on a `required` column it filled the field, so `required` never refused a record (b6f66cdf8 kept it; dropped again after 7.25.4). The trade-off: a `required` column whose literal really declares `'default': {}` loses it through save + apply, and a record created without that field is refused with *"Field required: '<col>'"*. **A draft that is the file in use again withdraws the saved schema**: after "edit, save, undo the edit", a saved schema newer than the file in use is removed (logged *"Saved schema withdrawn, the draft is the schema in use"*), so `saved-schema` answers `can_apply: false` and Apply cannot install what was taken back (after 7.25.4; the save answered "nothing to save" and left it). Both "nothing to save" answers carry `data: {treedb_name, withdrawn, schema_version, path, changes}`, for example `0: <role^name>: the draft of 'treedb_x' is the schema in use: the saved schema_version 13 is withdrawn` with `withdrawn: true`; `dry_run=1` says "would be withdrawn" and removes nothing. |
| `saved-schema` | What `save-schema` wrote, what it changes against the file in use (`diff`: `added` / `removed` / `changed`, one row per `json2flat` leaf), `impose_c_schema` for that treedb (the code's force included) and `can_apply`. `saved` is `true` only for a save still PENDING (newer than the file in use); a saved file that is not newer answers `stale: true`, no `diff` and `can_apply: false` (after 7.25.4 it answered `saved: true` with the diff of a schema already in use), for example `{"saved": false, "stale": true, "can_apply": false, "in_use_schema_version": 14, "saved_schema_version": 13, "diff": {}}`. A saved schema is withdrawn when an open writes the literal over the file it was saved against (no file in use, a newer literal, or an imposed one), with the warning *"Saved schema withdrawn: the schema from C replaces the file in use it was saved against"*. Permission `read`. `draft_changed` names the topics whose draft in `__system__` is NOT SAVED: it is diffed against the saved schema when there is one newer than the file in use, and against the file in use otherwise -- what the schema editor marks as unsaved, rebuilt from it after a reload. Until 7.25.3 it was always the file in use, so a topic saved a moment ago read as unsaved until an Apply. Example after a save: `{"draft_changed": {}, "can_apply": true}`. |
| `apply-schema` | Put the saved schema in place of the file in use: master only, only when C does not impose that treedb's schema, and only a saved `schema_version` higher than the one in use. It takes effect at the next open of the treedb (restart its yuno). Permission `create-delete`, like `create-topic` and `delete-topic`: it replaces the schema of a treedb whole (`save-schema` asks `write`; in 7.25.3 the two were the other way round). `command-yuno id=<id> service=treedbs command=apply-schema treedb_name=<name>`. The answer carries `data: {treedb_name, applied, saved_schema_version, in_use_schema_version}`; `applied` is `true` only when the file in use was replaced, and it is what says to restart. The file is written to a flushed temporary and renamed over the one in use (mode `rpermission`, 0660), never truncated in place. With no `treedb_name` it is **all or none up to the renames**: every treedb with something to apply is checked and written to its temporary first, and one that fails there leaves every file in use as it was (`-1`, every row `applied: false`). The renames that follow are one by one, and a rename that fails fails alone: its row `applied: false`, the others `true`, the answer `-1` with *"N of M treedb(s) applied, see each one"* -- read the rows, not the result. Each row is `{treedb_name, result, comment, data}` with that same `data`, and an apply with nothing to apply answers `0` and no row. The file is written without the derived `fkey` marks of `parse_schema()` (after 7.25.4 it carried them). Once in place the saved schema is removed from `saved_schemas/`: it IS the file in use (after 7.25.4 it stayed, and read as saved). See [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §3.11. |

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
| `with_link_events` | `bool` | A link/unlink publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED` instead of the parent's `EV_TREEDB_NODE_UPDATED`. Set by `C_TREEDB` at open; changed at run time with `set-link-events`. |
| `impose_c_schema` | `bool` | Open with `treedb_schema` over a newer schema on disk too (`treedb_open_db()` option `"impose"`). Set by `C_TREEDB`. |
| `files_max_size` | `integer` | Largest file a `file` column accepts. Default 128 MB. A **memory** limit as much as a policy one — see *File columns* below. |
| `files_content_types` | `json` | Mime types a `file` column may hold, checked on the **bytes**. The default carries images, PDF, video and audio; `image/svg+xml` is **not** in it on purpose (an SVG served from the app's own origin runs script). A column narrows the list, never widens it. |
| `import_root` | `string` | Root that `import-assets` is confined to. Empty: `import-assets` is refused. |

### Commands

| Command | Description |
|---------|-------------|
| `create-node` / `update-node` / `delete-node` | CRUD operations on nodes. A record with `file` columns carries its bytes **beside** the record, in `__files__` — see below. `delete-node` takes two options that are NOT the same thing: `force` unlinks the children, and `ignore_snaps` deletes a node a snapshot still holds (and so breaks that snap's rollback). Since after 7.24.1 `force` no longer implies `ignore_snaps`. `ignore_snaps` erases records a snapshot froze, so it asks `create` (what `shoot-snap` asks) besides `delete` (after 7.25.3). Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=delete-node topic_name=items record={"id":"item1"} options={"force":1}'`. The comments name the yuno (after 7.25.4): `<role^name>: Node update! 'item1' of topic 'items'`, `<role^name>: Node deleted, 'item1' of topic 'items'`. |
| `import-assets` | Turn a directory already on this node into N assets of `__assets__`: one command, no bytes on the wire. Confined to `import_root`. It creates index nodes, links nothing, and **answers the map `path -> id`** so the loader can link what it imported. `dry_run=1` says what it would take. The confinement is resolved, not only spelled: a `source_dir` with `..`, or one that resolves out of `import_root` through a symlink, is refused. |
| `gc-assets` | Delete the assets that **no live node and no snapshot** links — row and bytes. Never automatic: `delete-node force=1` unlinks children rather than deleting them, so an unlinked asset is a normal intermediate state of a bulk operation. `dry_run=1` lists what it would take. |
| `node` / `nodes` | Retrieve one node / list a topic's nodes (with filters). |
| `instances` | List node instances. |
| `link-nodes` / `unlink-nodes` | Manage parent-child relationships. |
| `parents` / `children` | Navigate the graph. |
| `hooks` / `links` | Inspect hook and fkey relationships. |
| `jtree` | Get a node's full subtree as JSON. |
| `shoot-snap` / `activate-snap` / `deactivate-snap` | Snapshot management. `deactivate-snap` answers `-1` when the save of the active snap fails: the snap is still ACTIVE on disk and the next start loads it (after 7.25.3; it answered *"Snap deactivated"*); its success is `<role^name>: Snap deactivated, treedb '<treedb>'`. `activate-snap` answers a snap that does not exist itself, `-1 <role^name>: snap not found: 's9'`, and any other failure as `cannot activate snap 's9' (see the log)` (after 7.25.4; it quoted the process-wide last error message, whoever wrote it). |
| `snaps` / `snap-content` | Inspect snapshots. |
| `import-db` / `export-db` | Bulk import/export. |
| `treedbs` / `topics` | List the treedbs of the tranger / the topics of a treedb. |
| `desc` / `descs` | Describe one topic's schema / every topic's. |
| `set-link-events` | Show (no `set`) or change (`set=1` / `set=0`) which events a link and an unlink publish, on the open treedb and at once: `1` publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED` with the relationship (`hook_name`, `parent_topic_name`, `parent_id`, `child_topic_name`, `child_id`), `0` the parent's `EV_TREEDB_NODE_UPDATED` (what the v1 SPAs read). Either/or for every subscriber of the treedb. Needs the permission `update`. Not persistent: the next start takes the configured `with_link_events` again. Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=set-link-events set=1'`. |
| `print-tranger` | Dump the tranger the treedb lives on as bounded JSON (`kw_collapse()`-truncated: unexpanded containers answer as `[[size]]`, and `lists_limit` and `dicts_limit` bound the expansion). Pass `path=` (backtick-delimited, `kw_find_path` style, arrays by numeric index) to lazily drill into one subtree — this is what feeds the gui_treedb "Raw JSON" viewer. |

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
delete a node on a READ-ONLY replica"* (after 7.25.4; they moved the links in
memory first and met the refused save last); `gobj_create_node()` is refused
by `treedb_create_node()`; and shooting or activating a snap by the treedb
(*"Only master can ..."*).

```C
/*  On a replica: -1, nothing moved  */
int ret = gobj_delete_node(gobj_node, "items",
    json_pack("{s:s}", "id", "item00"), json_pack("{s:b}", "force", 1), src);
```

**An autolink write that fails at its save is PARTIAL.** `update-node` (or
`gobj_update_node()`) with `autolink` answers `NULL` / `-1` when the append
after the links fails (a full disk, a store gone), and what it leaves is not
"nothing":

- **create + autolink**: the record was appended by the create with its
  ordinary fkeys EMPTY (a create stores none: only its `file` columns, which it
  links itself, and, for a secondary instance, the links it inherits from the
  primary); the links were then made in memory and their save failed. On disk
  the node exists without those parents; in memory it has them until the next
  reload.
- **update + autolink**: the fields and the links were changed in memory, and
  nothing reached the disk. Memory and disk differ until the next reload,
  which brings back the old record.

Write it again once the cause is fixed: the same `update-node` with `autolink`
converges.

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
