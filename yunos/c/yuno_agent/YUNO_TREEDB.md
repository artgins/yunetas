# timeranger2 + treedb, in 30 minutes

This is the crash course on Yuneta's persistence layer. At the end you
know the difference between **timeranger2** (the append-only
time-series log) and **treedb** (the graph database on top), how
schemas are declared, how nodes link to each other, and which rules
will ruin your day if you ignore them.

> **Conceptual frame.** This document describes the **information
> plane** of Yuneta's typed-graph model. The behavior plane is in
> [`GOBJ.md`](GOBJ.md). The claim that both planes share one set of
> primitives — `topic`/`gclass`, `node`/`gobj`, `hook`/subscription —
> is laid out in
> [The Typed-Graph Model](../../../docs/doc.yuneta.io/philosophy/typed_graph_model.md).
> Read that first if you want to know *why* treedb and gobj look so
> similar before diving into either one.

Companion to [`GOBJ.md`](GOBJ.md). Sibling to [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md)
(which uses these topics to store realms, yunos, binaries and
configurations), [`YUNO_AUTH.md`](YUNO_AUTH.md) (which uses them for users, roles
and audit), and [`REALMS.md`](REALMS.md) (the realm hooks lifecycle).

---

## 1. Mental model

```
                ┌───────────────────────────────────────┐
                │     your gclass calls gobj_*node()    │
                └───────────────────┬───────────────────┘
                                    │
                                    ▼
                ┌───────────────────────────────────────┐
                │            c_treedb / c_node          │   gobj wrappers
                │  (graph operations, in-memory hooks)  │
                └───────────────────┬───────────────────┘
                                    │
                                    ▼
                ┌───────────────────────────────────────┐
                │            tr_treedb.c                │   graph layer
                │  topics, nodes, hooks, fkeys, schema  │
                └───────────────────┬───────────────────┘
                                    │
                                    ▼
                ┌───────────────────────────────────────┐
                │            timeranger2.c              │   append-only log
                │  per-key files + md2 binary index     │
                └───────────────────┬───────────────────┘
                                    │
                                    ▼
                              filesystem
                       (one directory per topic,
                        one subdir per key,
                        one .json + .md2 per day)
```

Two distinct things:

| Layer        | What it is                                                                 |
|--------------|----------------------------------------------------------------------------|
| **timeranger2** | An append-only time-series log with a key index. Stores records keyed by a primary key, time-partitioned, with a 32-byte binary metadata index for fast lookup by `rowid`, time, or pkey. Knows nothing about graphs. |
| **treedb**     | A graph database that uses timeranger2 as its persistent store. Adds the notion of topics with schemas, typed columns, hooks (parent→children in-memory pointers), and fkeys (child→parent persistent references). |

If you want raw time-series, you go straight to timeranger2. If you want
a graph of typed nodes, you use treedb. The agent uses treedb for
everything: realms, yunos, binaries, configurations, users and roles. The
`logcenter` yuno uses raw timeranger2 to write records.

---

## 2. timeranger2

### 2.1 The on-disk layout

For each opened database (a top-level directory):

![timeranger2 on-disk layout: a database holds __timeranger2__.json and per-topic files. Records live under keys/<key>/<date>.json, paired with a 32-byte <date>.md2 index. The disks/<rt_id>/ tree holds hardlinks back into the keys/ files, which is how non-master and cross-yuno readers see the same data.](../../../docs/doc.yuneta.io/_static/treedb_ondisk.svg)

The same layout in text:

```
<database>/
  __timeranger2__.json                ← metadata + master lock
  <topic_1>/
    topic_desc.json                   ← {topic_name, pkey, tkey, system_flag}
    topic_cols.json                   ← persisted cols schema  ⚠ versioning trap
    topic_var.json                    ← user-mutable per-topic flags
    keys/
      <key_value_a>/
        2026-05-22.json               ← appended JSON records, one per line
        2026-05-22.md2                ← 32-byte binary index, one per record
        2026-05-23.json
        2026-05-23.md2
        2026-05-22.unordered          ← only if a LATE record went into that file
        …
      <key_value_b>/
        …
    disks/                            ← non-master / cross-yuno hardlink slots
      <rt_id>/
        <key_value_a>/                ← hardlinks to the keys/ files
        <key_value_b>/
        …
  <topic_2>/
    …
```

Path-building lives in [`kernel/c/timeranger2/src/timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c).
The data filename mask is `"%Y-%m-%d"` by default — each appended
record lands in the file whose mask matches its `__t__`. Big topics
naturally rotate every day.

### 2.2 Records and the `md2` index

Each `.md2` file is an array of fixed 32-byte records in big-endian
order. The struct ([`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c), in-memory shape
`md2_record_ex_t` at [`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h)):

![A md2 record is 32 bytes: four uint64 fields __t__, __tm__, __offset__, __size__. The .md2 file is an array of these, indexed by rowid. A lookup multiplies rowid by 32, seeks the .md2, reads offset and size, then seeks the paired .json. O(1).](../../../docs/doc.yuneta.io/_static/md2_record.svg)

```c
typedef struct {
    uint64_t __t__;         // storage timestamp + high-16-bit user flags
    uint64_t __tm__;        // creation timestamp + high-16-bit system flags
    uint64_t __offset__;    // byte offset of the record in the paired .json
    uint64_t __size__;      // byte size of the record
    // (in memory only:)
    uint16_t system_flag;
    uint16_t user_flag;
    uint64_t rowid;
} md2_record_ex_t;
```

The high 16 bits of `__t__` and `__tm__` are reserved for flags. Macros
at [`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c) extract and pack them. Lookup by rowid is
O(1) — multiply by 32, seek the `.md2`, read offset+size, seek the
`.json`. Lookup by time range is O(N) over `.md2` records, which is
still fast, because each record is 32 bytes.

**A file's time range comes from its first and last rows**, read at load, and
that is right only while the file is in time order. A record appended with a
`__t__` below what its file already holds (a late record: `append-record
__t__=`, a migration, a clock stepping back) breaks that, so the master drops
an empty marker beside the md2, `<file>.unordered`, and a load reads a marked
file WHOLE to get its real range. Without it, a time-range query skipped the
file after a reload (since 7.25.0). A follower that reads a cell
again from disk keeps the union of the ranges it knew and the ones it read.
Inside a marked file a time-range scan no longer stops at the first row past
the range: it reads the rows of the file, forward and backward, and a paged
iterator's index does the same (after 7.25.3). A `tm` condition never ends a
scan: `tm` is the producer's time and nothing orders it.

### 2.3 `g_rowid` vs `i_rowid` — the rule

Two rowids per record, both maintained **only** by timeranger2:

| Name        | Meaning                                                              |
|-------------|----------------------------------------------------------------------|
| `g_rowid`   | Global rowid for that key — cumulative across all files, never reset |
| `i_rowid`   | Rowid within the current `.md2` file — `(offset / sizeof(md2_record_t)) + 1` |

[`tranger2_append_record`](#tranger2_append_record) ([`timeranger2.c:2332`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L2332)) computes both and
returns them in `md_record_ex->rowid` ([`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c)). **Callers
never set them.** For topics with `sf_rowid_key`, timeranger2 also
asserts `g_rowid == i_rowid` ([`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c)) — a mismatch is
a data-corruption indicator.

If you write test fixtures and you fill `g_rowid` by hand, stop. That is
the work of the framework.

### 2.4 `__t__` vs `__tm__`

Both timestamps, but semantically distinct:

| Field    | What it means                                                  | When it is set                          |
|----------|----------------------------------------------------------------|----------------------------------------|
| `__t__`  | When timeranger2 wrote the record to disk                      | At append time. Defaults to "now".     |
| `__tm__` | When the underlying event happened (from the record's `tkey` field) | Caller-controlled via `tkey` config.   |

`__t__` partitions files. `__tm__` is the event-time for your queries.
For records that are events as they happen, the two are usually
identical (within milliseconds). For batch imports of historical data
the two diverge — `__tm__` is the original event, `__t__` is "now I
imported it".

### 2.5 Topic declaration

When you create a topic you provide a `topic_desc_t`
([`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h)):

```c
typedef struct {
    const char       *topic_name;
    const char       *pkey;          // primary-key field name
    const system_flag2_t system_flag;
    const char       *tkey;          // time-key field name
    const json_desc_t *jn_cols;       // column schema
    const json_desc_t *jn_topic_ext;
} topic_desc_t;
```

`system_flag` bits ([`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h)):

| Flag              | Meaning                                                |
|-------------------|--------------------------------------------------------|
| `sf_string_key`   | pkey is a string. Directory names use it verbatim.     |
| `sf_int_key`      | pkey is a uint64. Directory names zero-padded.         |
| `sf_rowid_key`    | pkey is auto-generated rowid. `g_rowid == i_rowid` enforced. |
| `sf_t_ms`         | `__t__` in milliseconds (default: seconds).            |
| `sf_tm_ms`        | `__tm__` in milliseconds.                              |
| `sf_zip_record`   | Declared, NOT implemented: both the writer and the reader carry the branch as `// TODO`. A topic created with it stores the bit and writes plain records. |
| `sf_cipher_record`| Declared, NOT implemented, same as above.              |

The two dead flags are reachable since 7.21.0, when `tranger2_str2system_flag()`
started mapping each name to its own bit (before, every name landed on the bit
of the one before it). Do not declare them: the bit is persisted, and the day
the branch is written it would apply to a store written plain.

Persisted in `topic_desc.json` at create time ([`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c))
and loaded on open.

### 2.6 Public API in 12 calls

[`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h). Grouped by purpose:

```c
// lifecycle
json_t *tranger2_startup    (hgobj, json_t *jn_tranger, yev_loop_h);
int     tranger2_stop       (json_t *tranger);
int     tranger2_shutdown   (json_t *tranger);
json_t *tranger2_create_topic(json_t *tranger, const char *topic_name,
                              const char *pkey, const char *tkey,
                              json_t *jn_topic_ext, system_flag2_t system_flag,
                              json_t *jn_cols, json_t *jn_var);
json_t *tranger2_open_topic  (json_t *tranger, const char *topic_name, BOOL verbose);
int     tranger2_close_topic (json_t *tranger, const char *topic_name);

// append
int     tranger2_append_record(json_t *tranger, const char *topic_name,
                               uint64_t __t__, uint16_t user_flag,
                               md2_record_ex_t *md_record_ex, json_t *jn_record);

// read
json_t *tranger2_open_iterator    (json_t *tranger, const char *topic_name, const char *key,
                                   json_t *match_cond, tranger2_load_record_callback_t,
                                   const char *iterator_id, hgobj creator, json_t *data, json_t *extra);
json_t *tranger2_iterator_get_page(json_t *tranger, json_t *iterator,
                                   uint64_t from_rowid, int limit, BOOL backward);
int     tranger2_close_iterator   (json_t *tranger, json_t *iterator);

// realtime
json_t *tranger2_open_rt_mem (…);   // master-side realtime (writes pushed via callback)
json_t *tranger2_open_rt_disk(…);   // non-master realtime (watches hardlinks)
```

[`tranger2_open_rt_disk`](#tranger2_open_rt_disk) is the workhorse for **cross-yuno** reads —
see §4.5.

### 2.6b The two time axes (`t` and `tm`)

Every record carries **two** timestamps, and they are independent:

| Axis | Meaning | Its source |
|------|---------|---------------------|
| `t`  | **Persistence** time — when the record was appended | the `__t__` argument of `tranger2_append_record` (now, if 0) |
| `tm` | **Message** time — when the event it carries happened | the record's **tkey** field (usually `tm`), set by the producer |

They diverge whenever data is backfilled or a device uploads a buffer late.
Both are in the **topic's** unit: seconds, or **milliseconds** when the topic
sets `sf_t_ms` / `sf_tm_ms` (read `system_flag` from the topic desc — over the
wire, `topics expanded=1`).

The `match_cond` of an iterator takes a range on each axis (`from_t` and
`to_t`, `from_tm` and `to_tm`), the `from_rowid` and `to_rowid` pair, and
the `user_flag` conditions,
and **ANDs** them. Every condition is honored **per record**: a filtered paging
iterator builds its row **index** when it opens, so `tranger2_iterator_size()`,
`pages` and the pages themselves count only matching records — and
`get_page`'s `from_rowid` is then a position among the *matching* rows, not a
global rowid. An unfiltered iterator builds no index (its open stays cheap
regardless of key size) and its positions are the global rowids.

`list-keys` reports, per key, `records` plus the key's span on both axes
(`fr_t`/`to_t`, `fr_tm`/`to_tm`), read from the topic's in-memory cache totals —
so a client can bound a time picker to the content of the key, and it reads
no record.

> **Note (in the md2 record, times carry flags).** On disk the 16 high bits of
> `__t__` hold the `user_flag` and those of `__tm__` the `system_flag`. Always
> read them through `get_time_t()` or `get_time_tm()`. The raw field gives
> you a timestamp that still contains the flags.

### 2.7 Master / non-master

Exactly one process owns a store for writing:

- The master can **read AND write**. Only the master can
  call `tranger2_append_record`, [`tranger2_delete_topic`](#tranger2_delete_topic) and the other write functions.
- Non-masters can only read. They are expected to use
  `tranger2_open_rt_disk` so the master can push updates to them via
  hardlinks in the `disks/<rt_id>/` directory.

**The role comes from the configuration, not from a start-up race.** The
`master` attr of the yuno's config goes directly to
[`tranger2_startup`](#tranger2_startup), and **only a yuno configured as master
opens `__timeranger2__.json` in exclusive mode**
([`timeranger2.c:443`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L443)).
A yuno configured `master: false` never competes for the lock: it opens the
file in shared mode and stays a replica, whatever the start order. To move the
mastership of a store, change the configuration. Do not change the order of the
restarts.

**A configured master that finds the store locked stops. It does not become a
replica.** The exclusive probe fails and logs a CRITICAL that carries
`on_critical_error`, which is `2` (`LOG_OPT_EXIT_ZERO`) by default and is
hardcoded to that value by `C_AUTHZ`. That CRITICAL calls `exit(0)` inside the
log, before the non-master fallback below it. The exit code `0` is deliberate:
the watcher does not relaunch a clean exit, so the second instance stays down
and the store keeps exactly one owner. The fallback that opens the file in
shared mode and clears the flag is reached only when `on_critical_error` is
`0` — a read-only replica that is configured to run non-master.

The lock is held for the lifetime of the process. If a master crashes, the OS
releases the flock on exit, and the next yuno **configured as master** takes
the store.

The flag is **per tranger**, not per yuno: one yuno is routinely the master of
its `treedb_system_schema` and a replica of a data treedb that it shares with
another yuno.

```
db_history_ce (1620):  "Authz.master": true    → master of the authzs store
gate_central  (2020):  "Authz.master": false   → replica of the same store
```

**Asking a running yuno: `treedb-info`.** Until 7.13.0 the flag was not
reachable from the control plane at all — it is an `SDF_RD` attr of the
tranger, absent from `services`, from `treedbs` and from the stats, so the only
place it surfaced was the whole `print-tranger` dump. `C_NODE` answers it now:

```bash
ycommand -c 'command-yuno id=<yuno> service=<treedb> command=treedb-info'
{
    "treedb_name": "treedb_authzs",
    "master": false,
    "schema_version": 19,
    "topics": ["__snaps__", "__graphs__", "__assets__", "roles", "users", "users_accesses"]
}
```

`schema_version` is the treedb's own `__schema_version__` inside the tranger
(written by `treedb_open_db`), and it is what tells a client whether the schema
it is looking at is the one it knows — a change of cols must bump it, or the
persisted `topic_cols.json` masks the new in-memory schema (§3.4).

**Writing to a replica is refused, and used to be silent.** `create-node`,
`update-node`, `delete-node`, `link-nodes`, `unlink-nodes` and `import-db` on a
non-master `C_NODE` now answer

```
ERROR -1: gate_central^2020: treedb 'treedb_authzs' is READ-ONLY, this yuno is not the master of its tranger
```

Before 7.13.0 they answered **success**: the node was built in the in-memory
treedb, `tranger2_append_record`'s own "NO master" guard was never reached
(a non-master treedb does not attempt the append), nothing was logged, and the
row was gone at the next reload. An editor showed a saved record that was
already lost. The check runs **before** the authz check on purpose: on a replica
nobody can write, whoever they are, and a `-403` would send an operator looking
for a permission that would not help.

**From C too, before anything moves.** `gobj_update_node()` (except a
`volatil` one, which writes memory only), `gobj_link_nodes()`,
`gobj_unlink_nodes()` and `gobj_delete_node()` on a replica return `NULL` /
`-1` and log *"Cannot write a node / link nodes / unlink nodes / delete a node
on a READ-ONLY replica"*; `gobj_create_node()` is refused by
`treedb_create_node()` itself. Until after 7.25.4 link, unlink and a forced
delete moved the links in MEMORY first and met the refused save last: the
caller got `-1` and the replica's memory said what its disk did not.

```C
/*  On a replica: -1, and item01 keeps the parent it had  */
int ret = gobj_link_nodes(gobj_node, "children",
    "items", json_pack("{s:s}", "id", "item00"),
    "items", json_pack("{s:s}", "id", "item01"),
    src);
```

**A master that lost its lock is a replica.** When a stopped master finds its
store taken by another process, timeranger2 leaves its tranger a replica
(`master` false, `master_lost` true). The services on top follow what the
tranger IS: `C_TRANGER`'s `master` attribute reads `false` and its feeds open
as a replica's (`rt_disk`), and `C_TREEDB`'s `treedbs` command reports
`master` per treedb:

```bash
ycommand -c 'command-yuno id=<yuno> service=treedbs command=treedbs'
# [{"treedb_name": "treedb_system_schema", ..., "master": false},
#  {"treedb_name": "treedb_authzs", ..., "master": true}]
```

### 2.8 Snapshots

The current timeranger2 API does **not** expose a snapshot primitive
named `tranger2_*_snap*` — those calls live one layer up at the treedb
level (`treedb_shoot_snap()` / `treedb_activate_snap()`). The closest
underlying mechanism is the `disks/<rt_id>/` hardlink trick that gives
non-masters a consistent view at the point the directory was wired.

What timeranger2 lends to it is one field: the md2 `user_flag` of a record,
which the treedb layer uses as the snap's **tag**. Only `shoot-snap` writes
it, in place, and a record is tagged once. **The whole behaviour — what a
snap writes, what an activated snap reads, what happens to writes made
meanwhile, and what a snap protects from a delete — is §3.9.**

### 2.9 The delete-record story

Two granularities, both implemented in v7 as of 2026-05-26.

- **Whole record** (= a primary key + every instance under it).
  **[`tranger2_delete_key()`](#tranger2_delete_key)** (renamed from `tranger2_delete_record`
  on 2026-05-25. A `#define` in [`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h) keeps the legacy alias).
  Removes `keys/<key>/` and drops the key from `topic_cache`.
  Irrecoverable. Used today by `treedb_delete_node`.
- **One instance** (one row in the `.md2` file).
  **`tranger2_delete_instance(tranger, topic, key, __t__, rowid,
  zero_payload)`** mutates the `.md2` row in place with
  `sf_deleted_instance = 0x0400` (back in `system_flag2_t`, inherited
  side of the mask so `rt_by_disk` followers see the tombstone).
  Optional `zero_payload` overwrites the matching `__size__` bytes in
  the data `.json` for sensitive-data wipes. Read paths
  ([`tranger2_open_iterator`](#tranger2_open_iterator) history, [`tranger2_iterator_get_page`](#tranger2_iterator_get_page),
  [`publish_new_rt_disk_records`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L354)) skip dead rows. Master-only,
  idempotent. Slot ids do NOT renumber — `iterator_size` /
  `total_rows` keep counting slots, not live rows.
  **Treedb uses it**: [`treedb_delete_instance()`](#treedb_delete_instance)
  drops one `pkey2` slot in memory AND calls it on every md2 row of that
  `(id, pkey2 value)`, so the instance stays deleted after a reopen.

#### Propagation to subscribers (2026-05-26)

`tranger2_delete_key()` now notifies every subscriber tracking
the deleted key. Two paths:

- **In-process** (rt_mem, rt_disk in the same yuno as the master,
  open_iterator): a registered
  `tranger2_key_deleted_callback_t` fires for each handle whose
  `key` filter matches (`""` = any).
- **Across-process** (`rt_by_disk` followers): the master
  [`rmrdir`](#rmrdir)s `topic/disks/<rt_id>/<key>/` BEFORE the live
  `keys/<key>/` so the follower's [inotify](https://man7.org/linux/man-pages/man7/inotify.7.html) watcher catches it as
  `FS_SUBDIR_DELETED_TYPE`, which fires the follower's
  `key_deleted_callback`. [`fire_key_deleted_locally()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c#L3366) is split by
  transport (`fs_followers` flag): the master in-process call
  serves non-watcher subscribers, the inotify branch serves the
  fs-watcher followers — each subscriber fires exactly once. No new
  IPC channel, no new file convention. (The inotify branch firing
  the fs-watcher callbacks was completed 2026-05-28. Before that date the
  shared distribution skipped them, and live deletes were dropped with no
  message. See the CHANGELOG.)

Register with:

```c
tranger2_set_rt_key_deleted_callback(handle, cb, user_data);
```

…on any handle returned by [`tranger2_open_rt_mem`](#tranger2_open_rt_mem),
`tranger2_open_rt_disk` or `tranger2_open_iterator`. Pre-2026-05-26
followers that polled their cache on a timer can drop the timer.

Memory: `project_tranger2_delete_record_deferred`.

### 2.10 Durability

`tranger2_append_record` performs the write but **does not `fsync`**
([`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c)). Durability is whatever the OS gives you —
on [EXT4](https://en.wikipedia.org/wiki/Ext4) with the default journal, that is "data on disk within the
journal commit interval, usually 5 s". If you need stronger guarantees,
add an explicit `fsync` in the wrapping code, but understand the
throughput cost.

---

## 3. treedb

### 3.1 The graph model

A treedb sits inside a tranger. Topics become entity types, nodes
become records keyed by `id`, hooks are in-memory pointers from parent
nodes to their children, fkeys are persistent references from child
nodes to their parent. Schema is JSON.

The schemas already documented in this repo's docs cover the canonical
examples:

- [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) §2.1-2.3 — `binaries`, `configurations`,
  `yunos`.
- [`REALMS.md`](REALMS.md) §2 — `realms`.
- [`YUNO_AUTH.md`](YUNO_AUTH.md) §4.1 — `users`, `roles`, `users_accesses`.

Read those for the operational shape. This section explains how the
schema *works*.

### 3.2 Topic schema JSON

A real, minimal example (the yuno_agent schema, cut down to one topic). Two
versions live at two levels: `schema_version` is the TREEDB's (§3.1, §3.4),
`topic_version` is each topic's own:

```json
{
  "id":             "treedb_yuneta_agent",
  "schema_version": "24",
  "topics": [
    {
      "id":            "yunos",
      "topic_version": "20",
      "pkey":          "id",
      "pkey2s":        "yuno_release",
      "tkey":          "",
      "system_flag":   "sf_string_key",
      "cols": {
        "id":         { "type": "string", "flag": ["persistent", "required", "rowid"] },
        "realm_id":   { "type": "string", "flag": ["persistent", "fkey"] },
        "yuno_role":  { "type": "string", "flag": ["persistent", "required"] },
        "configurations": { "type": "object", "flag": ["hook"],
                            "hook": { "configurations": "yunos" } }
      }
    }
  ]
}
```

Six things to notice:

1. **`pkey`** — column name that serves as the primary key. Maps to
   `topic_desc_t.pkey`.
2. **`pkey2s`** — optional secondary key (composite). Allows multiple
   records per primary key, for example several versions of a binary.
   [`treedb_get_instance()`](#treedb_get_instance), [`treedb_list_instances()`](#treedb_list_instances) and the agent's
   `instances` command query them. **Invariant (since dbf532ec9):** the pkey2
   secondary index shares the SAME node object as the primary index, and
   [`treedb_save_node()`](#treedb_save_node) points it again on every runtime save. Before that
   correction it held a separate object that only the disk-load filled. A
   runtime `update-node` was therefore invisible through `list_instances`
   until the next reload. That was the bug behind `list-binaries`, which
   showed a stale binary immediately after `update-binary`.
3. **`schema_version`** and **`topic_version`** — these are different.
   Schema is the overall layout. Topic is per-topic. **Raise
   `topic_version` every time you change `cols`** — §3.5.
4. **`cols`** declares typed columns. Type + flag list (next section).
5. **`fkey` field on the child** points at *(parent topic, hook name)*.
   Persisted.
6. **`hook` field on the parent** points at *(child topic, child fkey
   name)*. Rebuilt in-memory at load time.

Two optional topic keys are not in the example, because `yunos` needs
neither:

- **`system_topic: true`** — the topic cannot be deleted, not even with
  `force`. See §3.10.
- **`main_topic: true`** — the topic that the tree of the treedb hangs from
  (since 7.19.0). Viewers use it: the treedb graph opens its tree from this
  topic. Only a topic with a hook to **itself** can carry the mark (places
  inside places), and only one topic per treedb. `treedb_open_db()` logs a
  mark that breaks a rule and ignores it. Like any change to a topic, the
  mark is published by raising the `topic_version` of that topic, and the
  `schema_version` of the treedb when the runtime must use it. See §3.11.

An example of the mark, as a C schema literal. Places hold places, and each
place holds devices:

```c
static char treedb_schema_sample[]= "\
{                                                                   \n\
    'id': 'treedb_sample',                                          \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'places',                                         \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'main_topic': true,                                     \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'children': {                                       \n\
                    'header': 'Children',                           \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'places': 'parent'                          \n\
                    }                                               \n\
                },                                                  \n\
                'parent': {                                         \n\
                    'header': 'Parent',                             \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'devices': {                                        \n\
                    'header': 'Devices',                            \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'devices': 'place'                          \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'devices',                                        \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'place': {                                          \n\
                    'header': 'Place',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
```

The hook `places.children` points at `places` itself, through the fkey
`parent`. That self-hook is what lets `places` carry the mark. Put the same
mark on `devices` and `treedb_open_db()` logs an error and ignores it:
`devices` has no hook to itself. The parentless places are the roots of the tree, and the devices
hang from their place.

### 3.3 Column types and flags

Column types live in the JSON spec, parsed by [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c). There
are nine, and the `__system__` treedb refuses any other: `string`,
`integer`, `real`, `boolean`, `object` / `dict`, `array` / `list`, `blob`.
`enum`, `wild`, `email`, `url`, `password` and `time` are **flags** on one of
those types, never a type:

```c
'status': {
    'header': 'Status',
    'fillspace': 10,
    'type': 'string',
    'flag': ['persistent', 'enum'],
    'enum': ['on', 'off']
}
```

The keys of a topic are declared on the **topic**, not with a column flag:
`pkey` (always `id`), `pkey2s` for the secondary keys, `tkey` for the time
key:

```c
{
    'id': 'yunos',
    'pkey': 'id',
    'pkey2s': 'yuno_release',
    'tkey': '',
    'cols': { ... }
}
```

Flags (parsed by [`kw_has_word`](#kw_has_word) throughout [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c)):

| Flag         | Effect                                                                  |
|--------------|-------------------------------------------------------------------------|
| `persistent` | Written through to timeranger2 on save.                                 |
| `required`   | Cannot be null at creation.                                             |
| `notnull`    | Cannot be null ever.                                                    |
| `hook`       | Parent → children link. In-memory only (rebuilt on load from children's fkeys). Never on the same column as `fkey`: the schema is refused. A link that would hang a node from its own descendant through the SAME hook is refused (a tree is a tree); a cycle through two different hooks is accepted. |
| `fkey`       | Child → parent reference. Persisted. Encoded as `topic^parent_id^hook_name`. A node that is also a parent carries its hook in ANOTHER column. |
| `uuid`       | On the `id` column: a create that sends no `id` gets a random UUID.     |
| `rowid`      | On the `id` column: a create that sends no `id` gets one past every id the topic ever handed out. Never reused. |
| `qualified`  | On the `id` column: a create that sends no `id` gets the id of its parent, a dot, and its own name. |
| `password`   | Treated as opaque secret on inspection.                                 |
| `email`/`url`/`enum`/`wild` | Semantic types, mostly informational.                    |
| `inherit`    | Inherits a value from a related node.                                   |
| `time`/`now` | The column holds an instant (an integer epoch). `now` means the CLOCK writes it, whatever the kw says, on EVERY write — the create and every update, `writable` or not, persistent or volatile: *when this record was last written*. A `time` column WITHOUT `now` gets the clock at the create when the kw brings no value, and an update leaves it alone: *when this record was born*. (7.24.0 stamped a `now` column on an update only if it was `writable`; since after 7.24.1 `writable` plays no part.) |

Absence of `persistent` + absence of `hook`/`fkey` means **volatile** —
in-memory only.

The whole vocabulary a column may carry is the `enum` of the `flag` column of
`treedb_system_schema.c`, and a flag outside it is refused: `persistent`,
`required`, `notnull`, `wild`, `inherit`, `readable`, `writable`, `hidden`,
`stats`, `rstats`, `pstats`, `hook`, `fkey`, `enum`, `template`, `uuid`,
`rowid`, `qualified`, `password`, `email`, `url`, `time`, `now`, `date`,
`color`, `image`, `icon`, `file`, `tel`, `table`, `id`, `currency`, `hex`,
`binary`, `percent`, `base64`, `coordinates`, `gbuffer`. There is no `pkey`,
`pkey2` or `tkey` flag.

**`uuid`, `rowid` and `qualified` are the three ways the store hands a key
out, and a column carries at most one of them.** All three sit on the `id`
column, and all three act only when the create sends no `id`: an `id` in the
kw is always kept as it is. They are not equivalent.

- `uuid` gives an address that is unique everywhere and means nothing to a
  person.
- `rowid` gives one past every id the topic ever handed out, kept in the
  topic's `topic_var.json` as `last_rowid_id`, and never reused, not even after
  its node is deleted (a snap's id rides the records it tagged). That address
  is unique but arbitrary: it does not reproduce, and a `rowid` pkey has no
  update, so an editor that
  saves a record appends a second one instead of changing the first. It is
  here for the stores that already use it. Do not declare it in a new topic.
- `qualified` gives a name: the id of the parent, a dot, and the name of the
  record. The name is the first secondary key of the topic (`pkey2s`), and the
  parent is the one named in the fkey of the kw. Full story in §3.11.

`qualified` has three conditions. [`treedb_create_node()`](#treedb_create_node)
logs the cause and creates nothing when one of them is not true:

1. The topic declares `pkey2s`, and the kw carries that field. That field is
   the name.
2. The kw carries an fkey with a parent. A `qualified` record is always the
   child of something.
3. The composed id is not longer than a record key
   (`RECORD_KEY_VALUE_MAX`). A key too long is refused, never trimmed: a
   truncated id is the address of another node.

The separator is a dot, and it cannot be `^`. That is the character an fkey
reference is split on, so an id that carries one makes every reference to that
node undecodable (§3.11).

### 3.4 The `__md_treedb__` metadata block

Every loaded node carries a metadata sidecar ([`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c),
attached at [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c)):

```json
"__md_treedb__": {
    "treedb_name": "treedb_yuneta_agent",
    "topic_name":  "yunos",
    "g_rowid":     14,
    "i_rowid":     14,
    "t":           1737499200,
    "tm":          1737499200,
    "tag":         0,
    "pure_node":   true
}
```

- `g_rowid`, `i_rowid` — see §2.3. Never set them yourself.
- `t`, `tm` — the timeranger2 timestamps, surfaced to the node level.
- `tag` — user_flag from md2: the snap that froze this record, 0 for a record no snap holds (§2.8).
- `immutable` — present **only when set** (omitted on ordinary nodes).
  `true` means the record carries the `sf_immutable_record` md2 bit and
cannot be deleted. See §3.10.
- `pure_node` — true for ordinary nodes. This is the metadata that you read.

A node that appears in multiple places in a JSON dump (once under the
topic's `id` index, once nested inside its parent's hook) carries the
**same** `__md_treedb__` everywhere. Same record, multiple views.

### 3.5 The `topic_cols.json` versioning trap

Memory
`feedback_treedb_schema_versioning`:

> Any `cols` change needs a higher `topic_version`. If you do not raise it,
> the persisted `topic_cols.json` continues to mask the new schema. Delete
> `store/` when you reproduce the problem.

What happens: [`treedb_open_db()`](#treedb_open_db) ([`tr_treedb.c:485`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L485)) reads the
persisted `topic_cols.json` and compares its `topic_version` against
the schema in code. If they match, the persisted file wins. If you
edited the schema in code but forgot to bump `topic_version`, your
running yuno sees the **old** schema and silently ignores any new
columns you added.

The fix:

1. Bump `topic_version` in the schema JSON every time you change
   `cols`.
2. While debugging schema problems, wipe the topic's directory in
   `store/` to force a clean load.

One change does NOT wait for the bump: a schema whose columns are the same
and say the same things, in a different **order**. The freeze exists so that a
change to what a column declares cannot arrive unannounced; an order declares
nothing new, so `tranger2_create_topic()` rewrites the file for it (§3.11).

### 3.6 Node CRUD: the public API

Two layers — `treedb_*` (the low-level graph API) and `gobj_*node` (the
gobj wrappers most user code uses).

Low-level ([`tr_treedb.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.h)):

```c
json_t *treedb_create_node(json_t *tranger, const char *treedb_name,
                           const char *topic_name, json_t *kw);
json_t *treedb_update_node(json_t *tranger, json_t *node, json_t *kw, BOOL save);
int     treedb_delete_node(json_t *tranger, json_t *node, json_t *jn_options);
json_t *treedb_get_node   (json_t *tranger, const char *treedb_name,
                           const char *topic_name, const char *id);
json_t *treedb_list_nodes (json_t *tranger, const char *treedb_name,
                           const char *topic_name, json_t *jn_filter,
                           BOOL (*match_fn)(json_t *node, json_t *jn_filter));

// links (graph operations)
int     treedb_link_nodes  (json_t *tranger, const char *hook_name,
                            json_t *parent_node, json_t *child_node);
int     treedb_unlink_nodes(json_t *tranger, const char *hook_name,
                            json_t *parent_node, json_t *child_node);
```

gobj-level wrappers ([`gobj.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/gobj.h)):

```c
json_t *gobj_create_node(hgobj, const char *topic, json_t *kw, json_t *opt, hgobj src);
json_t *gobj_update_node(hgobj, const char *topic, json_t *kw, json_t *opt, hgobj src);
int     gobj_delete_node(hgobj, const char *topic, json_t *kw, json_t *opt, hgobj src);
json_t *gobj_list_nodes (hgobj, const char *topic, json_t *filter, json_t *opt, hgobj src);
int     gobj_link_nodes  (hgobj, const char *hook,
                          const char *parent_topic, json_t *parent_rec,
                          const char *child_topic,  json_t *child_rec, hgobj src);
int     gobj_unlink_nodes(hgobj, const char *hook,
                          const char *parent_topic, json_t *parent_rec,
                          const char *child_topic,  json_t *child_rec, hgobj src);
```

Most production code calls `gobj_*node`. Those functions route to the right
treedb from the `priv` of the gobj, and they integrate the authzs and the
traces.

**What a write validates** (since 7.13.0 — before it, less than this):

| | create | update |
|---|---|---|
| Type of each field, per the topic's cols | yes | **yes** |
| `required` on a missing field | yes | n/a (the node already has one) |
| `notnull` | yes | **yes** |
| `enum` membership | **yes** | **yes** |
| A pkey2 value | names the instance | **must not change** (refused) |
| A `now` column | stamped | **stamped** |

The `now` row is the one exception to *"only the fields the kw carries are
normalized"*: no kw ever carries a `now` column, because the whole point of
the flag is that the clock writes it and not the caller. Until 7.24.0,
`__graphs__.time` — the instant a treedb layout was saved — kept the instant
of the FIRST save for the life of the record. 7.24.0 made `writable` the gate,
and every "Update Time" of the projects, declared without it, stayed frozen at
the create. Now the two meanings are two flags: `now` is *when it was last
written* (`__graphs__.time`), and a `time` column without `now` is *when it was
born* (`__assets__.t`, the instant the bytes arrived — a rename of the asset
leaves it alone).

```C
'updated': {
    'header': 'Update Time',
    'type': 'integer',
    'flag': ['persistent', 'time', 'now']   /*  every write stamps it  */
},
'created': {
    'header': 'Create Time',
    'type': 'integer',
    'flag': ['persistent', 'time']          /*  the create stamps it  */
}
```

An update used to store whatever it was handed: no type, no `notnull`, no
`enum`. And `enum` was checked only when a *schema* was parsed, never when a
*node* was written, so the list a column declares did not survive the first
write — on either path. Both now run the same normalization, and an update
validates every incoming field **before** touching the node, so a refusal
leaves nothing half-applied.

A refused write returns NULL (`-1` for links), and `cmd_create_node` /
`cmd_update_node` answer `result: -1` with the cause. Until 7.13.0
`mt_update_node` dropped the return of `treedb_update_node` and answered the
collapsed view of the unchanged node — a refused update read as a success.

**Who may run each command.** `C_NODE` asks a permission inside every command
that reads or writes the treedb, with or without the global
`enable_command_authz` gate. The permission is the name of a role's
`permission` (or `*`), on the treedb's service. It matters only in a yuno
with an authz checker (`C_AUTHZ`). Without one, every permission is granted.

| Permission | Commands |
|---|---|
| `read` | `nodes`, `node`, `instances`, `pkey2s`, `parents`, `children`, `jtree`, `hooks`, `links`, `snaps`, `snap-content`, `print-tranger`, `export-db`, `treedbs`, `treedb-info`, `topics`, `desc`, `descs`, `schema-file`, `system-schema`, `set-link-events` (shown) |
| `create` | `create-node`, `import-assets`, `shoot-snap` |
| `update` | `update-node`, `link-nodes`, `unlink-nodes`, `set-link-events` (changed), `trace`, `activate-snap`, `deactivate-snap` |
| `delete` | `delete-node`, `gc-assets` |
| `create` and `update` | `import-db` |
| none | `help`, `authzs` |

**`descs` and `schema-file` are two different documents, and the difference
matters when a schema does not do what its literal says.** `descs` is the
schema the treedb is USING: one desc per topic, cols as a LIST, hooks
resolved. `schema-file` is the `<treedb>.treedb_schema.json` that sits beside
the topics on disk — cols keyed by name, the `schema_version`, a
`topic_version` per topic — which is what the C literal is compared against,
and what WON when the store already held a newer version than the one the
yuno was compiled with. The treedb GUI's *schema json* button reads it.

```bash
ycommand -S treedb_yuneta_agent -c "schema-file"
# {"id": "treedb_yuneta_agent", "schema_version": "24", "topics": [...]}
```

`update-node` with `options.create=1` is the upsert the SPAs create with. It
asks for `update`, plus `create` when the node does not exist yet. A refusal
answers `-403 No permission to '<permission>' in service '<treedb>'`, and
nothing is written. Until 2026-09-15 only `nodes` and the node writes asked;
`node`, `link-nodes`, `import-db` and the snaps answered anyone.

A read-only role for one treedb, in that yuno's `treedb_authzs`:

```bash
ycommand -c 'command-yuno id=<yuno> service=treedb_authzs command=create-node topic_name=roles record={"id":"devices_viewer","description":"Reads the devices treedb","realm_id":"*","service":"treedb_devices","permission":"read"}'
```

### 3.7 The link/unlink-saves-child rule

**A link writes the CHILD, never the parent**, and it writes only when the
child actually moved.

The persistent half of a relationship is the child's `fkey` field. The
parent's `hook` is in memory and is rebuilt on the next load by scanning the
children for `fkey == parent.id`. So:

```c
PUBLIC int treedb_link_nodes(...) {
    BOOL child_changed = FALSE;
    _link_nodes(..., &child_changed);
    if(!child_changed) {
        return 0;               // the link was already written
    }
    return treedb_save_node(tranger, child_node);   // only the child
}
```

Three consequences:

1. After a link that moved the child, the child's `g_rowid` advances by one.
   The parent's does **not**.
2. A link that only fills the parent's HOOK writes nothing. That is the
   ordinary case of a second instance of a node: the instance inherits the
   fkey of the instance before it (the ref names the parent's **id**, which
   both instances share) and the hook of the new parent is empty. Before
   2026-09-20 each one appended a record identical to the one under it —
   the agent did it on every `create-yuno`, to `binaries` and to
   `configurations`.
3. A link asked twice, with nothing to move on either side, writes nothing
   and publishes nothing. It warns: *"Parent ref already in child fkey,
   skipping duplicate"* / *"Child already in parent hook, skipping duplicate
   link"*.

The link EVENT (`EV_TREEDB_NODE_LINKED`, or `EV_TREEDB_NODE_UPDATED` for a
host that did not ask for link events) follows either side: filling a hook
is a new relationship in memory, even when nothing is written.

If you write tooling that watches rowids, the parent's rowid is a **bad**
signal of "has anything happened to this node's relationships" — look at the
children.

### 3.8 Cross-yuno reads: the `rt_by_disk` pattern

When a non-master yuno needs to read another yuno's store, it opens
the master's database in read-only mode and registers an
`rt_by_disk` watcher. The master, on every change, writes hardlinks
into `disks/<rt_id>/` for that subscriber. The subscriber's
filesystem watcher fires, and it re-reads the hardlinks.

Memory
`feedback_cross_yuno_via_store_not_command`:
in wattyzer (and by extension other multi-yuno SPAs), cross-yuno
queries from the SPA go through `db_history_wz` reading B+ yunos'
stores *non-master* via this pattern. **[`cmd_command_yuno`](https://github.com/artgins/yunetas/blob/7.25.4/yunos/c/yuno_agent/src/c_agent.c#L6286) does not
work** for B+ yunos, because they do not publish their service through
`__top_side__`. The store path is the correct one.

Code: `tranger2_open_rt_disk` at [`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h). The
mechanism is purely filesystem-mediated — no socket between the master
and the watchers.

### 3.9 Snapshots (treedb-level)

A snap is a **photo of an instant**, and it is never written into. This
section is the whole behaviour, as it was walked step by step on a node in
September 2026.

```c
int     treedb_shoot_snap   (json_t *tranger, const char *treedb_name,
                             const char *snap_name, const char *description);
int     treedb_activate_snap(json_t *tranger, const char *treedb_name,
                             const char *snap_name);
json_t *treedb_list_snaps   (json_t *tranger, const char *treedb_name,
                             json_t *jn_filter);
```

`gobj_list_snaps(gobj, filter, src)` is the gobj-level wrapper. The commands
of the agent are `shoot-snap`, `activate-snap name=<name>`, `deactivate-snap`
(which is `activate-snap name=__clear__`) and `snaps`.

#### What a snap writes

A snap is a row of the `__snaps__` topic. Its `id` is the tag, a number
handed out by the `rowid` flag. `shoot-snap` stamps that number on the md2
`user_flag` of the **current primary record of every key**, in place:

- one record per key, not one per instance: the record that is live at the
  shot;
- the meta-topics are skipped — **except `__graphs__`**, which holds how the
  treedb was ARRANGED and is as much what the store looked like as the
  records are, so the photo carries it (see the table below). `__snaps__`
  cannot tag itself, and `__assets__` is held by a snap another way:
  `assets_held_by_snaps()` walks the links of the records the snap froze,
  because its blobs are shared by every treedb of the tranger;
- a record an earlier snap already tagged cannot take a second tag
  (`user_flag` is one `uint16_t`), so that one is **cloned**: the clone is
  appended with the new tag and becomes the newest record of the key.

**Only `shoot-snap` tags a record, and a record is tagged once.** A save
never gives a tag, active snap or not. Two earlier rules broke this and are
gone: a save that inherited the tag the node carried in memory (so the
latest snap followed every later update and froze nothing; until 7.22.x), and
a save that took the tag of the ACTIVATED snap (so a binary installed during a
rollback became part of the photo, which then held two records of one key;
7.23.x). Since 7.24.0 a save is always untagged.

#### What an activated snap reads

Activating a snap is a **filtered load**, and it filters ONE index:

| Index | With no snap | With snap S activated |
|---|---|---|
| primary (`id`) | the newest record of each key | the newest record of each key **tagged S** |
| secondary (`pkey2`) | the newest record of each `(id, pkey2 value)` | unchanged: the newest record of each `(id, pkey2 value)`, whatever its tag |
| `__graphs__` | the newest layout of each topic | the layout of each topic **tagged S** |

That difference is not a leak, it is **the feature**. Keeping different
versions of a thing and going back and forward between them needs both
halves: the primary index puts the node (and, for the agent, the release it
launches) back to the photo, while the secondary indexes keep every version
installed since, so nothing is lost while the photo is being looked at.

A snap shot before anything was arranged holds no layout, so activating it
leaves `__graphs__` empty and the graph comes back to the automatic layout —
which is what that photo looked like. **A snap shot by a version that did
not tag `__graphs__` behaves the same way**: its records are right and its
arrangement is simply not in it. Shoot a new one to have both.

An activation changes no record: it sets `active` on the `__snaps__` row and
the **reload** rebuilds the indexes. In the agent, `deactivate-snap` is what
performs that reload for every yuno (`restart_nodes()`).

#### Writing while a snap is activated

It is allowed, and what is written carries tag 0. So:

- the photo does not change, however much is written;
- what is written does NOT show in the primary index while the snap is
  active (the load keeps only the records tagged S) and joins it at the
  next reload after the deactivation;
- it does show at once in the secondary indexes, which do not filter.

An install made during a rollback therefore lands on the node without
touching the snap it was rolled back to.

#### Working from a snap: what it ignores, and what it destroys

An activation is a **filtered load, not a restore**. Nothing is rewritten,
nothing is undone, and the store keeps every record it had: what changes is
which record each index answers with. That is what the mechanism is for — to
go back to a state that was marked as good, either to LOOK at it, or to carry
on working from there.

The second use has a consequence worth stating as a rule:

> **Working from an activated snap IGNORES everything written after the shot
> — for every key you touch — and it ignores it without destroying it.**

The mechanism is the one above, read forwards. The primary index answers with
the record the snap froze, so the node you read is the node of the photo.
Saving it appends a NEW record, tagged 0, whose content is *the photo's
content plus your change* — never the content of the records written in
between. That new record is the newest of its key, so after the deactivation
and its reload it is the primary. The records in between stay on disk, stay
in the secondary indexes, and stop being what the treedb reads.

One key of `binaries`, `id = ycommand`, walked through:

| rowid | written | content | tag | primary with S active | primary after deactivating |
|---|---|---|---|---|---|
| 1 | before the shot | `7.21.0` | S | **yes** | no |
| 2 | after the shot | `7.23.0` | 0 | no | no |
| 3 | from inside S, editing what rowid 1 said | `7.21.1` | 0 | no | **yes** |

Row 2 is not lost — it is on disk, and `treedb_list_instances()` still finds
it through the secondary index — but nothing reads it as the current state
any more. That is the whole of "ignoring".

Three things follow, and they are the ones that surprise:

**It is per key, not per store.** The activation does not put the treedb into
a past state; it makes the past the thing you WRITE FROM. A node you never
touch keeps the record written after the shot as its newest one, so the
deactivation brings it back exactly as it was, post-shot content included.
Only what you edit is carried back.

**A node born after the shot is invisible while the snap is active**, because
no record of it carries the tag. And [`treedb_create_node()`](#treedb_create_node)
tests existence against that same FILTERED primary index, so a create of that
id is accepted: it appends a record on top of the one already there. It reads
like a create and behaves like an overwrite. (With `pkey2s` the secondary
index is not filtered and still holds the node, which is why the create is
refused only when BOTH indexes already have that id.)

**A delete DOES destroy, and it is the only operation that does.** The two
guards of §3.9 refuse to take a record a snap holds, but a node born after the
shot is held by no snap: the delete goes through, erases the key, and the
deactivation does not bring it back. Everything else inside a snap is
additive; this one is not.

| From inside an activated snap | What it does to what came after |
|---|---|
| read | hides it: the primary answers with the photo |
| update / save | ignores it for that key: the new record descends from the photo |
| create of an id born after the shot | the same, by another door: it appends over it |
| delete | **destroys it**: the key is erased, and deactivating does not undo that |
| shoot-snap | **refused** (since 7.25.0): *"Cannot shoot a snap while snap 'S' is active: deactivate it first"* |

**No shot from inside a snap.** With S active the primary index holds S's
records, all of them tagged, so a shot would have to CLONE every key (a record
takes one tag) — and the clone, being the newest record of its key, becomes
the primary after the deactivation. The whole treedb would be back to S: the
activation as a RESTORE, which is exactly what this design refuses. So
`treedb_shoot_snap()` refuses while a snap is active, and also while the treedb
is still loaded from one (deactivated but not reloaded: *"reload it first"*).
To keep the state you reached from inside a snap, deactivate, reload, then
shoot:

```
deactivate-snap                     # reload onto the newest records
shoot-snap name=after-the-fix       # a photo of the live state
```

A replica can do none of it: shooting, activating and deactivating are writes
of `__snaps__`, and a replica answers *"READ-ONLY"* (C_NODE) or *"Only master
can shoot a snap"* / *"… activate a snap"* (the library).

**Going forward again.** Leaving a snap undoes nothing either: deactivating
and reloading puts every key back on its newest record. For the keys written
from inside the snap, that newest record is the one that descends from the
photo — which is the point of having worked there. And the versions installed
in between are still addressable, because the secondary indexes never
filtered: that is how the agent moves forward again, re-appending the highest
release with `promote_highest_release_yunos()`.

#### What a snap protects

A snap holds the records it tagged, and two deletes ask before taking them:

- **[`treedb_delete_node()`](#treedb_delete_node)** erases the whole key, so
  it refuses a node any existing snap holds a record of: *"cannot delete
  node, a snapshot still holds it"*.
- **[`treedb_delete_instance()`](#treedb_delete_instance)** tombstones every
  md2 row of one `(id, pkey2 value)`, so it asks the same question narrowed
  to that instance: *"cannot delete instance, a snapshot still holds it"*.

Neither guard reads the tag the node carries in memory, and a guard that
cannot read the records refuses (since 7.25.0: it used to let the
delete through). A save is untagged,
so a node or an instance updated after the shot carries 0 while the record
the snap froze is still under it: the guards walk the records of the key.
`ignore_snaps=1` overrides both; **`force=1` does not** (since 7.25.0). `force` unlinks the children and nothing else: it used to override
the snapshot guard too, and the agent's `delete-yuno` and gobj-ui's topic
table force every delete for the children, so no snapshot guard ever fired
for them. The agent keeps its own contract -- its `force=1` on the
`delete-*` commands still means "even if a snap holds it", and it passes
`ignore_snaps` for that. `treedb_gc_files()` follows the same rule for the
bytes of an asset a shot record names.

**What cannot be read is not "held by nothing".** Every guard fails CLOSED:

- `treedb_delete_instance()` refuses, before it tombstones or drops anything,
  when it cannot read every row of the key: *"Cannot delete instance, cannot
  read every row of its key"* (a row's metadata) or *"..., a row of its key
  cannot be read"* (a row's content, which cannot say whose instance it is).
  Until 7.25.4 it tombstoned the rows it had read, dropped the slot, answered
  0, and the instance came back at the next open.
- The asset guard (`treedb_gc_files()`, and `treedb_delete_node()` of an
  `__assets__` node) walks the tagged records of every topic with a `file`
  column. A tagged record of an existing snap that cannot be read, or a topic
  whose walk does not load, refuses: the gc answers `NULL` (*"gc refused:
  cannot tell which assets a snapshot links"*, and `gc-assets` answers -1), the
  delete answers -1 (*"cannot delete asset, cannot tell whether a snapshot
  links it"*). Until 7.25.4 the gc took the blob a snapshot needed.

```C
json_t *taken = treedb_gc_files(tranger, "treedb_files", FALSE);
if(!taken) {
    // refused: nothing was taken, the cause is in the log
}
```

**A failed activation keeps the snap it found.** `treedb_activate_snap()`
saves the active snap inactive, then the new one active. When the second save
fails it saves the old one active again (*"Cannot activate snap, the one active
before is active again"*): until 7.25.4 the treedb was left with no active snap,
and the next load went to the latest instances.

#### A child hooked by several instances of its parent

A child's fkey names the parent's **id**, not one of its instances
(`yunos^<yuno id>^binary`), so every instance of the parent can hook it: the
agent's `find-new-yunos create=1` leaves the same binary in the hook of the
old release and of the new one. Two rules keep that consistent (since 7.25.0):

- **An unlink takes the child out of the hook of EVERY instance of the
  parent.** It clears the one ref the child has, so no instance may go on
  hooking it. It used to leave the other instances holding it, and those
  could then be neither unlinked nor deleted, even with `force`, until a
  reload.
- **A DICT hook keeps the child's PRIMARY instance.** A dict hook holds one
  entry per child id; a new instance of the child (an `install-binary` of a
  new version) no longer replaces the entry unless it is the primary, as an
  array hook keeps the one it has. It took the newest: a `delete_instance` of
  that newest left it in the hook, and a forced delete of the parent then
  SAVED the deleted instance back to disk, the primary after a reload.

```C
/*  the agent's shape (treedb_schema_yuneta_agent.c): a dict hook over
 *  `binaries`, a topic with a pkey2 (`version`) of its own  */
'binary': {
    'header': 'binary',
    'type': 'object',
    'flag': ['hook'],
    'hook': {
        'binaries': 'yunos'
    }
},
```

#### What the AGENT adds (not treedb)

`deactivate-snap` runs `promote_highest_release_yunos()` before the reload:
the primary of an id is the record with the highest **rowid**, not the
highest `yuno_release`, so the newest release is re-appended to put it on
top. That is why a `yunos` key gets one more record per upgrade cycle, with
the same content as the one under it. It is agent behaviour, deliberate, and
it is what makes the reload start the new version.

Snapshots are also how the agent picks which binary version to run when
several are stored — see [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) §4.3. The
binary resolver tries the active snapshot first
([`gobj_list_snaps`](#gobj_list_snaps)); if that fails it does a direct
`(role, role_version)` lookup.

#### A cycle, end to end

```
shoot-snap name=S           # tags the live record of every key
activate-snap name=S        # + reload: primary index = the photo
install-binary ...          # a new record, tag 0: the photo is untouched
find-new-yunos create=1     # a new yuno instance, tag 0
deactivate-snap             # + reload: primary index = the newest again
activate-snap name=S        # and back, as many times as you want
```

### 3.10 Immutable nodes and non-deletable topics

Some records must never be deleted by CRUD (the seed `root` role and
`yuneta` user — see [`YUNO_AUTH.md`](YUNO_AUTH.md) §4.2), and some topics
must never be dropped (the `__system__` treedb's structural topics, and
every treedb's `__snaps__` / `__graphs__` / `__assets__`). The protection is **metadata,
never a data column** — it does not touch the user schema and never bumps
`topic_version`. Design write-up:
[`DESIGN-immutable-topics-records.md`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/DESIGN-immutable-topics-records.md).

**Record level** rides a free md2 `system_flag` bit, `sf_immutable_record`
(`0x0800`, inherited band) — the same metadata channel as the snapshot
`tag`, persisted on disk and decoded on every load:

- Set it with `treedb_set_node_immutable(tranger, node, set)`, which
  rewrites the node's current primary record **in place** (no new record)
  via the gated `tranger2_set_system_flag()`, and flips
  `__md_treedb__`immutable` in memory.
- `treedb_save_node()` re-stamps the bit after every update (the re-append
  inherits only the topic-default `system_flag`, so the bit is re-applied).
  The snap `tag` is NOT re-applied: a save is always untagged; only
  `shoot-snap` tags a record (§2.8).
- `treedb_delete_node()` and `treedb_delete_instance()` refuse an immutable
  record, and **`force` does NOT override** (stronger than the snapshot-tag
  guard). `tranger2_delete_instance()` carries the same refusal as a
  backstop.
- Because the mark is not a JSON field, a client **cannot inject it** via
  `create-node` / `update-node` — only an in-process `mt`-level caller can
  set it. No strip boundary needed.

**Topic level** rides `system_topic: true` in the topic's `topic_var.json`
(additive, no `topic_version` bump). Declare it in the schema next to
`topic_version`, or pass `system_topic=TRUE` to `treedb_create_topic()`.
`treedb_delete_topic()` (and `tranger2_delete_topic()` as a backstop) refuse
it. A system topic's **records stay deletable** — only the topic is frozen.

**Out of scope on purpose:** `delete-treedb` / a whole-store `rm -rf`. This
protects against CRUD/control-plane deletion, not against an operator wiping
the realm — "only a full store wipe removes them". Regression coverage:
[`tests/c/tr_treedb_immutable`](https://github.com/artgins/yunetas/tree/7.25.4/tests/c/tr_treedb_immutable).

**Declaring the seed: the `initial_load` attr of `C_NODE`.** Marking a record
immutable protects it; it does not put it there. The records a system cannot
come up without — the seed role, the admin account, the root of the tree a
scope hangs from — are declared in the treedb's configuration, and `C_NODE`
applies them in `mt_start` right after it opens the treedb, master only:

```json
"initial_load": {
    "roles": [
        {
            "id": "root",
            "disabled": false,
            "description": "Super-Owner of system",
            "realm_id": "*",
            "parent_role_id": "",
            "service": "*",
            "permission": "*"
        }
    ],
    "users": [
        {"id": "yuneta", "roles": ["roles^root^users"]}
    ]
}
```

That is the agent's own seed, as `yuno_agent/src/main.c` hands it to
`C_AUTHZ` (`Authz.initial_load`): the `root` role, and the `yuneta` user
hanging from it through the `users` hook of `roles`.

One entry per topic, a list of records, and **the links ride inside the
record as fkey values** (`parent_topic^parent_id^hook`) — the same form the
child stores, so what you declare is what you would read back. Reach it
through `open-treedb`'s `initial_load` parameter, or set the attr directly
when a gclass builds its own `C_NODE` (`C_AUTHZ` hands down its
`Authz.initial_load` that way).

What the loop does on **every** start, in **two passes** — the way
`treedb_open_db()` brings a store up from disk, records first and links
second, so that both ends of a link exist before it is written and the order
of the topics in `initial_load` does not matter (`users` could come before
`roles` above):

1. **Records.** A record that is **missing** is created, without its fkey
   values. A record that is **present** is never rewritten. Either way it is
   marked immutable.
2. **Links.** Every link a seed declares and does not have is written —
   all of them for a record just created, the missing one for a record that
   lost it.

It links, and it never re-writes, for a reason: an autolink over an existing
node replaces its links by the ones the record names
(`treedb_replace_links()`), which drops every link the seed does **not**
declare — including the ones a person added on purpose (§4.10 and the
partial-update trap in §3.6).

**A link a seed is declared with is as immutable as the seed.** The
immutable mark is one md2 bit on the *record*, and `tr_treedb` does not know
which links matter; the declaration does. So `C_NODE`, the owner of
`initial_load`, refuses the four writes that can cut a declared link, and
`force` overrides none of them:

- `unlink-nodes` of it: *"initial_load: cannot unlink a seed link"*.
- an `update-node` with `autolink` that does not repeat it (the partial-update
  trap: what kw omits, `treedb_replace_links()` drops): *"initial_load: update
  would drop a seed link"*. Repeat the declared refs in the update and it goes
  through.
- `delete-node` of the **parent** the seed hangs from — the one cut that never
  passes through `unlink_nodes`, because with `force`
  `treedb_delete_node()` unlinks every child itself: *"initial_load: cannot
  delete the parent of a seed link"*.
- `link-nodes` into a **single-valued** fkey: *"initial_load: link would
  overwrite a seed link"*. A link does not always add. `_link_nodes()`
  branches on the shape of the child's fkey column: a list takes the new ref
  beside the ones already there and an object keys it, but a **string** column
  has room for one, and the new ref is written over what it held without a
  comparison. So a link to another parent through a single-valued fkey cuts
  the declared link as surely as an unlink. Re-linking to the very parent the
  seed declares loses nothing and goes through.

No new column flag was needed, and none would do: a flag on the column would
freeze that column for every record of the topic, and the record's metadata
holds one bit, not a list of refs. The declaration is the list. The links a
person adds to a seed **afterwards** are ordinary: they can be cut, and a
node no seed hangs from can be deleted.

A node a seed hangs from should be a seed too. A parent that is not declared
is created by something else (a batch, a person), so on a fresh store the
first pass finds it missing and the second logs *"initial_load: parent of a
seed link not found"* until whatever creates it has run — the link is then
written on the next start. Declare the parent and the seed comes up whole on
the first start.

Do not put anything in `initial_load` that a person is meant to edit or remove
later: everything it names becomes undeletable, and so do the links it names.
It is for what the system cannot start without. Regression coverage:
[`tests/c/c_node_initial_load`](https://github.com/artgins/yunetas/tree/7.25.4/tests/c/c_node_initial_load).

### 3.11 The `__system__` treedb: a schema stored as data

A schema has two homes. The one you write is the C literal
(`treedb_schema_*.c`), persisted as
`<tranger_dir>/<treedb_name>.treedb_schema.json` on first open (§3.5). The
other is the **`__system__` treedb**, which every `C_TREEDB` service builds
next to the treedbs it manages, at `<path>/__system__`. There the same
schema is stored **as ordinary treedb data**:

```
treedbs   ── id, schema_version, c_schema_version,
             system_schema_version ──hook topics──▶
topics    ── id (<treedb>.<topic>), value, order, pkey, pkey2s, system_flag,
             tkey, topic_version, system_topic, main_topic ──hook cols──▶
cols      ── id (<topic id>.<column>), value, order, header, fillspace, type,
             placeholder,
             flag, enum, template, hook, pkey2s, default,
             description, properties
```

Its schema is `treedb_system_schema.c`, and it is the reason a schema can be
read, listed and edited at runtime with the same `nodes` / `create-node` /
`update-node` commands as any other data — no new command surface.

**`main_topic: true` marks the topic the tree of a treedb hangs from** (SDK
7.19). A viewer uses it: the treedb graph opens its tree from it, and the
schema editor shows it as a gold star. Only a topic **hooked to itself**
(places inside places) can carry it, and only one per treedb:
`treedb_open_db()` logs either mistake and ignores the mark. It lives in the
schema and not in the topic files of the store, so it is stamped in memory on
every open, and travels in `tranger2_topic_desc()` (the `desc` / `descs`
commands) together with `system_topic`. Without a mark the graph
deduces the trunk: the hierarchical topic that reaches the most other topics.

**The mark is a change to its topic, and it is published like one:** raise
the `topic_version` of that topic and, when the runtime must use it, the
`schema_version` of the treedb (see *A version is published by whoever
changes the schema*, below). The agent's own schema is the reference:
`realms` carries the mark at `topic_version` 8, in a treedb at
`schema_version` 24.

**`topics` and `cols` are keyed by the QUALIFIED name, and the bare one
lives in `value`.** A name is unique only inside its parent: two topics with
an `id` column would collide on a single `cols` topic keyed by name, and two
treedbs with a `users` topic would collide on a single `topics` topic keyed by
name — `users` is a topic of `authzs`, `mqtt_broker` and `controlcenter`
alike. So the id of a node is **the id of its parent, a dot, and its own
name**: `treedb_yunovatioscodb.yunos` for a topic,
`treedb_yunovatioscodb.yunos.yuno_role` for a column. Unique by construction,
and the projector composes it instead of looking it up.

The separator cannot be `^`. That is the character an fkey reference is split
on (`decode_parent_ref()` requires exactly `parent_topic^parent_id^hook`), so
an id carrying one makes every reference to that node undecodable.

`id` carries the flag **`qualified`**, a third way for the store to hand a key
out beside `uuid` and `rowid`: a create that sends no `id` gets one composed
from the parent named in its fkey and the value of the topic's first secondary
key ([`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c), `build_qualified_id`). So an editor creates a column the
same way it creates any other record.

These two topics used to be keyed by a **rowid** handed out from the topic
size. That address was unique but arbitrary: it did not reproduce, it made
every lookup a linear scan over `value`, and — because a rowid pkey has no
update — an editor saving a column appended a second one instead of changing
it. `migrate_schema_ids_to_qualified()` in
[`c_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_treedb.c) moves a projection made that way, node by node, content
and all, when a store written with an older meta-schema is opened. That
moves ids and re-projects nothing.

The keying is also why the descriptor used to validate a *user* column is
derived, not copied, from that topic: `_treedb_create_topic_cols_desc()`
renames `value` back to `id` and drops the storage-only fields (`id`,
`topics`, `order`, `_geometry`). Add a field for user columns to the `cols`
topic; add a storage-only field there **and** to that skip list.

**`order` is what keeps a schema in shape.** The order of the columns is part
of a schema — it is the order a table paints them in and the order a form asks
for them — and a projection cannot supply it by itself: its nodes are records,
they come back in the order the store holds them, which is the order
`readdir()` returns the key directories in. So the projector stamps the
position each node occupies in the schema compiled in C, and
`get_treedb_schema()` sorts by it and then **removes it**: in a schema the
order IS the sequence of the `cols` dict, and a schema carrying both would
hand every topic a column attribute nobody declared. A node the projection
cannot place — one projected before the index existed — falls back to where C
declares it, and goes last when C does not know it either. `order` defaults to
**9999**, so a column created here by hand, which says nothing about where it
goes, goes last too.

Two more things hold that order down, below the schema. The keys of a topic
are read **sorted** (`find_keys_in_disk()`), because `readdir()` order was
never a contract: the same store read back differently twice, and two replicas
of it differently from each other. And a `topic_cols.json` that differs from
the schema **only in the order** is rewritten instead of waiting for a
`topic_version` bump (§3.5) — the freeze is there to stop a change to WHAT a
column declares from arriving unannounced, and a change to the order announces
nothing new.

The name still has to reach a reader, and that is paid by
`tranger2_topic_desc()`, which carries `pkey2s` with the descriptor since
7.13.1. A qualified id names the record, but it names every ancestor with it,
and a rowid named nothing at all: that is how the agent console came to draw a
graph of cards reading `181`, `225`, `193`. With `pkey2s` in the descriptor a
reader can tell a key that is not the plain name (its column carries the
`rowid`, `uuid` or `qualified` flag) from one that is, and label by the
secondary key instead. The id stays the address; it is not the label.

**Who fills it, and who wins.** `C_TREEDB`'s `open-treedb` projects the C
literal into `__system__` the first time it sees a treedb, and afterwards only
when the literal is **strictly newer**, exactly as `schema_version` already
decides between the literal and the persisted schema file (§3.5). Raising the
version is how either side publishes a change.

The `treedbs` node carries **three** numbers, and they are not
interchangeable:

| | Written by | Means |
|---|---|---|
| `schema_version` | whoever edits the schema (an editor raises it on save) | what this schema is worth to `treedb_open_db` |
| `c_schema_version` | only the projection | which version of the C literal this projection came from |
| `system_schema_version` | only the projection | which version of the **meta-schema** produced it |

A change of the meta-schema re-projects **nothing**. The schema in use can be
a dynamic one, and a projection of the literal overwrites it. Only
structure moves: when a store was written with an older meta-schema, its
rowid ids move to qualified ones.

**A version is published by whoever changes the schema, and nobody else
invents one.** There are two ways to change a schema:

- **From the C literal.** Change a topic and raise the `topic_version` of
  that topic. When the runtime must use the change (maybe not yet), raise
  the `schema_version` of the treedb too.
- **Dynamically**, from an editor (gui_agent, ytreedb). The editor raises
  both versions on save, the change reaches the disk, and the yuno works
  with it.

Reconciliation compares the literal's `schema_version` with the stored one.
A higher literal is projected, with its own numbers. A literal that is
behind a dynamic schema is **not applied** — the schema is now changed
dynamically, which is a decision — and the log says so: *"TreeDB schema from
C is behind the schema in use, not applied"*. It says it only when the literal
is behind the FILE in use, the schema the treedb runs (after 7.25.4). A literal
that IS the file in use but is behind `__system__` -- whose number a
`save-schema` raises past the file, and a withdraw leaves there -- is behind
nothing that runs, and nothing is said (it was told "behind the schema in use"
at every open after a withdraw). An imposed literal behind `__system__` says
*"TreeDB schema from C is imposed, but it is behind __system__: the projection
is kept"*. A new installation that must carry the dynamic changes takes them
into the literal. Inside a projection,
a topic is written only if it is new or the literal raised its
`topic_version` **past the schema file in use** (see *One rule for a raised
topic*, below). A topic that the literal changed without raising it past the
file is left as it is, with a log line: *"Topic from C differs from __system__
but does not raise its topic_version past the file in use: not applied, the
topic runs from the file"*. A literal newer than `__system__` but NOT newer
than the file in use is not installed by `treedb_open_db()` (the file wins),
so nothing of it is projected either: *"TreeDB schema from C is newer than
__system__ but not than the file in use: not applied, the treedb opens from
the file"*. `c_schema_version` only records which literal the projection came
from, for `diff-schema`.

**"Behind" is judged against the FILE in use too** (after 7.25.3), because
`save-schema` writes the same number into `__system__` that a literal author
writes into the literal. When `__system__` holds a save that was never applied
(its `schema_version` is the file's + 1) and a literal arrives that is newer
than the FILE, `treedb_open_db()` installs the literal -- it is what the treedb
runs -- so it is projected too, with a warning: *"Schema from C is newer than
the file in use but not than __system__: it takes over the file, and replaces
in __system__ the drafts of the topics it raises past the file"*. Judged by
`__system__`'s number alone, that literal was "behind", was never projected,
and the next `save-schema` published the old draft over it, reverting the
literal's change with no word.

**One rule for a raised topic, whichever way the literal arrives** (after
7.25.4). Newer than `__system__` (the ordinary way) or only newer than the file
(the take-over above), a literal installed over the file in use projects a
topic when it raises that topic's `topic_version` **past the file in use** --
whatever number a save gave the draft in `__system__`. That is the topic the
treedb runs from the literal (tranger2 installs a topic only over a lower
`topic_version`), and `__system__` must say what runs: a draft kept there is
what the next `save-schema` publishes, over the developer's change, with no
word. When the topic carried a SAVED draft (its version in `__system__` above
the file's), the log names it: *"Topic from C raised past the file in use
replaces its saved draft in __system__"*. A topic the literal does not raise
past the file goes on running from the file, and its draft in `__system__` --
an operator's edit included -- is kept, with the log line of the paragraph
above.

Until 7.25.4 the two ways had two rules. The ordinary one compared the literal
with the version in `__system__`: an operator saves an edit of `users` (4 in
the file, 5 in `__system__`), the developer ships a literal two versions ahead
raising `users` to 5, and the treedb ran the developer's `users` while
`__system__` kept the operator's and logged *"not applied"*. Taken over (one
version ahead), the same edit was replaced. Before that, the take-over re-wrote
every topic that differed, and an operator's edit of a topic the developer
never touched was lost. With NO file in use at all, the treedb opens from the
literal and every topic is projected: *"No schema file in use: the treedb
opens with the schema from C, projected whole over __system__"*.

**`__system__`'s `schema_version` never goes down** (after 7.25.4). A literal
that takes over is older than `__system__` by definition -- a save raised
`__system__` past the file -- and it used to write its own number there, below
the save. Now the treedb node keeps the higher number and `c_schema_version`
records the literal: with `__system__` at 18 after a save, the file missing,
and a literal 17, `__system__` reads `schema_version: 18, c_schema_version:
17` after the open.

A literal that carries the SAME `schema_version` as a dynamic file in use but
another content is two schemas under one number: the file wins, as ties always
do, and the log says *"Schema from C has the schema_version of the dynamic
schema in use but another content: NOT applied, raise its schema_version to
publish it"* (with the flat diff), at every open until the literal moves on.
The comparison is of CONTENT: cols listed or keyed by name, each carrying its
`id` or not, are the same schema (until 7.25.4 a literal with its cols as a
list warned at every open against the file `apply-schema` wrote).

This all assumes the CLIENT treedb opens as a master. The projection is
decided before its tranger exists, with the `master` of `C_TREEDB`, which
`__system__` also has. If the lock of the client store is held by another
process, the client opens as a replica and runs its file while `__system__`
says the literal; the next open as master installs the literal, and finds its
projection already there.

| `__system__` | File in use | Literal | What happens at open (impose off) |
|---|---|---|---|
| 3, saved, not applied (from 2) | 2 | 3, raises `users` | literal runs; `users` projected over its draft, the other topics' drafts kept; warning |
| 3, `users` saved at 5, not applied | 2, `users` 4 | 4, raises `users` to 5 | literal runs; `users` projected over the saved draft (*"replaces its saved draft"*), the other topics' drafts kept |
| 3, saved, not applied | 2, `users` 4 | 4, `users` changed but left at 4 | literal runs, `users` runs from the file; `users` not projected (*"does not raise its topic_version past the file in use"*) |
| 3, saved, not applied | none | 3 | literal runs, projected whole; warning |
| 3, saved and applied | 3 | 3, other content | file runs, `__system__` kept, warning |
| 3, saved and applied | 3 | 3, the applied content (any form) | file runs, nothing said |
| 3, saved and applied | 3 | 2 | file runs, *"behind the schema in use"* |
| 4, saved then withdrawn | 3 | 3, the file's content | file runs, nothing said |

Up to 7.19.0 the projector did otherwise, and both halves were wrong. It
compared the literal with `c_schema_version`, so a new literal overwrote a
dynamic schema. And it published under `max(stored, literal) + 1`, for every
topic of every re-projection: because the treedb opens from the projection,
those numbers reached `topic_var.json` and `topic_cols.json`, and a store
drifted from its literal although nobody had edited anything.

**`impose_c_schema` takes the schema back from whoever changed it
dynamically.** It is an attribute of `C_TREEDB` (`SDF_RD|SDF_PERSIST`,
default `1`). With it on, `open-treedb` opens every treedb with its schema
from C, and does not read `__system__`. Against the disk the rule of the
versions still applies, with one more case, at both levels (the treedb's
`schema_version` and each `topic_version`):

| Stored version | What happens |
|---|---|
| lower than the literal's | the literal is installed, as always |
| equal | kept |
| **higher** | overwritten with the literal: a dynamic change being reverted |

The log says *"Opening TreeDB with the schema from C, __system__ not read"*,
then *"Imposing TreeDB schema from C over a newer one"* and *"Imposing
topic_version from C over a newer one"* for what it overwrites. `__system__`
keeps every change: they can still be read with `diff-schema`, or taken back
by turning the flag off. The records are not touched — a field that only the
changed schema declared stays in the records and is no longer read.

**It is not read, and the master still writes it.** `__system__` is the only
place a schema can be ASKED for — from ytreedb, from gui_agent, from any node
command — so a treedb that only ever opened with `impose` would have no
projection at all, and the schema it runs could be read from its binary and
nowhere else. Opening with `impose` therefore projects the schema in two
cases, and only in those two:

| Projection in `__system__` | What happens |
|---|---|
| none for this treedb | it is seeded from the literal |
| `schema_version` lower than the literal's | it is re-made from the literal |
| `schema_version` equal or **higher** | left as it is |

The third row is what keeps a dynamic change readable: `save-schema` publishes
an edit by raising the version (*"Save publishes it"*, below), so a saved edit
is never overwritten by the projection, whatever `impose` does to the disk.
That is the half `diff-schema` compares.

**Only the master writes `__system__`** — the ordinary rule of §2.7, and it
holds for the projection as for anything else. A replica reads the treedb from
disk as it is at that moment and reconciles nothing: the master's appends
reach it through the store, and a projection written by two owners is a
projection nobody can read. This is true of an ordinary open too, not only of
an imposed one.

Inside a projection that IS being re-made, `impose` does apply at topic level:
a topic is written because it DIFFERS, not because its `topic_version` is
higher — the same thing the disk gets. Written under the ordinary rule
instead, the projection would say a topic the store no longer holds, in the
one case `impose` exists to repair.

Turn it off (`0`) to let a user or a customer change the schema dynamically
(gui_agent, ytreedb). Turn it back on to impose the code again — because
somebody lost that permission, or because the system was broken or changed
by mistake — and restart the yuno.

**The value is configuration, not state: it is NOT persistent.** Set it where
the yuno is described, preferably its `main.c`, else its config file. A
C_TREEDB created in code is a service, so the `global` section reaches it by
its name (`treedbs`) — better than by gclass, which would reach every
C_TREEDB of the yuno. `impose_c_schema` is the DEFAULT of every treedb that
service opens; to let ONE treedb change dynamically and leave the rest
imposed, name it in `dynamic_schema_treedbs`:

```c
PRIVATE char variable_config[]= "\
{                                                                   \n\
    ...                                                             \n\
    'global': {                                                     \n\
        'treedbs.dynamic_schema_treedbs': ['treedb_wattyzer']       \n\
    },                                                              \n\
    ...                                                             \n\
}                                                                   \n\
";
```

**No command changes it.** Until 7.25.0 the value persisted and
`set-impose-c-schema` saved it: a `set=0` outranked the configuration,
survived every new binary and lived nowhere a deploy could see. The command
is gone, and not kept as an in-memory switch either: the value is read only
when a treedb opens, a treedb opens only when its yuno starts (`close-treedb`
refuses while the yuno plays), and a restart reads the configuration again —
so a value set at run time could never reach a treedb. To change it, change
the `main.c` or the config file and deploy.

`treedbs` lists what the service opened and, for each treedb, whether it
imposes and who decided it (`decided_by`: `code`, `dynamic_schema_treedbs`,
`impose_c_schema`, or `system` for `treedb_system_schema`), with the
versions of the literal, of the schema file in use and of the saved one:

```bash
ycommand -c 'command-yuno id=<id> service=treedbs command=treedbs'
```

**The yuno's code can force it, and then no command undoes it.** A binary
that must impose its schema, whatever its configuration says, says so itself,
per treedb, with the `open-treedb` parameter `impose_c_schema`:

```c
json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:o, s:b}",
    "filename_mask", "%Y",
    "exit_on_error", 0,
    "treedb_name", treedb_name,
    "treedb_schema", jn_treedb_schema,
    "impose_c_schema", 1    // the binary imposes; the attribute cannot undo it
);
json_t *jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb", kw_treedb, gobj);
```

Every treedb of the SDK is forced this way: the agent (`c_agent.c`),
`controlcenter` and `mqtt_broker`. `C_AUTHZ` opens `treedb_authzs` without
`open-treedb`, so it gives the same value to its `C_NODE` (attribute
`impose_c_schema`). Of the projects' `db_history*` yunos, `db_history_co`
forces it; `db_history_wz` and `db_history_ce` name their treedb in
`dynamic_schema_treedbs` in their `main.c`.

The order, from the strongest: the yuno's code, then
`dynamic_schema_treedbs`, then `impose_c_schema` (both as configured in
`main.c` or the config file), then the default `1`. To impose
the law, deploy a binary that forces it. To give the permission back, deploy
one that does not, and from then on the attribute decides again. When the
code overrides a `0`, the log says *"impose_c_schema forced by the code of the
yuno, over the attribute"*, and `treedbs` answers `decided_by: code` for
those treedbs.

This parameter takes the place of `use_internal_schema`, an option of
`open-treedb` that was removed in 7.19.0. That one opened with the literal
too, but the persisted schema file still won when it was newer, so it did not
revert anything.

**Reconciling is an upsert — nothing is ever deleted.** A delete is the one
destructive primitive of the store: it drops the schema's own history (the
reason to keep a schema in a treedb at all) and refuses a snapshot-tagged
node. An update appends a new version instead, so what a column used to
declare stays readable with `instances`. What exists in `__system__` and not
in the incoming schema is left alone: it is indistinguishable from an operator
addition, and removing a topic or a column is a deliberate action, never a
side effect of an upgrade. The one exception is the move to qualified ids,
which has to retire an address the store can no longer reach a node by, and
runs once per store.

**A published topic writes only the columns that changed.** A column node is
written only if it is new or the literal changes it. The comparison is the
one `diff-schema` uses: an attribute that exists only in `__system__` does
not count, because an update does not remove it.

**`diff-schema` says what the projection holds that C does not.** Nothing
deletes, and a version says that *something* was published, never *what*: a
`treedbs` node at `schema_version` 24 with `c_schema_version` 23 was edited in
`__system__`, but the numbers do not say what changed. The command tells
what. It is a command of `C_TREEDB`, the service that owns
`__system__`:

```bash
# One treedb of the node's own agent
ycommand -c 'command-agent service=treedbs command=diff-schema \
    treedb_name=treedb_yuneta_agent'

# Every treedb this service opened with a schema from C
ycommand -c 'command-agent service=treedbs command=diff-schema'

# In any other yuno, through its own C_TREEDB service
ycommand -c 'command-yuno id=<id> service=<treedb_service> command=diff-schema'
```

It answers one row per difference — `treedb`, `kind`, `topic`, `col`, `attr`,
`stored`, `from_c` — and a comment carrying the three stored numbers, the two
the running yuno has, and the count:

| `kind` | Means |
|---|---|
| `changed` | both sides declare the attribute, with different values |
| `only_in_stored` | in `__system__` and not in the schema from C: an operator addition, or something a later schema dropped and the upsert kept |
| `only_in_c` | declared in C and missing from the projection: it never took |
| `version` | the projection came from another release of the schema than the one running, or a topic's stored version is BEHIND it |

Three rules keep the answer readable, and all three are about the store rather
than about schemas.

A treedb record carries **every** column of its topic, filled with the empty
value of its type, so an attribute nobody wrote is stored as `""`, `{}`, `[]`
or `0`. That is not a difference. Read as one, those defaults buried 6 real
differences under 592 on the first run.

**And an attribute the schema never mentions is stored with its DECLARED
default, which is not the empty value of its type.** The projection of a column
copies what the schema declares and no more; the stored node went through
treedb, which fills every attribute the descriptor gives a `default`. So the
attribute is absent on one side and holds its default on the other, and the
comparison read that as an operator addition. `fillspace` defaults to 10 and
almost no schema writes it: on a treedb of 45 columns that was **43 differences
no `Apply` could ever settle**. The projection is deliberately not filled with
defaults instead — it is what the projector UPSERTS, so a default written there
would overwrite the value an operator set by hand.

And the version stamps are not compared as content — whoever publishes a
change raises them — so only the anomaly is reported: a projection that came
from a different release, or a topic the re-projection never reached.

The command compares against the schema the treedb was **opened** with, kept in
memory for that purpose, so it can only answer for a treedb opened with one. A
treedb opened from its projection alone has no other half to compare with.

**A treedb never opens from `__system__`.** It opens from the literal or from
its schema FILE, `<tranger_dir>/<treedb_name>.treedb_schema.json`, and which one
depends on `impose_c_schema`:

| `impose_c_schema` | Opens with |
|---|---|
| on (the default; every in-tree yuno forces it from its code) | the literal, **over** a newer file (a dynamic change being reverted) |
| off | the FILE: the literal is installed only when it is newer, and the file wins on ties and when it is ahead |

`__system__` is where a schema is **edited**: the master projects each literal
into it (seeded, and re-made when the literal moves ahead), and an operator
edits it there. Until after 7.24.1 it was also the source of an open with the
flag off, so every edit — half made or not — was the schema of the next start.

**An edit is a draft; `save-schema` publishes it; `apply-schema` puts it in
use.** Writing a `cols` or `topics` node of `__system__` moves no version. The
cycle is three steps, and each one is a command of `C_TREEDB`:

1. **`save-schema treedb_name=X`** compares the draft with the file IN USE,
   topic by topic. Each topic that differs gets `topic_version` = the one in
   use + 1, the treedb gets `schema_version` = the one in use + 1 (or the
   draft's own number, when it is already ahead: a number of `__system__` never
   goes down), the numbers are written into `__system__`, and the schema is
   written to `saved_schemas/X.treedb_schema.json` under the `__system__`
   tranger — **never** over the file in use. A second save of the same draft
   publishes the same numbers. `dry_run=1` answers the schema it would write
   and writes nothing (the GUI's *export as C literal* uses it). The saved
   schema reads like a literal: no empty attributes, no `_geometry`, no
   projection bookkeeping, and no `default: {}` -- the meta-schema's
   placeholder for "no default" -- on any column, `required` ones included.

   **The trade-off of a `required` container column.** The meta-schema stores
   `{}` for a column that declares no default, and it cannot tell that `{}`
   from a `default: {}` the author wrote. A default fills the field, so
   keeping `{}` would turn `required` off; dropping it loses a `default: {}`
   that was really declared. The second is the lesser harm, and it is what
   happens: a column declared in the literal as

   ```c
   'meta': {                                                   \n\
       'header': 'Meta',                                       \n\
       'type': 'dict',                                         \n\
       'flag': ['persistent','required'],                      \n\
       'default': {}                                           \n\
   },                                                          \n\
   ```

   comes out of `save-schema` + `apply-schema` without its `default`, and a
   record created without `meta` is then refused (*"Field required: 'meta'"*)
   instead of getting `{}`. Send the field, or drop `required`. (Between
   b6f66cdf8 and the review of the second fix round, 2026-09-23, `{}` was kept
   on every `required` column, and every required dict/list/array/blob column
   declared with NO default accepted a record without the field.)

   **A draft taken back is withdrawn by the next save** (after 7.25.4). When
   the draft is the file in use again -- an edit saved, then undone in the
   editor -- and a saved schema newer than the file in use exists, the save
   removes it, logs *"Saved schema withdrawn, the draft is the schema in
   use"*, and says so; `saved-schema` then answers `can_apply: false` and an
   empty `draft_changed`. Before, the save answered *"nothing to save"*, the
   editor's mark never cleared, and `apply-schema` installed the change that
   had been taken back. The versions that save wrote into `__system__` stay (a
   number there never goes down).

   ```bash
   ycommand -c 'command-yuno id=<id> service=treedbs command=save-schema treedb_name=treedb_x'
   # 0: <role>^<name>: the draft of 'treedb_x' is the schema in use: the saved schema_version 13 is withdrawn
   # data: {"treedb_name": "treedb_x", "withdrawn": true, "schema_version": 13,
   #        "path": ".../__system__/saved_schemas/treedb_x.treedb_schema.json", "changes": [...]}
   ```

   With nothing saved, the same answer says *"nothing to save, the draft of
   'treedb_x' is the schema in use"*, with `withdrawn: false` and the
   `schema_version` in use.
2. **`saved-schema treedb_name=X`** answers what was saved, what it changes
   against the file in use (a `flat_diff` of the two: `added`, `removed`,
   `changed`, one row per leaf), and `can_apply`. `draft_changed` names the
   topics whose draft is NOT SAVED: diffed against the saved schema when there
   is one newer than the file in use, against the file in use otherwise
   (until 7.25.3 always against the file in use, so a topic saved a moment ago
   still read as unsaved until an Apply -- for ever on an imposed treedb).
3. **`apply-schema treedb_name=X`** puts the saved schema in place of the file
   in use — only on the master, only when C does not impose that treedb's
   schema (the literal would overwrite it at the next open), and only when the
   saved `schema_version` is higher. It takes effect at the next open of the
   treedb: restart the yuno that owns it. The file is written to a temporary
   beside it, flushed, and renamed over it: the file in use is the old one or
   the new one, never a truncated one (until 7.25.3 it was rewritten in place,
   and a crash or a full disk left it empty -- read as version 0 and recreated
   from the literal at the next open). The answer says `data.applied`. The
   file is written without the `fkey` marks `parse_schema()` derives (a dict on
   every column a hook points at), as `treedb_open_db()` writes it; until
   7.25.4 the apply wrote the parsed copy it had validated, marks included.

```bash
ycommand -c 'command-yuno id=<id> service=treedbs command=save-schema treedb_name=treedb_x'
ycommand -c 'command-yuno id=<id> service=treedbs command=saved-schema treedb_name=treedb_x'
ycommand -c 'command-yuno id=<id> service=treedbs command=apply-schema treedb_name=treedb_x'
ycommand -c 'kill-yuno id=<id>'; ycommand -c 'run-yuno id=<id>'
```

Without `treedb_name` each command acts on every treedb that `C_TREEDB` opened
and lists the answers. It is what gui_agent's Schemas tab sends: one request
per `C_TREEDB` of the yuno. `apply-schema` then takes only the treedbs whose
saved schema can be applied (master, not imposed, a saved schema newer than
the one in use), and it takes them **all or none up to the renames** (after
7.25.3): each is checked first -- its saved schema loads and parses -- then
each is written to its temporary, and only when every one got that far are
they renamed in place. One that cannot be checked or written leaves every file
in use as it was, and the answer is `-1` with every row `applied: false`.
Before, A was applied, B refused, the answer was `-1`, and a console that read
it as "nothing applied" did not restart the yuno. An apply with nothing
applicable answers `0` and no row.

The renames themselves are not all or none: they are done one by one, and a
rename that fails (a full directory, a permission changed under it) fails on
its own. Its row says `applied: false` and the others `applied: true`, the
answer is `-1`, and the comment says *"N of M treedb(s) applied, see each
one"*. So a console reads the ROWS, never the result alone: a `-1` does not
mean nothing was applied.

```json
{"result": -1,
 "comment": "<role>^<name>: apply-schema refused, nothing was written: treedb_b cannot be applied",
 "data": [
   {"treedb_name": "treedb_a", "result": -1,
    "comment": "<role>^<name>: 'treedb_a' not applied: treedb_b cannot be applied, nothing was written",
    "data": {"treedb_name": "treedb_a", "applied": false,
             "saved_schema_version": 4, "in_use_schema_version": 3}},
   {"treedb_name": "treedb_b", "result": -1,
    "comment": "<role>^<name>: the saved schema of 'treedb_b' does not parse (see the log)",
    "data": {"treedb_name": "treedb_b", "applied": false,
             "saved_schema_version": 50, "in_use_schema_version": 1}}
 ]}
```

A console restarts the yuno when a row -- or the `data` of a named apply --
says `applied: true`, and only then.

Both numbers matter, and that is why the save raises them and nobody else
does: `schema_version` is what makes the file win over the literal, and
`topic_version` is what regenerates `topic_cols.json` — without it the new
columns exist in the schema and not in the topic.

**A write here is a schema change, so it answers to the rules of a schema.**
On top of the ordinary validation of §3.6, writes to these topics are refused
when they could not produce a working schema — at the point of writing,
because none of these is loud later:

- a column is checked against the descriptor a user column answers to, the
  same one `parse_schema_cols()` applies when a schema is opened. Stored
  unchecked, the column breaks the treedb at its **next open**, far from
  whoever wrote it. That includes the two per-column rules the descriptor
  cannot express: a `file` column is a string `fkey`, one column is never both
  `hook` and `fkey`, and a `hook` or `fkey` column is a dict, a list or a
  string. The refusal says *"Column definition refused"* after the rule's own
  message, for example:

  ```C
  /*  a `cols` node of __system__, refused: a hook is a dict, list or string  */
  gobj_create_node(gobj_node_system, "cols",
      json_pack("{s:s, s:s, s:s, s:[s], s:s}",
          "value", "children", "header", "Children", "type", "integer",
          "flag", "hook", "topics", "topics^<topic id>^cols"),
      json_pack("{s:b}", "refs", 1), src);
  ```
- `pkey` must be `id` and `system_flag` must be `sf_string_key` —
  `treedb_open_db()` silently drops a topic that disagrees;
- `pkey`, `tkey` and `system_flag` **cannot change once the topic exists**:
  `topic_desc.json` is written at creation and never rewritten, so the change
  would be stored here, shown by every reader, and ignored by the topic for
  good;
- two columns with the same name in one topic are refused **when the column
  is linked** to it, which is when the clash becomes real. The name is the key
  a schema is rebuilt by, so a duplicate drops one of the two definitions on
  the next read.

**Applying an edit: `pause-yuno` + `play-yuno`, never `close-treedb`.** An
edited schema reaches a running treedb only when the treedb is reopened, and
the reopen has to be driven by the yuno that opened it. `close-treedb`
destroys the treedb's `C_NODE` and its `C_TRANGER`, and an owner typically
keeps raw handles that no framework cleanup can reach — the service pointer,
the `tranger` json_t read from it, copies of both on a hot path, and whatever
else it opened on that same tranger (`db_history_co` opens its `msg2db_alarms`
there). Called from outside on a playing yuno, the next record processed
writes into released memory. Every in-tree consumer therefore closes only from
`mt_pause` and reopens in `mt_play`, which re-acquires every handle; from
outside, that pair is `pause-yuno` + `play-yuno`, and it does **not** restart
the process. `cmd_close_treedb` refuses while the yuno plays (`force=1` for a
caller that holds nothing of the treedb). Note `pause` stops the yuno's other
services too, so its gate goes down for the cycle.

Round-trip coverage:
[`tests/c/c_treedb_system_schema`](https://github.com/artgins/yunetas/tree/7.25.4/tests/c/c_treedb_system_schema).

**`delete-treedb` deletes the schema, and only of a CLOSED treedb.** It
removes the projection in `__system__` (`delete_client_treedb_schema()`: the
`treedbs` node with `force`, which unlinks its `topics` and `cols` itself) and
never touches the client treedb's store on disk. `force=1` is required and
means "yes, delete the schema"; it does not lift the refusal of an OPEN
treedb, because an open one goes on answering from its copy in memory with a
schema that exists nowhere, and the next `open-treedb` dies on the C_TRANGER
service still alive under its name, leaving the store orphaned. Close first
(`close-treedb force=1`, or `pause-yuno` + `play-yuno`), then delete:

```
command-yuno id=<id> service=treedbs command=close-treedb treedb_name=treedb_foo force=1
command-yuno id=<id> service=treedbs command=delete-treedb treedb_name=treedb_foo force=1
```

Until 7.22.0 this page called the command broken -- "removes the parent before
its children and passes collapsed views where pure nodes are required". Both
halves were wrong: `mt_delete_node` re-resolves the pure node by `id`, and a
parent deleted with `force` unlinks its children itself. The real defect was
that it deleted the schema UNDER a running treedb. Covered by test 12 of
`c_treedb_system_schema`: deleted closed, nothing of the projection remains;
deleted open, refused and nothing changes.

---

## 4. Sharp edges

### 4.1 `g_rowid` and `i_rowid` are read-only to user code

(§2.3, §3.4.) Never set them in test fixtures, code that calls
`treedb_create_node`, or anywhere else. timeranger2 computes them and
shows them in `__md_treedb__` for inspection only.

### 4.2 link/unlink saves the child, not the parent

(§3.7.) If you read `g_rowid` on the parent after a link operation and it
did not change, that is correct. Read the `g_rowid` of the child instead —
and if the CHILD's rowid did not change either, that is also correct: the
link found its fkey already written and saved nothing.

### 4.3 Schema changes need a higher `topic_version`

(§3.5.) A stale `topic_cols.json` overrides new code, and it gives no
message. The trap is worse because the yuno still **works**. For treedb
the new columns do not exist. Always raise the version.

### 4.4 Master-only writes

(§2.7.) `tranger2_append_record` does nothing on a non-master and returns
-1. If you write in a yuno that is the non-master, you have a deployment
bug: two yunos opened the same store.

### 4.5 timeranger2 is append-only — with two scoped deletes

(§2.9.) Nothing **ever rewrites** the `.json` data log itself. Appends go
to the end, and nothing else changes. What is mutable is the `.md2` index, and
two delete primitives operate on it:

- `tranger2_delete_key()` removes a key's directory wholesale (every
  instance with it) and propagates the deletion to in-process and
  cross-process subscribers via inotify + callback fan-out.
- [`tranger2_delete_instance()`](#tranger2_delete_instance) tombstones one row of the `.md2` index
  in place (bit `sf_deleted_instance = 0x0400`). Readers skip it, and
  rowids do not renumber. Opt-in `zero_payload` overwrites the
  matching bytes in the `.json` for [GDPR](https://en.wikipedia.org/wiki/General_Data_Protection_Regulation)-style wipes.

Both are master-only and irrecoverable. The append-only contract
still holds at the data-log level — only the index is mutated.

### 4.6 No `fsync` after append

(§2.10.) Durability is what the OS gives you. For audit logs where
a crash window of a few seconds is unacceptable, add an explicit
`fsync` — but understand the throughput cost.

### 4.7 Do not open the same store twice in the same process

`tranger2_startup` caches by path. Two starts of the same path return
the same tranger handle, but two distinct yunos in the same process
trying to coexist on the same store is unsupported.

### 4.8 The deprecated `range_ports`/`last_port` columns on `realms`

(See [`REALMS.md`](REALMS.md) §7.1.) Same class of trap as §3.5:
columns that the schema still declares but the runtime ignores. Reading
them returns stale data. Trust the agent's own attrs, not the schema
column.

### 4.9 Multiple node occurrences in dumps share one `g_rowid`

A node listed under `topic.id_index[id]` and also nested inside a
parent's `hook` array is the same record. They share the
`__md_treedb__.g_rowid`. Do not count it twice when you compute stats from
a dump.

### 4.10 Hooks rebuild on load — only fkeys persist

(§3.7.) In the database on disk you find the fkeys of the children but
**not** the hooks of the parents. Hooks are in-memory pointers only, and
treedb builds them again when it scans the children. This is why a corrupt
fkey on a child makes the hook of its parent look short. Read the child
first.

**The loader knows which hook fills an fkey from a mark it derives.** At every
open `parse_hooks()` writes, in memory, on each child fkey column the one hook
that fills it, and keeps only the links that mark names:

```C
/*  the parent declares the hook...  */
'users': {'type': 'dict', 'flag': ['hook'], 'hook': {'users': 'departments'}}

/*  ...and in memory the child's fkey column carries its mark
 *  (what `descs` answers; never in a file)  */
'departments': {'type': 'array', 'flag': ['fkey'], 'fkey': {'departments': 'users'}}
```

The mark is recomputed from the hooks of the schema in use at every open, and
**no file carries it** (since 7.25.0): not the child's
`topic_cols.json`, not the treedb schema file. It used to be written into both,
so a **renamed hook** — which raises only the PARENT's `topic_version` — left
the child reloading the old mark: *"Only can be one fkey"* at every open, and
the links made through the new hook dropped at every restart. A store written
by an older release still has the mark in its files; it is ignored, and it
goes the next time that topic's version rises. Renaming a hook is now: rename
the column in the parent, raise the parent's `topic_version` and the
`schema_version`. The references a child already holds name the OLD hook
(`departments^d1^users`) and hang from nothing after the rename: the loader
ignores them, and the first relink, clean or forced delete of that child
removes them with a warning (*"Parent ref names a hook that no longer
exists"*, then *"Removing wrong fkey ref"*). Link those children again through
the new hook if they are to keep their parent. Until after 7.24.1 that unlink
failed on the missing hook, and the child could be neither relinked, cleaned
nor deleted, even with `force`.

**Re-pointing a hook** to ANOTHER column of the child (the hook keeps its name,
its map names a new fkey column) leaves the same kind of residue: the refs in
the OLD column name a hook that exists and hooks this topic, but not through
that column. They are stale too, and a relink, clean or forced delete removes
them with *"Parent ref names a hook that fills another column"*, then
*"Removing wrong fkey ref"* (after 7.25.3; before, the unlink looked in the new
column, refused, and a forced delete of the child failed for ever). The old
column, if no hook fills it any more, is an fkey column with no hook: every
open logs *"Child node without fkey field"* once per node of that topic, until
the column goes from the schema.

```C
// v1: the hook fills `f1`       'kids': {'flag': ['hook'], 'hook': {'children': 'f1'}}
// v2: the same hook fills `f2`  'kids': {'flag': ['hook'], 'hook': {'children': 'f2'}}
// A child linked under v1 keeps "parents^p1^kids" in `f1`:
treedb_delete_node(tranger, c1, json_pack("{s:b}", "force", 1));   // 0: ref removed, warning
```

### 4.11 No raw `malloc` / `free` for treedb-allocated [`json_t`](https://jansson.readthedocs.io/en/latest/apiref.html#c.json_t)

CLAUDE.md hard rule. `gbmem_*` everywhere. Jansson is routed through
`gbmem_*`, so all `json_*` APIs are safe. Never `free()` a `json_t`
yourself.

### 4.12 Do not cache a `json_t *` from [`treedb_get_node`](#treedb_get_node) across a
restart

The pointer is valid for the life of the loaded tranger. After a
[`tranger2_stop`](#tranger2_stop) and `tranger2_startup` cycle the pointer is stale. If
you keep references across stops, the framework does not detect it. Your
crash does.

### 4.13 Link events are OFF by default — and turning them on REMOVES an event

`C_NODE` publishes `EV_TREEDB_NODE_LINKED` / `EV_TREEDB_NODE_UNLINKED`
only when its `with_link_events` attr is set (default **false**).
`C_TREEDB` copies its own `with_link_events` into each treedb it opens, and
since 7.20.0 a running treedb can be switched with the `set-link-events`
command of its service, at once and without a restart:

```bash
ycommand -c 'command-yuno id=<id> service=<treedb> command=set-link-events set=1'
ycommand -c 'command-yuno id=<id> service=<treedb> command=set-link-events'   # show
```

It needs the `update` permission and is **not persistent**: the next start
takes the configured value again. Put `with_link_events` in the yuno's
`C_TREEDB` config for a lasting default. Two things bite here:

- **It is an either/or, not additive.** With the flag ON, a link/unlink
  publishes the link event and **stops** publishing the
  backward-compatible `EV_TREEDB_NODE_UPDATED` of the **parent**. So
  enabling it on a treedb that also serves an older consumer changes
  what that consumer receives. Check every subscriber before flipping it.
- **The compat event names the wrong node for edge tracking.** An edge
  *is* a fkey of the **child** (§4.2, link-saves-child), but the compat
  path announces the **parent** — whose fkeys did not change. A consumer
  that derives edges from fkeys therefore sees "a node was updated" and
  correctly concludes there is nothing to redraw, so its graph shows
  **stale edges**. That is the reason for the dedicated link events. Their
  kw is the relationship, not a node:
  `{hook_name, parent_topic_name, child_topic_name, parent_id, child_id,
  treedb_name}` — note there is **no `topic_name`**, so a per-topic
  subscription filter matches nothing (filter by `treedb_name`).

---

## 5. Recipes

### 5.1 Browse a topic from the CLI

`yutils/c/ylist/` ships [`ylist`](#util-ylist) for this. Without it, raw `find +
jq`:

```bash
# every record in the realms topic (date-partitioned)
cat /yuneta/store/agent/treedb_yuneta_agent/realms/keys/*/*.json | jq .

# specific node
cat /yuneta/store/agent/treedb_yuneta_agent/yunos/keys/<id>/*.json | jq .
```

For machine-friendly access, prefer [`ycommand`](#util-ycommand) against the agent
(`list-yunos`, `list-realms`, `list-binaries`, `list-configs`) —
those go through the treedb's in-memory state and apply schema
correctly.

### 5.2 Add a new column to an existing topic

```diff
   "cols": {
       …
+      "my_new_field": { "type": "string", "flag": ["persistent"] }
   },
-  "topic_version": 19
+  "topic_version": 20
```

Without the `topic_version` bump the field will be silently ignored
on load. With it, treedb migrates: every existing node gets the
column with its default value on first save.

For a hot rollout in which you cannot restart the yunos:

1. Update the schema file in source. Bump `topic_version`.
2. Build + redeploy (see [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) §6.2).
3. Verify the new field shows up:

   ```bash
   ycommand -c 'command-yuno id=<yuno> service=<treedb> command=list-nodes topic=<topic>'
   ```

### 5.3 Read a topic a PAGE at a time

A treedb lives in memory, so walking it is not what costs: serializing every
node, pushing it through a websocket and parsing it in a browser is. So
`nodes` can cut the answer on the way out:

```bash
# every node, as always
ycommand -c 'command-yuno id=<yuno> service=<treedb> command=nodes topic_name=<topic>'

# the second page of 50
ycommand -c 'command-yuno id=<yuno> service=<treedb> command=nodes topic_name=<topic> from=51 limit=50'
```

`from` is **1-based**. With **no `limit`** the answer is the plain list it has
always been, so every client written before this keeps working; asking for a
page gets the envelope `get-page` uses:

```json
{"total_rows": 1234, "pages": 25, "data": [ ... ]}
```

That is deliberately the same contract as `list-keys` of `C_TRANGER`, so a
client pages nodes exactly as it pages records. A page past the end is empty
and still reports the true `total_rows`.

Filtering happens BEFORE the cut: `filter` selects, `from`/`limit` slice what
was selected, so `total_rows` is the size of the match and not of the topic.

Pinned by `tests/c/c_node_paged_nodes`.

### 5.4 Create a node and link it to a parent

In C, inside an action or command handler:

```c
json_t *node = gobj_create_node(
    gobj,
    "users",
    json_pack("{s:s, s:b}", "id", "alice", "disabled", 0),
    NULL,
    src
);

json_t *parent = gobj_get_node(gobj, "roles",
    json_pack("{s:s}", "id", "operator"), NULL, src);

gobj_link_nodes(gobj, "users",
    "roles", parent,
    "users", node,
    src);

// note: only `node` has been saved (the child with the fkey).
// `parent` is unchanged on disk.
```

### 5.5 Inspect snapshots

```bash
ycommand -c 'command-yuno id=<yuno> service=<treedb> command=snaps'
```

Snapshots are global to a treedb. You see one entry per "tag".

The command is `snaps`, not `list-snaps` — only its handler is called
`cmd_list_snaps`. And these commands live in `C_NODE`, so `service` is the
**treedb service**, never `__yuno__`: `C_YUNO` has no command parser that
forwards to other services, so `__yuno__` answers that the command does not
exist. The same applies to `list-nodes` above, which is an alias of `nodes`.

### 5.6 Recover from a botched schema change

```bash
# 1. stop the yuno that owns the store
ycommand -c 'kill-yuno id=<yuno>'

# 2. wipe the topic's data (do NOT do this in production — this is
#    for fresh-checkout / dev-loop recovery)
sudo rm -rf /yuneta/store/<realm>/<yuno>/treedb_<name>/<topic>/

# 3. restart — the topic is recreated from the schema
ycommand -c 'run-yuno id=<yuno>'
```

For production, do this against a backup. Never `rm -rf` a live store.

### 5.7 Read another yuno's topic non-master (`rt_by_disk`)

Pseudocode in a different yuno that does NOT own the store:

```c
json_t *tranger = tranger2_startup(gobj, json_pack(
    "{s:s, s:b}",
    "path",     "/yuneta/store/<other_yuno>",
    "master",   false
), yev_loop);

tranger2_open_rt_disk(
    tranger,
    "events",
    "*",                    // every key
    NULL,                   // no extra filter
    my_on_record_callback,
    "my_unique_rt_id",      // mandatory unique id
    gobj,
    NULL
);
```

The master will detect your `disks/my_unique_rt_id/` directory and
start writing hardlinks there on every change. Your callback fires
as soon as the kernel notifies the filesystem watcher. No socket
between the two yunos — pure inode plumbing.

---

## 6. Code pointers

| What                                              | Where                                                                 |
|---------------------------------------------------|-----------------------------------------------------------------------|
| timeranger2 public API                            | [`kernel/c/timeranger2/src/timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h) (747 lines)                  |
| timeranger2 runtime                               | [`kernel/c/timeranger2/src/timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c) (~7.8k lines)                |
| `md2_record_t` (32-byte index)                    | [`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c)                                                 |
| `md2_record_ex_t` (in-memory)                     | [`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h)                                               |
| `system_flag2_t` (sf_string_key, sf_int_key, …)   | [`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h)                                               |
| Master / non-master lock                          | [`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c)                                               |
| `tranger2_append_record`                          | [`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c) (g_rowid set at 2667, i_rowid at 2634)      |
| `tranger2_open_rt_disk` (cross-yuno reads)        | [`timeranger2.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.h)                                               |
| TRACE_FS sites                                    | [`timeranger2.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/timeranger2.c) (multiple)                   |
| treedb public API                                 | [`kernel/c/timeranger2/src/tr_treedb.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.h) (617 lines)                    |
| treedb runtime                                    | [`kernel/c/timeranger2/src/tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c) (~8.9k lines)                  |
| `__md_treedb__` builder                           | [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c)                                    |
| Topic schema loader (`topic_cols.json`)           | [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c)                                                 |
| `topic_version` matching                          | [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c)                                            |
| `treedb_link_nodes` / `treedb_unlink_nodes`       | [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c) (saves child only)                            |
| `treedb_create/update/delete/get/list_node[s]`    | [`tr_treedb.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.h), [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c)                        |
| Snapshot API                                      | [`tr_treedb.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.h)                                                 |
| gobj wrappers (`gobj_*node`)                      | [`gobj.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/gobj.h)                                                    |
| `gobj_list_snaps`                                 | [`gobj.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/gobj.h)                                                    |
| Canonical schemas                                 | [`yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.25.4/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c), [`kernel/c/root-linux/src/treedb_schema_authzs.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/treedb_schema_authzs.c) |
| Treedb gclass (gobj wrapper)                      | [`kernel/c/root-linux/src/c_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_treedb.c), [`c_node.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_node.c)                      |
