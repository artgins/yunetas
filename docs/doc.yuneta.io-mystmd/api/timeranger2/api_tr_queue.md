# Queue Functions

Source code in:

- [tr_queue.c](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_queue.c)
- [tr_queue.h](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_queue.h)

(trq_add_retries)=
## `trq_add_retries()`

Increments the retry count of the given queue message. This function updates the retry counter associated with the specified [`q_msg`](#q_msg).

```C
void trq_add_retries(
    q_msg msg,
    int    retries
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The queue message whose retry count will be incremented. |
| `retries` | `int` | The number of retries to add to the message. |

**Returns**

This function does not return a value.

**Notes**

Use [`trq_test_retries()`](<#trq_test_retries>) to check if the retry limit has been reached.

---

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

(trq_append)=
## `trq_append()`

`trq_append()` appends a new message to the queue, storing it persistently.

```C
q_msg trq_append(
    tr_queue  trq,
    json_t   *jn_msg  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance where the message will be appended. |
| `jn_msg` | `json_t *` | The JSON message to append. Ownership is transferred to the function. |

**Returns**

Returns a `q_msg` handle to the newly appended message, or `NULL` on failure.

**Notes**

The message must be flagged after appending if it needs to be recovered in the next session using [`trq_load()`](<#trq_load>).

---

(trq_check_backup)=
## `trq_check_backup()`

`trq_check_backup()` performs a backup operation on the queue if necessary.

```C
int trq_check_backup(
    tr_queue trq
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
    tr_queue  trq,
    uint64_t  rowid
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

(trq_clear_ack_timer)=
## `trq_clear_ack_timer()`

Clears the acknowledgment timer for the specified message, removing any pending acknowledgment timeout.

```C
void trq_clear_ack_timer(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message whose acknowledgment timer should be cleared. |

**Returns**

This function does not return a value.

**Notes**

Use [`trq_set_ack_timer()`](<#trq_set_ack_timer>) to set an acknowledgment timer before clearing it.

---

(trq_clear_retries)=
## `trq_clear_retries()`

Clears the retry count of the specified message in the queue.

```C
void trq_clear_retries(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message whose retry count should be cleared. |

**Returns**

This function does not return a value.

**Notes**

Use [`trq_test_retries()`](<#trq_test_retries>) to check the retry count before clearing it.

---

(trq_close)=
## `trq_close()`

Closes the given `tr_queue`, releasing associated resources. After calling `trq_close()`, ensure to invoke [`tranger2_shutdown()`](<#tranger2_shutdown>) if no other queues are in use.

```C
void trq_close(
    tr_queue trq
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

(trq_first_msg)=
## `trq_first_msg()`

Retrieves the first message in the queue associated with the given `tr_queue` instance.

```C
q_msg trq_first_msg(
    tr_queue trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance from which to retrieve the first message. |

**Returns**

A `q_msg` handle to the first message in the queue, or `NULL` if the queue is empty.

**Notes**

Use [`trq_next_msg()`](<#trq_next_msg>) to iterate through subsequent messages in the queue.

---

(trq_get_by_rowid)=
## `trq_get_by_rowid()`

`trq_get_by_rowid()` retrieves a message from the queue iterator using its row ID.

```C
q_msg trq_get_by_rowid(
    tr_queue  trq,
    uint64_t  rowid
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

(trq_get_soft_mark)=
## `trq_get_soft_mark()`

`trq_get_soft_mark()` retrieves the soft mark value of a given message in the queue.

```C
uint64_t trq_get_soft_mark(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message whose soft mark value is to be retrieved. |

**Returns**

Returns the soft mark value associated with the given message.

**Notes**

The soft mark is a user-defined value that can be used for tracking message states.

---

(trq_last_msg)=
## `trq_last_msg()`

`trq_last_msg()` retrieves the last message in the queue, allowing iteration from the most recent entry.

```C
q_msg trq_last_msg(
    tr_queue trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance from which to retrieve the last message. |

**Returns**

Returns a `q_msg` representing the last message in the queue, or `NULL` if the queue is empty.

**Notes**

Use [`trq_prev_msg()`](<#trq_prev_msg>) to iterate backward through the queue.

---

(trq_load)=
## `trq_load()`

`trq_load()` loads pending messages from the queue and returns an iterator for traversal.

```C
int trq_load(
    tr_queue trq
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
    tr_queue   trq,
    const char *key,
    int64_t    from_rowid,
    int64_t    to_rowid
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
    q_msg msg
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

(trq_msg_md_record)=
## `trq_msg_md_record()`

`trq_msg_md_record()` retrieves the metadata record of a given queue message.

```C
md2_record_ex_t trq_msg_md_record(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The queue message whose metadata record is to be retrieved. |

**Returns**

Returns an `md2_record_ex_t` structure containing the metadata record of the specified message.

**Notes**

The metadata record provides additional information about the message, such as timestamps and flags.

---

(trq_msg_rowid)=
## `trq_msg_rowid()`

`trq_msg_rowid()` retrieves the unique row ID associated with the given queue message.

```C
uint64_t trq_msg_rowid(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The queue message whose row ID is to be retrieved. |

**Returns**

Returns the unique row ID of the specified queue message.

**Notes**

The row ID is a unique identifier assigned to each message in the queue.

---

(trq_msg_time)=
## `trq_msg_time()`

`trq_msg_time()` retrieves the timestamp associated with a given queue message.

```C
uint64_t trq_msg_time(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The queue message whose timestamp is to be retrieved. |

**Returns**

Returns the timestamp of the message as a `uint64_t` value.

**Notes**

The timestamp represents the time the message was added to the queue.

---

(trq_next_msg)=
## `trq_next_msg()`

Retrieves the next message in the queue iteration after the given [`q_msg`](#q_msg).

```C
q_msg trq_next_msg(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The current message in the queue iteration. |

**Returns**

Returns the next [`q_msg`](#q_msg) in the queue iteration, or `NULL` if there are no more messages.

**Notes**

Use [`trq_first_msg()`](<#trq_first_msg>) to obtain the first message before calling [`trq_next_msg()`](<#trq_next_msg>) in a loop.

---

(trq_open)=
## `trq_open()`

`trq_open()` initializes and opens a persistent queue using the specified `tranger` instance and topic configuration.

```C
tr_queue trq_open(
    json_t           *tranger,
    const char       *topic_name,
    const char       *pkey,
    const char       *tkey,
    system_flag2_t    system_flag,
    size_t           backup_queue_size
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

(trq_prev_msg)=
## `trq_prev_msg()`

`trq_prev_msg()` retrieves the previous message in the queue iteration.

```C
q_msg trq_prev_msg(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The current message in the queue iteration. |

**Returns**

Returns the previous message in the queue iteration, or `NULL` if there are no more messages.

**Notes**

Use [`trq_first_msg()`](<#trq_first_msg>) or [`trq_last_msg()`](<#trq_last_msg>) to obtain an initial message before iterating with [`trq_prev_msg()`](<#trq_prev_msg>).

---

(trq_set_ack_timer)=
## `trq_set_ack_timer()`

Sets an acknowledgment timer for the given `q_msg` message, specifying the timeout in seconds.

```C
time_t trq_set_ack_timer(
    q_msg  msg,
    time_t seconds
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message for which the acknowledgment timer is being set. |
| `seconds` | `time_t` | The timeout duration in seconds before the acknowledgment timer expires. |

**Returns**

Returns the previously set acknowledgment timer value in seconds.

**Notes**

Use [`trq_test_ack_timer()`](<#trq_test_ack_timer>) to check if the acknowledgment timer has expired.

---

(trq_set_first_rowid)=
## `trq_set_first_rowid()`

`trq_set_first_rowid()` sets the starting row ID for iterating over messages in the queue, optimizing retrieval performance.

```C
void trq_set_first_rowid(
    tr_queue  trq,
    uint64_t  first_rowid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance to modify. |
| `first_rowid` | `uint64_t` | The row ID from which message iteration should begin. |

**Returns**

This function does not return a value.

**Notes**

Setting a lower `first_rowid` can improve performance when iterating over messages using [`trq_load()`](<#trq_load>) or [`trq_load_all()`](<#trq_load_all>).

---

(trq_set_hard_flag)=
## `trq_set_hard_flag()`

`trq_set_hard_flag()` marks a message with a hard flag, allowing it to be recovered in the next queue open if the flag is used in [`trq_load()`](<#trq_load>).

```C
int trq_set_hard_flag(
    q_msg   msg,
    uint32_t hard_mark,
    BOOL    set
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

(trq_set_maximum_retries)=
## `trq_set_maximum_retries()`

Sets the maximum number of retries for messages in the queue `trq_set_maximum_retries()`.

```C
void trq_set_maximum_retries(
    tr_queue trq,
    int      maximum_retries
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance for which the maximum retries will be set. |
| `maximum_retries` | `int` | The maximum number of retries allowed for messages in the queue. |

**Returns**

This function does not return a value.

**Notes**

Setting a maximum retry count using `trq_set_maximum_retries()` helps control the number of times a message is retried before being discarded or handled differently.

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
    q_msg   msg,
    uint64_t soft_mark,
    BOOL    set
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

(trq_size)=
## `trq_size()`

`trq_size()` returns the number of messages currently stored in the specified queue.

```C
size_t trq_size(
    tr_queue trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue whose size is to be determined. |

**Returns**

The number of messages in the queue.

**Notes**

This function provides a quick way to check the number of messages stored in a queue.

---

(trq_size_by_key)=
## `trq_size_by_key()`

`trq_size_by_key()` returns the number of messages in the queue that match the specified key.

```C
int trq_size_by_key(
    tr_queue   trq,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance to search within. |
| `key` | `const char *` | The key used to filter messages in the queue. |

**Returns**

The number of messages in the queue that match the given key.

**Notes**

If no messages match the key, `trq_size_by_key()` returns `0`.

---

(trq_test_ack_timer)=
## `trq_test_ack_timer()`

Tests whether the acknowledgment timer for a given `q_msg` is active.

```C
BOOL trq_test_ack_timer(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message whose acknowledgment timer is to be tested. |

**Returns**

Returns `TRUE` if the acknowledgment timer is active, otherwise `FALSE`.

**Notes**

Use [`trq_set_ack_timer()`](<#trq_set_ack_timer>) to set an acknowledgment timer and [`trq_clear_ack_timer()`](<#trq_clear_ack_timer>) to clear it.

---

(trq_test_retries)=
## `trq_test_retries()`

`trq_test_retries()` checks whether the retry limit for a given message has been reached.

```C
BOOL trq_test_retries(
    q_msg msg
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q_msg` | The message whose retry status is being checked. |

**Returns**

Returns `TRUE` if the retry limit has been reached, otherwise `FALSE`.

**Notes**

Use [`trq_add_retries()`](<#trq_add_retries>) to increment the retry count and [`trq_clear_retries()`](<#trq_clear_retries>) to reset it.

---

(trq_topic)=
## `trq_topic()`

`trq_topic()` returns the topic associated with the given queue.

```C
json_t *trq_topic(
    tr_queue trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance from which to retrieve the topic. |

**Returns**

A `json_t *` representing the topic of the queue.

**Notes**

The returned `json_t *` is owned by the queue and should not be modified or freed by the caller.

---

(trq_tranger)=
## `trq_tranger()`

Returns the `tranger` instance associated with the given `tr_queue`.

```C
json_t *trq_tranger(
    tr_queue trq
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr_queue` | The queue instance from which to retrieve the `tranger`. |

**Returns**

A `json_t *` representing the `tranger` instance associated with the queue.

**Notes**

The returned `json_t *` is managed internally and should not be modified or freed by the caller.

---

(trq_unload_msg)=
## `trq_unload_msg()`

The `trq_unload_msg()` function unloads a message from the queue iterator, removing it from memory.

```C
void trq_unload_msg(
    q_msg   msg,
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
