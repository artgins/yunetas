# Design: a `file` column, and `__assets__` as a treedb system topic

Status: **PROPOSED**, nothing implemented. Every decision of model closed
between 2026-09-04 and 2026-09-05. Reviewed against the code on 2026-09-05
(§15): four decisions are reopened, and §14 is superseded by §15.

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

**The derived hooks are never persisted, and there is nothing to version.** A
hook's CONTENT is in-memory already — `link_nodes` saves the child's fkey and
never the parent — so the hook is a reverse index treedb rebuilds from the fkeys
at load. Its DECLARATION can live in the same place: the hooks are added to the
in-memory desc and never written to `topic_cols.json`. So `__assets__` has a
fixed persisted schema and a fixed `topic_version`, like `__snaps__` (3) and
`__graphs__` (12), and no host adding a `file` column ever has to bump anything.
The `desc` a client reads is the in-memory one, so a GUI sees the hooks.

The one consequence is an ordering note for whoever writes it: the derivation is
a **second pass at open**. `__assets__` is created before the host's topics are
known, so the hooks can only be added once every schema has been parsed.

**And it is shown the way `__snaps__` and `__graphs__` are: in system mode
only.** The name decides it — a GUI skips the `__`-prefixed topics unless it was
asked for the system ones (`c_yui_treedb_topics.js`) — and that is the intended
place. What a person wants to look at is the photo OF A DEVICE, not the row of
an index; whoever wants the index is the one who goes to system mode. Once a
`file` column draws its asset properly, the topic itself stops being the way to
look at one.

It also settles a worry that is not worth a rule. Creating a row by hand in
`__assets__` would make an index entry with no bytes behind it — the id is the
hash of content nobody wrote — but reaching it takes system mode AND an
administrator typing into an index. `__snaps__` has carried the same
theoretical hole for years and nobody has fallen in it.

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
4. if it does not, the bytes ride along in the same `create-node` /
   `update-node`.

### The client's hash is an OPTIMISATION, never a requirement

A client may not be able to hash: `crypto.subtle` exists only in a secure
context, so a dev server on plain `http://` cannot — and hashing wants the whole
file in an `ArrayBuffer`, which for a large video is the whole file in the
browser's memory.

Then it does not hash, and **sends no id at all**. The manifest is keyed by
COLUMN, so *"this column takes this file"* is all the message needs; the write
path hashes what arrives — which it does anyway, because it never trusts the
client's id — and fills the fkey itself. The same code minus one comparison.

What that costs is exactly what step 3 was buying: with no id up front the bytes
must ALWAYS travel, because nothing can be known to be a duplicate until it has
arrived. A census reload is 12 134 assets already stored: with the hash first it
sends ~0, without it sends **346 MB** to discover that nothing was new. The
optimisation trades microseconds of sha256 for minutes of wire, which is why it
is the default and not the rule.

**What must NOT be done is to send the file NAME as the id.** If a name ever
became the identity, the three things content addressing gives would go with it:
two different `foto.jpg` would collide, the same bytes under two names would
become two assets (the 148 photographs of Can Tunis, again), and the
cache-for-ever url of §6 would start meaning different bytes over time.

### What travels on the wire

The column's value is **always the id**, and the bytes ride BESIDE the record,
never inside the column:

```json
{"id": "E22000041",
 "foto": "<sha256>",
 "__files__": {"foto": {"content64": "...",
                        "original_name": "E22000041.jpg",
                        "content_type": "image/jpeg"}}}
```

`__files__` is not a column. It is an instruction to the write path, consumed
and dropped at the door — the same key/value beside key/value that an event and
its kw already are, and that a kw carrying its `gbuffer` next to its json
already is one level down.

**`__files__` is a MANIFEST, and the bytes are spelled two ways.** A browser
sends json, so it carries `content64`. A C caller over an ievent channel puts a
real `gbuffer` in the kw and the manifest says where each file is inside it:

```json
{"id": "E22000041",
 "foto": "<sha256>",
 "qr":   "<sha256>",
 "__files__": {"foto": {"offset": 0,     "size": 40123, "original_name": "..."},
               "qr":   {"offset": 40123, "size": 3319,  "original_name": "..."}}}
```

**It buys no bandwidth, and that is not what it is for.** A gbuffer is binary
INSIDE a yuno and base64 on the wire: `gbuffer_serialize()` encodes it on the
way out and `gbuffer_deserialize()` decodes it on the way in
(`gbuffer.c:617`, `:657`). So the bytes on the channel are the same size either
way.

What it buys is the boundary. The encode and the decode belong to the
FRAMEWORK, at the edge of the channel, so the write path receives **bytes**
whichever door the file came through and there is no `content64` anywhere for
it to remember to decode — one representation inside the yuno, and a mechanism
that already exists, is already refcounted and is already tested, instead of a
convention of ours.

Two things about that slot, and both are load-bearing:

- **There is ONE binary field per kw, at the TOP level.** `gobj_start_up()`
  registers exactly one (`kw_add_binary_type("gbuffer", "__gbuffer___", …)`,
  `gobj.c:592`) and `kw_serialize()` looks it up with a plain
  `json_object_get(kw, "gbuffer")`. It will not find one nested inside
  `__files__`. Hence the manifest with offsets: one buffer holds every file of
  the record, and `__files__` says which slice is whose. A record setting a
  photo AND a qr at once is not exotic — commissioning does it.
- **`kw["gbuffer"]` is auto-decref'd by the serializer table.** The write path
  takes it with `extract=TRUE` exactly once and owns it from there; reading it
  with `extract=FALSE` and then decref'ing is the *"BAD gbuf_decref()"* double
  free. And a kw that carries one is refcounted with `kw_incref` /
  `kw_decref`, **never** with the json pair — `kw_decref` drops the binary on
  every call, and `json_incref` does not balance it. Two shipped bugs came from
  exactly this.

The alternative was to let the column hold either an id or an object, and it is
the shape to avoid: `normalize_node_field_value()` runs every incoming field
against its column's declared type, so an fkey column receiving an object is
refused or quietly coerced; and on the other side `yui_asset_id()` already
reads three shapes a link comes back in, and would learn a fourth that only
exists on the way out. A type that lies is what makes a guard necessary.

**One message, not two**, and that is the argument for it over storing the file
with its own command first: a Save that creates a new asset would otherwise be
two orders with a window between them, the blob written and the record not.
Here it either happened or the command failed.

Two rules the write path cannot bend:

- **Treedb re-hashes what arrives.** The client's sha256 is an optimisation,
  never an authority: a client that lies would make the store serve one file's
  bytes under another file's hash. Hashing on write is cheap next to writing.
- **The base64 is TRANSPORT and dies at the door.** It arrives in the kw, treedb
  writes the blob and the index node, and it is dropped from the record. A field
  that *carries* the bytes and a field that *keeps* them are one word apart and
  460 MB apart (§1).
- **The blob first, the node second** — which is not a precaution, it is what
  timeranger already does: the content, then the index. Interrupted the other
  way round leaves a node pointing at nothing, and nothing repairs that on its
  own; interrupted this way it leaves an orphan blob, which the gc takes.

### An asset node is a node like any other

Arriving twice is not a special case to suppress. The second arrival of a file
whose id is already there is an **update of its node**, and an update appends an
instance like every other record in the store: the history then says that this
file arrived again, under this name, at this time. (Worth knowing when writing
it: `treedb_create_node` on an existing pkey does NOT append — `save_id` stays
false, with a TODO beside it asking whether it should. The append comes from
`update_node`.)

**And that gives back what §3 threw away.** `source_path` was dropped because
one path is a lie and a list of them is a field nobody reads. The list returns
by itself as HISTORY: every name that file has ever arrived under is an instance
of its node, read with `tr2list` like any other history. No column, and a better
place than the one it had.

The blob is untouched by any of this, because it is not part of the record. The
same id is the same bytes by construction, so writing it again writes the same
bytes to the same place: skipping it is an optimisation, not a rule about the
node. What the rule really says is that **the content cannot change** — a
different content would be a different id.

Which is also the answer to the only question a hash raises. Collisions exist by
the pigeonhole principle, no SHA-256 collision has ever been exhibited, and an
accidental one among 10⁹ assets sits around 10⁻⁶⁰ — far below the disk
corrupting a file in silence. And if one were ever manufactured it could not
SUBSTITUTE anything: treedb re-hashes what arrives, so an id cannot be claimed
without the bytes that make it, and the bytes already there are not rewritten.
The failure mode is "the newcomer's file is not stored", never "the stored file
is replaced".

Today `put-asset` always sends the whole file and dedupes on arrival, at roughly
2.3× the file in RAM. On a census reload, where nearly every asset is already
stored, step 3 is the difference between seconds and half an hour.

### The second door: a directory already on the node

The bulk load is treedb's too. A directory that is already on the machine
becomes N assets in **one command and no bytes on the wire** — which is what
rebuilding a store from scratch means (§12), and 346 MB is not something to send
one file at a time.

Two things it carries over from `C_ASSETS`, and one it must add:

- **`import_root`, and empty still means REFUSED.** The import is confined to
  that root, and the guard is the whole security of the feature: without it, a
  command that reads a path is a command that reads anything on the node.
- **It creates index nodes, it does not link them.** Linking is the loader's
  business, as it is for any other node.
- **It must ANSWER the map** `path -> id`, and this is the piece that closes a
  loop the other decisions opened. `C_ASSETS` let a loader link by matching
  `source_path`; §3 removed that column on purpose, because the same bytes are
  one asset and a single path is a lie. So the bridge has to come back in the
  ANSWER instead of being stored: the loader already knows that
  `E22000041.jpg` belongs to device `E22000041` — it only needs the id that
  file got. Told once, at import time, and then thrown away. The path stays a
  fact of the load, which is where §3 said it belonged.

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

**And it reads the SNAPSHOTS, not only the live state.** Otherwise: delete a
device, its asset is orphaned, the gc takes it — and then somebody activates a
snap where that device existed and linked it, and the link dangles. So "nothing
links it" means *no live node and no snapshotted version of one*, which turns
the sweep from a walk over the in-memory hooks into a walk that also reads the
tagged record instances. The derived hooks still say which columns to look at;
what changes is how many versions of a node are asked.

Two things follow from that:

- **A snapshot holds bytes alive.** What frees an asset is not deleting the node
  that linked it, it is deleting the SNAP that still remembers the link. That is
  the same rule `treedb_delete_node` already applies to a node carrying a snap
  tag, extended to the bytes hanging off it.
- **The gc is conservative by construction.** It takes only what no version of
  anything has pointed at since the oldest surviving snapshot.

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

## 9. Between nodes: the bytes travel WITH the record

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

**So the bytes travel WITH the record**, in the `__files__` of §5, in the one
message that is already going. Nothing new is needed for a node that was not
already needed for a browser: it is one order, one-way, from the side that HAS
the file, and either it happened or it failed.

The sender may skip the *"do you have it?"* of §5 and send bytes that were
already there; that costs bandwidth and nothing else. The difference is the
whole point: **a handshake BEFORE sending is an optimisation the sender may
skip; a handshake AFTER receiving is a protocol the receiver cannot.**

One rule covers the browser and the node: *whoever has the bytes sends them
with the record that needs them.*

One asymmetry, and it is decided: **between C nodes the bytes go as a real
`gbuffer`** (§5) — which on the wire is base64 all the same, because that is
what the serializer does with one; what changes is that the encoding is the
framework's business and not the write path's. A browser has no gbuffer and
sends json, so both spellings of `__files__` exist for good, and the write path
forks once at the door: take the buffer as it is, or decode the string.
Everything after that point — hash, size, type, write — works on bytes and does
not care which door they came through.

## 10. The form: one request, and three things that are not obvious

Creating a record with a `file` column is **one** request. The second exists
only if the client takes the optimisation, and it is a lookup, not a write:

```
plain:      create-node { record + __files__ }                        1
optimised:  is <sha> there? -> create-node { record + __files__ if not }   2
```

The split is the one that already exists: the **form** talks to no backend — it
is a widget — and the **table** does, because it owns the transport and already
sends `EV_CREATE_RECORD`. Same shape as `EV_REQUEST_PAGE`: the view asks, the
host carries.

Three things that will be got wrong if they are not written down:

- **The file is read at SAVE, not at pick.** An `<input type="file">` hands over
  a `File`, which is a reference and not the bytes. Picking shows a name and a
  size and reads nothing, so cancelling a form does not mean a 40 MB video was
  read for nothing. Reading — and hashing, if it hashes — belongs to Save.
- **The `File` cannot travel in a kw.** A kw is plain json: no gobjs, no
  widgets, no DOM nodes, because the machine trace serialises it and a `File` is
  a host object. So the form KEEPS it and hands up an identity — the record goes
  with the column empty and the table asks the form for the picked files through
  a local method. The same reason a card sends `{key, mode}` and not the object.
- **Saving stops being synchronous.** Reading a file is a promise, and a
  resolved promise is an OS notification that must enter the machine as an
  EVENT, not as a chain of callbacks. So Save on a form with a file column is
  two states — *reading* and *waiting for the write* — which is also what makes
  it possible to show anything at all while 40 MB go up.

Replacing a file is the same flow: the new asset is linked, the old one is left
unlinked, and the gc takes it when nothing (and no snapshot, §7) holds it.

## 11. What changes, by layer

| Layer | Change |
|---|---|
| `timeranger2` | the `.blobs` store: put by content (sha256), get by id |
| `tr_treedb` | the `file` field type; `__assets__` created as a system topic; the derived hooks; the write path of §5 and its bulk door; the gc of §7; the limits of §8 |
| `treedb_system_schema.c` | `file` in the enforced `flag` enum; system `schema_version` + `cols` `topic_version` bumped |
| `C_ASSETS` | loses `put-asset` / `put-assets` / `import-assets` / `gc-assets` and both limits; keeps `get-asset` — and is then one command |
| `gobj-js` | `file` in `treedb_field_types` |
| `gobj-ui` | the form control: pick, hash, ask, attach; the table cell already draws an asset (`yui_asset.js`) |
| hosts | delete the `assets` topic from their schema; flag their columns `file` |

## 12. Migration: there is none

There is none, and that is a decision: **the stores are wiped and everything is
built again from scratch.**

Renaming `assets` to `__assets__` changes the fkey literal in every record that
links one, so a store that survived would carry links to a topic that no longer
exists. Migrating it in place is work to produce a state that a reload produces
anyway — the yunovatios stores have been wiped for less (schema 17, the same
day this note was written).

What that does NOT remove is the way back in. Rebuilding from scratch is exactly
re-ingesting 12 134 blobs and 346 MB, so the bulk load is not a convenience, it
is the only path — which is why it is treedb's too, and why it has to answer the
`path -> id` map (§5).

## 13. The tests: `tests/c/tr_treedb_files`

One suite, named like its neighbours (`tr_treedb_immutable`,
`tr_treedb_hook_hygiene`, `tr_treedb_snap`). Each case nails a claim of this
note that, if it broke, would break **quietly**:

1. **The bytes are not in the record.** Write a node with a `file` column; the
   blob is on disk under `.blobs/`, the index node is in `__assets__`, the fkey
   is on the child — and the stored record contains no `content64` and no
   `__files__`. This is the 460 MB of §1, and it has to be asserted, not
   assumed.
2. **The id is the hash of the BYTES.** Send a wrong id with good bytes: the
   write is refused. A client that lies must not be able to file one file's
   content under another file's hash (§5).
3. **The ceiling is applied before decoding** (§8). A file over `max_size` is
   refused, and refused while it is still encoded — the check that runs after
   the decode has already spent what it was defending.
4. **The type is read from the bytes** (§8). A png called `image/jpeg` is
   refused; an svg called `image/png` is refused, which is the case the
   allowlist exists for.
5. **The derived hooks persist nothing** (§3). Two topics with `file` columns:
   the in-memory desc of `__assets__` carries both hooks, `topic_cols.json`
   carries neither, and after a close/reopen they are derived again.
6. **The gc, three ways** (§7). An asset nothing links is collected; one a live
   node links is not; **one that only a SNAPSHOTTED version of a node links is
   not** — that third is the one nobody would notice being wrong.
7. **One kw, two files** (§5). A record setting two `file` columns at once
   travels with one gbuffer and a manifest of two slices, and produces two
   assets and two links.
8. **No leak.** `gobj_end()` before `get_cur_system_memory()`, per the repo
   rule — this design allocates buffers at a boundary, which is exactly where
   one hides.

## 14. Open items: none — SUPERSEDED, see §15

Every decision of model is taken. What remains is the work of §11 and the suite
of §13 — and the two things that will be got wrong if they are read past: the
manifest carries offsets because a kw holds ONE binary field at its top level
(§5), and the derived hooks are never persisted, so nothing about `__assets__`
is ever versioned (§3).

## 15. Review against the code (2026-09-05)

Every section above was checked against `tr_treedb.c`, `timeranger2.c`,
`treedb_system_schema.c`, `c_treedb.c`, `c_node.c`, `c_assets.c`,
`msg_ievent.c`, `kwid.c`, the JS side and the yunovatios schema. The model
holds. Seven statements do not, and each one changes what §11 has to build.
None of them is open in §14, so §14 is wrong to say "none".

### 15.1 Reopened decisions

1. **Who composes the fkey.** §5 puts a bare `<sha256>` in the column. Treedb
   refuses it: `filtra_fkeys()` (`tr_treedb.c:2266`) and `decode_parent_ref()`
   (`tr_treedb.c:3838`) demand exactly two `^`, and a bare string logs *"Wrong
   fkey reference"* and fails the create. Either the write path expands a bare
   id into `__assets__^<sha>^as_<T>_<C>` for a `file` column — a normalisation
   step that is not in §11 — or the client sends the full reference, and then
   the client has to know the derived hook name, which is the opposite of
   *"the host declares nothing but the column"*. `fkey_only_id` (§6) is no
   symmetry: it is a READ option.

2. **`file` is not `fkey` unless something says so.** Every link behaviour in
   `tr_treedb.c` keys on the literal word `fkey` in the flag — 30 sites of
   `kw_has_word(..., "fkey")` / `"hook"`: autolink, normalize, link, unlink,
   `load_all_links`, the delete guard. In JS, `treedb_get_field_desc()` takes
   as `type` the FIRST word it finds in `treedb_field_types`, and
   `c_yui_form.js:1304` asks `type === "fkey"`. Two ways out: (a) the author
   writes `['fkey', 'file']` and `file` qualifies the fkey, as `enum` does;
   (b) the derivation pass injects `fkey` into the in-memory flag of every
   `file` column. **(a) is recommended**: one word in the schema, no code in
   30 places — and `devices.foto` is `['fkey']` in yunovatios today
   (`db_history_schema.c:1048`), so the host ADDS a word rather than
   replacing one.

3. **The gc's only protection is the snapshot walk.** `treedb_shoot_snap()`
   skips every `__` topic (*"Ignore meta-tables"*, after `tr_treedb.c:9717`),
   so a node of `__assets__` never carries a tag and the guard *"cannot
   delete node, it has a tag"* never fires for an asset. §7 reaches the right
   rule for the wrong reason: it presents reading the snapshots as an
   extension of the tag guard, and the tag guard is inert here. Reading the
   host topics' tagged instances is not an extension, it is the whole guard.
   And *"a dry run costs nothing"* stops being true: it is a disk pass over
   every tagged instance of every topic with a `file` column.

4. **There is no sha256 at the timeranger2 layer.** `C_ASSETS` takes it from
   the TLS backend and refuses to compile without one (`c_assets.c:38-44`).
   `kernel/c/timeranger2/CMakeLists.txt` links no TLS library. Moving the hash
   into `tr_treedb` is a new dependency of the persistence layer on the
   crypto layer, with two backends selectable at run time; the alternative is
   a standalone sha256 in gobj-c's helpers. §11 lists neither.

### 15.2 Statements to correct

- **Three writes, not two.** §5 says *"the blob first, the node second"*. The
  host record's autolink needs the parent to EXIST — `treedb_autolink()`
  calls `treedb_get_node()` and fails with *"parent node not found"*. The
  order is blob → index node in `__assets__` → host record with its link. The
  state between the second and the third is an index node nothing links,
  which the gc takes; benign, but *"either it happened or the command
  failed"* is not exact.

- **§4 leans on a dead function.** `tranger2_list_topics()` has no caller in
  the tree. Topics come from the schema, and the replica watcher watches
  `<topic>/disks/<rt_id>/` per topic (`timeranger2.c:4940`). `.blobs` under
  the treedb directory collides with nothing today. Keep the dot as a
  convention; drop the ⚠️.

- **`__assets__` will not be in the `__system__` projection.** c_treedb opens
  every treedb from its projection (`c_treedb.c:940`), and `__snaps__` /
  `__graphs__` are created inside `treedb_open_db()`, outside it; `__assets__`
  follows them. So *"a GUI sees the hooks"* (§3) is true for the `desc` /
  `topics` commands of C_NODE — the treedb-topics view — and false for the
  Schemas workspace, which reads `__system__`. Say both.

- **Two paragraphs of §5 disagree.** One says that when the asset exists *"no
  bytes travel — the record is saved with the fkey and that is all"*. The
  other says *"the second arrival is an update of its node"* and sells the
  history of names. If the client skips the bytes, no `__files__` arrives and
  the index node is not touched: the history is recorded ONLY when bytes
  travel. Choose, and name the cost — a census reload that does send bytes
  appends one instance per asset, 12 134 appends with nothing changed.

- **`C_ASSETS` loses more than §11 lists.** `list-assets` and `delete-asset`
  (`c_assets.c:203-204`) become `nodes` / `delete-node` on `__assets__` and
  go too.

### 15.3 Constraints the note understates

- **The size ceiling arrives late.** §8 checks the base64 *"before spending
  what it defends"*. Before treedb sees anything, the transport has received
  the whole packet and jansson has parsed it into a string. `c_prot_tcp4h`
  takes `max_pkt_size` from `gbmem_get_maximum_block()` by default — that is
  `MEM_MAX_BLOCK`, 200 MB in the agent and 1 GB in `db_history_ce`. The real
  defence is the transport's; treedb's `max_size` has to sit BELOW it or it
  is unreachable. The `c_websocket` frame limit was not checked.

- **`treedb_create_node()` on an existing pkey warns WITH a stack trace**
  (`tr_treedb.c:5160`). The write path looks the asset up before creating it,
  or a census reload prints thousands of traces.

- **The derived hooks must be derived at run time too.** `parse_hooks()` runs
  at open (`tr_treedb.c:1113`) and from `parse_schema()` (`:1970`); a `file`
  column or a topic added while the yuno runs has to re-derive. *"A second
  pass at open"* is not enough.

- **`content64` already means the RECORD at C_NODE's door.** `create-node` /
  `update-node` take a top-level `content64` that is the whole record in
  base64, with priority over `record` (`c_node.c:2184-2238`). The one nested
  in `__files__` does not collide; a reader of `c_node.c` will trip on it.

- **A non-master replica gets the index and not the bytes.** The watcher
  replicates the topic's files; `.blobs` travels by another means or not at
  all.

### 15.4 Verified as written

The line citations hold: `tr_treedb.c:745` / `:834`, `timeranger2.c:1289`,
`c_assets.h:61`, `c_assets.c:151` / `:1090` / `:1624`, `gobj.c:592`,
`gbuffer.c:617` / `:657`. `system_topic` protects the topic and not its
records, so the gc may delete nodes of `__assets__`. A kw carries ONE
top-level binary field, and the ievent channel serialises and deserialises it
(`msg_ievent.c:126`, `:212`). `image` and `icon` are in the `flag` enum; the
bump is system `schema_version` 16 → 17 and `cols` `topic_version` 10 → 11.
`delete-node` with `force` unlinks the children. `yui_asset_id()` exists with
tests for the three shapes (its literals say `assets`, not `__assets__`). The
open sequence already has the place for the derivation: `parse_hooks()` runs
after every topic is created and before `load_all_links()`. The neighbour
suites named in §13 exist.
