# Design: a `file` column, and `__assets__` as a treedb system topic

Status: **IMPLEMENTED and merged** (2026-09-05). The four decisions §15.1
reopened were taken as §15 recommended: treedb expands a bare id into the full
fkey; a `file` column is flagged `['fkey','file']`; the gc's only guard is the
snapshot walk; sha256 is a standalone helper of gobj-c. Column-level policy
lives in the column's `properties` (`max_size`, `content_types`).

The body below has been corrected where §15.2 said it was wrong, so §1-§13
describe **what was built**, not what was proposed. §15 is kept as the dated
review that found those statements. **§16 is what the implementation itself
found**, and it is the part to read before touching this code: six defects
that the design could not have predicted, and what is still open.

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
   (Done: system 16 → 17, `cols` 10 → 11.)

⚠️ **`file` goes WITH `fkey`, and the column is a `string`:**
`{'type': 'string', 'flag': ['fkey', 'file']}`. Every link behaviour of
`tr_treedb.c` keys on the literal word `fkey` — 30 sites of
`kw_has_word(..., "fkey")` — and so does the GUI (`c_yui_form.js` asks
`type === "fkey"`). So `file` QUALIFIES the fkey, the way `enum` qualifies a
string; it does not replace it. `derive_file_hooks()` refuses a schema that
gets this wrong, and the refusal is fatal at open.

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
The `desc` a client reads is the in-memory one, **so the treedb-topics view
sees the hooks and the Schemas workspace does not**: `desc` / `topics` of
`C_NODE` answer from memory, while the Schemas workspace reads the `__system__`
projection, and `__assets__` is created inside `treedb_open_db()`, outside it —
like `__snaps__` and `__graphs__`. Both are true and both are worth knowing.

**The derivation is a reconciliation, and it runs whenever the schema can have
moved.** At open it is a second pass — `__assets__` is created after the host's
topics, so the hooks can only be added once every schema has been parsed — but
`create-topic` and `delete-topic` are live commands of `C_TREEDB`, so
`treedb_create_topic()` and `treedb_delete_topic()` run it too. It adds what the
schema now asks for and removes what it stopped asking for, and both halves are
needed: see §16.

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
    __assets__/              the index: the topic's timeranger2 files
    .blobs/ab/cd/<id>.<ext>  the content, addressed by sha256
    devices/  places/  ...
```

Two levels of fanout, not one: a flat directory of ten thousand files is a
directory nobody can look at. And the extension is derived from the CONTENT
TYPE STORED, never from the name that was given — it is what a web server
reads to set the `Content-Type`, so it is part of the served url (§16).

Both halves inside the treedb directory, so a `cp -a` of it carries the nodes
**and** their bytes. Today they are split — the index inside, the blobs a
sibling of the treedb at realm level — which is what `C_ASSETS` was reaching for
and did not quite get.

**The blob directory starts with a dot**, as a convention rather than a
requirement: `tranger2_list_topics()` — which returns every entry of the
directory as a topic, skipping the ones that start with `.` — has no caller in
the tree, and topics come from the schema. The dot costs nothing and keeps the
guarantee if that ever changes.

## 5. The write path

The client computes the sha256 **before sending anything**, which is what makes
this cheaper than `put-asset`:

1. the browser hashes the file (`crypto.subtle.digest`) and fills the `id`;
2. it asks the treedb whether that node exists;
3. **if it exists, no bytes travel** — the record is saved with the fkey and
   that is all, and the index node is NOT touched;
4. if it does not, the bytes ride along in the same `create-node` /
   `update-node`.

Step 3 is where the optimisation is paid for, and it costs one thing: the
history of names below is written only when bytes travel. A census reload that
takes the optimisation records nothing new; one that does not appends an
instance per asset — 12 134 appends with nothing changed. Both are correct, and
the cheap one is the default.

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

The column's value on the wire is **the bare id**, and the bytes ride BESIDE
the record, never inside the column. **Treedb expands it** into the full
reference the links speak — `__assets__^<sha256>^as_<T>_<C>` — so the client
never has to know the derived hook name, which is the whole point of the host
declaring nothing but the column. A client that sends the full reference
already is taken as it is:

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

One message, but **three writes**, in this order: the blob, the index node in
`__assets__`, then the host record with its link — the link needs the parent
to EXIST. Interrupted between the second and the third it leaves an index node
nothing links, which the gc takes; interrupted before the second, an orphan
blob, which the gc also takes. Never a link to nothing. So *"either it happened
or the command failed"* is the right shape, but it is not exact: what a failure
can leave behind is garbage, never a dangling reference.

**And the third write is the write path's own, `autolink` or not.** An
ordinary fkey is edited by linking — `treedb_update_node()` skips every fkey
in its field loop and `treedb_create_node()` links nothing, so a link moves
only through `link-nodes` or an `autolink`. A `file` column is edited by
handing over a file, and the link is part of the hand-over: `link_file_columns()`
runs inside `treedb_create_node()` and `treedb_update_node()`, links what the
kw's `file` columns name, unlinks what they stop naming (`""`), and leaves a
column the kw does not carry alone. Before that (§16.8) a `create-node`, or an
`update-node` without `autolink`, stored the bytes and the index node, answered
success, and left the column as it was — an orphan asset and a device with no
photo, the one message split in two after all.

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

**And the FIRST arrival names the file, for ever.** The stored `content_type`
is never changed by a later arrival, because the extension it picks is part of
the path a web server serves and the url is cached for ever (§6) — the name
cannot move under it. It is not only about the url: one container can be
declared as more than one type, so the same bytes as `video/mp4` and as
`audio/mp4` would land on two blobs for one asset (§16).

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

- **The LIVE refusal comes for free, the snapshot one does not.** Deleting an
  asset that a node still links is refused by treedb's ordinary delete guard,
  unless `force` is given. The snapshot half is not free and it is not the tag
  guard either: `treedb_shoot_snap()` skips every `__` topic, so a node of
  `__assets__` never carries a tag and *"cannot delete node, it has a tag"*
  never fires for an asset. `treedb_delete_node()` runs the snapshot walk of
  this section for a node of `__assets__`, and — like the tag guard it stands
  in for — **`force` does not override it**: force means "unlink the children",
  never "ignore what a snapshot needs".
- **The sweep is ON DEMAND, never automatic.** Dropping the blob the moment the
  last link goes is tempting and wrong: `treedb_delete_node` with `force`
  UNLINKS children rather than deleting them, so an asset sitting unlinked is a
  normal intermediate state of a bulk operation. An automatic gc would delete
  bytes that are re-linked a second later.
- **It says what it would take before taking it**, and that is worth its
  price: the dry run is not free, it is a disk pass over every tagged instance
  of every topic with a `file` column. Same cost as the run itself, minus the
  deletes.

**And it reads the SNAPSHOTS, not only the live state.** Otherwise: delete a
device, its asset is orphaned, the gc takes it — and then somebody activates a
snap where that device existed and linked it, and the link dangles. So "nothing
links it" means *no live node and no snapshotted version of one*, which turns
the sweep from a walk over the in-memory hooks into a walk that also reads the
tagged record instances. The derived hooks still say which columns to look at;
what changes is how many versions of a node are asked.

Two things follow from that:

- **A snapshot holds bytes alive.** What frees an asset is not deleting the node
  that linked it, it is deleting the SNAP that still remembers the link. It
  reads like the rule `treedb_delete_node` applies to a node carrying a snap
  tag, and it is not the same mechanism: the tag guard is inert here (above),
  so this walk is the whole of it.
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

The split shipped as written: the ceiling is the treedb's
(`treedb_set_files_limits()`, and the `C_NODE` attrs `files_max_size` /
`files_content_types`), the policy is the column's, read from its `properties`
(`{'max_size': 4096, 'content_types': ['application/pdf']}`) — the catch-all
the `__system__` projection carries verbatim, so it needs no schema of its own.

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

| Layer | Change | |
|---|---|---|
| `gobj-c` | `sha256_digest()` / `sha256_hex()` in `helpers.c`, standalone (FIPS 180-4): the persistence layer hashes what it stores and links no TLS backend | done |
| `tr_treedb` | the `file` field type; `__assets__` created as a system topic; the derived hooks and their reconciliation; `treedb_store_files()`; `treedb_import_files()`; `treedb_gc_files()`; the limits of §8; the `.blobs` store, put by content and got by id | done |
| `treedb_system_schema.c` | `file` in the enforced `flag` enum; system `schema_version` 16 → 17, `cols` `topic_version` 10 → 11 | done |
| `C_NODE` | `import-assets` and `gc-assets`; the attrs `files_max_size`, `files_content_types`, `import_root`; the bytes handed from the command kw to the record | done |
| `C_ASSETS` | loses `put-asset` / `put-assets` / `list-assets` / `delete-asset` / `import-assets` / `gc-assets`, both limits and the store attrs; keeps `get-asset` — and is then one command | done |
| `gobj-js` | `file` in `treedb_field_types` (7.16.4) | done |
| `gobj-ui` | the form control: pick, hash, ask, attach (`yui_file_field.js`, 7.23.60); the table cell draws the id, the asset itself is a `get-asset` away | done |
| hosts | delete the `assets` topic from their schema; flag their columns `['fkey','file']`; rebuild the store | local done, **remote open** |

`list-assets` and `delete-asset` are gone because `nodes` and `delete-node` on
`__assets__` are the same two commands with nothing added.

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

Two more came out of the implementation, and each of them is a bug that shipped
in the first pass (§16):

9. **One asset is ONE blob**, and a second arrival does not rename it: the same
   container under its two legal names writes one file and keeps the stored
   type. Plus the delete of an asset a SNAPSHOT holds, refused with `force`.
10. **The hooks follow the schema at RUN TIME**, both ways: a topic with a
    `file` column created with the store running links through its hook, and
    one deleted takes its hook — and the children it held — away with it.

`tests/c/c_assets` covers the same ground through the commands: both doors of
`__files__`, `get-asset` inline and signed, `import-assets` with hostile paths,
`gc-assets`, and a `gbuffer` on writes that FAIL.

## 14. Open items — SUPERSEDED, see §16

Written when nothing was implemented, and wrong then too (§15 says why). The
two things it is right about are still the two that will be got wrong if they
are read past: the manifest carries offsets because a kw holds ONE binary field
at its top level (§5), and the derived hooks are never persisted, so nothing
about `__assets__` is ever versioned (§3).

What is actually open is in §16.

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

## 16. What the implementation found (2026-09-05)

§15 was a review of the note against the code. This is the other direction:
what writing the code found that the note could not have. Six defects, each
with a regression test verified to fail with its fix reverted. They are here
because every one of them would have broken QUIETLY.

### 16.1 The bytes of a `file` column were released twice

`create-node` / `update-node` are the first commands in the tree to take a
`gbuffer`, and the door they came through releases it twice.

`expand_command()` builds the command kw by copying the caller's keys **by
value** (`json_object_update_missing`, `command_parser.c`), so a top-level
binary field is named by TWO kws and both are `KW_DECREF`'d — the command's
answer releases one, `command_parser` the other. Every exit of the command
earlier than the hand-off reached it, `-403` included, which is the one a
controller forwarding a record with its bytes meets in production.

And there was a second owner downstream: every exit of `mt_create_node()` /
`mt_update_node()` ends in `KW_DECREF(kw)`, which drops the binary field it
finds. So `treedb_store_files()` consuming it and that `KW_DECREF` are the
only two releases, and never both — anything else is one too many.

**The rule:** a command that receives a `gbuffer` TAKES it out of its kw
before its first possible exit (`take_files_gbuffer()`), and nothing else
releases it.

### 16.2 A NULL where `json_typeof()` is read

`gc_scan_callback()` handed `filtra_fkeys()` the value of a column that a
tagged instance older than the `file` column does not carry, and
`filtra_fkeys()` reads `json_typeof()` with no NULL guard. `gc-assets`
segfaulted on any store that gained a `file` column after its snapshots —
which is a topic_version bump, an ordinary thing to do.

### 16.3 A guard that was documented and not written

`treedb_open_db()` ignored the return of `derive_file_hooks()`, so a `file`
column not flagged `fkey` was *documented* as refused at open (§2) and was
not: the write path still stored the asset and wrote an fkey-looking string
into a column nothing links — and then `gc-assets` collected the bytes that
string names. It is fatal at open now.

### 16.4 `as_<T>_<C>` is ambiguous

Topic `a_b` column `c` and topic `a` column `b_c` write the same hook name,
and so does anything `snprintf` truncates at `NAME_MAX`. The loser was
skipped in silence and linked through the winner's hook, onto another
topic's column. The derivation compares the mapping now, and a real
collision refuses the open.

### 16.5 One asset, two blobs

`content_type_compatible()` accepts any two members of a shared container —
the same isobmff bytes are `video/mp4` and `audio/mp4` alike, and so are
`video/webm`/`audio/webm` and `video/ogg`/`audio/ogg` — but the blob path
was built from the DECLARED type, so the second arrival wrote `<id>.m4a`
beside the `<id>.mp4` the first one wrote. Two blobs for one asset, and the
row names only one: the other could never be served, never be seen by the gc
(which reads rows, not files) and never be removed by the delete.

The stored `content_type` wins now. That is not only about the leak: the
extension is part of the path a web server serves and the url is cached for
ever, so the name cannot change under it.

### 16.6 The hooks did not follow the schema at run time

`create-topic` and `delete-topic` are live commands of `C_TREEDB`, and the
derivation ran only at open. A topic **added** while the yuno runs had a
`file` column whose hook did not exist, so nothing could link through it; a
topic **deleted** left its hook behind, still holding the children it had —
and the gc reads a hook that is not empty as *"some node links this asset"*,
so those bytes were never collected again.

Adding a hook is two things, and the second is the one that bites: **a hook
is a field OF THE NODE**, put there from the desc when the record was read,
so a hook added later exists in the desc and in no node and `_link_nodes()`
answers *"hook field not found"*. The derivation seeds it into the
`__assets__` nodes already loaded.

One bound on the removal: `__assets__` is a topic of the TRANGER, not of the
treedb, so a tranger holding two treedbs shares it. The reconciliation takes
only what maps to a topic of THIS treedb or to one no longer open at all.

### 16.7 Still open

- **The hosts' remote stores** (§11, §12): the yunovatios nodes still run the
  `<realm>/assets/` model, and rebuilding them is the migration §12 says there
  is not.
- **A non-master replica gets the index and not the bytes.** The watcher
  replicates the topic's files; `.blobs` travels by another means or not at
  all, so `get-asset` inline answers *"asset has no bytes on disk"* there.
  Not decided, not implemented, and the docs do not say it.
- **`treedb_create_node()` on an existing pkey warns WITH a stack trace**
  (§15.3). The write path looks the asset up before creating it, so the
  census reload is quiet — but the trap is still there for the next caller.

### 16.8 Found by the review of 2026-09-05

Two defects, one in each layer, and the same symptom for both: a save that
says yes and leaves the photo behind.

- **The write path stored the asset and did not link it.** `treedb_update_node()`
  skips every fkey in its field loop — right for a link a person edits by
  linking — and `treedb_create_node()` links nothing at all, so the link of a
  `file` column existed only where an `autolink` followed. A `create-node`, or
  an `update-node` without `autolink`, wrote the blob and the index node,
  answered success and left the column as it was: an orphan asset, and a
  device with no photo. The GUI always sends `autolink`, so it never saw it;
  `ycommand` and the `EV_TREEDB_UPDATE_NODE` event do not. `link_file_columns()`
  now runs inside both writes (§5); test 11 of `tr_treedb_files` covers create,
  move, clear and a reopen.
- **The GUI dropped a read-only `file` column from the record, and `autolink`
  then cut its link.** The topic view sends back only the writable cols, the
  fkeys and the pkey; a `file` column IS an fkey but answers `type: "file"`
  since gobj-js 7.16.5, so a `file` column without `writable` — the one only a
  load fills, which the open declares legal with a warning — fell out of the
  record, and every save of any other field of its record unlinked it. gobj-ui
  7.23.63 asks `is_file` beside the type.

Two more from the same review, closed the same day:

- **The gc's bypass of the snapshot guard was a key of the options.**
  `treedb_gc_files()` walks the snapshots once for the whole run and told
  `treedb_delete_node()` so with `__snaps_walked__` in `jn_options` — and
  `delete-node` forwards its `options` from the wire as they are, so a client
  with `delete` could spell the bypass. It is a parameter of the private
  `delete_node()` now; the public entry always walks. Test 6 sends the old key
  and is refused.
- **A run-time `create-topic` did not refuse a `file` column that is not
  `fkey`.** `derive_file_hooks()` refused it at open, fatally, but
  `treedb_create_topic()` ignored its return and `treedb_store_files()` keyed
  on the word `file` alone — the §16.3 hole, through the live command. The
  check is `check_file_column()` now, asked by `treedb_create_topic()` BEFORE
  the topic exists (refused, with the cause in `last_message`), by the open
  (which now says with `on_critical_error` when a topic of the schema could not
  be created, instead of leaving it for the first write to find as *"Topic
  name not found"*), and by the write path, which refuses a `file` without
  `fkey` rather than storing into it. Test 10 creates such a topic and finds
  it nowhere.

A third, and it took a test with two treedbs on one tranger to find its
second half:

- **Two treedbs on one tranger: the gc and the delete read one copy.**
  `__assets__` is the tranger's, but each treedb loads its OWN copy of every
  node and a link made from a treedb lands in that copy. Read from one copy,
  *"nothing links it"* was true of one treedb and false of the other, and the
  row and the bytes are the tranger's: `gc-assets` from the first took what
  the second linked, and `delete-node` did too. Both ask every treedb's copy
  now (`asset_linked_by_other_treedb()`), the delete refuses `force` or not —
  force unlinks the children of THIS copy and the other's would dangle — and a
  deleted row leaves every treedb's index, or the other one keeps answering a
  row that is not on disk. And the derived hooks are seeded into — and taken
  from — every treedb's copies, not the deriving treedb's only: the desc is
  shared, `get_node_down_refs()` walks it and expects each field in the node,
  and the first delete of a copy that lacked the other treedb's hook said
  *"field not found in the node"*. Test 12. What is NOT done: an asset stored
  from one treedb is not in the other's index until it reopens, so the other
  can link it with its bytes (a second instance of the same key, its own
  copy) but not by bare id — the client's fallback of sending the bytes covers
  it, which is why it is left.

Still open from the same review, in order of weight: §7's *"deleting the snap frees the
asset"* names an operation that does not exist, and `treedb_save_node()`
inherits the tag, so the gc holds for ever what any tagged instance ever
linked; the form has no *reading* state, so a second Save during a long read
sends two writes; the `gbuffer` door works through the commands only, not
through `EV_TREEDB_UPDATE_NODE`; a plain update can drop a SEED link of a
`file` column, because the seed guard of `mt_update_node()` runs only with
`autolink` (no seed carries a file today).
