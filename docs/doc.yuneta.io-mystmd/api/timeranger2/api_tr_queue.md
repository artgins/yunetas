# Queue Functions

Source code in:

- [tr_queue.c](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_queue.c)
- [tr_queue.h](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_queue.h)

(trq_answer)=
## `trq_answer()`

`trq_answer()` extracts and returns a new JSON object containing only the metadata from the given message.

```C
json_t *trq_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TRQ__
    int     result
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_message` | `json_t *` | A JSON object representing the original message. The ownership of this object is not transferred. |
| `result` | `int` | An integer result code that may be included in the returned metadata. |

**Returns**

A new JSON object containing only the metadata extracted from `jn_message`. The caller assumes ownership of the returned object.

**Notes**

The function is specifically designed to extract the `__MD_TRQ__` metadata field from the input message.

---

(trq_check_backup)=
## `trq_check_backup()`

`trq_check_backup()` performs a backup operation on the queue if necessary.

```C
int trq_check_backup(
    tr_queue_t * trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance to check and perform backup if needed. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

This function ensures that the queue's backup mechanism is triggered when required.

---

(trq_check_pending_rowid)=
## `trq_check_pending_rowid()`

`trq_check_pending_rowid()` checks the pending status of a message identified by its `rowid` in the queue.

```C
int trq_check_pending_rowid(
    tr_queue_t * trq,
    uint64_t __t__,
    uint64_t rowid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance to check. |
| `rowid` | `uint64_t` | The unique row identifier of the message. |

**Returns**

Returns `-1` if the `rowid` does not exist, `1` if the message is pending, and `0` if it is not pending.

**Notes**

This function provides a low-level check for message status in the queue.

---

(trq_close)=
## `trq_close()`

Closes the given `tr_queue`, releasing associated resources. After calling `trq_close()`, ensure to invoke [`tranger2_shutdown()`](<#tranger2_shutdown>) if no other queues are in use.

```C
void trq_close(
    tr_queue_t * trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance to be closed. |

**Returns**

This function does not return a value.

**Notes**

Ensure that [`trq_close()`](<#trq_close>) is called before shutting down the underlying TimeRanger instance with [`tranger2_shutdown()`](<#tranger2_shutdown>).

---

(trq_get_by_rowid)=
## `trq_get_by_rowid()`

`trq_get_by_rowid()` retrieves a message from the queue iterator using its row ID.

```C
q_msg_t * trq_get_by_rowid(
    tr_queue_t * trq,
    uint64_t rowid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance from which to retrieve the message. |
| `rowid` | `uint64_t` | The row ID of the message to retrieve. |

**Returns**

Returns a `q_msg` handle to the retrieved message, or `NULL` if the message is not found.

**Notes**

The returned message remains owned by the queue and should not be freed manually.

---

(trq_get_metadata)=
## `trq_get_metadata()`

Retrieves the metadata associated with a given JSON object. The returned JSON object is not owned by the caller.

```C
json_t *trq_get_metadata(
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | The JSON object containing metadata. |

**Returns**

A pointer to a JSON object containing the metadata. The returned JSON object is not owned by the caller and should not be modified or freed.

**Notes**

The returned JSON object is a reference and should not be altered or deallocated by the caller.

---

(trq_load)=
## `trq_load()`

`trq_load()` loads pending messages from the queue and returns an iterator for traversal.

```C
int trq_load(
    tr_queue_t * trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance from which pending messages will be loaded. |

**Returns**

Returns an iterator for traversing the loaded messages.

**Notes**

Use [`trq_load_all()`](<#trq_load_all>) to load all messages, including non-pending ones.

---

(trq_load_all)=
## `trq_load_all()`

`trq_load_all()` loads all messages from the queue within the specified rowid range, optionally filtering by key.

```C
int trq_load_all(
    tr_queue_t * trq,
    int64_t from_rowid,
    int64_t to_rowid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance from which messages will be loaded. |
| `key` | `const char *` | An optional key to filter messages; if NULL, all messages are loaded. |
| `from_rowid` | `int64_t` | The starting rowid for loading messages. |
| `to_rowid` | `int64_t` | The ending rowid for loading messages. |

**Returns**

Returns an iterator over the loaded messages or an error code if the operation fails.

**Notes**

Use [`trq_load_all()`](<#trq_load_all>) to retrieve messages efficiently within a specific rowid range.

---

(trq_msg_json)=
## `trq_msg_json()`

`trq_msg_json()` retrieves the JSON representation of a queue message. The returned JSON object is not owned by the caller and must not be modified or freed.

```C
json_t *trq_msg_json(
    q_msg_t *msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The queue message whose JSON representation is to be retrieved. |

**Returns**

A pointer to a `json_t` object representing the message. The returned JSON object is not owned by the caller.

**Notes**

The returned JSON object must not be modified or freed by the caller.

---

(trq_open)=
## `trq_open()`

`trq_open()` initializes and opens a persistent queue using the specified `tranger` instance and topic configuration.

```C
tr_queue_t *trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag,
    size_t backup_queue_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the `tranger` instance managing the queue. |
| `topic_name` | `const char *` | Name of the topic associated with the queue. |
| `pkey` | `const char *` | Primary key used for indexing messages in the queue. |
| `tkey` | `const char *` | Time key used for ordering messages in the queue. |
| `system_flag` | `system_flag2_t` | System flags controlling queue behavior. |
| `backup_queue_size` | `size_t` | Maximum number of messages to retain in the backup queue. |

**Returns**

Returns a `tr_queue` handle representing the opened queue, or `NULL` on failure.

**Notes**

Ensure that [`tranger2_startup()`](<#tranger2_startup>) is called before invoking [`trq_open()`](<#trq_open>).

---

(trq_set_hard_flag)=
## `trq_set_hard_flag()`

`trq_set_hard_flag()` marks a message with a hard flag, allowing it to be recovered in the next queue open if the flag is used in [`trq_load()`](<#trq_load>).

```C
int trq_set_hard_flag(
    q_msg_t *msg,
    uint16_t hard_mark,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message to be marked. |
| `hard_mark` | `uint32_t` | The hard flag to set on the message. |
| `set` | `BOOL` | If `TRUE`, the flag is set; if `FALSE`, the flag is cleared. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

A message must be flagged after being appended to the queue if it needs to be recovered in the next queue open using [`trq_load()`](<#trq_load>).

---

(trq_set_metadata)=
## `trq_set_metadata()`

`trq_set_metadata()` sets a metadata key-value pair in the given JSON object.

```C
int trq_set_metadata(
    json_t  *kw,
    const char *key,
    json_t  *jn_value  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | The JSON object where the metadata will be stored. |
| `key` | `const char *` | The key under which the metadata value will be stored. |
| `jn_value` | `json_t *` | The JSON value to be stored as metadata. Ownership is transferred. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

The caller must ensure that `kw` is a valid JSON object before calling [`trq_set_metadata()`](<#trq_set_metadata>).

---

(trq_set_soft_mark)=
## `trq_set_soft_mark()`

`trq_set_soft_mark()` sets or clears a soft mark on a given queue message.

```C
uint64_t trq_set_soft_mark(
    q_msg_t *msg,
    uint64_t soft_mark,
    BOOL set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The queue message on which the soft mark is to be set or cleared. |
| `soft_mark` | `uint64_t` | The soft mark value to be applied to the message. |
| `set` | `BOOL` | If `TRUE`, the soft mark is set; if `FALSE`, the soft mark is cleared. |

**Returns**

Returns the updated soft mark value of the message.

**Notes**

Soft marks are used for temporary message state tracking and do not persist across queue restarts.

---

(trq_unload_msg)=
## `trq_unload_msg()`

The `trq_unload_msg()` function unloads a message from the queue iterator, removing it from memory.

```C
void trq_unload_msg(
    q_msg_t *msg,
    int32_t result
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message to be unloaded from the queue iterator. |
| `result` | `int32_t` | The result code associated with the message unloading operation. |

**Returns**

This function does not return a value.

**Notes**

Use `trq_unload_msg()` to free a message from the queue iterator after processing it.

---

(trq_append2)=
## `trq_append2()`

*Description pending — signature extracted from header.*

```C
q_msg_t * trq_append2(
    tr_queue_t * trq,
    json_int_t t,
    json_t *kw,
    uint16_t user_flag
);
```

---

(trq_load_all_by_time)=
## `trq_load_all_by_time()`

*Description pending — signature extracted from header.*

```C
int trq_load_all_by_time(
    tr_queue_t * trq,
    int64_t from_t,
    int64_t to_t
);
```

---

(trq_msg_md)=
## `trq_msg_md()`

*Description pending — signature extracted from header.*

```C
md2_record_ex_t *trq_msg_md(
    q_msg_t *msg
);
```

---

