# Design: immutable (non-deletable) topics and records

Status: **IMPLEMENTED** on branch `feature/treedb-immutable-topics-records`
(commit `3f7d31cd5`). Full test suite 113/113, incl. the new regression test
`tests/c/tr_treedb_immutable`. The §7 open items were resolved during
implementation: the per-record bit is set post-append via the new
`tranger2_set_system_flag` (append keeps its topic-default system_flag), and
`treedb_save_node` re-applies it on every update; the delete guards read the
in-memory `__md_treedb__`immutable` with no extra disk read.

This supersedes the reverted "system-protected records" attempt (feat
`0e2b4a025`, reverted `e7c4d7d6d`). That attempt was rejected for one concrete
reason: it stored the record-level protection as a **persistent data column**
(`__system__`) on `roles`/`users`, polluting the user schema and forcing
`topic_version` bumps. **Immutability is metadata, not data.** This design keeps
the protection entirely in metadata channels that already exist and already
survive reload / snapshot activation.

## 1. Two independent requirements

| Requirement | Granularity | Example | Records protected too? |
|-------------|-------------|---------|------------------------|
| Non-deletable **records** | per-node | `roles/root`, `users/yuneta` (Authz seed) — but **general**, any topic | n/a |
| Non-deletable **topics**  | per-topic | the `__system__` treedb topics (`treedbs`/`topics`/`cols`), per-treedb `__snaps__`/`__graphs__` | **No** — only the topic, its records stay deletable |

These are orthogonal and use different metadata channels. A topic can be
non-deletable while its records are freely deletable, and vice versa.

## 2. Where the metadata lives (verified)

### Record level → a free `system_flag` bit in the md2 metadata record

Every timeranger2 record carries a 32-byte `md2_record_t`
(`timeranger2.c:75-85`) with two 16-bit bitfields bit-packed into the spare
high bits of the timestamps:

- `user_flag` (from `__t__`) — **fully owned by treedb as the snapshot `tag`**
  (`tr_treedb.c:3209`, `:8967`). `treedb_shoot_snap` overwrites the whole field,
  so it is **not** usable for a second per-record mark. This is the channel the
  existing **snap-tag delete guard** reads (`tr_treedb.c:5175`, `:5536`):
  `__md_treedb__\`tag != 0 && !force → refuse`. It is our precedent: a
  metadata-driven, reload-surviving delete guard already ships.
- `system_flag` (from `__tm__`) — key-type nibble + zip/cipher/ms +
  `sf_deleted_instance` (0x0400, the per-instance tombstone). Enum at
  `timeranger2.h:132-142`, names at `timeranger2.c:50-69`. **Free inherited bits:
  `0x0008`, `0x0040`, `0x0080`, `0x0800`.** Bits persist on disk and are decoded
  on every read (`timeranger2.c:7514-7520`), so they survive restart and
  snapshot activation for free.

→ **Define `sf_immutable_record = 0x0800`** (free slot in both the enum,
`timeranger2.h:141`, and the `sf_names` table, `timeranger2.c:62`; adjacent to
`sf_deleted_instance` — "deleted" vs "can't-be-deleted"). It rides the md2
metadata, never touches record JSON, never touches `topic_cols.json`, and never
collides with the snap `tag` (different word).

> **Inherited-band caveat (confirmed in code 2026-06-13).** `NOT_INHERITED_MASK
> = 0xF000` (`timeranger2.h:130`): the low 12 bits (`0x0FFF`) of a TOPIC's
> `system_flag` are inherited by every record appended to that topic; the high
> nibble is not. So `0x0800` is in the **inherited** band, which is exactly why
> a per-record value persists on the record — but it also means the bit must
> **NOT** be set on the topic's own `system_flag` default, or *every* record in
> the topic would become immutable. Per-record marking only. And because
> `treedb_save_node` re-appends an updated record with the topic default
> `system_flag` (masked), the per-record bit must be **read-and-reapplied across
> update**, exactly as `tag`/`user_flag` is inherited today
> (`tr_treedb.c:4939-4946`). (A topic-wide "all records immutable" mode would
> instead be the topic default carrying `0x0800` — a possible future extension,
> distinct from the non-deletable-*topic* requirement below.)

### Topic level → `system_topic: true` in `topic_var.json`

Per-topic metadata lives in three files (`timeranger2.c` writers):
`topic_desc.json` (immutable identity, `only_read=TRUE`), `topic_cols.json`
(column schema — touching it forces a `topic_version` bump, `timeranger2.c:846-918`),
and `topic_var.json` (mutable, **additive**, merged into the in-memory topic via
`kw_update_except(...topic_fields)` at `timeranger2.c:1008`/`:1650`).

→ **Stamp `system_topic: true` into `topic_var.json`.** Additive, no
`topic_version` bump, no column-schema change, readable at runtime as
`kw_get_bool(topic, "system_topic", ...)`. This is the part of the reverted
design that was already correct (its own commit message: *"persisted in
topic_var.json"*). `topic_version` itself lives in this same file and is
effectively immutable-once-set, so the precedent for "stable value in the vars
file" exists.

> **Decided (2026-06-13): `topic_var.json`.** Alternative considered and
> rejected: a topic-only `system_flag` bit (`0x2000`, in the
> `NOT_INHERITED_MASK=0xF000` band so records don't inherit it) stored in the
> immutable `topic_desc.json` — more orthodox, but unverified that a high-band
> bit round-trips the desc load cleanly. `topic_var` is proven (Fable's reverted
> branch shipped it) and lower-risk, so it wins.

## 3. Guard points (the delete surface, all verified)

Records (both reachable only via `mt_delete_node`, `c_node.c:942`):

- `treedb_delete_node()` (`tr_treedb.c:5137`) — whole-key, physical `rmrdir` via
  `tranger2_delete_key`. Add: refuse if node's `__md_treedb__` immutable mark is
  set. **`force` does NOT override** (stronger than the snap-tag guard — that is
  the whole point).
- `treedb_delete_instance()` (`tr_treedb.c:5491`) — per-instance tombstone. Same
  refusal, `force` does not override.

Topics:

- `treedb_delete_topic()` (`tr_treedb.c:1482`, currently has **no** guard) —
  refuse if `system_topic` is set on the in-memory topic.
- `tranger2_delete_topic()` (`timeranger2.c:1335`, unconditional `rmrdir` today)
  — defense-in-depth refusal if the topic dict carries `system_topic`.

Out of scope (intentional escape hatch, matches the original framing
*"only a full store wipe removes them"*): `delete-treedb` / `cmd_delete_treedb`
(`c_treedb.c:683`) and a manual `rm -rf` of the store. A superuser with FS
access can always wipe; we protect against CRUD/control-plane deletion, not
against the operator deleting the whole realm.

## 4. Setting the marks (provenance / trust boundary)

**Record immutability is NOT a JSON field**, so — unlike the reverted design — a
client literally cannot inject it through `create-node`/`update-node` content.
This removes the need for `c_node`'s `strip_system_mark` boundary entirely. The
only way to set `sf_immutable_record` is an internal API:

- New timeranger2 setter modeled on `tranger2_set_user_flag`
  (`timeranger2.c:3241-3317`): a per-record `system_flag` mask set/clear,
  **gated to touch only `sf_immutable_record`** (must refuse to flip key-type/ms
  bits). timeranger2 currently has no per-record `system_flag` setter — this is
  the one genuinely new primitive.
- New treedb helper `treedb_set_node_immutable(node)` (mt-level, internal) that
  surfaces the bit into `__md_treedb__` (new field, e.g. `"immutable": true`, set
  in `md2json` at `tr_treedb.c:3209` next to `tag`) and persists it.
- **Inheritance across update:** `treedb_save_node` re-appends on update; the
  immutable bit must be carried over exactly as `tag`/`user_flag` is today
  (`tr_treedb.c:4939-4946`, *"Tag is inherited"*). Add the symmetric read-and-
  reapply for `sf_immutable_record`.

**Topic immutability** is stamped at topic creation: add a `BOOL system_topic`
parameter to `treedb_create_topic()` (`tr_treedb.h:194`) that writes the
`topic_var` key, and pass `TRUE` for the `__system__` treedb structural topics
(in `c_treedb.c` materialization) and for per-treedb `__snaps__`/`__graphs__`
(`tr_treedb.c:678`/`:763` inside `treedb_open_db`). Runtime/client topic
creation (`cmd_create_topic`, `c_treedb.c`) passes `FALSE`.

## 5. Seeding the known case (Authz root/yuneta) — idempotent, master-only

Carry over the one good idea from the reverted `c_authz` change: an idempotent
ensure-loop in `c_authz` `mt_start`, **master only** (agent22 shares the store
as non-master and must not write), over `Authz.initial_load`
(`yuno_agent/src/main.c`): for each seed record, create it if missing, then mark
it immutable via `treedb_set_node_immutable` if not already marked. This way
already-deployed stores get protected on their next restart with **no schema
change and no wipe**. Because the mark is metadata, re-stamping is a cheap md2
rewrite, not a node re-append with a new column.

## 6. What changes, by layer

- **timeranger2** (`timeranger2.h/.c`): `sf_immutable_record` enum bit + name;
  gated per-record `system_flag` setter; refusal in `tranger2_delete_topic`
  (system_topic) — and optionally in the delete primitives as defense-in-depth.
- **tr_treedb** (`tr_treedb.c/.h`): surface `immutable` in `__md_treedb__`;
  inherit it in `treedb_save_node`; guards in `treedb_delete_node` /
  `treedb_delete_instance` / `treedb_delete_topic` (force does not override);
  `treedb_set_node_immutable()` helper; `system_topic` param on
  `treedb_create_topic`; stamp the per-treedb system topics.
- **root-linux** (`c_treedb.c`, `c_authz.c`): pass `system_topic=TRUE` for the
  `__system__` treedb topics and `FALSE` for client topic creation; the
  master-only idempotent ensure-loop. `c_node.c` needs **no** strip boundary
  (the mark is not a JSON field).
- **No user-schema change**: `treedb_schema_authzs.c` stays at schema 19,
  `roles` v8, `users` v12. No `topic_version` bumps anywhere.

## 7. Open items to verify before implementing

1. Confirm `treedb_save_node` can read-and-reapply `sf_immutable_record` the
   same way it inherits `tag`; wire the per-record `system_flag` through append
   (append takes `user_flag` but not a per-record `system_flag` today — confirm
   whether to extend append or set-after-append).
2. Confirm the in-memory `node` passed to the delete guards already carries the
   surfaced `immutable` field at guard time (it should, via `md2json` on load),
   so the guard is a pure in-memory check with no extra disk read.
3. Regression test (mirror the reverted `tr_treedb_system_records`): guard fires,
   `force` does NOT override, mark is immutable/inherited across update,
   survives reopen and snapshot activation, control cases (non-marked deletes
   still work, topic records still deletable under a system_topic).
