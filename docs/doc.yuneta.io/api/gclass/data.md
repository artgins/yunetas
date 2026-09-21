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

### Commands

| Command | Description |
|---------|-------------|
| `topics` | List all topics: their names, or with `expanded=1` a desc each (`{topic_name, system_flag, pkey, tkey, topic_version}`). `system_flag` is what tells whether the topic's `t`/`tm` are seconds or **milliseconds** (`sf_t_ms` / `sf_tm_ms`). |
| `create-topic` | Create a new topic. |
| `open-topic` | Open an existing topic. |
| `delete-topic` | Delete a topic. **Irrecoverable.** A topic that still holds records needs `force=1`, and the refusal says so: `command-yuno id=<id> service=<tranger> command=delete-topic topic_name=frames force=1`. Before 7.20.x (Unreleased) the guard never fired and a topic with records was deleted on the first call. |
| `delete-key` | Delete a whole key (primary key) of a topic and every record it holds. **Irrecoverable and master-only**; the delete propagates to the in-process subscribers and to the `rt_by_disk` followers. A key that still holds records needs `force=1` — the refusal names the record count. A key that is not there is an error, not a silent success. |
| `open-list` / `close-list` | Open or close a record list (one-shot snapshot with `return_data=1`, else a live list collecting realtime appends). A live list opened by a remote session is that session's, like its iterators: it is closed when the session closes, and survives the session closing its last Live card. A **keyless** list accepts `rkey` (PCRE2 regex over the keys), and it governs both the disk load **and** the realtime feed. |
| `get-list-data` | Retrieve an open list's data. |
| `list-keys` | List a topic's keys with their record counts **and their time span on both axes**: `[{key, records, fr_t, to_t, fr_tm, to_tm}]`. Lets a client bound a time picker to what the key really holds without reading a record. Filters, sorts and pages **in the server**: `rkey` (PCRE2 regex), `order=key\|records` + `desc`, and `from`/`limit` (with `limit>0` the answer is a page `{total_rows, pages, data}`, and `limit=0` keeps the plain full list). |
| `open-iterator` / `close-iterator` | Open/close a stateful per-key iterator (row index only, no upfront load) for cursor pagination. The handles a remote session opens are stamped with it and reaped when the session closes, whether or not it subscribed to anything (the service watches the session's `EV_ON_CLOSE`, once per session). Takes the match conditions below. A filtered iterator indexes the matching rows at open, so `total_rows` and the pages count only those. Give `key` for one key, or `rkey` (PCRE2 regex) for several keys: see *Several keys in one iterator* below. |
| `get-page` | Get a page `{total_rows, pages, data}` from an open iterator (`limit`, optional `backward`). `from_rowid` is 1-based and, on a **filtered** iterator, is a position among the MATCHING rows (a global rowid only when the iterator does not filter). **`backward` counts from the END and returns the newest rows first**, for every kind of iterator — one key filtered or not, several keys: `get-page iterator_id=it1 from_rowid=1 limit=100 backward=1` is the newest 100. A `get-page` that does not say takes the direction the iterator was OPENED with (`open-iterator … backward=1`). Before this (unreleased, after 7.24.1) an unfiltered one-key iterator kept the window counted from the start and only reversed it, and `open-iterator backward=1` did nothing. An iterator whose key was deleted since it opened (by `delete-key`, or by a treedb sharing the tranger) is closed at its next `get-page`, which answers `-1` with *"iterator 'it1' closed, its key 'D' was deleted: open it again"*; asked again, it is *"Iterator not found"*. A multi-key iterator closes when ANY of its keys goes. |
| `open-rt` / `close-rt` | Open/close a realtime feed on a topic key (no history load). New appends are published as `EV_TRANGER_RECORD_ADDED` to subscribers. |
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

**What it costs.** The open counts the rows of each key once (for a filtered
iterator that is building and dropping each key's index) and keeps only a
number per key; a `get-page` opens the keys its page touches, reads, and closes
them. Until after 7.24.1 it kept one tranger2 iterator per key for its whole
life — 1000 keys with 60 daily files was +147 MB for one card. The counts are
the ones taken at open. Bound it anyway: a negative `from_rowid` reads the last
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
| `impose_c_schema` | `bool` | `SDF_RD`, default `1`, **not persistent**. Open every treedb with its schema from C, over a newer schema file on disk. `0`: open from the schema FILE (the literal is installed only when it is newer), which `apply-schema` replaces. Either way `__system__` is not read at open: it is where a schema is edited, and the MASTER projects into it when it has no projection of that treedb or a lower `schema_version`; a replica writes nothing and reads what the master wrote. It is configuration: set it in the yuno's `main.c` (`'global': {'C_TREEDB.impose_c_schema': false}`) or in its config file. `set-impose-c-schema` changes it until the next restart. See [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §3.11. |

### Commands

| Command | Description |
|---------|-------------|
| `open-treedb` / `close-treedb` | Open or close a treedb instance. `open-treedb impose_c_schema=1`, passed by the yuno's code, imposes the schema from C whatever the attribute says. `close-treedb` acts only on a treedb that THIS service opened, never on its own `__system__` treedb, whatever `force` says: `command-yuno id=<id> service=treedbs command=close-treedb treedb_name=<name> force=1`. |
| `delete-treedb` | Delete a treedb's SCHEMA: its projection in `__system__` (the `treedbs` / `topics` / `cols` nodes). It never touches the treedb's own store on disk. `force=1` is required and means "yes, delete the schema". It refuses the system schema by name, and it refuses a treedb that is OPEN, and `force` does not lift that: an open treedb keeps answering from the copy in memory while its schema no longer exists anywhere, and the next `open-treedb` dies on the C_TRANGER service still alive under its name. Close it first (`close-treedb`, with `force=1` while the yuno plays, or `pause-yuno` + `play-yuno`): `command-yuno id=<id> service=treedbs command=delete-treedb treedb_name=<name> force=1`. |
| `create-topic` / `delete-topic` | Manage topics within a treedb that THIS service opened (not `__system__`): `command-yuno id=<id> service=treedbs command=delete-topic treedb_name=<name> topic_name=<topic>`. |
| `diff-schema` | What the `__system__` projection of a treedb says that its schema from C does not. |
| `set-impose-c-schema` | Show (no `set`) or change (`set=1` / `set=0`) `impose_c_schema`. Needs the permission `impose-c-schema`. Acts the next time a treedb opens, and only in memory: a restart takes the configured value back. Its answer lists in `forced_by_code` the treedbs whose code imposes, which the value does not reach. |
| `save-schema` | Publish the draft of a schema edited in `__system__`: every topic that differs from the schema file IN USE gets its `topic_version` + 1, the treedb its `schema_version` + 1, and the schema is written to `saved_schemas/<treedb>.treedb_schema.json` under the `__system__` tranger, never over the file in use. `dry_run=1` answers it and writes nothing. Master only; permission `write`. `command-yuno id=<id> service=treedbs command=save-schema treedb_name=<name>`. With no `treedb_name` (this and the next two) it acts on every treedb opened there and lists the answers. |
| `saved-schema` | What `save-schema` wrote, what it changes against the file in use (`diff`: `added` / `removed` / `changed`, one row per `json2flat` leaf), `impose_c_schema` for that treedb (the code's force included) and `can_apply`. Permission `read`. |
| `apply-schema` | Put the saved schema in place of the file in use: master only, only when C does not impose that treedb's schema, and only a saved `schema_version` higher than the one in use. It takes effect at the next open of the treedb (restart its yuno). Permission `write`. |

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
| `create-node` / `update-node` / `delete-node` | CRUD operations on nodes. A record with `file` columns carries its bytes **beside** the record, in `__files__` — see below. `delete-node` takes two options that are NOT the same thing: `force` unlinks the children, and `ignore_snaps` deletes a node a snapshot still holds (and so breaks that snap's rollback). Since after 7.24.1 `force` no longer implies `ignore_snaps`. Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=delete-node topic_name=items record={"id":"item1"} options={"force":1}'`. |
| `import-assets` | Turn a directory already on this node into N assets of `__assets__`: one command, no bytes on the wire. Confined to `import_root`. It creates index nodes, links nothing, and **answers the map `path -> id`** so the loader can link what it imported. `dry_run=1` says what it would take. The confinement is resolved, not only spelled: a `source_dir` with `..`, or one that resolves out of `import_root` through a symlink, is refused. |
| `gc-assets` | Delete the assets that **no live node and no snapshot** links — row and bytes. Never automatic: `delete-node force=1` unlinks children rather than deleting them, so an unlinked asset is a normal intermediate state of a bulk operation. `dry_run=1` lists what it would take. |
| `node` / `nodes` | Retrieve one node / list a topic's nodes (with filters). |
| `instances` | List node instances. |
| `link-nodes` / `unlink-nodes` | Manage parent-child relationships. |
| `parents` / `children` | Navigate the graph. |
| `hooks` / `links` | Inspect hook and fkey relationships. |
| `jtree` | Get a node's full subtree as JSON. |
| `shoot-snap` / `activate-snap` / `deactivate-snap` | Snapshot management. |
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

Only `help` and `authzs` ask nothing. `system-schema`, `trace` and the
`set-link-events` display answered anyone until after 7.24.1; the test walks
C_NODE's command table (`tests/c/c_node_authz`), so a command added without a
permission fails it.

`update-node` with `options.create=1` also asks for `create` when the node
does not exist yet. A refusal answers `-403`, and nothing is written.

`options.create_only=1` makes a NEW node and refuses one that exists
(*"Node already exists"*); `options.create=1` alone is an upsert, and a taken
id there is an update of the existing record. A table's +New sends
`create_only` (gobj-ui 7.23.194).

```
command-yuno id=<id> service=<treedb> command=update-node topic_name=users record='{"id":"bob","username":"Bob"}' options='{"create_only":1}'
```

An `update-node` with `autolink` whose record names a link that cannot be
made saves the record and answers **-1**: *"node 'x' saved, but its links were
NOT changed"*. The fkey column keeps the links it had (see
`treedb_replace_links()`); it used to answer *"Node update!"*.

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
[`DESIGN-treedb-files.md`](https://github.com/artgins/yunetas/blob/7.25.0/kernel/c/timeranger2/DESIGN-treedb-files.md).

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
