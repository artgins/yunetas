# Node Functions

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

(gobj_activate_snap)=
## `gobj_activate_snap()`

Activates a previously saved snapshot identified by `tag` in the given `hgobj` instance. This operation typically involves stopping and restarting the associated `hgobj` to restore the saved state.

```C
int gobj_activate_snap(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance where the snapshot will be activated. |
| `tag` | `const char *` | The identifier of the snapshot to activate. |
| `kw` | `json_t *` | Additional options for activation, passed as a JSON object. Owned by the function. |
| `src` | `hgobj` | The source `hgobj` initiating the activation request. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., if `gobj` is NULL, destroyed, or if `mt_activate_snap` is not defined).

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and the function returns -1.', "If `mt_activate_snap` is not defined in the `hgobj`'s gclass, an error is logged and the function returns -1.", "The function ensures that the snapshot activation is properly handled by the `hgobj`'s gclass."]

---

(gobj_create_node)=
## `gobj_create_node()`

Creates a new node in the specified topic. The function [`gobj_create_node()`](#gobj_create_node) allows inserting a new record into a hierarchical data structure managed by the gobj system.

```C
json_t *gobj_create_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj instance managing the hierarchical data structure. |
| `topic_name` | `const char *` | The name of the topic where the node will be created. |
| `kw` | `json_t *` | A JSON object containing the attributes of the new node. This parameter is owned by the function. |
| `jn_options` | `json_t *` | A JSON object specifying options such as foreign key and hook configurations. This parameter is owned by the function. |
| `src` | `hgobj` | The source gobj that initiated the node creation request. |

**Returns**

Returns a JSON object representing the newly created node. If the operation fails, returns NULL.

**Notes**

The function [`gobj_create_node()`](#gobj_create_node) requires that the gobj instance supports the `mt_create_node` method. If the method is not defined, an error is logged, and the function returns NULL.

---

(gobj_delete_node)=
## `gobj_delete_node()`

Deletes a node from a tree database in the given `hgobj` instance. The node is identified by its topic name and key attributes.

```C
int gobj_delete_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance managing the tree database. |
| `topic_name` | `const char *` | The name of the topic from which the node will be deleted. |
| `kw` | `json_t *` | A JSON object containing the key attributes used to identify the node. This parameter is owned and will be decremented. |
| `jn_options` | `json_t *` | A JSON object containing additional options for deletion, such as 'force'. This parameter is owned and will be decremented. |
| `src` | `hgobj` | The source `hgobj` instance initiating the deletion request. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., if the `hgobj` is NULL, destroyed, or lacks the `mt_delete_node` method).

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and the function returns -1.', "If `mt_delete_node` is not defined in the `hgobj`'s gclass, an error is logged and the function returns -1.", 'The `kw` and `jn_options` parameters are owned and will be decremented within the function.']

---

(gobj_get_node)=
## `gobj_get_node()`

Retrieves a node from a tree database in the given `hgobj` instance. The node is identified by its topic name and a set of key-value filters.

```C
json_t *gobj_get_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance representing the tree database. |
| `topic_name` | `const char *` | The name of the topic in the tree database. |
| `kw` | `json_t *` | A JSON object containing key-value filters to identify the node. This parameter is owned and will be decremented. |
| `jn_options` | `json_t *` | A JSON object specifying additional options such as foreign key and hook options. This parameter is owned and will be decremented. |
| `src` | `hgobj` | The source `hgobj` instance requesting the node. |

**Returns**

Returns a JSON object representing the requested node. If the node is not found or an error occurs, `NULL` is returned.

**Notes**

This function requires the `mt_get_node` method to be implemented in the `hgobj`'s gclass. If the method is not defined, an error is logged.

---

(gobj_link_nodes)=
## `gobj_link_nodes()`

The `gobj_link_nodes()` function establishes a relationship between two nodes in a hierarchical data structure by linking a child node to a parent node using a specified hook.

```C
int gobj_link_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,
    const char *child_topic_name,
    json_t *child_record,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance managing the hierarchical data structure. |
| `hook` | `const char *` | The name of the hook defining the relationship between the parent and child nodes. |
| `parent_topic_name` | `const char *` | The topic name of the parent node. |
| `parent_record` | `json_t *` | A JSON object representing the parent node. This parameter is owned by the function. |
| `child_topic_name` | `const char *` | The topic name of the child node. |
| `child_record` | `json_t *` | A JSON object representing the child node. This parameter is owned by the function. |
| `src` | `hgobj` | The source GObj initiating the link operation. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., if `gobj` is NULL, destroyed, or if the `mt_link_nodes` method is not defined).

**Notes**

This function relies on the `mt_link_nodes` method of the GObj's class to perform the actual linking operation. If `mt_link_nodes` is not defined, an error is logged and the function returns -1.

---

(gobj_list_instances)=
## `gobj_list_instances()`

Retrieves a list of instances for a given topic in a tree database. The function allows filtering and additional options to refine the query.

```C
json_t *gobj_list_instances(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2_field,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj handler representing the tree database. |
| `topic_name` | `const char *` | The name of the topic whose instances are to be listed. |
| `pkey2_field` | `const char *` | The secondary key field used for filtering instances. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria for selecting instances. |
| `jn_options` | `json_t *` | A JSON object specifying additional options such as foreign key and hook options. |
| `src` | `hgobj` | The source GObj initiating the request. |

**Returns**

Returns a JSON object containing the list of instances matching the specified criteria. The caller must free the returned JSON object.

**Notes**

If the GObj is destroyed or the method is not implemented, the function logs an error and returns NULL.

---

(gobj_list_nodes)=
## `gobj_list_nodes()`

Retrieves a list of nodes from a specified topic in the given `hgobj`. The function allows filtering and additional options to refine the query.

```C
json_t *gobj_list_nodes(
    hgobj gobj,
    const char *topic_name,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which to retrieve the nodes. |
| `topic_name` | `const char *` | The name of the topic from which nodes will be listed. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria to apply to the node list. Owned by the caller. |
| `jn_options` | `json_t *` | A JSON object specifying additional options such as 'include-instances' and fkey/hook options. Owned by the caller. |
| `src` | `hgobj` | The source `hgobj` making the request. |

**Returns**

Returns a JSON object containing the list of nodes that match the specified criteria. The caller must free the returned JSON object.

**Notes**

If `gobj` is `NULL` or destroyed, an error is logged and `NULL` is returned. If the `mt_list_nodes` method is not defined in the gclass, an error is logged and `NULL` is returned.

---

(gobj_list_snaps)=
## `gobj_list_snaps()`

Retrieves a list of snapshots associated with the given `hgobj`.

```C
json_t *gobj_list_snaps(
    hgobj gobj,
    json_t *filter,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which to retrieve the snapshots. |
| `filter` | `json_t *` | A JSON object containing filter criteria for the snapshots. Owned by the caller. |
| `src` | `hgobj` | The source `hgobj` requesting the snapshot list. |

**Returns**

A JSON array containing the list of snapshots. The caller must decrement the reference count when done.

**Notes**

If `gobj` is `NULL` or destroyed, an error is logged and `NULL` is returned.

---

(gobj_node_children)=
## `gobj_node_children()`

Returns a list of child nodes for a given topic in a hierarchical tree structure. The function retrieves child nodes based on the specified hook and applies optional filters and options.

```C
json_t *gobj_node_children(
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,
    const char *hook,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_` | `hgobj` | The gobj instance representing the tree database. |
| `topic_name` | `const char *` | The name of the topic whose child nodes are to be retrieved. |
| `kw` | `json_t *` | A JSON object containing the 'id' and primary key fields used to locate the node. Owned by the caller. |
| `hook` | `const char *` | The hook name that defines the relationship between parent and child nodes. If NULL, all hooks are considered. |
| `jn_filter` | `json_t *` | A JSON object specifying filters to apply to the child nodes. Owned by the caller. |
| `jn_options` | `json_t *` | A JSON object containing options such as fkey, hook options, and recursion settings. Owned by the caller. |
| `src` | `hgobj` | The source gobj making the request. |

**Returns**

Returns a JSON object containing the list of child nodes. The caller must decrement the reference count of the returned JSON object.

**Notes**

If `gobj_` is NULL or destroyed, an error is logged, and NULL is returned. If the method `mt_node_children` is not defined in the gclass, an error is logged, and NULL is returned.

---

(gobj_node_parents)=
## `gobj_node_parents()`

`gobj_node_parents()` returns a list of parent references for a given node in a tree database, optionally filtered by a specific link.

```C
json_t *gobj_node_parents(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *link,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance managing the tree database. |
| `topic_name` | `const char *` | The name of the topic representing the node. |
| `kw` | `json_t *` | A JSON object containing the node's primary key fields. Owned by the caller. |
| `link` | `const char *` | The specific link to filter parent references. If NULL, all parent references are returned. |
| `jn_options` | `json_t *` | A JSON object specifying options for retrieving parent references. Owned by the caller. |
| `src` | `hgobj` | The source GObj requesting the operation. |

**Returns**

A JSON array containing the parent references. The caller must decrement the reference count when done.

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `gobj->gclass->gmt->mt_node_parents` is not defined, an error is logged and NULL is returned.', 'The returned JSON array contains references formatted according to the specified options.']

---

(gobj_node_tree)=
## `gobj_node_tree()`

Returns the full hierarchical tree of a node in a given topic. The tree is duplicated and can include metadata if specified in `jn_options`.

```C
json_t *gobj_node_tree(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance that manages the tree database. |
| `topic_name` | `const char *` | The name of the topic whose node tree is to be retrieved. |
| `kw` | `json_t *` | A JSON object containing the 'id' and primary key fields used to locate the root node. Owned by the function. |
| `jn_options` | `json_t *` | A JSON object specifying options such as 'with_metadata' to include metadata in the response. Owned by the function. |
| `src` | `hgobj` | The source GObj requesting the node tree. |

**Returns**

A JSON object representing the full hierarchical tree of the specified node. The caller must free the returned JSON object.

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `gobj` does not implement `mt_node_tree`, an error is logged and NULL is returned.', 'The returned JSON object must be freed by the caller using `json_decref()`.']

---

(gobj_shoot_snap)=
## `gobj_shoot_snap()`

The `gobj_shoot_snap()` function creates a snapshot of the current state of a GObj, identified by a given tag.

```C
int gobj_shoot_snap(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj whose state will be captured in the snapshot. |
| `tag` | `const char *` | A string identifier for the snapshot. |
| `kw` | `json_t *` | A JSON object containing additional options for the snapshot. The ownership is transferred. |
| `src` | `hgobj` | The source GObj initiating the snapshot. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

If the GObj does not support snapshots, an error is logged and the function returns -1.

---

(gobj_topic_desc)=
## `gobj_topic_desc()`

Retrieves the description of a topic in the given `hgobj`. The function returns a JSON object containing metadata about the specified topic.

```C
PUBLIC json_t *gobj_topic_desc(
    hgobj gobj,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance representing the object that manages the topic. |
| `topic_name` | `const char *` | The name of the topic whose description is to be retrieved. |

**Returns**

Returns a JSON object containing the topic description. If the `hgobj` is NULL or destroyed, or if the function is not implemented, it returns NULL.

**Notes**

This function checks if the `hgobj` is valid before proceeding. If the method `mt_topic_desc` is not defined in the `hgobj`'s gclass, an error is logged and NULL is returned.

---

(gobj_topic_hooks)=
## `gobj_topic_hooks()`

Retrieves the hooks of a topic in a TreeDB. The function `gobj_topic_hooks()` queries the specified topic within a TreeDB and returns its associated hooks.

```C
json_t *gobj_topic_hooks(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance that manages the TreeDB. |
| `treedb_name` | `const char *` | The name of the TreeDB containing the topic. |
| `topic_name` | `const char *` | The name of the topic whose hooks are to be retrieved. |
| `kw` | `json_t *` | A JSON object containing query parameters. Owned by the caller. |
| `src` | `hgobj` | The source GObj requesting the hooks. |

**Returns**

Returns a JSON object containing the hooks of the specified topic. If the function fails, it returns NULL.

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `gobj` does not implement `mt_topic_hooks`, an error is logged and NULL is returned.', 'The returned JSON object must be decremented by the caller when no longer needed.']

---

(gobj_topic_jtree)=
## `gobj_topic_jtree()`

`gobj_topic_jtree()` returns a hierarchical tree representation of a topic's self-linked structure, optionally filtering and renaming hooks.

```C
json_t *gobj_topic_jtree(
    hgobj gobj,
    const char *topic_name,
    const char *hook,
    const char *rename_hook,
    json_t *kw,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance managing the topic tree. |
| `topic_name` | `const char *` | The name of the topic whose tree structure is to be retrieved. |
| `hook` | `const char *` | The hook defining the hierarchical relationship between nodes. |
| `rename_hook` | `const char *` | An optional new name for the hook in the returned tree. |
| `kw` | `json_t *` | A JSON object containing the 'id' and pkey2s fields used to find the root node. |
| `jn_filter` | `json_t *` | A JSON object specifying filters to match records in the tree. |
| `jn_options` | `json_t *` | A JSON object containing options such as fkey and hook configurations. |
| `src` | `hgobj` | The source GObj requesting the topic tree. |

**Returns**

Returns a JSON object representing the hierarchical tree of the specified topic. The caller must free the returned JSON object.

**Notes**

["If 'webix' is set in `jn_options`, the function returns the tree in Webix format (dict-list).", "The `__path__` field in all records follows the 'id`id`...' format.", 'If no root node is specified, the first node with no parent is used.', 'If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `mt_topic_jtree` is not defined in the GClass, an error is logged and NULL is returned.']

---

(gobj_topic_links)=
## `gobj_topic_links()`

`gobj_topic_links()` retrieves the links of a specified topic within a TreeDB instance.

```C
json_t *gobj_topic_links(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance managing the TreeDB. |
| `treedb_name` | `const char *` | The name of the TreeDB instance. |
| `topic_name` | `const char *` | The name of the topic whose links are to be retrieved. |
| `kw` | `json_t *` | A JSON object containing additional query parameters. Owned by the caller. |
| `src` | `hgobj` | The source GObj requesting the operation. |

**Returns**

A JSON object containing the topic links, or `NULL` if an error occurs.

**Notes**

['If `gobj` is `NULL` or destroyed, an error is logged and `NULL` is returned.', 'If the method `mt_topic_links` is not defined in the GClass, an error is logged and `NULL` is returned.', 'The returned JSON object must be decremented (`json_decref()`) by the caller when no longer needed.']

---

(gobj_topic_size)=
## `gobj_topic_size()`

`gobj_topic_size()` returns the size of a specified topic in a GObj.

```C
size_t gobj_topic_size(
    hgobj gobj,
    const char *topic_name,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance. |
| `topic_name` | `const char *` | The name of the topic. |
| `key` | `const char *` | The key within the topic to retrieve the size. |

**Returns**

Returns the size of the specified topic.

**Notes**

['If `gobj` is `NULL` or destroyed, an error is logged and `0` is returned.', 'If `mt_topic_size` is not defined in the GClass, an error is logged and `0` is returned.']

---

(gobj_treedb_topics)=
## `gobj_treedb_topics()`

`gobj_treedb_topics()` retrieves a list of topics from a TreeDB instance, returning either a list of topic names or a list of topic descriptions based on the provided options.

```C
json_t *gobj_treedb_topics(
    hgobj gobj,
    const char *treedb_name,
    json_t *options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance managing the TreeDB. |
| `treedb_name` | `const char *` | The name of the TreeDB instance. |
| `options` | `json_t *` | A JSON object specifying retrieval options, such as returning a list of topic names or full topic descriptions. |
| `src` | `hgobj` | The source GObj requesting the topic list. |

**Returns**

A JSON array containing the list of topics. If `options` specifies 'dict', the array contains topic descriptions; otherwise, it contains topic names.

**Notes**

['If `gobj` is NULL or destroyed, an error is logged, and NULL is returned.', 'If `gobj` does not implement `mt_treedb_topics`, an error is logged, and NULL is returned.', 'The returned JSON object must be freed by the caller.']

---

(gobj_treedbs)=
## `gobj_treedbs()`

Retrieves a list of TreeDB names available in the given `hgobj`.

```C
json_t *gobj_treedbs(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which to retrieve the TreeDB names. |
| `kw` | `json_t *` | A JSON object containing additional options for filtering the TreeDB list. Owned by the function. |
| `src` | `hgobj` | The source `hgobj` requesting the TreeDB list. |

**Returns**

A JSON array containing the names of available TreeDBs. Returns `NULL` if an error occurs.

**Notes**

If the `gobj` does not implement `mt_treedbs`, an error is logged and `NULL` is returned.

---

(gobj_unlink_nodes)=
## `gobj_unlink_nodes()`

The `gobj_unlink_nodes()` function removes the relationship between a parent and child node in a hierarchical data structure managed by a `hgobj`.

```C
int gobj_unlink_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,
    const char *child_topic_name,
    json_t *child_record,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance managing the nodes. |
| `hook` | `const char *` | The name of the relationship (hook) to be removed. |
| `parent_topic_name` | `const char *` | The topic name of the parent node. |
| `parent_record` | `json_t *` | A JSON object containing the parent node's data. This parameter is owned and will be decremented. |
| `child_topic_name` | `const char *` | The topic name of the child node. |
| `child_record` | `json_t *` | A JSON object containing the child node's data. This parameter is owned and will be decremented. |
| `src` | `hgobj` | The source `hgobj` initiating the unlink operation. |

**Returns**

Returns `0` on success, or `-1` if an error occurs (e.g., if `gobj` is `NULL` or destroyed, or if `mt_unlink_nodes` is not defined).

**Notes**

['The function checks if `gobj` is valid before proceeding.', "If `mt_unlink_nodes` is not defined in the `hgobj`'s gclass, an error is logged and `-1` is returned.", 'Both `parent_record` and `child_record` are decremented after use.']

---

(gobj_update_node)=
## `gobj_update_node()`

Updates an existing node in the specified topic. If the node does not exist, it can be created based on the provided options. The function allows for automatic linking and volatile node creation.

```C
json_t *gobj_update_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance managing the topic. |
| `topic_name` | `const char *` | The name of the topic where the node resides. |
| `kw` | `json_t *` | A JSON object containing the node data to be updated. The 'id' and primary key fields are used to locate the node. |
| `jn_options` | `json_t *` | A JSON object specifying update options such as 'create', 'autolink', and 'volatil'. |
| `src` | `hgobj` | The source GObj initiating the update request. |

**Returns**

Returns a JSON object representing the updated node. If the update fails, NULL is returned.

**Notes**

If the GObj does not implement `mt_update_node`, an error is logged and NULL is returned. The function ensures that the GObj is not destroyed before proceeding.

---
