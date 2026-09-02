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
| `delete-topic` | Delete a topic. |
| `delete-key` | Delete a whole key (primary key) of a topic and every record it holds. **Irrecoverable and master-only**; the delete propagates to the in-process subscribers and to the `rt_by_disk` followers. A key that still holds records needs `force=1` — the refusal names the record count. A key that is not there is an error, not a silent success. |
| `open-list` / `close-list` | Open or close a record list (one-shot snapshot with `return_data=1`, else a live list collecting realtime appends). A **keyless** list accepts `rkey` (PCRE2 regex over the keys), and it governs both the disk load **and** the realtime feed. |
| `get-list-data` | Retrieve an open list's data. |
| `list-keys` | List a topic's keys with their record counts **and their time span on both axes**: `[{key, records, fr_t, to_t, fr_tm, to_tm}]`. Lets a client bound a time picker to what the key really holds without reading a record. Filters, sorts and pages **in the server**: `rkey` (PCRE2 regex), `order=key\|records` + `desc`, and `from`/`limit` (with `limit>0` the answer is a page `{total_rows, pages, data}`, and `limit=0` keeps the plain full list). |
| `open-iterator` / `close-iterator` | Open/close a stateful per-key iterator (row index only, no upfront load) for cursor pagination. Takes the match conditions below. A filtered iterator indexes the matching rows at open, so `total_rows` and the pages count only those. |
| `get-page` | Get a page `{total_rows, pages, data}` from an open iterator (`limit`, optional `backward`). `from_rowid` is 1-based and, on a **filtered** iterator, is a position among the MATCHING rows (a global rowid only when the iterator does not filter). |
| `open-rt` / `close-rt` | Open/close a realtime feed on a topic key (no history load). New appends are published as `EV_TRANGER_RECORD_ADDED` to subscribers. |
| `add-record` | Append a record. |
| `print-tranger` | Dump tranger state as bounded JSON (`expanded`, `lists_limit` and `dicts_limit`. Unexpanded containers answer as `[[size]]`). |
| `desc` | Describe topic schema. |

**`open-iterator` match conditions** (all optional, and `0` or empty means unset):
`from_t`/`to_t`, `from_tm`/`to_tm`, `from_rowid`/`to_rowid`, `backward`, and the
user_flag conditions (`user_flag`, `not_user_flag`, `user_flag_mask_set`,
`user_flag_mask_notset`). They are ANDed, and every one is honored **per
record**.

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
| `exit_on_error` | `bool` | Exit on schema errors. |

### Commands

| Command | Description |
|---------|-------------|
| `open-treedb` / `close-treedb` | Open or close a treedb instance. |
| `delete-treedb` | Delete a treedb and its data. |
| `create-topic` / `delete-topic` | Manage topics within a treedb. |

---

(gclass-c-node)=
## C_NODE

Node resource interface for TreeDB — full CRUD and graph operations on
tree nodes with linking, snapshots, and import/export.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Commands

| Command | Description |
|---------|-------------|
| `create-node` / `update-node` / `delete-node` | CRUD operations on nodes. |
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
| `print-tranger` | Dump the tranger the treedb lives on as bounded JSON (`kw_collapse()`-truncated: unexpanded containers answer as `[[size]]`, and `lists_limit` and `dicts_limit` bound the expansion). Pass `path=` (backtick-delimited, `kw_find_path` style, arrays by numeric index) to lazily drill into one subtree — this is what feeds the gui_treedb "Raw JSON" viewer. |

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

The bytes a treedb node owns but cannot hold — a photo, a plan, a signed
pdf. The bytes go in a directory this service owns, next to the treedb of
the same realm; one node per asset goes in the treedb; and these commands
are the only way in.

They cannot go *in* the treedb: it is held in memory, and timeranger2
rewrites the whole record on every update, so a 40 KB photo would be
rewritten every time its node changed state and would ride along in every
page of `nodes`. (The `blob` column type is not binary — it is free-form
JSON.)

The asset id is the **sha256 of its content**. The same bytes stored twice
are one asset, a bulk reload creates nothing, a served URL can be cached for
ever, and replacing an asset is a new id plus a relinked node — which is
what makes the node's own history say which file it carried, and when.

The consumer's column stops being a path and becomes an **fkey** into the
asset topic, so an asset is linked, listed, graphed, scope-checked and
cascade-deleted like any other node, and an asset no node links any more is
visibly garbage.

**The topic belongs to the host, not to this gclass.** An asset's fkeys
point at the host's own topics, so only the host can write those hooks.
`C_ASSETS` never creates the topic and refuses to work when the one it was
pointed at cannot hold what it is about to write — a blob on disk whose row
failed to be written is a file nothing can ever find again. The canonical
topic is in
[`c_assets.h`](https://github.com/artgins/yunetas/blob/7.17.0/kernel/c/root-linux/src/c_assets.h).

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `treedb` | `pointer` | The `C_NODE` that owns the treedb. Set by the host; takes precedence over `treedb_service`. |
| `treedb_service` | `string` | Its service name, when the host did not build it itself. |
| `topic_name` | `string` | Topic holding the asset metadata. Default `assets`. |
| `store_path` | `string` | Absolute blob directory. Empty: built under the realm store from `store_service` / `store_tenant` / `store_dir`. |
| `import_root` | `string` | Root that `import-assets` is confined to. Empty: `import-assets` is refused. |
| `public_url` | `string` | URL prefix a web server serves the blobs from. Empty: `get-asset` answers inline. |
| `sign_secret` | `string` | Shared secret of the web server's `secure_link_md5`. Empty: `get-asset` answers inline. |
| `url_ttl` | `integer` | Seconds a signed URL stays valid. Default `900`. |
| `max_size` | `integer` | Largest asset accepted. Default 128 MB. It is a **memory** limit as much as a policy one — see below. |
| `allowed_content_types` | `json` | Mime types accepted. The default carries images, PDF, video and audio. `image/svg+xml` is **not** in it on purpose: an SVG served from the app's own origin runs script. |

### What it accepts, and what that costs

Images (`jpeg`, `png`, `webp`, `gif`), `application/pdf`, video (`mp4`,
`webm`, `quicktime`, `ogg`, `x-matroska`) and audio (`mpeg`, `mp4`, `ogg`,
`wav`, `webm`, `flac`).

The pairs that share a container are split by **extension**, because the
extension is the only thing a web server reads to pick a `Content-Type`:
`.webm` is video and `.weba` audio, `.mp4` video and `.m4a` audio, `.ogv`
video and `.ogg` audio.

**There is no streaming path**: an asset is hashed and written whole, so
`max_size` bounds RAM as much as policy. `put-asset` costs the worst — the
base64 arrives inside the kw and is then decoded, so one call peaks at
roughly 2.3x the file; `import-assets` only pays the file itself. Before
raising `max_size` for big media, check the yuno's own `MEM_MAX_BLOCK`: a
single base64 string above it is refused by the allocator, not by this
gclass.

### Commands

| Command | Description |
|---------|-------------|
| `put-asset` | Store one asset from `content64`, return its id. Same bytes, same id: idempotent. |
| `get-asset` | Return a signed URL, or the bytes inline. See below. |
| `list-assets` | The asset metadata, never the bytes. `orphan=1` lists the ones no node links. |
| `delete-asset` | Delete one. Refused while a node links it, unless `force`. |
| `import-assets` | Turn a directory already on this node into N assets. |
| `gc-assets` | Delete the assets no node links any more. Never automatic. |

Writes are refused on a replica (the tranger is not the master of its store)
and gated by the `write` / `read` authz of the service.

### Two ways out to a browser, and the service picks

`get-asset` answers in one of two shapes:

```json
{"mode": "url",    "url": "/assets/ab/cd/<id>.jpg?e=<expires>&s=<token>"}
{"mode": "inline", "content_type": "image/jpeg", "content64": "..."}
```

It signs a URL when `public_url` **and** `sign_secret` are both configured,
and answers inline when they are not. So the caller has **one** code path,
and a node with no web server in front of it still shows its images instead
of showing nothing.

The signed form reproduces, byte for byte, what this nginx block hashes:

```nginx
location /assets/ {
    secure_link      $arg_s,$arg_e;
    secure_link_md5  "$secure_link_expires$uri <sign_secret>";
    if ($secure_link = "")  { return 403; }   # bad signature
    if ($secure_link = "0") { return 410; }   # expired
    alias <store_path>/;
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

### `import-assets` moves no bytes

It walks a directory that is **already on the node** and turns it into N
assets, recording each file's path relative to `import_root` as
`source_path` — the field a loader links by. Pushing hundreds of megabytes
through the control plane, one base64 message per file, is the thing this
command exists to avoid.

It reads an arbitrary path, so it is confined, and three separate things do
the confining:

- an explicit `..` guard, which refuses rather than silently resolving
  somewhere else;
- `build_path()`, which strips the leading `/` of every segment after the
  first and clamps `..` against it — so an **absolute** `source_dir` lands
  *inside* the root (`/etc` → `<import_root>/etc`), not at `/etc`;
- `walk_dir_tree()`, which `lstat()`s, so a symlink is neither a regular
  file nor a directory and cannot lead the walk out.

`tests/c/c_assets` checks all three with hostile input.
