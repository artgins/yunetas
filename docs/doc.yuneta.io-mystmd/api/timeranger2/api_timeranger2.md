# Timeranger2 API

**Append-only time-series storage** for Yuneta. Records are grouped into
**topics**, keyed by a primary id, and indexed for fast access by time
range or row id.

## Higher-level stores built on top

- **`tr_msg2db`** — dict-style message store (one value per key).
- **`tr_queue`** — message queue with at-least-once semantics (used by
  the MQTT broker).
- **TreeDB** — graph memory database with hook/fkey relationships. See
  `kernel/c/root-linux/src/c_treedb.c` and the `CLAUDE.md` TreeDB section
  for the full rules.

## Core API (selected)

```c
tranger2_startup(...) / tranger2_shutdown(...)
tranger2_open_topic(...) / tranger2_close_topic(...)
tranger2_append_record(...)
tranger2_iterator_open(...) / tranger2_iterator_next(...) / _close(...)
```

## Persistence semantics

Every call to `tranger2_append_record()` writes one record to disk,
increments the per-key `g_rowid` (monotonic, never resets) and `i_rowid`
(row position in the `.md2` index file).

:::{important}
TreeDB relies on these semantics for its `link` / `unlink` behaviour:
**link/unlink saves only the child node** (the one carrying the fkey
field), never the parent. See `CLAUDE.md` for the full rules and
`g_rowid` tracing examples.
:::

## Filesystem watcher

`src/fs_watcher.c` implements an inotify-based watcher (`fs_event_t`)
used by `root-linux/C_FS` and by the stores themselves to react to
on-disk changes. See the **fs_watcher** section in the sidebar.

## CLI companions

| Tool | Purpose |
|---|---|
| `utils/c/tr2list` | List records in a topic |
| `utils/c/tr2keys` | List keys in a topic |
| `utils/c/tr2search` | Search by filter |
| `utils/c/tr2migrate` | Migrate from legacy timeranger v1 |
| `utils/c/list_queue_msgs2` | List a `tr_queue` queue |
| `utils/c/msg2db_list` | List a `tr_msg2db` topic |
| `utils/c/treedb_list` | List TreeDB nodes |

## Tests

`tests/c/timeranger2`, `tests/c/tr_msg`, `tests/c/tr_queue`,
`tests/c/tr_treedb`, `tests/c/tr_treedb_link_events`.

## Source code

- [`timeranger2.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/timeranger2.c)
- [`timeranger2.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/timeranger2.h)

## Function reference

The individual function reference pages are listed in the left-hand
sidebar under **Timeranger2 API**.

(tranger2_append_record)=
## `tranger2_append_record()`

Appends a new record to a topic in the TimeRanger database. The function assigns a timestamp if `__t__` is zero and returns the metadata of the newly created record.

```C
int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,
    uint16_t user_flag,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic where the record will be appended. |
| `__t__` | `uint64_t` | Timestamp of the record. If set to 0, the current time is used. |
| `user_flag` | `uint16_t` | User-defined flag associated with the record. |
| `md2_record_ex` | `md2_record_ex_t *` | Pointer to the metadata structure of the record. This field is required. |
| `jn_record` | `json_t *` | JSON object containing the record data. Ownership is transferred to the function. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

The function ensures that the record is appended to the specified topic in [`tranger2_startup()`](<#tranger2_startup>). If the topic does not exist, it must be created using [`tranger2_create_topic()`](<#tranger2_create_topic>) before calling this function.

---

(tranger2_backup_topic)=
## `tranger2_backup_topic()`

Creates a backup of a topic in the TimeRanger database. If `backup_path` is empty, the topic path is used. If `backup_name` is empty, the backup file is named `topic_name.bak`. If `overwrite_backup` is true and the backup exists, it is overwritten unless `tranger_backup_deleting_callback` returns true.

```C
json_t *tranger2_backup_topic(
    json_t *tranger,
    const char *topic_name,
    const char *backup_path,
    const char *backup_name,
    BOOL overwrite_backup,
    tranger_backup_deleting_callback_t tranger_backup_deleting_callback
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to back up. |
| `backup_path` | `const char *` | The directory where the backup will be stored. If empty, the topic path is used. |
| `backup_name` | `const char *` | The name of the backup file. If empty, `topic_name.bak` is used. |
| `overwrite_backup` | `BOOL` | If true, an existing backup is overwritten unless `tranger_backup_deleting_callback` prevents it. |
| `tranger_backup_deleting_callback` | `tranger_backup_deleting_callback_t` | A callback function that determines whether an existing backup should be deleted before overwriting. |

**Returns**

A JSON object representing the new topic backup.

**Notes**

If `overwrite_backup` is true and the backup exists, `tranger_backup_deleting_callback` is called. If it returns true, the existing backup is not removed.

---

(tranger2_close_all_lists)=
## `tranger2_close_all_lists()`

Closes all iterators, disk lists, or memory lists associated with a given `creator` in the specified `topic_name`. If `rt_id` is provided, only lists matching the `rt_id` are closed.

```C
int tranger2_close_all_lists(
    json_t *tranger,
    const char *topic_name,
    const char *rt_id,      // if empty, remove all lists of creator
    const char *creator     // if empty, remove all
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic whose lists should be closed. |
| `rt_id` | `const char *` | Realtime list identifier. If empty, all lists of the `creator` are removed. |
| `creator` | `const char *` | Creator identifier. If empty, all lists are removed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function ensures that all iterators and lists associated with a specific `creator` are properly closed, preventing resource leaks. If both `rt_id` and `creator` are empty, all lists in the `topic_name` are closed.

---

(tranger2_close_iterator)=
## `tranger2_close_iterator()`

Closes an iterator in the TimeRanger 2 database, releasing associated resources.

```C
int tranger2_close_iterator(
    json_t *tranger,
    json_t *iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger 2 database instance. |
| `iterator` | `json_t *` | A pointer to the iterator object to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

Closing an iterator ensures that any allocated memory or resources are properly released.

---

(tranger2_close_list)=
## `tranger2_close_list()`

The function `tranger2_close_list()` closes a previously opened list, which can be a real-time memory list (`rt_mem`), a real-time disk list (`rt_disk`), or a non-real-time list.

```C
int tranger2_close_list(
    json_t *tranger,
    json_t *list
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `list` | `json_t *` | A pointer to the list object to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function ensures that resources associated with the list are properly released. If the list is a real-time list, it stops receiving updates.

---

(tranger2_close_rt_disk)=
## `tranger2_close_rt_disk()`

The `tranger2_close_rt_disk()` function closes a previously opened real-time disk stream in the TimeRanger database, releasing associated resources.

```C
int tranger2_close_rt_disk(
    json_t *tranger,
    json_t *disk
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `disk` | `json_t *` | A pointer to the real-time disk stream to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function should be called when a real-time disk stream is no longer needed to free resources.

---

(tranger2_close_rt_mem)=
## `tranger2_close_rt_mem()`

The `tranger2_close_rt_mem()` function closes a real-time memory stream associated with a given TimeRanger instance.

```C
int tranger2_close_rt_mem(
    json_t *tranger,
    json_t *mem
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger instance managing the real-time memory stream. |
| `mem` | `json_t *` | A pointer to the real-time memory stream to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

Closing a real-time memory stream using [`tranger2_close_rt_mem()`](#tranger2_close_rt_mem) ensures that resources are properly released.

---

(tranger2_close_topic)=
## `tranger2_close_topic()`

The `tranger2_close_topic()` function closes an open topic in the TimeRanger database, releasing associated resources.

```C
int tranger2_close_topic(
    json_t  *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

Closing a topic does not delete its data; it only releases resources associated with the open topic.

---

(tranger2_create_topic)=
## `tranger2_create_topic()`

The `tranger2_create_topic()` function creates a new topic in the TimeRanger database if it does not already exist. If the topic exists, it returns the existing topic metadata. The function ensures that the topic is properly initialized with the specified primary key, time key, system flags, and additional metadata.

```C
json_t *tranger2_create_topic(
    json_t              *tranger,
    const char          *topic_name,
    const char          *pkey,
    const char          *tkey,
    json_t              *jn_topic_ext,
    system_flag2_t      system_flag,
    json_t              *jn_cols,
    json_t              *jn_var
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. If the topic exists, only `tranger` and `topic_name` are required. |
| `topic_name` | `const char *` | Name of the topic to be created. |
| `pkey` | `const char *` | Primary key field of the topic. If not specified, the key type defaults to `sf_int_key`. |
| `tkey` | `const char *` | Time key field of the topic. |
| `jn_topic_ext` | `json_t *` | JSON object containing additional topic parameters. See [`topic_json_desc`](#topic_json_desc) for details. This parameter is owned by the function. |
| `system_flag` | `system_flag2_t` | System flags for the topic, defining its behavior and properties. |
| `jn_cols` | `json_t *` | JSON object defining the topic's columns. This parameter is owned by the function. |
| `jn_var` | `json_t *` | JSON object containing variable metadata for the topic. This parameter is owned by the function. |

**Returns**

Returns a JSON object representing the topic metadata. The returned JSON object is not owned by the caller and should not be modified or freed.

**Notes**

This function is idempotent, meaning that if the topic already exists, it will return the existing topic metadata instead of creating a new one. If the primary key (`pkey`) is not specified, the function defaults to `sf_string_key` if `pkey` is defined, otherwise it defaults to `sf_int_key`.

---

(tranger2_delete_record)=
## `tranger2_delete_record()`

The `tranger2_delete_record()` function deletes a record from the specified topic in the TimeRanger database.

```C
int tranger2_delete_record(
    json_t  *tranger,
    const char  *topic_name,
    const char  *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic from which the record will be deleted. |
| `key` | `const char *` | The key identifying the record to be deleted. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

If the specified `key` does not exist in the topic, `tranger2_delete_record()` will fail.

---

(tranger2_delete_topic)=
## `tranger2_delete_topic()`

The `tranger2_delete_topic()` function deletes a topic from the TimeRanger database, effectively removing all associated records and metadata.

```C
int tranger2_delete_topic(
    json_t  *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to be deleted. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

Deleting a topic is irreversible. Ensure that the topic is no longer needed before calling [`tranger2_delete_topic()`](<#tranger2_delete_topic>).

---

(tranger2_dict_topic_desc_cols)=
## `tranger2_dict_topic_desc_cols()`

`tranger2_dict_topic_desc_cols()` retrieves the column descriptions of a specified topic in dictionary format.

```C
json_t *tranger2_dict_topic_desc_cols(
    json_t      *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose column descriptions are to be retrieved. |

**Returns**

A JSON object containing the column descriptions of the specified topic. The caller must decrement the reference count of the returned JSON object.

**Notes**

This function is similar to [`tranger2_list_topic_desc_cols()`](<#tranger2_list_topic_desc_cols>), but returns the data in dictionary format instead of a list.

---

(tranger2_get_iterator_by_id)=
## `tranger2_get_iterator_by_id()`

Retrieve an iterator by its identifier. If the iterator exists, it is returned; otherwise, NULL is returned.

```C
json_t *tranger2_get_iterator_by_id(
    json_t      *tranger,
    const char  *topic_name,
    const char  *iterator_id,
    const char  *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic associated with the iterator. |
| `iterator_id` | `const char *` | Unique identifier of the iterator to retrieve. |
| `creator` | `const char *` | Identifier of the entity that created the iterator. |

**Returns**

Returns a pointer to the iterator JSON object if found; otherwise, returns NULL.

**Notes**

This function does not produce any error messages if the iterator is not found.

---

(tranger2_get_rt_disk_by_id)=
## `tranger2_get_rt_disk_by_id()`

Retrieve a real-time disk instance by its identifier. If the specified real-time disk exists, it returns the corresponding JSON object; otherwise, it returns NULL.

```C
json_t *tranger2_get_rt_disk_by_id(
    json_t      *tranger,
    const char  *topic_name,
    const char  *rt_id,
    const char  *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic associated with the real-time disk. |
| `rt_id` | `const char *` | Identifier of the real-time disk to retrieve. |
| `creator` | `const char *` | Creator identifier used to filter the real-time disk instances. |

**Returns**

Returns a JSON object representing the real-time disk instance if found; otherwise, returns NULL.

**Notes**

This function does not produce any error messages if the real-time disk is not found.

---

(tranger2_get_rt_mem_by_id)=
## `tranger2_get_rt_mem_by_id()`

Retrieve a real-time memory instance by its identifier. If the specified real-time memory instance exists, it is returned; otherwise, NULL is returned.

```C
json_t *tranger2_get_rt_mem_by_id(
    json_t      *tranger,
    const char  *topic_name,
    const char  *rt_id,
    const char  *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic associated with the real-time memory instance. |
| `rt_id` | `const char *` | Identifier of the real-time memory instance to retrieve. |
| `creator` | `const char *` | Creator identifier used to filter the real-time memory instance. |

**Returns**

Returns a pointer to the real-time memory instance as a `json_t *` if found, otherwise returns NULL.

**Notes**

This function does not produce any internal logging or error messages if the requested real-time memory instance is not found.

---

(tranger2_iterator_get_page)=
## `tranger2_iterator_get_page()`

Retrieves a page of records from an iterator in the TimeRanger database. The function returns a JSON object containing the total number of rows, the number of pages based on the specified limit, and the list of retrieved records.

```C
json_t *tranger2_iterator_get_page(
    json_t *tranger,
    json_t *iterator,
    json_int_t from_rowid,    // based 1
    size_t limit,
    BOOL backward
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `iterator` | `json_t *` | A pointer to the iterator from which records will be retrieved. |
| `from_rowid` | `json_int_t` | The starting row ID for retrieving records, based on a 1-based index. |
| `limit` | `size_t` | The maximum number of records to retrieve in the page. |
| `backward` | `BOOL` | If `TRUE`, records are retrieved in reverse order; otherwise, they are retrieved in forward order. |

**Returns**

A JSON object containing the retrieved records, total row count, and pagination details. The caller owns the returned JSON object and must manage its memory.

**Notes**

This function is useful for paginating through records in an iterator. The `from_rowid` parameter determines the starting point, and the `limit` parameter controls the number of records retrieved per page.

---

(tranger2_iterator_size)=
## `tranger2_iterator_size()`

`tranger2_iterator_size()` returns the number of records in the specified iterator.

```C
size_t tranger2_iterator_size(
    json_t *iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `iterator` | `json_t *` | The iterator whose size is to be determined. |

**Returns**

The number of records in the iterator.

**Notes**

This function provides the total count of records available in the given iterator.

---

(tranger2_list_keys)=
## `tranger2_list_keys()`

Returns a list of keys from the specified topic in the TimeRanger database. The function allows filtering keys using a regular expression.

```C
json_t *tranger2_list_keys(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic from which to retrieve the keys. |
| `rkey` | `const char *` | A regular expression pattern to filter the keys. If NULL, all keys are returned. |

**Returns**

A JSON array containing the list of keys. The caller owns the returned JSON object and must free it when no longer needed.

**Notes**

If the topic does not exist or an error occurs, the function may return NULL.

---

(tranger2_list_topic_desc_cols)=
## `tranger2_list_topic_desc_cols()`

Returns a JSON array containing the column descriptions of a topic in the TimeRanger database. The returned JSON object must be decremented after use.

```C
json_t *tranger2_list_topic_desc_cols(
    json_t      *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A JSON object representing the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose column descriptions are to be retrieved. |

**Returns**

A JSON array containing the column descriptions of the specified topic. The caller must decrement the reference count of the returned JSON object after use.

**Notes**

This function was previously known as `tranger_list_topic_desc()`.

---

(tranger2_list_topics)=
## `tranger2_list_topics()`

Returns a list of topic names from the `tranger` database.

```C
json_t *tranger2_list_topics(
    json_t *tranger
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The `tranger` database instance. |

**Returns**

A JSON array containing the names of all topics in the `tranger` database. The caller owns the returned JSON object and must free it when no longer needed.

**Notes**

This function retrieves all topic names, regardless of whether they are currently open or not.

---

(tranger2_open_iterator)=
## `tranger2_open_iterator()`

Opens an iterator for traversing records in a topic within the TimeRanger database. The iterator allows filtering records based on specified conditions and supports real-time data loading.

```C
json_t *tranger2_open_iterator(
    json_t *tranger,
    const char *topic_name,
    const char *key,    // required
    json_t *match_cond, // owned
    tranger2_load_record_callback_t load_record_callback, // called on LOADING and APPENDING, optional
    const char *iterator_id,     // iterator id, optional, if empty will be the key
    const char *creator,     // creator
    json_t *data,       // JSON array, if not empty, fills it with the LOADING data, not owned
    json_t *extra       // owned, user data, this json will be added to the return iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic to iterate over. |
| `key` | `const char *` | Specific key to filter records. Required. |
| `match_cond` | `json_t *` | JSON object containing filtering conditions. Owned by the function. |
| `load_record_callback` | `tranger2_load_record_callback_t` | Callback function invoked during data loading and appending. Optional. |
| `iterator_id` | `const char *` | Unique identifier for the iterator. If empty, the key is used. |
| `creator` | `const char *` | Identifier of the entity creating the iterator. |
| `data` | `json_t *` | JSON array to store loaded data. Not owned by the function. |
| `extra` | `json_t *` | Additional user data attached to the iterator. Owned by the function. |

**Returns**

Returns a JSON object representing the iterator. The caller is responsible for managing its lifecycle.

**Notes**

The iterator supports real-time data loading and filtering based on various conditions. Use [`tranger2_close_iterator()`](<#tranger2_close_iterator>) to release resources when done.

---

(tranger2_open_list)=
## `tranger2_open_list()`

`tranger2_open_list()` opens a list of records in memory, optionally enabling real-time updates via memory or disk.

```C
json_t *tranger2_open_list(
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond, // owned
    json_t *extra,      // owned, will be added to the returned rt
    const char *rt_id,
    BOOL rt_by_disk,
    const char *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic to open. |
| `match_cond` | `json_t *` | A JSON object specifying filtering conditions for records. Owned by the function. |
| `extra` | `json_t *` | Additional user data to be added to the returned real-time object. Owned by the function. |
| `rt_id` | `const char *` | The real-time list identifier. |
| `rt_by_disk` | `BOOL` | If `TRUE`, enables real-time updates via disk; otherwise, uses memory. |
| `creator` | `const char *` | The identifier of the entity creating the list. |

**Returns**

Returns a JSON object representing the real-time list (`rt_mem` or `rt_disk`) or a static list if real-time is disabled. The caller does not own the returned object.

**Notes**

Loading all records may introduce delays in application startup. Use filtering conditions in `match_cond` to optimize performance.

---

(tranger2_open_rt_disk)=
## `tranger2_open_rt_disk()`

Opens a real-time disk-based iterator for monitoring changes in a topic. The function allows tracking new records appended to the topic by monitoring disk events.

```C
json_t *tranger2_open_rt_disk(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on disk
    const char *rt_id,      // rt disk id, REQUIRED
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic to monitor. |
| `key` | `const char *` | Specific key to monitor. If empty, all keys are monitored. |
| `match_cond` | `json_t *` | JSON object containing conditions for filtering records. Owned by the function. |
| `load_record_callback` | `tranger2_load_record_callback_t` | Callback function invoked when a new record is appended to the disk. |
| `rt_id` | `const char *` | Unique identifier for the real-time disk iterator. Required. |
| `creator` | `const char *` | Identifier of the entity creating the iterator. |
| `extra` | `json_t *` | Additional user data to be associated with the iterator. Owned by the function. |

**Returns**

Returns a JSON object representing the real-time disk iterator. The caller does not own the returned object.

**Notes**

This function is used when monitoring real-time changes in a topic via disk events. The iterator remains active until explicitly closed using [`tranger2_close_rt_disk()`](<#tranger2_close_rt_disk>).

---

(tranger2_open_rt_mem)=
## `tranger2_open_rt_mem()`

Opens a real-time memory stream for a given topic in `tranger`. This function enables real-time message processing for the specified `key` and applies filtering conditions from `match_cond`. The callback [`tranger2_load_record_callback_t`](#tranger2_load_record_callback_t) is invoked when new records are appended.

```C
json_t *tranger2_open_rt_mem(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on mem
    const char *list_id,    // rt list id, optional (internally will use the pointer of rt)
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic for which real-time memory streaming is enabled. |
| `key` | `const char *` | Specific key to monitor. If empty, all keys are monitored. |
| `match_cond` | `json_t *` | JSON object containing filtering conditions. Owned by the function. |
| `load_record_callback` | `tranger2_load_record_callback_t` | Callback function invoked when a new record is appended in memory. |
| `list_id` | `const char *` | Optional identifier for the real-time list. If empty, the internal pointer is used. |
| `creator` | `const char *` | Identifier of the entity creating the real-time memory stream. |
| `extra` | `json_t *` | Additional user data stored in the returned iterator. Owned by the function. |

**Returns**

Returns a JSON object representing the real-time memory stream. The caller does not own the returned object.

**Notes**

This function is valid when the Yuno instance is the master writing real-time messages from [`tranger2_append_record()`](<#tranger2_append_record>).

---

(tranger2_open_topic)=
## `tranger2_open_topic()`

The `tranger2_open_topic()` function opens a topic in the TimeRanger database. If the topic is already open, it returns the existing topic JSON object.

```C
json_t *tranger2_open_topic(
    json_t  *tranger,
    const char  *topic_name,
    BOOL  verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A JSON object representing the TimeRanger database. |
| `topic_name` | `const char *` | The name of the topic to open. |
| `verbose` | `BOOL` | If `TRUE`, additional debug information is printed. |

**Returns**

A JSON object representing the opened topic. The returned object is not owned by the caller and should not be modified or freed.

**Notes**

This function is idempotent, meaning that calling it multiple times with the same `topic_name` will return the same JSON object without creating a new instance.

---

(tranger2_print_md0_record)=
## `tranger2_print_md0_record()`

Prints metadata of a record, including row ID, time, message time, and key, into a buffer.

```C
void tranger2_print_md0_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the formatted metadata string. |
| `bfsize` | `int` | Size of the buffer to prevent overflow. |
| `key` | `const char *` | Key associated with the record. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the record metadata structure. |
| `print_local_time` | `BOOL` | Flag indicating whether to print time in local format. |

**Returns**

This function does not return a value.

**Notes**

The function formats the metadata into the provided buffer, ensuring it does not exceed `bfsize`. It prints row ID, time, message time, and key.

---

(tranger2_print_md1_record)=
## `tranger2_print_md1_record()`

Prints metadata information of a record, including row ID, user flag, system flag, timestamps, and key, into a buffer.

```C
void tranger2_print_md1_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer where the formatted metadata string will be stored. |
| `bfsize` | `int` | Size of the buffer to ensure safe writing. |
| `key` | `const char *` | Key associated with the record. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the record metadata structure containing the information to be printed. |
| `print_local_time` | `BOOL` | Flag indicating whether to print timestamps in local time (if `TRUE`) or UTC (if `FALSE`). |

**Returns**

This function does not return a value.

**Notes**

The function formats and writes metadata details into the provided buffer, ensuring that the output does not exceed `bfsize` bytes.

---

(tranger2_print_md2_record)=
## `tranger2_print_md2_record()`

Prints detailed metadata of a record, including row ID, offset, size, timestamp, and file path, into the provided buffer.

```C
void tranger2_print_md2_record(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer where the formatted metadata string will be stored. |
| `bfsize` | `int` | Size of the buffer to ensure safe writing. |
| `tranger` | `json_t *` | Reference to the TimeRanger database instance. |
| `topic` | `json_t *` | JSON object representing the topic associated with the record. |
| `key` | `const char *` | Key identifying the record within the topic. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the extended metadata structure of the record. |
| `print_local_time` | `BOOL` | Flag indicating whether to print timestamps in local time. |

**Returns**

This function does not return a value.

**Notes**

The function formats metadata details into the provided buffer, ensuring that the output remains within the specified buffer size.

---

(tranger2_print_record_filename)=
## `tranger2_print_record_filename()`

Formats and stores the filename of a record in a buffer, using metadata from [`tranger2_print_record_filename()`](#tranger2_print_record_filename).

```C
void tranger2_print_record_filename(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Pointer to the buffer where the formatted filename will be stored. |
| `bfsize` | `int` | Size of the buffer in bytes. |
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic` | `json_t *` | Pointer to the topic associated with the record. |
| `md_record_ex` | `const md2_record_ex_t *` | Pointer to the record metadata structure. |
| `print_local_time` | `BOOL` | Flag indicating whether to format timestamps in local time. |

**Returns**

This function does not return a value.

**Notes**

The buffer `bf` must be large enough to store the formatted filename. The function retrieves metadata from [`tranger2_print_record_filename()`](#tranger2_print_record_filename) to construct the filename.

---

(tranger2_read_record_content)=
## `tranger2_read_record_content()`

Reads the content of a record from a given topic in the TimeRanger database. The function retrieves the record's data based on its metadata.

```C
json_t *tranger2_read_record_content(
    json_t            *tranger,
    json_t            *topic,
    const char        *key,
    md2_record_ex_t   *md_record_ex
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic` | `json_t *` | Pointer to the topic from which the record will be read. |
| `key` | `const char *` | The key identifying the record to be read. |
| `md_record_ex` | `md2_record_ex_t *` | Pointer to the metadata structure of the record to be retrieved. |

**Returns**

Returns a `json_t *` object containing the record's content. The caller owns the returned JSON object and must free it when no longer needed.

**Notes**

This function is useful when only metadata has been loaded and the full record content needs to be retrieved.

---

(tranger2_read_user_flag)=
## `tranger2_read_user_flag()`

The `tranger2_read_user_flag()` function retrieves the user flag associated with a specific record in a given topic.

```C
uint16_t tranger2_read_user_flag(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the record. |
| `rowid` | `uint64_t` | Unique identifier of the record whose user flag is to be retrieved. |

**Returns**

Returns the `uint16_t` user flag associated with the specified record.

**Notes**

The function is used in writing mode to check the user flag of a record before modifying it.

---

(tranger2_set_trace_level)=
## `tranger2_set_trace_level()`

Sets the trace level of the `tranger` instance, controlling the verbosity of logging and debugging output.

```C
void tranger2_set_trace_level(
    json_t *tranger,
    int      trace_level
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the `tranger` instance whose trace level is to be modified. |
| `trace_level` | `int` | The new trace level to be set, determining the verbosity of logging. |

**Returns**

This function does not return a value.

**Notes**

Higher trace levels typically enable more detailed logging, which can be useful for debugging.

---

(tranger2_set_user_flag)=
## `tranger2_set_user_flag()`

Sets or clears specific bits in the user flag of a record in a topic within the TimeRanger database.

```C
int tranger2_set_user_flag(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid,
    uint16_t mask,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the record. |
| `rowid` | `uint64_t` | Unique identifier of the record whose user flag is being modified. |
| `mask` | `uint32_t` | Bitmask specifying which bits in the user flag should be modified. |
| `set` | `BOOL` | If `TRUE`, the bits specified in `mask` are set; if `FALSE`, they are cleared. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

This function modifies only the bits specified in `mask`, leaving other bits in the user flag unchanged.

---

(tranger2_shutdown)=
## `tranger2_shutdown()`

The `tranger2_shutdown()` function shuts down the TimeRanger database, releasing all allocated memory.

```C
int tranger2_shutdown(
    json_t *tranger
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance to be shut down. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

This function should be called when the database is no longer needed to free resources.

---

(tranger2_startup)=
## `tranger2_startup()`

Initializes the TimeRanger 2 database, setting up its internal structures and preparing it for use. The function requires a `hgobj` instance and a JSON configuration object that defines database parameters.

```C
json_t *tranger2_startup(
    hgobj      gobj,
    json_t    *jn_tranger,
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj framework instance managing the database. |
| `jn_tranger` | `json_t *` | A JSON object containing database configuration parameters. See [`tranger2_json_desc`](#tranger2_json_desc) for details. |
| `yev_loop` | `yev_loop_h` | An event loop handle for asynchronous operations. |

**Returns**

A JSON object representing the initialized TimeRanger 2 database instance.

**Notes**

The returned JSON object must be properly managed and eventually passed to [`tranger2_stop()`](#tranger2_stop) or [`tranger2_shutdown()`](#tranger2_shutdown) to release resources.

---

(tranger2_stop)=
## `tranger2_stop()`

The `tranger2_stop()` function closes the TimeRanger database, ensuring that all topics and file descriptors are properly closed.

```C
int tranger2_stop(
    json_t *tranger
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance to be closed. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

This function should be called before shutting down the database using [`tranger2_shutdown()`](<#tranger2_shutdown>).

---

(tranger2_str2system_flag)=
## `tranger2_str2system_flag()`

Converts a formatted string containing system flag representations into a `system_flag2_t` integer. The input string can use delimiters such as '|', ' ', or ','.

```C
system_flag2_t tranger2_str2system_flag(
    const char *system_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `system_flag` | `const char *` | A string containing system flag values separated by '\|', ' ', or ','. |

**Returns**

Returns the corresponding `system_flag2_t` integer representing the parsed system flags.

**Notes**

This function is useful for converting human-readable flag representations into their corresponding bitmask values.

---

(tranger2_topic)=
## `tranger2_topic()`

Retrieve a topic by its name from the TimeRanger database. If the topic is not already opened, [`tranger2_open_topic()`](<#tranger2_open_topic>) is called to open it.

```C
json_t *tranger2_topic(
    json_t      *tranger,
    const char  *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic to retrieve. |

**Returns**

A JSON object representing the topic. The returned object is not owned by the caller and should not be modified or freed.

**Notes**

If the topic exists on disk but has not been opened yet, [`tranger2_open_topic()`](<#tranger2_open_topic>) is automatically invoked.

---

(tranger2_topic_desc)=
## `tranger2_topic_desc()`

`tranger2_topic_desc()` retrieves the description of a specified topic from the TimeRanger database.

```C
json_t *tranger2_topic_desc(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose description is to be retrieved. |

**Returns**

A JSON object containing the topic description. The caller must decrement the reference count of the returned JSON object.

**Notes**

The returned JSON object must be properly decremented using `json_decref()` to avoid memory leaks.

---

(tranger2_topic_key_size)=
## `tranger2_topic_key_size()`

Retrieves the number of records associated with a specific key in a given topic within the TimeRanger database.

```C
uint64_t tranger2_topic_key_size(
    json_t  *tranger,
    const char *topic_name,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic containing the key. |
| `key` | `const char *` | The key whose record count is to be retrieved. |

**Returns**

Returns the number of records associated with the specified `key` in the given `topic_name`.

**Notes**

If the topic or key does not exist, the function may return zero.

---

(tranger2_topic_name)=
## `tranger2_topic_name()`

Retrieves the topic name from the given `json_t *` topic object.

```C
const char *tranger2_topic_name(
    json_t *topic
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `topic` | `json_t *` | A JSON object representing the topic. |

**Returns**

A pointer to a string containing the topic name. The returned string must not be modified or freed by the caller.

**Notes**

If the `topic` parameter is `NULL` or invalid, the behavior is undefined.

---

(tranger2_topic_size)=
## `tranger2_topic_size()`

`tranger2_topic_size()` retrieves the total number of records present in a specified topic within the TimeRanger database.

```C
uint64_t tranger2_topic_size(
    json_t *tranger,
    const char *topic_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic whose record count is to be retrieved. |

**Returns**

Returns the total number of records in the specified topic as a `uint64_t` value.

**Notes**

If the topic does not exist, the function may return `0`.

---

(tranger2_write_topic_cols)=
## `tranger2_write_topic_cols()`

The `tranger2_write_topic_cols()` function updates the column definitions of a specified topic in the TimeRanger database.

```C
int tranger2_write_topic_cols(
    json_t  *tranger,
    const char  *topic_name,
    json_t  *jn_cols  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose column definitions are being updated. |
| `jn_cols` | `json_t *` | A JSON object containing the new column definitions. The ownership of this object is transferred to [`tranger2_write_topic_cols()`](<#tranger2_write_topic_cols>). |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

This function modifies the column definitions of an existing topic. Ensure that the topic exists before calling [`tranger2_write_topic_cols()`](<#tranger2_write_topic_cols>).

---

(tranger2_write_topic_var)=
## `tranger2_write_topic_var()`

The `tranger2_write_topic_var()` function updates the variable metadata of a specified topic in the TimeRanger database.

```C
int tranger2_write_topic_var(
    json_t  *tranger,
    const char  *topic_name,
    json_t  *jn_topic_var
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic whose variable metadata is to be updated. |
| `jn_topic_var` | `json_t *` | A JSON object containing the new variable metadata for the topic. The ownership of this object is transferred to [`tranger2_write_topic_var()`](<#tranger2_write_topic_var>). |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function modifies the variable metadata of a topic, which may include user-defined flags or other dynamic properties.

---

(tranger2_write_user_flag)=
## `tranger2_write_user_flag()`

The `tranger2_write_user_flag()` function updates the user flag of a specific record identified by `rowid` in the given `topic_name` within the TimeRanger database.

```C
int tranger2_write_user_flag(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid,
    uint16_t user_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | The name of the topic containing the record. |
| `rowid` | `uint64_t` | The unique row identifier of the record to update. |
| `user_flag` | `uint32_t` | The new user flag value to be assigned to the record. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function modifies the user flag of an existing record but does not alter other record attributes. Use [`tranger2_set_user_flag()`](<#tranger2_set_user_flag>) if you need to update the flag using a mask.

---

(tranger2_list_topic_names)=
## `tranger2_list_topic_names()`

*Description pending — signature extracted from header.*

```C
json_t *tranger2_list_topic_names(
    json_t *tranger
);
```

---

(tranger2_topic_path)=
## `tranger2_topic_path()`

*Description pending — signature extracted from header.*

```C
int tranger2_topic_path(
    char *bf,
    size_t bfsize,
    json_t *tranger,
    const char *topic_name
);
```

---

