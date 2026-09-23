# tr_treedb

Graph memory database with hook/fkey relationships, persisted through timeranger2. See the [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) for link/unlink rules and `g_rowid` semantics.

Source code:

- [`tr_treedb.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.h)
- [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c)

(_treedb_create_topic_cols_desc)=
## [`_treedb_create_topic_cols_desc()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L553)

The `_treedb_create_topic_cols_desc()` function creates and returns a JSON object describing the column schema for a TreeDB topic.

```C
json_t *_treedb_create_topic_cols_desc(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON list describing what a user column may declare. **The return is yours**
— every caller decrefs it (`parse_schema()`, `treedb_open_db()`).

**Notes**

It is DERIVED from the `cols` topic of `treedb_system_schema`, not written by
hand: `value` is renamed back to `id`, and the storage-only fields of that
topic (`id`, `topics`, `order`, `_geometry`) are dropped, because they say how
a column is stored in `__system__` and not what a column may declare. A field
added there for user columns needs nothing here; a storage-only one has to be
added to that skip list too.

---

(add_jtree_path)=
## [`add_jtree_path()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11070)

The `add_jtree_path()` function appends a child node to a parent node in a hierarchical JSON tree structure.

```C
int add_jtree_path(
    json_t *parent,  // not owned
    json_t *child    // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parent` | `json_t *` | A pointer to the parent JSON node. This parameter is not owned by the function. |
| `child` | `json_t *` | A pointer to the child JSON node to be added. This parameter is not owned by the function. |

**Returns**

Returns `0` on success, or a negative error code if the operation fails.

**Notes**

The function does not take ownership of the `parent` or `child` nodes. This means the caller is responsible for managing their memory.

---

(create_template_record)=
## [`create_template_record()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L14680)

`create_template_record()` generates a new template record based on the provided column definitions and input data.

```C
json_t *create_template_record(
    const char *template_name, // used only for log
    json_t *cols,       // NOT owned
    json_t *kw          // Owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `template_name` | `const char *` | The name of the template, used only for logging purposes. |
| `cols` | `json_t *` | A JSON object describing the column definitions. This parameter is not owned by the function. |
| `kw` | `json_t *` | A JSON object containing the input data for the template record. This parameter is owned by the function. |

**Returns**

Returns a JSON object representing the created template record.

**Notes**

The returned JSON object must be decremented (`json_decref()`) by the caller when no longer needed.

---

(current_snap_tag)=
## [`current_snap_tag()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L202)

Retrieves the current snapshot tag of the specified `treedb_name` in the given `tranger` instance.

```C
int current_snap_tag(
    json_t  *tranger,
    const char  *treedb_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the `tranger` instance managing the tree database. |
| `treedb_name` | `const char *` | Name of the tree database whose current snapshot tag is to be retrieved. |

**Returns**

Returns the current snapshot tag as an integer.

**Notes**

The snapshot tag is used to track versions of the tree database.

---

(decode_child_ref)=
## [`decode_child_ref()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L4409)

Parses a child reference string formatted as 'child_topic_name^child_id' and extracts its components into separate buffers.

```C
BOOL decode_child_ref(
    const char *pref,
    char *topic_name,    int topic_name_size,
    char *id,           int id_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `pref` | `const char *` | The child reference string in the format 'child_topic_name^child_id'. |
| `topic_name` | `char *` | Buffer to store the extracted child topic name. |
| `topic_name_size` | `int` | Size of the 'topic_name' buffer. |
| `id` | `char *` | Buffer to store the extracted child ID. |
| `id_size` | `int` | Size of the 'id' buffer. |

**Returns**

Returns `TRUE` if the reference was successfully parsed, otherwise returns `FALSE`.

**Notes**

This function is used to extract child references from hierarchical tree structures in the TreeDB system.

---

(decode_parent_ref)=
## [`decode_parent_ref()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L4355)

Parses a parent reference string into its components: topic name, ID, and hook name. The reference format is 'parent_topic_name^parent_id^hook_name'.

```C
BOOL decode_parent_ref(
    const char *pref,
    char *topic_name,    int topic_name_size,
    char *id,           int id_size,
    char *hook_name,    int hook_name_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `pref` | `const char *` | The parent reference string in the format 'parent_topic_name^parent_id^hook_name'. |
| `topic_name` | `char *` | Buffer to store the extracted topic name. |
| `topic_name_size` | `int` | Size of the `topic_name` buffer. |
| `id` | `char *` | Buffer to store the extracted parent ID. |
| `id_size` | `int` | Size of the `id` buffer. |
| `hook_name` | `char *` | Buffer to store the extracted hook name. |
| `hook_name_size` | `int` | Size of the `hook_name` buffer. |

**Returns**

Returns `TRUE` if the reference was successfully parsed, otherwise returns `FALSE`.

**Notes**

This function is used to extract structured information from a parent reference string, which is used in hierarchical relationships within the tree database.

---

(node_collapsed_view)=
## [`node_collapsed_view()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L10017)

Generates a collapsed view of a node in the tree database, applying filtering and transformation options.

```C
json_t *node_collapsed_view(
    json_t *tranger,    // NOT owned
    json_t *node,       // NOT owned
    json_t *jn_options  // owned, fkey, hook options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger instance managing the tree database. Not owned by the caller. |
| `node` | `json_t *` | Pointer to the node to be collapsed. Not owned by the caller. |
| `jn_options` | `json_t *` | JSON object containing options for collapsing the node, including fkey and hook options. Owned by the caller. |

**Returns**

A JSON object representing the collapsed view of the node. The caller must decrement the reference when done.

**Notes**

The function applies filtering and transformation rules based on `jn_options` to generate a simplified representation of the node.

---

(parse_hooks)=
## [`parse_hooks()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2535)

`parse_hooks()` processes the schema to extract and validate hook definitions.

```C
int parse_hooks(
    json_t *schema  // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `schema` | `json_t *` | A JSON object representing the schema. It is not owned by the function. |

**Returns**

Returns `0` on success or a negative number if an error occurs during parsing.

**Notes**

This function makes sure that hooks in the schema are correctly defined and structured.

---

(parse_schema)=
## [`parse_schema()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2402)

`parse_schema()` validates and processes a JSON schema definition. This makes sure of its structure and integrity.

```C
int parse_schema(
    json_t *schema  // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `schema` | `json_t *` | A JSON object representing the schema definition. The caller retains ownership. |

**Returns**

Returns `0` on success, or a negative error code if the schema is invalid.

**Notes**

This function does not modify the input `schema` and does not take ownership of it.

A column flagged both `hook` and `fkey` is refused (*"A column cannot be both
'hook' and 'fkey'"*). A node that is both child and parent carries two
columns, the hook and the fkey:

```C
'manager': {
    'header': 'Manager',
    'type': 'array',
    'flag': ['fkey']
},
'managers': {
    'header': 'Managers',
    'type': 'object',
    'flag': ['hook'],
    'hook': {
        'departments': 'manager'
    }
}
```

---

(parse_schema_cols)=
## [`parse_schema_cols()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2437)

`parse_schema_cols()` validates and processes the column definitions in a schema. This makes sure of correctness and consistency.

```C
int parse_schema_cols(
    json_t *cols_desc,  // NOT owned
    json_t *data        // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `cols_desc` | `json_t *` | A JSON object describing the schema columns. This parameter is not owned by the function. |
| `data` | `json_t *` | A JSON object containing the column data to be validated. This parameter is owned by the function. |

**Returns**

Returns `0` if the schema columns are valid, or a negative number indicating the number of errors encountered.

**Notes**

The function makes sure that the column definitions conform to the expected schema format. If errors are found, the return value indicates the number of issues detected.

---

(set_volatil_values)=
## [`set_volatil_values()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L3539)

The `set_volatil_values()` function assigns volatile values to a record in the TreeDB. This makes sure that non-persistent fields are set using default values if not provided.

```C
int set_volatil_values(
    json_t *tranger,
    const char *topic_name,
    json_t *record,
    json_t *kw,
    BOOL broadcast
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to which the record belongs. |
| `record` | `json_t *` | The record to update with volatile values. This parameter is not owned by the function. |
| `kw` | `json_t *` | A JSON object containing the values to be set. This parameter is not owned by the function. |

**Returns**

Returns `0` on success, or a negative error code if an issue occurs.

**Notes**

This function does not modify foreign key (`fkey`), hook, or persistent fields. It only updates non-persistent attributes.

---

(treedb_activate_snap)=
## [`treedb_activate_snap()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L14505)

Marks a previously shot snapshot as active (`active: true` on the snap node in `__snaps__`). Use the reserved name `"__clear__"` to instead deactivate whichever snap is currently active — this is the *deactivate-snap* path.

```C
int treedb_activate_snap(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *snap_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger instance managing the TreeDB. |
| `treedb_name` | `const char *` | The name of the TreeDB where the snapshot is stored. |
| `snap_name` | `const char *` | The name of the snapshot to activate, or `"__clear__"` to deactivate the current active snap. |

**Returns**

Returns the snapshot tag (snap `id` = its `g_rowid` in `__snaps__`) as an integer on success, `0` for the `"__clear__"` deactivate path, or a negative value on error (`-1` if the named snap is not found, on a replica, or when a snap node cannot be saved).

A save that fails changes nothing, in memory either: the snap that was active stays active, and the one being activated stays inactive. Until 7.25.3 the `"__clear__"` path answered `0` with the save failed, and the snap went on being active on disk.

An activation saves the active snap inactive FIRST, then the new one active. When the second save fails, the old snap is saved active again and the error says so (*"Cannot activate snap, the one active before is active again"*); until 7.25.4 it stayed inactive, and the treedb was left with no active snap. Only if that save fails too is no snap active, on disk and in memory, and the error says that instead.

```C
if(treedb_activate_snap(tranger, "treedb_agent", "__clear__") < 0) {
    // still active: the cause is in the log (and gobj_log_last_message())
}
```

**Behavior**

This call only toggles the `active` flag on the snap node. The primary index of every topic is **not** refreshed in memory. The new visibility takes effect on the next `treedb_open_db()`:

- With a snap active, the topic loader filters by `user_flag = snap_tag` and only records carrying that tag become primary — this gives rollback to the state captured by [`treedb_shoot_snap()`](<#treedb_shoot_snap>).
- After `"__clear__"` (or with no snap ever shot) the loader applies no filter. The backward scan picks the highest-`rowid` record per key as primary — that is, the latest live record.

Consumers that need the change visible immediately must close and reopen the resource (for example `gobj_stop` + `gobj_start` on the gclass that owns the treedb, which is what the agent's `restart_nodes` does).

**Working from an activated snap**

An activation is a filtered load, **not a restore**: nothing is rewritten and nothing is undone, so the store keeps every record it had and only the answer of the primary index changes. It serves two purposes — looking at a state that was marked as good, and carrying on working from it. The second one has a rule:

> Working from an activated snap **ignores** everything written after the shot, for every key you touch, and it ignores it without destroying it.

Reading a node under snap S gives the record S froze. Saving it appends a new record, tagged 0, whose content is the photo's plus the change — never the content of the records written in between. That record is the newest of its key, so it becomes the primary after the deactivation and its reload; the records in between stay on disk and in the secondary indexes, and stop being read as the current state.

| From inside an activated snap | What it does to what came after |
|---|---|
| read | hides it: the primary answers with the photo |
| update / save | ignores it for that key: the new record descends from the photo |
| create of an id born after the shot | the same, by another door: the id is absent from the FILTERED primary index, so the create is accepted and appends over the record already there |
| delete | **destroys it**: [`treedb_delete_node()`](<#treedb_delete_node>) refuses only what a snap holds, and a node born after the shot is held by none — the key is erased and deactivating does not bring it back |

It is **per key, not per store**: a node never touched keeps the record written after the shot as its newest, so the deactivation brings it back as it was. The activation does not put the treedb into a past state, it makes the past the thing you write from.

```C
/*  binaries^ycommand: 7.21.0 at the shot, 7.23.0 installed after it  */
treedb_activate_snap(tranger, treedb_name, "pre-upgrade");   // + reload
json_t *node = treedb_get_node(tranger, treedb_name, "binaries", "ycommand");
/*  node says 7.21.0, not 7.23.0                                      */
treedb_update_node(tranger, node, json_pack("{s:s}", "description", "patched"), TRUE);
/*  the appended record says 7.21.0 + the new description: the 7.23.0
    record is still on disk and in the pkey2 index, and is no longer
    what the primary index answers after the deactivation.            */
treedb_activate_snap(tranger, treedb_name, "__clear__");     // + reload
```

The full account, with the worked walk of one key, is in [Snapshots](https://doc.yuneta.io/yuno-treedb#id-3-9-snapshots-treedb-level).

**Notes**

Make sure that the snapshot exists before calling [`treedb_activate_snap()`](<#treedb_activate_snap>) (except for `"__clear__"`, which is always valid).

---

(treedb_autolink)=
## [`treedb_autolink()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L8954)

`treedb_autolink()` automatically links a node using foreign key fields from the provided JSON object.

```C
int treedb_autolink(
    json_t  *tranger,
    json_t  *node,   // NOT owned, pure node
    json_t  *kw,     // owned
    BOOL    save
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `node` | `json_t *` | Pointer to the node to be linked. This node is not owned by the function. |
| `kw` | `json_t *` | JSON object containing foreign key fields used for auto-linking. This object is owned by the function. |
| `save` | `BOOL` | Flag indicating whether to save the changes to the database. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

The function uses the foreign key fields in `kw` to establish links between nodes.
If `save` is `TRUE`, the changes are persisted in the database.
The `node` parameter must be a valid pure node object.

It only ADDS links, and it stops at the first ref it cannot link. To make the links of a node equal to the ones a record names, use [`treedb_replace_links()`](<#treedb_replace_links>): it does not touch the links that do not change, and a bad ref does not stop the others. `C_NODE`'s `update-node` with `autolink` uses `treedb_replace_links()`, not `treedb_clean_node()` + `treedb_autolink()`.

---

(treedb_blob_path)=
## [`treedb_blob_path()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11622)

Where the bytes of an asset of a `file` column live: `<treedb dir>/.blobs/ab/cd/<id>.<ext>`. The id is the lowercase sha256 of the bytes; the two fanout levels are its first four hex characters, and the extension comes from the stored content type ([`treedb_file_ext()`](#treedb_file_ext)), never from the name the file was given. The directory starts with a dot so no scan of the treedb directory takes it for a topic.

```C
int treedb_blob_path(
    json_t      *tranger,
    const char  *id,
    const char  *content_type,
    char        *bf,
    size_t      bflen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The tranger of the treedb (its `directory` is the root). |
| `id` | `const char *` | The asset id: 64 lowercase hex characters. |
| `content_type` | `const char *` | The stored mime type; it picks the extension. |
| `bf` | `char *` | Output buffer. |
| `bflen` | `size_t` | Size of `bf` (`PATH_MAX`). |

**Returns**

`0`, or `-1` with *"Asset id is not a lowercase sha256"* logged when `id` is not one, or when the path does not fit.

**Example**

```C
char path[PATH_MAX];
treedb_blob_path(tranger,
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
    "image/png", path, sizeof(path));
/* <directory>/.blobs/ba/78/ba7816bf...15ad.png */
```

---

(treedb_clean_node)=
## [`treedb_clean_node()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L8881)

`treedb_clean_node()` removes all foreign key links from a given node in the tree database, effectively disconnecting it from its parent and child relationships.

```C
int treedb_clean_node(
    json_t  *tranger,
    json_t  *node,   // NOT owned, pure node
    BOOL     save
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `node` | `json_t *` | The target node to be cleaned. This node is not owned by the function. |
| `save` | `BOOL` | If `TRUE`, the changes are saved to the database. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

This function only removes foreign key links. It does not delete the node itself. If `save` is `TRUE`, the changes are persisted in the database.

---

(treedb_close_db)=
## [`treedb_close_db()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L1388)

Closes the TreeDB instance identified by `treedb_name` in the given `json_t *` `tranger`. This function makes sure that all resources associated with the TreeDB instance are properly released.

```C
int treedb_close_db(
    json_t *tranger,
    const char *treedb_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the `json_t *` object representing the TimeRanger instance. |
| `treedb_name` | `const char *` | The name of the TreeDB instance to be closed. |

**Returns**

Returns `0` on success, or a negative error code if the operation fails.

**Notes**

Make sure that [`treedb_open_db()`](<#treedb_open_db>) was previously called before attempting to close the TreeDB instance.

---

(treedb_close_topic)=
## [`treedb_close_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L1943)

Closes the specified topic in the TreeDB system. This makes sure that all associated resources are properly released.

```C
int treedb_close_topic(
    json_t  *tranger,
    const char  *treedb_name,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger instance managing the TreeDB. |
| `treedb_name` | `const char *` | Name of the TreeDB containing the topic to be closed. |
| `topic_name` | `const char *` | Name of the topic to be closed. |

**Returns**

Returns `0` on success, or a negative error code if the operation fails.

**Notes**

Make sure that the topic is not in use before calling [`treedb_close_topic()`](<#treedb_close_topic>).

---

(treedb_content_type_of_name)=
## [`treedb_content_type_of_name()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11473)

The mime type a file NAME claims, by its extension (case-insensitive). The pairs that share a container are told apart by extension on purpose: `.webm` is video and `.weba` audio, `.mp4` video and `.m4a` audio, `.ogv` video and `.ogg` audio. A name is only a claim: the write path checks it against the bytes ([`treedb_sniff_content_type()`](#treedb_sniff_content_type)).

```C
const char *treedb_content_type_of_name(
    const char  *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | A file name or path. |

**Returns**

A static string: one of `image/jpeg`, `image/png`, `image/webp`, `image/gif`, `application/pdf`, `video/mp4`, `video/webm`, `video/quicktime`, `video/ogg`, `video/x-matroska`, `audio/mpeg`, `audio/mp4`, `audio/ogg`, `audio/wav`, `audio/webm`, `audio/flac`. `""` when the name has no extension or an unknown one.

**Example**

```C
treedb_content_type_of_name("nave-1.JPG");     /* "image/jpeg" */
treedb_content_type_of_name("voice.m4a");      /* "audio/mp4" */
treedb_content_type_of_name("notes.txt");      /* "" */
```

---

(treedb_create_node)=
## [`treedb_create_node()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L5689)

Creates a new node in the TreeDB. The node is stored in [`tranger`](<#treedb_create_node>) under the specified [`treedb_name`](<#treedb_create_node>) and [`topic_name`](<#treedb_create_node>).

```C
json_t *treedb_create_node(
    json_t       *tranger,
    const char   *treedb_name,
    const char   *topic_name,
    json_t       *kw // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the [`tranger`](<#treedb_create_node>) database instance. |
| `treedb_name` | `const char *` | Name of the TreeDB where the node will be created. |
| `topic_name` | `const char *` | Name of the topic under which the node will be stored. |
| `kw` | `json_t *` | JSON object containing the attributes of the new node. This parameter is owned by the function. |

**Returns**

Returns a JSON object representing the newly created node. WARNING: The returned object is NOT owned by the caller and must not be modified or freed.

**Notes**

This function creates a 'pure node' without loading hook links. The primary key (`pkey`) of all topics must be 'id', and it must be a string.

`id` is mandatory, but the topic can hand it out. When the `kw` carries no
`id`, the flag on the `id` column decides what the new one is:

| Flag on `id` | The id the topic hands out |
|---|---|
| `uuid` | A random UUID. |
| `rowid` | One past every id the topic ever handed out. An id is never reused, not even after its node is deleted: a snap's id rides the records it tagged. The counter lives in the topic's `topic_var.json` (`last_rowid_id`) and survives a `topic_version` change, which re-creates that file from the schema. It is raised past every numeric key of the TOPIC (tranger2's cache, every key on disk), not past the treedb's index: with a snap active that index holds only what the snap loaded, and a node created after the shot got its id handed out again (fixed after 7.24.1). |
| `qualified` | The id of the parent, a dot, and the name of the record. The name is the first secondary key of the topic (`pkey2s`). The parent is the one named in the fkey of the `kw`. |

A column carries at most one of the three. With none of them, a `kw` with no
`id` is an error. A `qualified` id is an error too when the `kw` carries no
secondary key, when it carries no parent fkey, or when the composed id is
longer than a record key: a key too long is refused, never trimmed, because a
truncated id is the address of another node. In all of these the function logs
the cause and returns `NULL`.

See the [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md)
§3.3 for the flags and §3.11 for why the schema topics are keyed this way.

---

(treedb_create_topic)=
## [`treedb_create_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L1564)

`treedb_create_topic()` creates a new topic in the TreeDB with the specified schema and primary key constraints.

```C
json_t *treedb_create_topic(
    json_t       *tranger,
    const char   *treedb_name,
    const char   *topic_name,
    int          topic_version,
    const char   *topic_tkey,
    json_t       *pkey2s,      // owned, string or dict of string | [strings]
    json_t       *jn_cols,     // owned
    uint32_t     snap_tag,
    BOOL         system_topic, // TRUE: topic cannot be deleted (persisted in topic_var)
    BOOL         create_schema
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger instance managing the TreeDB. |
| `treedb_name` | `const char *` | Name of the TreeDB where the topic will be created. |
| `topic_name` | `const char *` | Name of the topic to be created. |
| `topic_version` | `int` | Version number of the topic schema. |
| `topic_tkey` | `const char *` | Topic key used for indexing. |
| `pkey2s` | `json_t *` | Primary key(s) for the topic, either a string or a dictionary of strings. |
| `jn_cols` | `json_t *` | JSON object defining the schema of the topic, including field types and attributes. |
| `snap_tag` | `uint32_t` | Snapshot tag associated with the topic creation. |
| `system_topic` | `BOOL` | If `TRUE`, the topic is marked non-deletable: [`treedb_delete_topic()`](<#treedb_delete_topic>) refuses it and `force` does NOT override. The flag is persisted in `topic_var.json` (metadata, not a data column), so it survives reload and needs no `topic_version` bump. |
| `create_schema` | `BOOL` | Flag indicating whether to create the schema if it does not exist. |

**Returns**

Returns a JSON object representing the created topic. WARNING: The returned object is not owned by the caller.

**Notes**

The primary key (`pkey`) of all topics must be `id`.
This function does not load hook links.
The returned JSON object must not be modified or freed by the caller.

The `__system__` structural topics and the per-treedb `__snaps__` / `__graphs__`
topics are created with `system_topic = TRUE`. See
[`treedb_set_node_immutable()`](<#treedb_set_node_immutable>) for marking
individual records (rather than whole topics) non-deletable.

There is no parameter for the main topic. That mark comes only from the
schema that [`treedb_open_db()`](<#treedb_open_db>) reads.

A topic with a column flagged both `hook` and `fkey` is refused, and the
function returns `NULL`. See [`parse_schema()`](<#parse_schema>).

So is a topic whose columns would not open: no columns, no `id` column, or a
column that fails the validation [`parse_schema()`](<#parse_schema>) applies
when a schema is read. The function logs *"Topic refused: bad columns"* with
the reason, sets the last message, and creates nothing on disk. Until 7.24.1
the topic was written first and validated after, so a caller was told
*"Topic created!"* for a topic the next open could not load.

```C
/*  refused: no `id` column  */
treedb_create_topic(tranger, "my_db", "things", 1, "", 0,
    json_pack("{s:{s:s, s:s, s:[s]}}",    /* cols, keyed by column name */
        "name", "header", "Name", "type", "string", "flag", "persistent"),
    0, FALSE, FALSE);   /* -> NULL, and no topic on disk */
```

---

(treedb_delete_instance)=
## [`treedb_delete_instance()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L7169)

`treedb_delete_instance()` durably deletes ONE instance of a node — one value of a secondary key (`pkey2`). Its slot in that `pkey2` index goes, and every md2 row of that `(id, pkey2 value)` is tombstoned on disk, so a reopen does not bring it back. The primary `id` index is not touched: route only a NON-primary instance here. [`treedb_delete_node()`](<#treedb_delete_node>) deletes a whole key.

```C
int treedb_delete_instance(
    json_t      *tranger,
    json_t      *node,       // pure node borrowed from the index; consumed only on success
    const char  *pkey2_name,
    json_t      *jn_options  // owned, bool "ignore_snaps"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `node` | `json_t *` | The instance as the `pkey2` index holds it. Borrowed: the index's reference goes on success only. |
| `pkey2_name` | `const char *` | Name of the secondary key that identifies the instance. |
| `jn_options` | `json_t *` | Owned. `ignore_snaps` skips the snapshot guard (not the immutable one). `force` does nothing here: it is about links, and this does not look at them. |

**Returns**

Returns `0` on success, or a negative value if the deletion is refused or fails.

**Notes**

It does NOT look at links: an instance is one version of a node, and the links belong to the node.

It refuses, before it tombstones or drops anything, when it cannot read every
row of the key: a row whose metadata cannot be read (*"Cannot delete instance,
cannot read every row of its key"*), or whose content cannot be read (*"Cannot
delete instance, a row of its key cannot be read"*: a row that cannot be read
cannot say whose instance it is). Until 7.25.4 it tombstoned what it had read,
dropped the slot, answered `0`, and the instance came back at the next open.

```C
// Delete the release "1.2.0" of yuno "gate1" (topic `yunos`, pkey2 `yuno_release`)
json_t *inst = treedb_get_instance(tranger, "treedb_yuneta_agent", "yunos",
    "yuno_release", "gate1", "1.2.0");
if(inst && treedb_delete_instance(tranger, inst, "yuno_release", 0) < 0) {
    // refused: immutable, a snapshot holds it, or a row of the key cannot be read (logged)
}
```

A record marked immutable (`__md_treedb__`immutable`, see
[`treedb_set_node_immutable()`](<#treedb_set_node_immutable>)) is refused and
`force` does NOT override it.

The function tombstones every md2 row of this `(id, pkey2 value)`, so it
refuses an instance that a snapshot holds a record of: *"cannot delete
instance, a snapshot still holds it"*. It does not read the tag the node
carries in memory — a save is untagged, so an instance updated after a shot
carries 0 while the record the snap froze is still under it. It reads the
records of the key instead and keeps the ones of this instance (which
instance a record belongs to is a FIELD, so that walk reads the content of
the record). `ignore_snaps` overrides this guard, and `force` does not
(since after 7.24.1; it used to). It is the twin of the one
[`treedb_delete_node()`](<#treedb_delete_node>) has for a whole key.

---

(treedb_delete_node)=
## [`treedb_delete_node()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L6672)

The `treedb_delete_node()` function deletes a node from the tree database. If the node has existing links, the deletion will fail unless the 'force' option is enabled.

```C
int treedb_delete_node(
    json_t *tranger,
    json_t *node,       // NOT owned: borrowed from the index, whose reference goes on success
    json_t *jn_options  // bool "force"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `node` | `json_t *` | The node to be deleted: the pure node as the index holds it. It is **borrowed**, never the caller's own reference: do not decref it, before or after. On success the index's reference is released with the key; on a refusal the node is left as it was, still indexed. |
| `jn_options` | `json_t *` | A JSON object containing options for deletion. The 'force' boolean option determines whether to forcibly delete linked nodes. |

**Returns**

Returns 0 on success, or a negative error code if the deletion fails.

**Notes**

If the node has existing links and 'force' is not enabled, [`treedb_delete_node()`](<#treedb_delete_node>) will fail.

A node that a snapshot holds a record of is refused (*"cannot delete node, a
snapshot still holds it"*) unless `ignore_snaps` is given. **`force` does not
override that** (since 7.25.0): `force` means "unlink the
children" and nothing else. It used to mean both, and the two callers that
force every delete to get the first -- the agent's `delete-yuno` and the
gobj-ui topic table -- switched the snapshot guard off with it.

```C
/*  a node with children, that no snap holds  */
treedb_delete_node(tranger, node, json_pack("{s:b}", "force", 1));

/*  a node a snap froze: deleting it breaks that snap's rollback  */
treedb_delete_node(tranger, node, json_pack("{s:b, s:b}",
    "force", 1, "ignore_snaps", 1));
```

A record marked immutable (`__md_treedb__`immutable`, see
[`treedb_set_node_immutable()`](<#treedb_set_node_immutable>)) is refused and
`force` does NOT override it.

---

(treedb_delete_topic)=
## [`treedb_delete_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L1993)

Deletes a topic from the TreeDB identified by `treedb_name`. The topic and all its associated data will be permanently removed.

```C
int treedb_delete_topic(
    json_t  *tranger,
    const char  *treedb_name,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `treedb_name` | `const char *` | Name of the TreeDB from which the topic will be deleted. |
| `topic_name` | `const char *` | Name of the topic to be deleted. |

**Returns**

Returns `0` on success, or a negative error code if the operation fails.

**Notes**

Make sure that the topic does not contain critical data before calling [`treedb_delete_topic()`](<#treedb_delete_topic>).

A topic created with `system_topic = TRUE` (see [`treedb_create_topic()`](<#treedb_create_topic>))
is refused. There is no `force` override.

---

(treedb_file_ext)=
## [`treedb_file_ext()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11446)

The extension a blob is stored with, derived from its mime type: it is what a web server reads to set the `Content-Type`, so it comes from the type stored and never from the name given.

```C
const char *treedb_file_ext(
    const char  *content_type
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `content_type` | `const char *` | A mime type. |

**Returns**

A static string without the dot: `jpg`, `png`, `webp`, `gif`, `pdf`, `mp4`, `webm`, `mov`, `ogv`, `mkv`, `mp3`, `m4a`, `ogg`, `wav`, `weba`, `flac`; `bin` for an empty or unknown type.

**Example**

```C
treedb_file_ext("video/quicktime");    /* "mov" */
treedb_file_ext("text/plain");         /* "bin" */
```

---

(treedb_gc_files)=
## [`treedb_gc_files()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L13767)

The garbage collector of the bytes of `file` columns. It takes every asset of `__assets__` that **no live node and no snapshotted version of a node** links (row and bytes), and every blob of `.blobs/` that no row names (what an interrupted write leaves: the blob goes down before the index node). Never automatic: `treedb_delete_node()` with `force` UNLINKS the children instead of deleting them, so an unlinked asset is a normal intermediate state of a bulk operation. It reads the snapshots on disk, which makes the answer conservative. The command is C_NODE's `gc-assets`.

```C
json_t *treedb_gc_files(
    json_t      *tranger,
    const char  *treedb_name,
    BOOL        dry_run
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The tranger of the treedb. |
| `treedb_name` | `const char *` | The treedb. |
| `dry_run` | `BOOL` | `TRUE`: take nothing, answer what would be taken. |

**Returns**

The list of asset ids taken (or that would be taken), **yours** to decref. `NULL` with *"__assets__ index not found"* logged when the treedb has no `__assets__` topic.

`NULL`, no asset row taken, when what the snapshots hold cannot be read whole: a tagged record of an existing snap whose content cannot be read, a key of a topic with a `file` column whose records do not load (*"cannot read every instance of a topic: the assets a snapshot holds are unknown"*), or a `__snaps__` that did not load whole. The log says *"cannot read a tagged record: the assets a snapshot holds are unknown"* (or which topic) and *"gc refused: cannot tell which assets a snapshot links"*. Such a record may name any blob, so no asset is taken; until 7.25.4 the gc took the ones it could not see held. [`treedb_delete_node()`](<#treedb_delete_node>) of an `__assets__` node refuses in the same case: *"cannot delete asset, cannot tell whether a snapshot links it"*.

`NULL` too when a topic that links assets, or `__assets__`, in any treedb of the tranger, did not load whole (see [A topic that did not load whole](<#treedb-topic-not-loaded-whole>)): a node that did not load links its asset all the same, so the live links are unknown (*"gc refused: a topic that links assets did not load whole, the live links are unknown"*).

A refusal of the asset rows does not stop the sweep of the blobs NO row names: no link and no snapshot can lead to them. They are swept (or listed, `dry_run`) all the same and logged as info, *"gc: the asset rows were refused; blobs no row names were taken"* with their ids, since the answer is the refusal. The sweep refuses on its own when an `__assets__` did not load whole (*"gc: the blobs are not swept, __assets__ did not load whole"*): the bytes of a row that did not load would read as bytes no row names. What to do after a refusal: see *What the operator does* under [`treedb_open_db()`](<#treedb_open_db>).

**Example**

```C
json_t *would = treedb_gc_files(tranger, "treedb_yunovatioscedb", TRUE);
if(!would) {
    /* refused: no __assets__, or a snapshot that cannot be read (logged) */
}
/* ["3f1c...", "9a0b..."]: look at them before running it for real */
JSON_DECREF(would)
```

---

(treedb_get_id_index)=
## [`treedb_get_id_index()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L329)

`treedb_get_id_index()` retrieves the index of node IDs for a given topic in a TreeDB instance.

```C
json_t *treedb_get_id_index(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the `tranger` instance managing the TreeDB. |
| `treedb_name` | `const char *` | Name of the TreeDB instance. |
| `topic_name` | `const char *` | Name of the topic whose ID index is to be retrieved. |

**Returns**

A JSON object containing the ID index of the specified topic. WARNING: The returned object is NOT owned by the caller.

**Notes**

The returned JSON object must not be modified or freed by the caller.

---

(treedb_get_instance)=
## [`treedb_get_instance()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L9958)

`treedb_get_instance()` retrieves a specific node instance from a TreeDB topic using both primary and secondary keys.

```C
json_t *treedb_get_instance(
    json_t       *tranger,
    const char   *treedb_name,
    const char   *topic_name,
    const char   *pkey2_name, // required
    const char   *id,         // primary key
    const char   *key2        // secondary key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `treedb_name` | `const char *` | Name of the TreeDB database. |
| `topic_name` | `const char *` | Name of the topic within the TreeDB. |
| `pkey2_name` | `const char *` | Name of the secondary key field (required). |
| `id` | `const char *` | Primary key of the node instance. |
| `key2` | `const char *` | Secondary key of the node instance. |

**Returns**

Returns a pointer to a `json_t` object representing the requested node instance. The returned object is NOT owned by the caller and must not be modified or freed.

**Notes**

If the specified instance does not exist, `NULL` is returned. Use [`treedb_get_node()`](<#treedb_get_node>) if only the primary key is needed.

---

(treedb_get_node)=
## [`treedb_get_node()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L9915)

Retrieves a node from the TreeDB using its primary key. The function returns a reference to the node stored in the database, which must not be modified directly.

```C
json_t *treedb_get_node(
    json_t       *tranger,
    const char   *treedb_name,
    const char   *topic_name,
    const char   *id
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `treedb_name` | `const char *` | The name of the TreeDB from which to retrieve the node. |
| `topic_name` | `const char *` | The topic within the TreeDB that contains the node. |
| `id` | `const char *` | The primary key of the node to retrieve. |

**Returns**

A reference to the requested node as a `json_t *`. The returned node must not be modified directly.

**Notes**

The returned node is not owned by the caller and must not be modified or freed. Use [`treedb_update_node()`](<#treedb_update_node>) to modify the node safely.

---

(treedb_get_topic_hooks)=
## [`treedb_get_topic_hooks()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11302)

Retrieves a list of column names that are hooks in the specified topic of the `treedb_name` tree database.

```C
json_t *treedb_get_topic_hooks(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `treedb_name` | `const char *` | The name of the tree database containing the topic. |
| `topic_name` | `const char *` | The name of the topic whose hook columns are to be retrieved. |

**Returns**

A JSON array containing the names of the columns that are hooks in the specified topic. The returned JSON object is NOT owned by the caller.

**Notes**

Hooks define relationships between nodes in the tree database. Use [`treedb_get_topic_links()`](<#treedb_get_topic_links>) to retrieve foreign key links instead.

---

(treedb_get_topic_links)=
## [`treedb_get_topic_links()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11261)

`treedb_get_topic_links()` returns a list of column names that are foreign key links in the specified topic of a TreeDB.

```C
json_t *treedb_get_topic_links(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `treedb_name` | `const char *` | The name of the TreeDB to query. |
| `topic_name` | `const char *` | The name of the topic whose foreign key links are to be retrieved. |

**Returns**

A JSON array containing the names of the columns that are foreign key links in the specified topic. The returned JSON object is not owned by the caller.

**Notes**

The function provides insight into the schema of a topic by identifying its foreign key relationships. The returned list must not be modified or freed by the caller.

---

(treedb_import_files)=
## [`treedb_import_files()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L14039)

The second door of `file` columns: a directory already on the node becomes N assets in one call, with no bytes on the wire. It creates index nodes in `__assets__` and links nothing; it **answers the map `path -> id`**, so the loader can link what it imported (where a file came from is a fact of the load, not of the asset). The command is C_NODE's `import-assets`.

It is confined to `import_root`, and an empty root means **refused** -- a call that reads a path is a call that reads anything on the node. The confinement is resolved, not only spelled: a `source_dir` with `..`, or one that resolves out of `import_root` through a symlink, is refused.

```C
json_t *treedb_import_files(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *import_root,
    const char  *source_dir,
    BOOL        dry_run,
    const char  *uploaded_by
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The tranger of the treedb. |
| `treedb_name` | `const char *` | The treedb. |
| `import_root` | `const char *` | The only tree it may read. Empty: refused. |
| `source_dir` | `const char *` | Directory under `import_root` to import, walked recursively. |
| `dry_run` | `BOOL` | `TRUE`: store nothing, answer what would be imported. |
| `uploaded_by` | `const char *` | Kept in each asset node. |

**Returns**

`{"source_dir", "dry_run", "imported", "would_import", "skipped", "failed", "bytes", "files": {"<relative path>": "<id>"}}`, **yours**. `NULL` with the cause in `gobj_log_last_message()` when refused (no root, `..`, not a directory, out of the root).

**Example**

```C
json_t *result = treedb_import_files(
    tranger, "treedb_yunovatioscedb",
    "/yuneta/store/censo", "memorias/malaga",
    FALSE, "yuneta"
);
/* result.files: {"memorias/malaga/nave-1.jpg": "ba7816bf..."} */
JSON_DECREF(result)
```

---

(treedb_is_treedbs_topic)=
## [`treedb_is_treedbs_topic()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L314)

`treedb_is_treedbs_topic()` checks if a given topic belongs to the internal system topics of a TreeDB instance.

```C
BOOL treedb_is_treedbs_topic(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger instance managing the TreeDB. |
| `treedb_name` | `const char *` | Name of the TreeDB instance to check. |
| `topic_name` | `const char *` | Name of the topic to verify. |

**Returns**

Returns `TRUE` if the topic is an internal system topic of the TreeDB, otherwise returns `FALSE`.

**Notes**

System topics include `__snaps__` and `__graphs__`.

---

(treedb_link_nodes)=
## [`treedb_link_nodes()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L9537)

The `treedb_link_nodes()` function establishes a hierarchical relationship between a parent node and a child node using the specified hook.

```C
int treedb_link_nodes(
    json_t      *tranger,
    const char  *hook,
    json_t      *parent_node,    // NOT owned, pure node
    json_t      *child_node      // NOT owned, pure node
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `hook` | `const char *` | The name of the hook defining the relationship between the parent and child nodes. |
| `parent_node` | `json_t *` | The parent node to which the child node will be linked. This parameter is not owned by the function. |
| `child_node` | `json_t *` | The child node that will be linked to the parent node. This parameter is not owned by the function. |

**Returns**

Returns `0` on success, or a negative error code if the operation fails.

**Notes**

The function does not take ownership of `parent_node` or `child_node`. Make sure that both nodes exist and are valid before calling [`treedb_link_nodes()`](<#treedb_link_nodes>).

**A link writes the CHILD, and only when the child moved.** The persistent
half of a relationship is the child's `fkey`; the parent's `hook` is in
memory and is rebuilt at the next load. So a link that only fills a hook
writes nothing: that is the ordinary case of a second instance of a node,
which inherits the fkey of the instance before it (the ref names the
parent's **id**, shared by both instances) while the hook of the new parent
is empty. A link asked twice, with nothing to move on either side, writes
nothing and publishes nothing, and it warns (*"Parent ref already in child
fkey, skipping duplicate"*). The link EVENT follows either side: filling a
hook is a new relationship in memory even when nothing is written.

A link into a **single-valued** fkey (a `string` column) replaces the old one:
the child is first unlinked from the parent its string names, which emits
`EV_TREEDB_NODE_UNLINKED` for it. [`treedb_unlink_nodes()`](<#treedb_unlink_nodes>)
from a parent the child does not name is refused (*"Cannot unlink, the child
does not hang from that parent"*): nothing moves, no event, no save.

A link that would hang a node from its own descendant through the **same**
hook is refused (*"Cannot link, the link would close a cycle in the hook"*),
and nothing moves. A cycle through two different hooks is data, and is
accepted. For example, with the `departments.departments` hook, linking
`direction` as a child of `administration` fails when `administration` is
already a child of `direction`:

```C
treedb_link_nodes(tranger, "departments", direction, administration);   // 0
treedb_link_nodes(tranger, "departments", administration, direction);   // -1
```

---

(treedb_list_instances)=
## [`treedb_list_instances()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L10290)

`treedb_list_instances()` returns a list of instances from a specified topic in a tree database, optionally filtered by a given JSON filter and a custom match function.

```C
json_t *treedb_list_instances(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *topic_desc, // NOT owned
        json_t *node,       // NOT owned
        json_t *jn_filter   // NOT owned
    )
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `treedb_name` | `const char *` | The name of the tree database. |
| `topic_name` | `const char *` | The name of the topic from which instances are retrieved. |
| `pkey2_name` | `const char *` | The secondary key name used to identify instances. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria. This parameter is owned by the function. |
| `match_fn` | `BOOL (*)(json_t *, json_t *, json_t *)` | A function pointer to a custom match function that determines whether an instance matches the filter criteria. |

**Returns**

A JSON array containing the list of matching instances. The caller must decrement the reference count when done.

**Notes**

The returned list must be decrefed by the caller to avoid memory leaks. Filtering is applied using both `jn_filter` and `match_fn` if provided.

---

(treedb_list_nodes)=
## [`treedb_list_nodes()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L10149)

`treedb_list_nodes()` retrieves a list of nodes from a specified topic in a tree database, optionally filtering the results based on a provided filter and a custom matching function.

```C
json_t *treedb_list_nodes(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *topic_desc, // NOT owned
        json_t *node,       // NOT owned
        json_t *jn_filter   // NOT owned
    )
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `treedb_name` | `const char *` | The name of the tree database to query. |
| `topic_name` | `const char *` | The name of the topic from which to retrieve nodes. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria for selecting nodes. This parameter is owned by the function. |
| `match_fn` | `BOOL (*)(json_t *, json_t *, json_t *)` | A function pointer to a custom matching function that determines whether a node matches the filter criteria. |

**Returns**

Returns a JSON array containing the list of nodes that match the filter criteria. The caller must decrement the reference count of the returned JSON object when done.

**Notes**

If `match_fn` is provided, it is used to further refine the selection of nodes based on custom logic. The returned JSON object must be properly decremented to avoid memory leaks.

---

(treedb_list_parents)=
## [`treedb_list_parents()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L10765)

`treedb_list_parents()` returns a list of parent nodes linked to the given node through a specified foreign key (`fkey`). The function can return either full parent nodes or collapsed views based on the `collapsed_view` parameter.

```C
json_t *treedb_list_parents(
    json_t *tranger,
    const char *fkey,
    json_t *node,
    json_t *jn_options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `fkey` | `const char *` | The foreign key field used to identify parent nodes. |
| `node` | `json_t *` | The node whose parents are to be retrieved. This parameter is not owned by the function. |
| `collapsed_view` | `BOOL` | If `TRUE`, returns a collapsed view of the parent nodes. Otherwise, returns full parent nodes. |
| `jn_options` | `json_t *` | Options for filtering and formatting the returned parent nodes. This parameter is owned by the function. |

**Returns**

A JSON array containing the list of parent nodes. The caller must decrement the reference count of the returned JSON object.

**Notes**

The function retrieves parent nodes based on the specified `fkey`. If `collapsed_view` is `TRUE`, the function returns a simplified representation of the parent nodes. The `jn_options` parameter allows customization of the output format.

---

(treedb_list_snaps)=
## [`treedb_list_snaps()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L14660)

`treedb_list_snaps()` returns a list of snapshots associated with a given TreeDB.

```C
json_t *treedb_list_snaps(
    json_t       *tranger,
    const char   *treedb_name,
    json_t       *filter
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `treedb_name` | `const char *` | The name of the TreeDB whose snapshots are to be listed. |
| `filter` | `json_t *` | A JSON object containing filtering criteria for the snapshots. Owned by the caller. |

**Returns**

A JSON array containing the list of snapshots. The caller must decrement the reference count when done.

**Notes**

The returned JSON array must be properly decremented using `json_decref()` to avoid memory leaks.

---

(treedb_list_treedb)=
## [`treedb_list_treedb()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2066)

`treedb_list_treedb()` returns a list of available TreeDB names stored in the given `tranger` instance.

```C
json_t *treedb_list_treedb(
    json_t *tranger,
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the `tranger` instance containing the TreeDBs. |
| `kw` | `json_t *` | Optional filtering options (owned). |

**Returns**

A JSON array containing the names of available TreeDBs. The caller must not modify or free the returned value.

**Notes**

The returned list is managed internally and must not be altered or freed by the caller.

---

(treedb_node_children)=
## [`treedb_node_children()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11014)

`treedb_node_children()` returns a list of child nodes linked to a given node through a specified hook, optionally applying filters and recursive traversal.

```C
json_t *treedb_node_children(
    json_t       *tranger,
    const char   *hook,
    json_t       *node,       // NOT owned, pure node
    json_t       *jn_filter,  // filter to children tree
    json_t       *jn_options  // fkey,hook options, "recursive"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `hook` | `const char *` | The hook name used to retrieve child nodes. |
| `node` | `json_t *` | The parent node from which child nodes are retrieved. This parameter is not owned. |
| `jn_filter` | `json_t *` | Optional filter criteria to apply to the child nodes. This parameter is owned. |
| `jn_options` | `json_t *` | Options for controlling the retrieval, including fkey and hook options, and whether to perform recursive traversal. |

**Returns**

Returns a JSON array containing the child nodes that match the specified criteria. The caller must decrement the reference count when done.

**Notes**

If the `recursive` option is enabled in `jn_options`, [`treedb_node_children()`](<#treedb_node_children>) will traverse the hierarchy recursively.

---

(treedb_node_jtree)=
## [`treedb_node_jtree()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11186)

`treedb_node_jtree()` constructs a hierarchical tree representation of child nodes linked through a specified hook.

```C
json_t *treedb_node_jtree(
    json_t      *tranger,
    const char  *hook,
    const char  *rename_hook,
    json_t      *node,
    json_t      *jn_filter,
    json_t      *jn_options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `hook` | `const char *` | Hook name used to establish parent-child relationships. |
| `rename_hook` | `const char *` | Optional new name for the hook in the resulting tree. |
| `node` | `json_t *` | Pointer to the parent node from which the tree is built. Not owned. |
| `jn_filter` | `json_t *` | Filter criteria for selecting child nodes. Not owned. |
| `jn_options` | `json_t *` | Options for controlling the structure of the resulting tree, including fkey and hook options. |

**Returns**

A JSON object representing the hierarchical tree of child nodes. The caller must decrement the reference count when done.

**Notes**

The function recursively traverses child nodes using the specified `hook`. The `rename_hook` parameter allows renaming the hook in the output tree.

---

(treedb_open_db)=
## [`treedb_open_db()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L639)

`treedb_open_db()` initializes and opens a tree database within a `tranger` instance, using the specified schema and options.

```C
json_t *treedb_open_db(
    json_t      *tranger,
    const char  *treedb_name,
    json_t      *jn_schema,  // owned
    const char  *options     // "persistent"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the `tranger` instance managing the database. |
| `treedb_name` | `const char *` | The name of the tree database to open. |
| `jn_schema` | `json_t *` | A JSON object defining the schema of the tree database. This parameter is owned by the function. |
| `options` | `const char *` | `"persistent"`: load the schema from its file, which wins unless `jn_schema` has a strictly higher `schema_version`. `"persistent,impose"`: `jn_schema` also wins over a HIGHER version on disk (see below). |

**Returns**

A JSON dictionary representing the opened tree database inside `tranger`. The returned object must not be used directly by the caller.

**Notes**

Make sure that `tranger` is already initialized before calling [`treedb_open_db()`](<#treedb_open_db>).
The function follows a hierarchical structure where nodes are linked via parent-child relationships.
If the `persistent` option is enabled, the schema is loaded from a file, and modifications require a version update.

**The `impose` option.** With `"persistent,impose"`, and only on the master,
`jn_schema` wins over a newer schema on disk too. The rule is the same at both
levels, the treedb and each topic: a stored `schema_version` or
`topic_version` lower than the one passed takes the new one, an equal one is
kept, and a **higher** one is overwritten with the one passed (the log says
*"Imposing TreeDB schema from C over a newer one"* and *"Imposing
topic_version from C over a newer one"*). It exists to revert changes made to
the schema outside the code. `C_TREEDB` uses it when its `impose_c_schema` is
on, and, on the master, projects the schema into its `__system__` treedb when
that projection is missing or behind, so a schema in use can always be asked
for. The records are not touched.

**The main topic.** A schema topic can carry `'main_topic': true` (since
7.19.0). The mark names the topic that the tree of the treedb hangs from.
Viewers use it: the treedb graph of gobj-ui opens its tree from this topic.
Two rules apply:

- Only a topic with a hook to itself can carry the mark (for example, places
  inside places).
- Only one topic in each treedb can carry the mark.

If a mark breaks a rule, [`treedb_open_db()`](<#treedb_open_db>) logs an error
and ignores that mark. With two marks, the first one stays. The mark is not
kept in the topic files of the store: the function sets it in memory on each
open. [`tranger2_topic_desc()`](<#tranger2_topic_desc>) sends the mark to
clients, together with `system_topic`.

A non-master open reads the mark from the persisted schema file, like the rest
of the schema, so a reader that is not the master sees it too. A tool that
opens the topics with timeranger2 only (`tr2list`) does not see it: the mark is
not in `topic_desc.json` or `topic_var.json`.

The mark is a change to its topic, and it is published like one: raise the
`topic_version` of that topic and, when the runtime must use it, the
`schema_version` of the treedb. With the `persistent` option, the persisted
schema file wins unless `jn_schema` has a strictly higher `schema_version`.

Example — the agent's schema marks `realms`, which holds its sub-realms
through the fkey `parent_realm_id`
([`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.25.4/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c)):

```c
    'schema_version': '24',                                         \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'realms',                                         \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '8',                                   \n\
            'pkey2s': '',                                           \n\
            'main_topic': true,                                     \n\
            'cols': {                                               \n\
                ...
                'realms': {                                         \n\
                    'header': 'Realms',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'realms': 'parent_realm_id'                 \n\
                    }                                               \n\
                },                                                  \n\
                'parent_realm_id': {                                \n\
                    'header': 'Realm Parent',                       \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
```



(treedb-topic-not-loaded-whole)=
**A topic that did not load whole.** A topic is loaded with keyless
[`tranger2_open_list()`](<timeranger2.md#tranger2_open_list>)s (one for the
`id` index, one per `pkey2`). A key whose records cannot be read (an md2 file
cut or unreadable) is left out: every other key is loaded, the realtime feed
is opened, and the list names the keys it lacks. treedb logs them:

```text
ERROR tranger2_open_list: Cannot load the history of a key of the list, the list goes on without it
      topic_name=items key=k2
ERROR note_keys_not_loaded: treedb topic loaded WITHOUT the records of keys that cannot be read:
      their nodes are not in memory and a create of those ids is refused
      treedb_name=treedb_items topic_name=items keys=["k2"]
```

Those nodes are not in memory, and memory is what treedb answers from, so it
refuses what it would answer wrong, until the topic is opened again:

| What | With a topic that did not load whole |
|---|---|
| [`treedb_create_node()`](<#treedb_create_node>) of such an id | refused: *"Cannot create node, its id has records on disk that could not be loaded"*. Its record would become the newest of the key, over records nobody read. Other ids are created as usual. |
| `__snaps__` | also logs *"__snaps__ loaded without some snaps: the active snap is unknown, ..."*. The treedb is loaded from the live records (the active snap may be the one that did not load). [`treedb_shoot_snap()`](<#treedb_shoot_snap>) and [`treedb_activate_snap()`](<#treedb_activate_snap>) refuse; [`treedb_delete_node()`](<#treedb_delete_node>), `treedb_delete_instance()` and the delete of an asset refuse too, because which snap holds a record is unknown (*"cannot tell which snaps exist: __snaps__ did not load whole"*). `ignore_snaps` still overrides the node and instance deletes. |
| a topic that links assets (a `file` column), or `__assets__`, in ANY treedb of the tranger | [`treedb_gc_files()`](<#treedb_gc_files>) refuses the asset rows: *"gc refused: a topic that links assets did not load whole, the live links are unknown"*. |
| `__assets__`, in any treedb of the tranger | the sweep of the blobs no row names refuses too: *"gc: the blobs are not swept, __assets__ did not load whole"*. |

Until 7.25.4 such a topic loaded without the key and nothing said so; the
first fix after it (6d5760377) refused the whole list, so a topic came up
with the keys BEFORE the bad one only, its feed was not opened, a create
shadowed stored records, and an active snap was ignored.

**What the operator does** when a guard fails closed (this refusal, or the
snapshot guard of [`treedb_gc_files()`](<#treedb_gc_files>) that cannot read
a tagged record):

1. Read the log: the read error names the file (`path`, `rowid`), the lines
   above name the treedb, the topic and the keys.
2. Look at the key's directory, `<store>/<topic>/keys/<key>/`: a `.md2` whose
   size is not a multiple of 32 bytes or is shorter than what was written, a
   `.json` that was cut, a permission.
3. Repair it with the yuno STOPPED (the running yuno caches the store and
   writes it): put the key's directory back from a backup copy of the store.
   When the records of the key are lost for good and that is acceptable,
   remove the key instead, on the master, with the `delete-key` command of
   `C_TRANGER` (`force=1` when the key still holds rows):

   ```bash
   ycommand -c 'command-yuno id=<id> service=<tranger service> command=delete-key topic_name=items key=k2 force=1'
   ```
4. Open the treedb again (restart the yuno): the keys a topic could not load
   are forgotten only when the topic is closed, and the next load is whole.

---

(treedb_parent_refs)=
## [`treedb_parent_refs()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L10675)

Retrieves a list of parent references for a given node using a specified foreign key. The references are formatted according to the provided options.

```C
json_t *treedb_parent_refs(
    json_t       *tranger,
    const char   *fkey,
    json_t       *node,       // NOT owned, pure node
    json_t       *jn_options  // owned, fkey options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `fkey` | `const char *` | The foreign key field used to retrieve parent references. |
| `node` | `json_t *` | The node whose parent references are to be retrieved. This parameter is not owned by the function. |
| `jn_options` | `json_t *` | Options for formatting the returned references. This parameter is owned by the function. |

**Returns**

A JSON array containing the parent references. The caller must decrement the reference count when done using the returned value.

**Notes**

The function supports multiple formatting options for the returned references, including full references, only IDs, and list dictionaries.

---

(treedb_replace_links)=
## [`treedb_replace_links()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L9366)

`treedb_replace_links()` replaces the links of a node by the ones the fkey columns of `kw` name, and touches only what differs. It is what [`C_NODE`](#gclass-c-node) runs for an `update-node` with `autolink`.

```C
int treedb_replace_links(
    json_t  *tranger,
    json_t  *node,   // NOT owned, pure node
    json_t  *kw,     // owned
    BOOL    save
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `node` | `json_t *` | The child node whose links are replaced. Must be a pure node. Not owned. |
| `kw` | `json_t *` | The record. Its fkey columns name the parents the node must have. Owned. |
| `save` | `BOOL` | If `TRUE`, the node is saved when a link changed. |

**Returns**

Returns `0` when every column was replaced, or `-1` when at least one column was refused. Every refusal is logged, and the other columns are processed.

**Behavior**

For each fkey column of the node's topic, the refs of the node are compared with the refs of the same column in `kw`:

- A ref that `kw` does not name is unlinked (`EV_TREEDB_NODE_UNLINKED`).
- A ref that `kw` names and the node does not have is linked (`EV_TREEDB_NODE_LINKED`).
- A ref in both is not touched: no event, no write.

A column that `kw` does not carry is an **empty** column, and its links are removed (with the warning *"fkey empty"*). Send every fkey column, or do not use `autolink` for a partial update.

**A column is replaced whole, or not at all** (since 7.25.0). Every new ref of a column is checked BEFORE any old link of it is undone, and a column with one ref that cannot be linked keeps the links it has. A ref cannot be linked when:

- it is malformed: *"Wrong parent reference: …"*;
- its hook does not link the topic into the column where the ref arrived: *"fkey reference: its hook does not link into this column"*;
- its parent does not exist: *"fkey reference: parent node not found"*;
- it names the node itself: *"Cannot link self node"*;
- the link would close a cycle in the hook: *"Cannot link, the link would close a cycle in the hook"*.

Before, the old links were undone first and a refused new one failed after, so a node whose only parent was replaced by an impossible one was left with NO parent, on disk, `EV_TREEDB_NODE_UNLINKED` published. A refused column does not stop the other columns. The caller can still save the record, and repair the link later with [`treedb_link_nodes()`](<#treedb_link_nodes>).

**Example**

The node `users^alice` is linked to `departments^engineering`. This record keeps that link and adds `departments^research`:

```C
json_t *kw = json_pack("{s:s, s:s, s:[s, s, s]}",
    "id", "alice",
    "username", "alice_w",
    "departments",
        "departments^engineering^users",
        "departments^research^users"
);
if(treedb_replace_links(tranger, alice, kw, TRUE) < 0) {
    // Error already logged: the column kept the links it had
}
```

The result: one `EV_TREEDB_NODE_LINKED` (research), no event for engineering. Had the column also named `departments^ghost^users` (not found), NOTHING of it would move: no link to research, no event, one error for ghost, and alice keeps engineering.

---

(treedb_save_node)=
## [`treedb_save_node()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L6274)

The `treedb_save_node()` function directly saves a given node to the `tranger` database. The record is always written with tag 0 (`user_flag`), whether a snap is activated or not: only [`treedb_shoot_snap()`](<#treedb_shoot_snap>) tags a record, so a snap holds exactly what was live when it was shot.

```C
int treedb_save_node(
    json_t *tranger,
    json_t *node    // NOT owned, pure node.
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the `tranger` database instance where the node will be saved. |
| `node` | `json_t *` | A pointer to the node to be saved. This node is not owned by the function. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

The record is always written with tag 0, whether a snap is activated or not. A record gets a snap's tag only once, from [`treedb_shoot_snap()`](<#treedb_shoot_snap>). A save never gives a tag, so it never writes into a snap. With a snap activated, the primary index is loaded from the records that the snap tagged. An edit made during that time appears in the primary index only after the snap is deactivated.

---

(treedb_set_callback)=
## [`treedb_set_callback()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L1343)

Sets a callback function for `treedb_name` in `tranger`. The callback is triggered on node operations such as creation, update, or deletion.

```C
int treedb_set_callback(
    json_t *tranger,
    const char *treedb_name,
    treedb_callback_t treedb_callback,
    void *user_data,
    treedb_callback_flag_t flags
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the `tranger` instance managing the tree database. |
| `treedb_name` | `const char *` | Name of the tree database where the callback will be set. |
| `treedb_callback` | `treedb_callback_t` | Function pointer to the callback that will be invoked on node operations. |
| `user_data` | `void *` | User-defined data that will be passed to the callback function. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

The callback function must follow the `treedb_callback_t` signature and will receive parameters such as `tranger`, `treedb_name`, `topic_name`, `operation`, and `node`. The callback is triggered on events like `EV_TREEDB_NODE_CREATED`, `EV_TREEDB_NODE_UPDATED`, and `EV_TREEDB_NODE_DELETED`.

---

(treedb_set_files_limits)=
## [`treedb_set_files_limits()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11659)

The ceiling of a treedb's `file` columns: the largest file one write may cost this process, and the mime types the store will ever hold. A column may NARROW it with its `properties.max_size` / `properties.content_types`, never raise it. Without a call the ceiling is 128 MB and the sixteen types of [`treedb_content_type_of_name()`](#treedb_content_type_of_name). C_TREEDB forwards `files_max_size` / `files_content_types` of `open-treedb` here.

```C
int treedb_set_files_limits(
    json_t      *tranger,
    const char  *treedb_name,
    json_int_t  max_size,
    json_t      *content_types  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The tranger of the treedb. |
| `treedb_name` | `const char *` | The treedb (must be open). |
| `max_size` | `json_int_t` | Bytes. `0` keeps the current ceiling. |
| `content_types` | `json_t *` | Owned. A list of mime types; `NULL` keeps the current list. |

**Returns**

`0`, or `-1` with *"TreeDB not found"* or *"files content_types must be a list"* logged.

**Example**

```C
treedb_set_files_limits(tranger, "treedb_wattyzer",
    20*1024*1024,
    json_pack("[s,s,s]", "image/jpeg", "image/png", "application/pdf")
);
```

---

(treedb_set_node_immutable)=
## [`treedb_set_node_immutable()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L6395)

`treedb_set_node_immutable()` marks (or unmarks) a single node as immutable —
once set, the record cannot be deleted by [`treedb_delete_node()`](<#treedb_delete_node>)
or [`treedb_delete_instance()`](<#treedb_delete_instance>), and `force` does NOT
override it.

```C
int treedb_set_node_immutable(
    json_t *tranger,
    json_t *node,   // NOT owned, pure node.
    BOOL   set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the `tranger` database instance. |
| `node` | `json_t *` | The node to mark/unmark. NOT owned, must be a pure node. |
| `set` | `BOOL` | `TRUE` to mark the node immutable, `FALSE` to clear the mark. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

The mark rides the md2 `system_flag` bit (`sf_immutable_record`), NOT a data
column: it persists across reload and is surfaced as `__md_treedb__`immutable`.
No user-schema change and no `topic_version` bump are required.

The current primary record is rewritten in place (no new record is appended).
[`treedb_save_node()`](<#treedb_save_node>) re-applies the bit on every later
update, like the snap tag — so the mark is inherited across updates.

To protect a whole topic from deletion (rather than individual records), pass
`system_topic = TRUE` to [`treedb_create_topic()`](<#treedb_create_topic>).

---

(treedb_set_trace)=
## [`treedb_set_trace()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L5349)

Enables or disables trace logging for the TreeDB system.

```C
int treedb_set_trace(
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `set` | `BOOL` | If `TRUE`, enables trace logging. If `FALSE`, disables it. |

**Returns**

Returns `TRUE` if trace logging was successfully enabled or disabled, otherwise `FALSE`.

**Notes**

This function is useful for debugging and monitoring TreeDB operations.

---

(treedb_shoot_snap)=
## [`treedb_shoot_snap()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L14251)

Captures the current primary set of every user topic by stamping each primary record's `user_flag` field with the snap's id. The snap is registered as a row in `__snaps__` (assigned an integer `id` from its `g_rowid`). That same `id` is then written *in place* via `tranger2_write_user_flag()` on the live `.md2` record of each current primary. The snap is created with `active: false` — use [`treedb_activate_snap()`](<#treedb_activate_snap>) to switch to it.

```C
int treedb_shoot_snap(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *snap_name,
    const char  *description
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the `tranger` instance managing the TreeDB. |
| `treedb_name` | `const char *` | Name of the TreeDB instance to snapshot. |
| `snap_name` | `const char *` | Name assigned to the snapshot. Must be unique within the treedb — the call fails if one already exists. The literal `"__clear__"` is reserved and cannot be used. |
| `description` | `const char *` | Optional description providing details about the snapshot. |

**Returns**

Returns `0` on success, or a negative error code on failure (snap already exists, or the snap `id` will exceed the 16-bit `user_flag` ceiling — that is, `>= 0xFFFF` snaps in this treedb's history).

**Behavior**

For every user topic — and for `__graphs__`, the one meta-topic the photo carries — the function walks the primary index and, for each current primary node, calls `tranger2_write_user_flag(tranger, topic_name, key, t, i_rowid, snap_id)`. This modifies the underlying `.md2` record byte without appending a new instance — so the chronological `rowid` order is preserved, and `tranger2_read_user_flag()` on the same `(topic, key, t, rowid)` immediately returns the new tag.

Because the tag rides on the existing record, the snap captures *exactly* the primaries that were live at shoot-time — including records originally written with `user_flag = 0`. The tag stays on THAT record: a later save of the node appends an untagged record (see [`treedb_save_node()`](<#treedb_save_node>)), so the snap goes on holding what the node was when it was shot.

When the next shoot finds a primary record that *already* carries a tag from an earlier snap (that is, `__md_treedb__.tag != 0 && != snap_id`), the function appends a **clone** of that record via `tranger2_append_record()` with the new snap's id, rather than overwriting the prior tag in place. The cloned record sits at a higher `rowid` and carries only the new snap's tag. The original record keeps its earlier tag intact. This makes multiple snaps over an unchanged set of primaries co-exist: `activate-snap` of either snap can find its own tagged records on reload. Untagged primaries still take the cheaper in-place path — no clone cost when the record is snapped for the first time.

The clone is the newest record of its key, so a reload makes it the primary. The node in memory moves to the clone at once (`g_rowid`, `i_rowid`, `t`, `tm` and `tag` in `__md_treedb__`), and an immutable node keeps its immutable bit on the clone. The clone does not publish `EV_TREEDB_NODE_UPDATED`.

**The layout travels with the photo.** `__graphs__` holds how the treedb was arranged — one record per topic, written by the graph view — and a snap tags it like any other topic, so an activation reads back the arrangement of the shot and not the one in use. `treedb_open_db()` opens `__graphs__` filtered by the activated tag for exactly that. A snap shot before anything was arranged holds no layout, so activating it leaves `__graphs__` empty and the graph falls back to its automatic layout. The other two meta-topics stay out: `__snaps__` cannot tag itself, and `__assets__` is held another way — `assets_held_by_snaps()` walks the links of the records the snap froze, because its blobs are shared by every treedb of the tranger.

```C
/*  the arrangement of `devices`, as the graph view writes it  */
treedb_create_node(tranger, treedb_name, "__graphs__",
    json_pack("{s:s, s:s, s:b, s:{s:{s:{s:i, s:i}}}}",
        "id", "devices", "topic", "devices", "active", 1,
        "properties", "nodes", "dev-a", "x", 10, "y", 20));

treedb_shoot_snap(tranger, treedb_name, "arranged", "");
/*  ... the cards are moved and saved again ...  */
treedb_activate_snap(tranger, treedb_name, "arranged");  // reload: x is 10 again
```

**What a snap holds, and for how long.** Only `shoot-snap` tags records, and a save is always written with tag 0. So `activate-snap` returns every topic to what it was when the snap was shot: rows created after it are absent, and rows updated after it show their content at the shot. This is also true for rows written WHILE the snap is activated. Two earlier rules broke this. Up to 7.22.x a save inherited the node's tag, so the latest snap followed every later update (fixed in 7.23.0). In 7.23.x a save took the tag of the activated snap, so a binary installed during a rollback went into the photo (fixed in 7.24.0). Two guards follow the snap rather than the node's tag in memory:

**After `deactivate-snap`, `shoot-snap` waits for a reload.** The refusal of a
shot while a snap is active (7.25.0) also holds while the treedb is still
LOADED from one: `deactivate-snap` clears the tag on disk but the primary
index in memory is the filtered one until the treedb is opened again, and a
shot of that index would be a shot of the photo. C_NODE has no reload command;
the agent's `deactivate-snap` reloads (`restart_nodes()`), any other yuno is
restarted. Until then `shoot-snap` answers *"reload it first"*.


- [`treedb_delete_node()`](<#treedb_delete_node>) erases the whole key, so it refuses a node any existing snap holds a record of (*"cannot delete node, a snapshot still holds it"*), asking the key's records when the primary carries no tag; `ignore_snaps` overrides (`force` does not, since after 7.24.1).
- `treedb_gc_files()` holds an asset a node named when a snap was shot for as long as that snap's row exists, whether or not the node has moved on. Deleting the snap frees it.

```C
treedb_shoot_snap(tranger, "treedb_yuneta_agent", "pre-upgrade", "before 7.23");
// ... rows created, rows updated ...
treedb_activate_snap(tranger, "treedb_yuneta_agent", "pre-upgrade");   // reload: the state at the shot
```

**Notes**

Snapshots allow restoring the TreeDB to a previous state using [`treedb_activate_snap()`](<#treedb_activate_snap>). Like all snap operations, the visibility change is materialised on the next `treedb_open_db()`, not in memory at call time — see [`treedb_activate_snap()`](<#treedb_activate_snap>) for the reload semantics.

---

(treedb_sniff_content_type)=
## [`treedb_sniff_content_type()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L11507)

The mime type the BYTES say, from their first bytes (magic numbers). Containers shared by several types answer one representative (`video/mp4` for the ISO-BMFF family, `video/webm` for EBML, `audio/ogg` for Ogg); the declared type then picks the member. An SVG or any HTML/XML text is recognised on purpose, as `image/svg+xml` / `text/html`, so the allowlist can REFUSE it by name: declared as `image/png` by a client, it would otherwise walk past the check.

```C
const char *treedb_sniff_content_type(
    const char  *data,
    size_t      len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `data` | `const char *` | The first bytes of the file (a few dozen are enough). |
| `len` | `size_t` | How many. Under 4 answers `""`. |

**Returns**

A static string: a mime type, or `""` when the content is not recognised.

**Example**

```C
treedb_sniff_content_type("\x89PNG\r\n\x1a\n....", 12);   /* "image/png" */
treedb_sniff_content_type("  <svg xmlns=...", 16);          /* "image/svg+xml": refused */
```

---

(treedb_store_files)=
## [`treedb_store_files()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L12300)

The write path of a record with `file` columns: it consumes the `__files__` manifest (and the kw's `gbuffer`), stores the bytes under `.blobs/`, creates or refreshes the `__assets__` node and rewrites every `file` column into its full fkey reference, `__assets__^<id>^as_<topic>_<column>`. [`treedb_create_node()`](#treedb_create_node), [`treedb_update_node()`](#treedb_update_node) and [`treedb_autolink()`](#treedb_autolink) call it; a direct caller needs it only for a kw that never goes through them. Idempotent: a second pass finds full references and no manifest, and does nothing. Design: [`DESIGN-treedb-files.md`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/DESIGN-treedb-files.md).

```C
int treedb_store_files(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *topic_name,
    json_t      *kw     // NOT owned, modified in place
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The tranger of the treedb. |
| `treedb_name` | `const char *` | The treedb. |
| `topic_name` | `const char *` | The topic of the record. |
| `kw` | `json_t *` | The record as it arrived. Modified: each `file` column leaves holding its reference, and `__files__`, `gbuffer` and `__username__` are dropped. A column with a bare id and no manifest names an asset that must already exist. |

The manifest has two doors, one per transport. The second one slices the kw's single `gbuffer`, which carries the bytes of every column:

```json
"__files__": {"plano": {"content64": "...", "original_name": "plano.pdf", "content_type": "application/pdf"}}
"__files__": {"plano": {"offset": 0, "size": 51234, "original_name": "plano.pdf", "content_type": "application/pdf"}}
```

**Returns**

`0`, or `-1` with the cause in `gobj_log_last_message()` (a `file` column not flagged `fkey`, bytes that do not match the declared type, a type outside the allowlist, a file over the ceiling).

**Example**

```C
json_t *kw = json_pack("{s:s, s:{s:{s:s, s:s, s:s}}}",
    "id", "nave-1",
    "__files__",
        "foto", "content64", b64, "original_name", "nave-1.jpg", "content_type", "image/jpeg"
);
if(treedb_store_files(tranger, "treedb_yunovatioscedb", "places", kw) == 0) {
    /* kw.foto == "__assets__^<sha256>^as_places_foto" */
}
```

---

(treedb_topic_pkey2s)=
## [`treedb_topic_pkey2s()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L215)

`treedb_topic_pkey2s()` returns a list of primary key secondary values (`pkey2s`) for a given topic in the tree database.

```C
json_t *treedb_topic_pkey2s(
    json_t      *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `topic_name` | `const char *` | The name of the topic whose `pkey2s` values are to be retrieved. |

**Returns**

A JSON list containing the `pkey2s` values of the specified topic. The returned value is not owned by the caller.

**Notes**

The returned list must not be modified or freed by the caller.

---

(treedb_topic_pkey2s_filter)=
## [`treedb_topic_pkey2s_filter()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L230)

`treedb_topic_pkey2s_filter()` retrieves a filtered list of primary key secondary values (`pkey2s`) for a given topic in a TreeDB, based on the provided node and identifier.

```C
json_t *treedb_topic_pkey2s_filter(
    json_t      *tranger,
    const char  *topic_name,
    json_t      *node,      // NOT owned
    const char  *id
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger instance managing the TreeDB. |
| `topic_name` | `const char *` | The name of the topic from which to retrieve `pkey2s` values. |
| `node` | `json_t *` | A JSON object representing the node to filter against. This parameter is not owned by the function. |
| `id` | `const char *` | The primary key identifier used to filter the `pkey2s` values. |

**Returns**

Returns a JSON array containing the filtered `pkey2s` values. The returned object is not owned by the caller and must not be modified or freed.

**Notes**

This function is useful for retrieving secondary key values associated with a primary key in a structured TreeDB topic. The filtering is based on the provided `node` and `id` parameters.

---

(treedb_create_system_schema)=
## [`treedb_create_system_schema()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L522)

The treedb **meta-schema**: the schema that describes what a schema may say.

```C
json_t *treedb_create_system_schema(void);
```

**Returns**

The meta-schema, which the caller owns.

**Notes**

It is what validates a treedb schema before it is opened, so a schema that
declares an unknown column type or flag is refused where it is written instead
of failing later, once, on the record that happens to use it.

---

(treedb_topic_size)=
## [`treedb_topic_size()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2148)

`treedb_topic_size()` returns the number of nodes in the specified topic within the given TreeDB instance.

```C
size_t treedb_topic_size(
    json_t      *tranger,
    const char  *treedb_name,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the `tranger` instance managing the TreeDB. |
| `treedb_name` | `const char *` | Name of the TreeDB instance containing the topic. |
| `topic_name` | `const char *` | Name of the topic whose node count is to be retrieved. |

**Returns**

Returns the number of nodes in the specified topic.

**Notes**

If the topic does not exist, the function can return `0`.

---

(treedb_topics)=
## [`treedb_topics()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2099)

`treedb_topics()` retrieves a list of topic names from the specified TreeDB, optionally returning detailed information in dictionary format.

```C
json_t *treedb_topics(
    json_t       *tranger,
    const char   *treedb_name,
    json_t       *jn_options  // "dict" return list of dicts, otherwise return list of strings
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the `tranger` database instance. |
| `treedb_name` | `const char *` | The name of the TreeDB from which to retrieve topic names. |
| `jn_options` | `json_t *` | Options for the output format. If set to `"dict"`, returns a list of dictionaries. Otherwise, returns a list of strings. |

**Returns**

A JSON array containing the topic names or a list of dictionaries if `jn_options` is set to `"dict"`. The returned value is not owned by the caller.

**Notes**

The returned JSON object must not be modified or freed by the caller. Use [`treedb_list_treedb()`](<#treedb_list_treedb>) to retrieve available TreeDB names.

---

(treedb_unlink_nodes)=
## [`treedb_unlink_nodes()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L9614)

The `treedb_unlink_nodes()` function removes the hierarchical relationship between a parent and a child node in the tree database, identified by the specified hook.

```C
int treedb_unlink_nodes(
    json_t      *tranger,
    const char  *hook,
    json_t      *parent_node,    // NOT owned, pure node
    json_t      *child_node      // NOT owned, pure node
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the tranger database instance. |
| `hook` | `const char *` | The name of the hook defining the relationship to be removed. |
| `parent_node` | `json_t *` | A pointer to the parent node from which the child node will be unlinked. This node is not owned by the function. |
| `child_node` | `json_t *` | A pointer to the child node that will be unlinked from the parent node. This node is not owned by the function. |

**Returns**

Returns `0` on success, or a negative error code if the unlinking operation fails.
A child whose fkey does not name `parent_node` is not linked to it, so the
call is refused with *"Cannot unlink, the child does not hang from that
parent"*: the hook and the child are left as they are, no
`EV_TREEDB_NODE_UNLINKED` is published and the child is not saved. The check
is the same for the three shapes of an fkey: the string itself, one of the
strings of an array, or a key of a dict.

```C
// ch hangs from p2
treedb_unlink_nodes(tranger, "departments", p1, ch);   // -1, refused, ch untouched
treedb_unlink_nodes(tranger, "departments", p2, ch);   // 0, UNLINKED published, ch saved
```

**Notes**

The function does not take ownership of `parent_node` or `child_node`. This means the caller is responsible for managing their memory. Make sure that the specified `hook` exists before calling [`treedb_unlink_nodes()`](<#treedb_unlink_nodes>).

---

(treedb_update_node)=
## [`treedb_update_node()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L6446)

`treedb_update_node()` updates an existing node with the provided fields from `kw`, without modifying foreign keys (`fkeys`) or hook fields.

```C
json_t *treedb_update_node(
    json_t *tranger,
    json_t *node,   // NOT owned, pure node.
    json_t *kw,     // owned
    BOOL save
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `node` | `json_t *` | Pointer to the existing node to be updated. This parameter is not owned by the function. |
| `kw` | `json_t *` | JSON object containing the fields to update. This parameter is owned by the function. |
| `save` | `BOOL` | If `TRUE`, the updated node is saved to the database. |

**Returns**

Returns a pointer to the updated node. The returned node is not owned by the caller.
Returns `NULL` if the update is refused. A refused update does not change the node and does not save it.

A saved update (`save` TRUE) on a **replica** is refused before the node moves,
with *"Cannot update node, NO master"*: the append would be refused anyway,
and the node in memory used to take the update all the same, answered as if
written, until the next reload. A memory-only update (`save` FALSE) is a
replica's business and goes on. And a save that fails on a master answers
`NULL` too, where the node was answered whatever the save said.

```C
/*  tranger opened with "master": false  */
json_t *n = treedb_update_node(tranger, node, json_pack("{s:s}", "name", "x"), TRUE);
/*  n == NULL, one error logged, node unchanged  */
```

**Notes**

Foreign keys (`fkeys`) and hook fields are not updated by [`treedb_update_node()`](<#treedb_update_node>).
The returned node must not be modified or freed by the caller.

A pkey2 value names an **instance**, so an update cannot change it. A `kw` that
carries a pkey2 with a different value, the empty string included, is refused
(*"An update cannot change a pkey2 value, create the instance"*). A `kw` that
carries the same value is an ordinary update. To add an instance, create it:

```C
/*  topic `binaries`, pkey2s: 'version'  */
json_t *node = treedb_get_node(tranger, treedb_name, "binaries", "ycommand");

/*  Refused: this would move the node to another instance  */
treedb_update_node(tranger, node, json_pack("{s:s}", "version", "7.21.0"), TRUE);

/*  Right: a second instance of the same id  */
treedb_create_node(tranger, treedb_name, "binaries",
    json_pack("{s:s, s:s}", "id", "ycommand", "version", "7.21.0"));
```

Only the fields the `kw` carries are normalized — with ONE exception, a column
flagged `now`: the clock writes it, not the caller, so no `kw` ever carries
one. Every write stamps it, create and update, `writable` or not, persistent
or volatile (since after 7.24.1; 7.24.0 stamped it on an update only when it
was `writable`). The instant a record was BORN is a `time` column without
`now`: the create gives it the clock when the kw brings no value, and an
update leaves it alone. A `now` column is an integer epoch; of any other type
it is not stamped.

```C
/*  `now`: every write stamps it -- when this record was last written  */
"updated", "id","updated", "type","integer", "flag",["persistent","time","now"]

/*  `time` alone: the create stamps it -- when this record was born  */
"t",       "id","t",       "type","integer", "flag",["persistent","time"]
```

```C
/*  `time` moves to the clock although the kw says nothing about it  */
treedb_update_node(tranger, layout,
    json_pack("{s:o}", "properties", json_pack("{s:{s:i,s:i}}",
        "dev-a", "x", 30, "y", 40)),
    TRUE);
```

---

(get_hook_list)=
## [`get_hook_list()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L5359)

`get_hook_list()` converts hook data of various JSON types into a uniform JSON array of child node references. This normalizes the different internal representations of hook data (array, object, or dict) into a single list format for iteration.

```C
json_t *get_hook_list(
    hgobj gobj,
    json_t *hook_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance used for logging. |
| `hook_data` | `json_t *` | **Not owned.** The hook field data from a node. Can be a JSON array (returned as-is with incremented refcount), a JSON object (values are collected into a new array), or a JSON string (currently unsupported, logs an error). |

**Returns**

A new JSON array containing the child references from the hook data. The caller owns the returned array and must call `json_decref()` on it. Returns `NULL` if the hook data type is not supported.

**Notes**

When `hook_data` is a JSON array, the returned array is the same object with an incremented reference count. When it is a JSON object (dict-based hook), the object values are extracted into a new array.

---

(topic_desc_fkey_names)=
## [`topic_desc_fkey_names()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2766)

`topic_desc_fkey_names()` extracts the names of all foreign key (`fkey`) fields from a topic descriptor. It iterates over the columns in the topic descriptor and collects the `id` of each column whose `flag` contains the word `"fkey"`.

```C
json_t *topic_desc_fkey_names(
    json_t *topic_desc
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `topic_desc` | `json_t *` | **Owned.** A JSON array describing the topic columns (the topic descriptor). It is consumed (decremented) by this function. |

**Returns**

A new JSON array of strings, each being the `id` of a column flagged as `fkey`. The caller owns the returned array and must call `json_decref()` on it.

**Notes**

The `topic_desc` parameter is consumed by this function. Do not use it after calling `topic_desc_fkey_names()`. See also [`topic_desc_hook_names()`](#topic_desc_hook_names) for the equivalent function for hook fields.

---

(topic_desc_hook_names)=
## [`topic_desc_hook_names()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_treedb.c#L2741)

`topic_desc_hook_names()` extracts the names of all hook fields from a topic descriptor. It iterates over the columns in the topic descriptor and collects the `id` of each column whose `flag` contains the word `"hook"`.

```C
json_t *topic_desc_hook_names(
    json_t *topic_desc
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `topic_desc` | `json_t *` | **Owned.** A JSON array describing the topic columns (the topic descriptor). It is consumed (decremented) by this function. |

**Returns**

A new JSON array of strings, each being the `id` of a column flagged as `hook`. The caller owns the returned array and must call `json_decref()` on it.

**Notes**

The `topic_desc` parameter is consumed by this function. Do not use it after calling `topic_desc_hook_names()`. See also [`topic_desc_fkey_names()`](#topic_desc_fkey_names) for the equivalent function for fkey fields.

---

