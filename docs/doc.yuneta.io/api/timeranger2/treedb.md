# tr_treedb

Graph memory database with hook/fkey relationships, persisted through timeranger2. See the TreeDB section in the top-level `CLAUDE.md` for link/unlink rules and `g_rowid` semantics.

Source code:

- [`tr_treedb.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_treedb.h)
- [`tr_treedb.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_treedb.c)

(_treedb_create_topic_cols_desc)=
## `_treedb_create_topic_cols_desc()`

The `_treedb_create_topic_cols_desc()` function creates and returns a JSON object describing the column schema for a TreeDB topic.

```C
json_t *_treedb_create_topic_cols_desc(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON object containing the column schema description. The returned object is not owned by the caller.

**Notes**

The returned JSON object must not be modified or freed by the caller.

---

(add_jtree_path)=
## `add_jtree_path()`

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

The function does not take ownership of the `parent` or `child` nodes, meaning the caller is responsible for managing their memory.

---

(create_template_record)=
## `create_template_record()`

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
## `current_snap_tag()`

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
## `decode_child_ref()`

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
## `decode_parent_ref()`

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
## `node_collapsed_view()`

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
## `parse_hooks()`

`parse_hooks()` processes the schema to extract and validate hook definitions.

```C
int parse_hooks(
    json_t *schema  // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `schema` | `json_t *` | A JSON object representing the schema; it is not owned by the function. |

**Returns**

Returns `0` on success or a negative number if an error occurs during parsing.

**Notes**

This function ensures that hooks in the schema are correctly defined and structured.

---

(parse_schema)=
## `parse_schema()`

`parse_schema()` validates and processes a JSON schema definition, ensuring its structure and integrity.

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

---

(parse_schema_cols)=
## `parse_schema_cols()`

`parse_schema_cols()` validates and processes the column definitions in a schema, ensuring correctness and consistency.

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

The function ensures that the column definitions conform to the expected schema format. If errors are found, the return value indicates the number of issues detected.

---

(set_volatil_values)=
## `set_volatil_values()`

The `set_volatil_values()` function assigns volatile values to a record in the TreeDB, ensuring that non-persistent fields are set using default values if not provided.

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
## `treedb_activate_snap()`

Activates a snapshot in the TreeDB, returning the corresponding snapshot tag. The function updates the current state of the database to match the specified snapshot.

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
| `snap_name` | `const char *` | The name of the snapshot to activate. |

**Returns**

Returns the snapshot tag as an integer if successful, or a negative value if an error occurs.

**Notes**

Ensure that the snapshot exists before calling [`treedb_activate_snap()`](<#treedb_activate_snap>).

---

(treedb_autolink)=
## `treedb_autolink()`

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

---

(treedb_clean_node)=
## `treedb_clean_node()`

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

This function only removes foreign key links; it does not delete the node itself. If `save` is `TRUE`, the changes are persisted in the database.

---

(treedb_close_db)=
## `treedb_close_db()`

Closes the TreeDB instance identified by `treedb_name` in the given `json_t *` `tranger`. This function ensures that all resources associated with the TreeDB instance are properly released.

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

Ensure that [`treedb_open_db()`](<#treedb_open_db>) was previously called before attempting to close the TreeDB instance.

---

(treedb_close_topic)=
## `treedb_close_topic()`

Closes the specified topic in the TreeDB system, ensuring that all associated resources are properly released.

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

Ensure that the topic is not in use before calling [`treedb_close_topic()`](<#treedb_close_topic>).

---

(treedb_create_node)=
## `treedb_create_node()`

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

---

(treedb_create_topic)=
## `treedb_create_topic()`

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
| `create_schema` | `BOOL` | Flag indicating whether to create the schema if it does not exist. |

**Returns**

Returns a JSON object representing the created topic. WARNING: The returned object is not owned by the caller.

**Notes**

The primary key (`pkey`) of all topics must be `id`.
This function does not load hook links.
The returned JSON object should not be modified or freed by the caller.

---

(treedb_delete_instance)=
## `treedb_delete_instance()`

`treedb_delete_instance()` deletes a specific instance of a node in the TreeDB, identified by its primary and secondary keys. If links exist and the `force` option is not set, the deletion will fail.

```C
int treedb_delete_instance(
    json_t      *tranger,
    json_t      *node,       // owned, pure node
    const char  *pkey2_name,
    json_t      *jn_options  // bool "force"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the tranger database instance. |
| `node` | `json_t *` | Owned JSON object representing the node to be deleted. |
| `pkey2_name` | `const char *` | Name of the secondary key used to identify the instance. |
| `jn_options` | `json_t *` | JSON object containing options, including the `force` flag to override link constraints. |

**Returns**

Returns `0` on success, or a negative error code if the deletion fails.

**Notes**

If links exist and `force` is not set in `jn_options`, [`treedb_delete_instance()`](<#treedb_delete_instance>) will fail.

---

(treedb_delete_node)=
## `treedb_delete_node()`

The `treedb_delete_node()` function deletes a node from the tree database. If the node has existing links, the deletion will fail unless the 'force' option is enabled.

```C
int treedb_delete_node(
    json_t *tranger,
    json_t *node,       // owned, pure node
    json_t *jn_options  // bool "force"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the tranger database instance. |
| `node` | `json_t *` | The node to be deleted. This parameter is owned and must be a pure node. |
| `jn_options` | `json_t *` | A JSON object containing options for deletion. The 'force' boolean option determines whether to forcibly delete linked nodes. |

**Returns**

Returns 0 on success, or a negative error code if the deletion fails.

**Notes**

If the node has existing links and 'force' is not enabled, [`treedb_delete_node()`](<#treedb_delete_node>) will fail.

---

(treedb_delete_topic)=
## `treedb_delete_topic()`

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

Ensure that the topic does not contain critical data before calling [`treedb_delete_topic()`](<#treedb_delete_topic>).

---

(treedb_get_id_index)=
## `treedb_get_id_index()`

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

The returned JSON object should not be modified or freed by the caller.

---

(treedb_get_instance)=
## `treedb_get_instance()`

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
## `treedb_get_node()`

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

The returned node is not owned by the caller and should not be modified or freed. Use [`treedb_update_node()`](<#treedb_update_node>) to modify the node safely.

---

(treedb_get_topic_hooks)=
## `treedb_get_topic_hooks()`

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
## `treedb_get_topic_links()`

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

The function provides insight into the schema of a topic by identifying its foreign key relationships. The returned list should not be modified or freed by the caller.

---

(treedb_is_treedbs_topic)=
## `treedb_is_treedbs_topic()`

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
## `treedb_link_nodes()`

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

The function does not take ownership of `parent_node` or `child_node`. Ensure that both nodes exist and are valid before calling [`treedb_link_nodes()`](<#treedb_link_nodes>).

---

(treedb_list_instances)=
## `treedb_list_instances()`

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

The returned list must be decref'd by the caller to avoid memory leaks. Filtering is applied using both `jn_filter` and `match_fn` if provided.

---

(treedb_list_nodes)=
## `treedb_list_nodes()`

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
## `treedb_list_parents()`

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
| `collapsed_view` | `BOOL` | If `TRUE`, returns a collapsed view of the parent nodes; otherwise, returns full parent nodes. |
| `jn_options` | `json_t *` | Options for filtering and formatting the returned parent nodes. This parameter is owned by the function. |

**Returns**

A JSON array containing the list of parent nodes. The caller must decrement the reference count of the returned JSON object.

**Notes**

The function retrieves parent nodes based on the specified `fkey`. If `collapsed_view` is `TRUE`, the function returns a simplified representation of the parent nodes. The `jn_options` parameter allows customization of the output format.

---

(treedb_list_snaps)=
## `treedb_list_snaps()`

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
## `treedb_list_treedb()`

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

The returned list is managed internally and should not be altered or freed by the caller.

---

(treedb_node_children)=
## `treedb_node_children()`

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
## `treedb_node_jtree()`

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
## `treedb_open_db()`

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
| `options` | `const char *` | A string specifying options for opening the database, such as `"persistent"` to load the schema from a file. |

**Returns**

A JSON dictionary representing the opened tree database inside `tranger`. The returned object must not be used directly by the caller.

**Notes**

Ensure that `tranger` is already initialized before calling [`treedb_open_db()`](<#treedb_open_db>).
The function follows a hierarchical structure where nodes are linked via parent-child relationships.
If the `persistent` option is enabled, the schema is loaded from a file, and modifications require a version update.

---

(treedb_parent_refs)=
## `treedb_parent_refs()`

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

(treedb_save_node)=
## `treedb_save_node()`

The `treedb_save_node()` function directly saves a given node to the `tranger` database. The `__tag__` (user_flag) attribute is inherited during the save operation.

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

The `__tag__` (user_flag) attribute of the node is inherited during the save operation.

---

(treedb_set_callback)=
## `treedb_set_callback()`

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

(treedb_set_trace)=
## `treedb_set_trace()`

Enables or disables trace logging for the TreeDB system.

```C
int treedb_set_trace(
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `set` | `BOOL` | If `TRUE`, enables trace logging; if `FALSE`, disables it. |

**Returns**

Returns `TRUE` if trace logging was successfully enabled or disabled, otherwise `FALSE`.

**Notes**

This function is useful for debugging and monitoring TreeDB operations.

---

(treedb_shoot_snap)=
## `treedb_shoot_snap()`

`treedb_shoot_snap()` tags the current state of a TreeDB instance, creating a snapshot with an associated name and description.

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
| `snap_name` | `const char *` | Name assigned to the snapshot. |
| `description` | `const char *` | Optional description providing details about the snapshot. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

Snapshots allow restoring the TreeDB to a previous state using [`treedb_activate_snap()`](<#treedb_activate_snap>).

---

(treedb_topic_pkey2s)=
## `treedb_topic_pkey2s()`

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

The returned list should not be modified or freed by the caller.

---

(treedb_topic_pkey2s_filter)=
## `treedb_topic_pkey2s_filter()`

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

(treedb_topic_size)=
## `treedb_topic_size()`

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

If the topic does not exist, the function may return `0`.

---

(treedb_topics)=
## `treedb_topics()`

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
| `jn_options` | `json_t *` | Options for the output format. If set to `"dict"`, returns a list of dictionaries; otherwise, returns a list of strings. |

**Returns**

A JSON array containing the topic names or a list of dictionaries if `jn_options` is set to `"dict"`. The returned value is not owned by the caller.

**Notes**

The returned JSON object should not be modified or freed by the caller. Use [`treedb_list_treedb()`](<#treedb_list_treedb>) to retrieve available TreeDB names.

---

(treedb_unlink_nodes)=
## `treedb_unlink_nodes()`

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

**Notes**

The function does not take ownership of `parent_node` or `child_node`, meaning the caller is responsible for managing their memory. Ensure that the specified `hook` exists before calling [`treedb_unlink_nodes()`](<#treedb_unlink_nodes>).

---

(treedb_update_node)=
## `treedb_update_node()`

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

**Notes**

Foreign keys (`fkeys`) and hook fields are not updated by [`treedb_update_node()`](<#treedb_update_node>).
The returned node must not be modified or freed by the caller.

---

(get_hook_list)=
## `get_hook_list()`

*Description pending — signature extracted from header.*

```C
json_t *get_hook_list(
    hgobj gobj,
    json_t *hook_data
);
```

---

(topic_desc_fkey_names)=
## `topic_desc_fkey_names()`

*Description pending — signature extracted from header.*

```C
json_t *topic_desc_fkey_names(
    json_t *topic_desc
);
```

---

(topic_desc_hook_names)=
## `topic_desc_hook_names()`

*Description pending — signature extracted from header.*

```C
json_t *topic_desc_hook_names(
    json_t *topic_desc
);
```

---

