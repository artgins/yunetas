# Design: a `file` column, and `__assets__` as a treedb system topic

Status: **PROPOSED**, nothing implemented. Agreed in design on 2026-09-04.

The shape in one sentence: **you mark a column `file`, and treedb gives you a
pseudo-filesystem** — you hand it a file, you get it back, the *index* lives in
memory and the *content* on disk.

It replaces the storage half of `C_ASSETS` (SDK 7.17.0, shipped and running).
`C_ASSETS` keeps the other half — publishing the bytes to a browser — which is
a different job and does not belong in `tr_treedb`.

## 1. The premise, and the number that fixes it

A treedb is a **memory database**, and timeranger2 rewrites the **whole record**
on every update. So a photo cannot be a field of a node: it would sit in RAM for
the life of the yuno and be rewritten every time the node changed state.

Measured on the yunovatios central (2026-09-04):

| | |
|---|---|
| blobs | **12 134** |
| on disk | **346 MB** |
| the same as base64 in records | **~460 MB of RAM, permanently** |

That is the whole reason the bytes live outside the record. It is **not** a
reason for them to live outside timeranger2: *index in memory, content on disk*
is timeranger2's own model with a different payload.

## 2. What a `file` column is

A new **field type** in the treedb vocabulary. The value stored in the record is
an **fkey into `__assets__`** — not the bytes, not a path.

An fkey and not a bare sha256, deliberately: the link then gets, for free, the
things a link already has — cascade delete, the graph, the scope check, and the
count the delete dialog prints ("N children will be UNLINKED").

It is a different thing from the two neighbours it will sit next to in the list:

- `image` — the field **holds** the file: a url or a data uri, drawn with
  `<img>`. That is the model the yunovatios census used before `C_ASSETS`, where
  `devices.foto` was a path under `censo_memorias/` that nothing owned.
- `icon` — the **name** of an icon of the app's set (`yi-bolt`).

⚠️ A new field type goes in **three** places, and the third is the only one that
can stop a yuno from starting:

1. the documented list in `tr_treedb.h` — a comment;
2. `treedb_field_types` in `gobj-js/src/lib_treedb.js` — what makes
   `treedb_get_field_desc()` answer `type: "file"`;
3. **the `enum` of the `cols` topic's `flag` column in
   `treedb_system_schema.c`** — the list `check_desc_field()` validates against.
   A schema using a flag that is only in the comment is refused at
   `treedb_open_db` (*"Wrong enum type"*) and the yuno exits at `mt_play`.
   Bump the system `schema_version` and the `cols` `topic_version` with it.

## 3. `__assets__` is a system topic, and its hooks are DERIVED

This is the pivot of the design.

Today the asset topic cannot be a system topic, and `c_assets.h:61` says why:

> THE TOPIC IS THE HOST'S, NOT THIS GCLASS'S. C_ASSETS never creates it: the
> fkeys of an asset point at the host's own topics, so only the host can write
> those hooks.

Treedb does not have that problem, because **treedb reads every schema**. For
every column flagged `file` in topic `T` named `C`, `__assets__` gains the
reciprocal hook:

```
__assets__.as_<T>_<C>   hook -> {"<T>": "<C>"}
```

So the host declares **nothing but the column**. `__assets__` is created by
`tr_treedb` alongside `__snaps__` and `__graphs__` (`tr_treedb.c:745`, `:834`),
with `system_topic: true` so it cannot be deleted.

Its columns are the index entry, and they are the ones `C_ASSETS` already
writes: `id` (the sha256, pkey), `content_type`, `size`, `t`, `original_name`,
`uploaded_by` — plus the derived hooks. `original_name` is the only writable
one: every other column describes the bytes, and editing it would lie about
them.

**There is no `source_path` and no subpath.** Dropped on purpose: the same bytes
are ONE asset and each copy came from its own place (148 points of Can Tunis
share one photograph), so a single path is a lie and a list is a field nobody
reads. The blobs live in one default directory of the treedb; where a file came
from, if it matters, is a column of the node that links it.

## 4. On disk

```
<store>/<realm>/treedb_<name>/
    __assets__/          the index: the topic's timeranger2 files
    .blobs/00..ff/       the content, addressed by sha256
    devices/  places/  ...
```

Both halves inside the treedb directory, so a `cp -a` of it carries the nodes
**and** their bytes. Today they are split — the index inside, the blobs a
sibling of the treedb at realm level — which is what `C_ASSETS` was reaching for
and did not quite get.

⚠️ **The blob directory must start with a dot.** `tranger2_list_topics()` scans
the treedb directory and returns every entry as a topic, skipping only the ones
whose name starts with `.` (`timeranger2.c:1289-1293`). The skip rule is already
there; the name has to use it.

The 256-way shard is not new: `C_ASSETS` already writes `00`..`ff` (verified:
257 directories for 12 134 blobs, ~47 each).

## 5. The write path

The client computes the sha256 **before sending anything**, which is what makes
this cheaper than `put-asset`:

1. the browser hashes the file (`crypto.subtle.digest`) and fills the `id`;
2. it asks the treedb whether that node exists;
3. **if it exists, no bytes travel** — the record is saved with the fkey and
   that is all;
4. if it does not, the base64 rides along in the `create-node` / `update-node`.

Two rules the write path cannot bend:

- **Treedb re-hashes what arrives.** The client's sha256 is an optimisation,
  never an authority: a client that lies would make the store serve one file's
  bytes under another file's hash. Hashing on write is cheap next to writing.
- **The base64 is TRANSPORT and dies at the door.** It arrives in the kw, treedb
  writes the blob and the index node, and it is dropped from the record. A field
  that *carries* the bytes and a field that *keeps* them are one word apart and
  460 MB apart (§1).

Today `put-asset` always sends the whole file and dedupes on arrival, at roughly
2.3× the file in RAM. On a census reload, where nearly every asset is already
stored, step 3 is the difference between seconds and half an hour.

## 6. The read path stays with C_ASSETS

Serving bytes to a browser is not storing them:

- a **signed url** that a web server checks by itself, when `public_url` and
  `sign_secret` are configured;
- the **bytes inline** in the answer when they are not, so a node with no web
  server in front of it still shows its images.

That is `get-asset`, and it stays where it is. Treedb owns the store; `C_ASSETS`
owns the way out.

**It takes the asset ID, and nothing else** — not a node plus a column. Three
things follow, and they all point the same way:

- **The way out needs to know nothing about the structure.** No topic, no
  column, no hook: id in, bytes or url out. Which is right for the half that
  is not treedb's — publishing is not structure.
- **It is what keeps a served url immutable.** The id is the sha256 of the
  content, so the url can be cached for ever (`c_assets.h`). Keyed by node and
  column instead, the same url would mean different bytes the moment the node
  was relinked, and "cache for ever" would become a bug.
- **The client half is already written.** `yui_asset_id()` in gobj-ui reads the
  id out of an fkey in the three shapes a link comes back in
  (`"__assets__^<id>^as_..."`, the bare `<id>` that `fkey_only_id` collapses it
  to, or an expanded `{id: ...}`), so nothing changes on the frontend.

## 7. The gc is treedb's, and the derived hooks are what it reads

An asset nothing links any more is garbage, and deciding that is reading the
hooks. Today `C_ASSETS gc-assets` has to be HANDED the hook names to do it
(`node_is_linked()`, `c_assets.c:1090`); treedb derived them (§3), so it already
knows them — and it knows them for every host, without being told.

Three consequences, and one of them deletes a rule:

- **The refusal comes for free.** Deleting an asset that a node still links must
  be refused, which `C_ASSETS` states as a rule of its own. Treedb's ordinary
  delete guard already refuses a node that has links unless `force` is given, so
  there is nothing to write.
- **The sweep is ON DEMAND, never automatic.** Dropping the blob the moment the
  last link goes is tempting and wrong: `treedb_delete_node` with `force`
  UNLINKS children rather than deleting them, so an asset sitting unlinked is a
  normal intermediate state of a bulk operation. An automatic gc would delete
  bytes that are re-linked a second later.
- **It says what it would take before taking it.** A dry run costs nothing here,
  because the hooks and the blob paths are both known.

A note, and it is a comfort rather than a licence: the id is the sha256 of the
content, so a gc that took too much is undone by adding the same file again — it
comes back under the same id, and every fkey that pointed at it is valid once
more.

## 8. The door checks the size and the type, and the door is treedb

Both limits move in with the store. Two notes on *how*, because each one is the
difference between a check and a formality:

**Size is checked on the base64, before decoding.** `max_size` is a MEMORY limit
as much as a policy one — the file is hashed and written whole — so a check that
runs after decoding has already spent what it was defending. The encoded length
gives the byte count exactly (`len * 3 / 4`, minus the padding), so the refusal
can happen while the bytes are still a string.

**The type is checked on the BYTES, not on the word.** Today the content type is
either handed over by the client or guessed from the filename
(`c_assets.c:151`, `:1624`) — it is never read from the content. That makes the
allowlist a formality, and it matters most where the list is most deliberate:
`image/svg+xml` is left out because an svg served from the app's own origin runs
script, and a client that calls its svg `image/png` walks straight past the
omission. Treedb holds the whole buffer at the door anyway, because it has to
hash it: sniffing the first bytes costs one comparison, and turns the declared
type into a claim to verify instead of a fact to trust.

**And there are two levels, not one.** The two things `max_size` fuses today
want different homes:

| | Where | What it is |
|---|---|---|
| the ceiling | the **treedb** | what ONE write may cost this process, and the families the store will ever hold. A safety rule: a column author must not be able to opt into `image/svg+xml` |
| the policy | the **column** | what THIS field is for. A device photo has no business being 100 MB; a signed plan is a pdf and not a video |

The column's cannot raise the treedb's — the ceiling wins, and the column
narrows. A column that declares nothing takes the treedb's whole list.

*(This split is a recommendation, not something we settled out loud. What we
settled is that treedb enforces, at the door.)*

## 9. Between nodes: the bytes go FIRST, and it stays an order

The bytes crossing to another node is not a new problem and it is not a hard
one: **the agent has been doing this design for years**. `install-binary` takes
the file as `content64` in the kw, decodes it, writes it under
`/yuneta/repos/{tags}/{role}/{version}/` and creates a node in the `binaries`
topic whose columns are `id`, `version`, `size`, `date`, `description`, `tags`
— **the metadata, never the bytes**. Index in the treedb, content on disk, and
it reaches a remote agent over `wss` exactly as it reaches the local one.
`ycommand`'s `content64=$$(path)` is already the client half of §5, done with a
shell instead of `crypto.subtle`.

What that precedent settles is the transport. What it does not settle is the
case where a RECORD moves rather than a person uploading: the controller
forwards a device to the central and the record carries an fkey to an asset the
central does not have. Nobody is holding the file at that moment.

**The answer is not for the receiver to pull.** A pull reads well — the receiver
knows the sha256, so it can ask for it and verify what arrives — and it costs
more than it looks: the receiver is left holding a record it cannot complete, so
it needs a queue of assets it owes, retries, and state that survives a restart.
A one-way message has become a protocol, and it lands on the side that did not
ask for the record.

**So the order is: the bytes, then the record.** Two commands, both one-way,
both from the side that HAS the file — the same shape as `install-binary`:

1. store the file (idempotent by content: a repeat costs nothing);
2. write the record, which arrives at a node that already has the asset.

The sender may skip the *"do you have it?"* of §5 and send bytes that were
already there; that costs bandwidth and nothing else. The difference is the
whole point: **a handshake BEFORE sending is an optimisation the sender may
skip; a handshake AFTER receiving is a protocol the receiver cannot.**

One rule covers the browser and the node: *whoever has the bytes sends them
before sending the record that needs them.*

## 10. What changes, by layer

| Layer | Change |
|---|---|
| `timeranger2` | the `.blobs` store: put by content (sha256), get by id |
| `tr_treedb` | the `file` field type; `__assets__` created as a system topic; the derived hooks; the write path of §5; the gc of §7; the limits of §8 |
| `treedb_system_schema.c` | `file` in the enforced `flag` enum; system `schema_version` + `cols` `topic_version` bumped |
| `C_ASSETS` | loses `put-asset` / `put-assets` / `import-assets` / `gc-assets` and both limits; keeps `get-asset` |
| `gobj-js` | `file` in `treedb_field_types` |
| `gobj-ui` | the form control: pick, hash, ask, attach; the table cell already draws an asset (`yui_asset.js`) |
| hosts | delete the `assets` topic from their schema; flag their columns `file` |

## 11. Migration

Renaming `assets` to `__assets__` changes the fkey literal in every record that
links one, so it needs a **new store**. It is cheaper than it sounds: the blobs
are addressed by content and do not move, so the migration is wipe → re-ingest
the directory → re-link, which is what a census reload does anyway.

## 12. Open items

1. **The browser's sha256 needs a secure context** (`crypto.subtle` is https or
   localhost only). The deployed SPAs are https; a dev server on `http://` is
   not.
2. **The GUI must stop offering "+ Nuevo" on `__assets__`.** Creating a row by
   hand makes an index entry with no bytes behind it, since the id IS the hash
   of content that was never written. A system topic is the signal the GUI
   already understands.
3. **The gc against the snapshots.** `treedb_delete_node` already refuses a node
   carrying a snap tag, so an asset inside a snapshot is safe. But the gc decides
   by the hooks of the LIVE state: delete a device, its asset is orphaned, the gc
   sweeps it — and then somebody activates a snap where that device existed and
   linked it. Content addressing makes that recoverable only if the file is still
   somewhere. Decide whether the gc reads the snapshotted states too.
4. **`import-assets` has no owner.** It is in the list of what `C_ASSETS` loses
   and in none of what anything gains — and it is the census path (a directory
   already on the node, one command and N assets with no bytes on the wire),
   which is exactly what makes the migration of §11 cheap. It has to land in
   treedb as a command, or that migration does not exist.
5. **How this is tested.** The sibling design note ends by naming its regression
   test; this one should too, before anybody starts: what proves that the base64
   dies at the door, that the ceiling is applied before decoding, and that the
   content type is read from the bytes.
