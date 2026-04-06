# tr_msg

Low-level message store built on top of timeranger2. Used as the common substrate for `tr_msg2db` and `tr_queue`.

Source code:

- [`tr_msg.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_msg.h)
- [`tr_msg.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_msg.c)

(trmsg_active_records)=
## `trmsg_active_records()`

`trmsg_active_records()` returns a list of cloned active records from the given `list`, applying the optional filter `jn_filter`.

```C
json_t *trmsg_active_records(
    json_t *list,
    json_t *jn_filter  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing messages from which active records will be retrieved. |
| `jn_filter` | `json_t *` | An optional filter to apply when selecting active records. The caller owns this parameter. |

**Returns**

A list of cloned active records. The caller owns the returned value and must decrement its reference when done.

**Notes**

The returned list contains cloned records, making it safe for modification. The caller is responsible for managing the memory of the returned value.

---

(trmsg_add_instance)=
## `trmsg_add_instance()`

`trmsg_add_instance()` adds a new message instance to the specified topic in the TimeRanger database.

```C
int trmsg_add_instance(
    json_t            *tranger,
    const char        *topic_name,
    json_t            *jn_msg,      // owned
    md2_record_ex_t   *md_record
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic where the instance will be added. |
| `jn_msg` | `json_t *` | JSON object containing the message instance data. Ownership is transferred to [`trmsg_add_instance()`](<#trmsg_add_instance>). |
| `md_record` | `md2_record_ex_t *` | Pointer to the metadata record associated with the message instance. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

The `jn_msg` parameter is owned by [`trmsg_add_instance()`](<#trmsg_add_instance>), meaning it will be managed and freed internally.

---

(trmsg_close_list)=
## `trmsg_close_list()`

`trmsg_close_list()` closes a previously opened list of messages in the TimeRanger system, releasing associated resources.

```C
int trmsg_close_list(
    json_t *tranger,
    json_t *list
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger instance managing the list. |
| `list` | `json_t *` | Pointer to the list of messages to be closed. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

Ensure that [`trmsg_open_list()`](<#trmsg_open_list>) was called before invoking [`trmsg_close_list()`](<#trmsg_close_list>) to properly manage resources.

---

(trmsg_close_topics)=
## `trmsg_close_topics()`

The `trmsg_close_topics()` function closes the specified topics in the given `tranger` instance, ensuring proper resource cleanup.

```C
int trmsg_close_topics(
    json_t           *tranger,
    const topic_desc_t *descs
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A pointer to the `tranger` instance managing the topics. |
| `descs` | `const topic_desc_t *` | A pointer to an array of topic descriptors specifying the topics to be closed. |

**Returns**

Returns `0` on success, or a negative value if an error occurs during topic closure.

**Notes**

Ensure that [`trmsg_open_topics()`](<#trmsg_open_topics>) was previously called before invoking this function to close topics properly.

---

(trmsg_data_tree)=
## `trmsg_data_tree()`

`trmsg_data_tree()` returns a list of cloned records with instances stored in the `data` hook, formatted for Webix usage.

```C
json_t *trmsg_data_tree(
    json_t *list,
    json_t *jn_filter  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing messages to be processed. |
| `jn_filter` | `json_t *` | A JSON object specifying filtering criteria. The ownership of this parameter is transferred to the function. |

**Returns**

A JSON list of cloned records, structured for Webix. The caller is responsible for decrementing the reference count.

**Notes**

The returned JSON object must be manually decremented to avoid memory leaks.

---

(trmsg_foreach_active_messages)=
## `trmsg_foreach_active_messages()`

Iterates over all active messages in the given `list`, invoking the specified callback function for each message.

```C
int trmsg_foreach_active_messages(
    json_t *list,
    int (*callback)(
        json_t *list,  // Not yours!
        const char *key,
        json_t *record, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | A JSON object representing the list of messages to iterate over. |
| `callback` | `int (*)(json_t *, const char *, json_t *, void *, void *)` | A function pointer to the callback that will be invoked for each active message. The callback receives the list, message key, record, and user-defined data. |
| `user_data1` | `void *` | A user-defined pointer passed to the callback function. |
| `user_data2` | `void *` | A second user-defined pointer passed to the callback function. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria for selecting messages. The caller owns this object. |

**Returns**

Returns 0 on success, a negative value if the iteration was interrupted by the callback, or a positive error code on failure.

**Notes**

The callback function should return a negative value to break the iteration, 0 to continue, or 1 to add the record to the returned list. See [`trmsg_foreach_messages()`](<#trmsg_foreach_messages>) for iterating over all messages.

---

(trmsg_foreach_instances_messages)=
## `trmsg_foreach_instances_messages()`

Iterates over all instance messages in the given `list`, invoking the specified callback function for each message. The callback receives a cloned instance of each message.

```C
int trmsg_foreach_instances_messages(
    json_t *list,
    int (*callback)(
        json_t *list,      // Not yours!
        const char *key,
        json_t *instances, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing messages to iterate over. |
| `callback` | `int (*)(json_t *, const char *, json_t *, void *, void *)` | Function pointer to the callback that will be invoked for each message instance. |
| `user_data1` | `void *` | User-defined data passed to the callback function. |
| `user_data2` | `void *` | Additional user-defined data passed to the callback function. |
| `jn_filter` | `json_t *` | Filter criteria to select specific messages. The caller owns this parameter. |

**Returns**

Returns 0 on success, a negative value if the iteration was interrupted by the callback, or an error code on failure.

**Notes**

The callback function should return a negative value to break the iteration. The `instances` parameter passed to the callback is owned by the caller and must be managed accordingly.

---

(trmsg_foreach_messages)=
## `trmsg_foreach_messages()`

`trmsg_foreach_messages()` iterates over all messages in the given `list`, invoking the provided callback function for each message. The callback receives either duplicated or cloned message instances based on the `duplicated` flag.

```C
int trmsg_foreach_messages(
    json_t *list,
    BOOL duplicated,
    int (*callback)(
        json_t *list,
        const char *key,
        json_t *instances,
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing messages to iterate over. |
| `duplicated` | `BOOL` | If `true`, the callback receives duplicated messages; otherwise, it receives cloned messages. |
| `callback` | `int (*)(json_t *, const char *, json_t *, void *, void *)` | A function pointer to the callback that processes each message. It should return a negative value to break the iteration. |
| `user_data1` | `void *` | User-defined data passed to the callback function. |
| `user_data2` | `void *` | Additional user-defined data passed to the callback function. |
| `jn_filter` | `json_t *` | A JSON object specifying filtering criteria for selecting messages. The caller owns this parameter. |

**Returns**

Returns `0` on success, a negative value if the iteration was interrupted by the callback, or a positive error code on failure.

**Notes**

The callback function receives a reference to the `list`, the message `key`, and the `instances` JSON object. The `instances` parameter must be owned by the callback function. This function is similar to [`trmsg_foreach_active_messages()`](<#trmsg_foreach_active_messages>) and [`trmsg_foreach_instances_messages()`](<#trmsg_foreach_instances_messages>), but it allows selecting between duplicated and cloned messages.

---

(trmsg_get_active_md)=
## `trmsg_get_active_md()`

`trmsg_get_active_md()` retrieves the metadata of the active message associated with the specified key from the given list.

```C
json_t *trmsg_get_active_md(
    json_t      *list,
    const char  *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing messages. |
| `key` | `const char *` | The key identifying the message whose active metadata is to be retrieved. |

**Returns**

A pointer to a `json_t` dictionary containing the active message's metadata. The returned value is not owned by the caller.

**Notes**

The returned metadata is part of the internal data structure and should not be modified or freed by the caller.

---

(trmsg_get_active_message)=
## `trmsg_get_active_message()`

`trmsg_get_active_message()` retrieves the active message associated with the specified key from the given list.

```C
json_t *trmsg_get_active_message(
    json_t      *list,
    const char  *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | A JSON object representing the list of messages. |
| `key` | `const char *` | The key identifying the message to retrieve. |

**Returns**

A JSON object representing the active message associated with the given key. The returned object is not owned by the caller.

**Notes**

The returned JSON object should not be modified or freed by the caller. To modify the data, create a copy before making changes.

---

(trmsg_get_instances)=
## `trmsg_get_instances()`

`trmsg_get_instances()` retrieves the list of instances associated with a specific message key from the given list.

```C
json_t *trmsg_get_instances(
    json_t      *list,
    const char  *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing messages, from which instances will be retrieved. |
| `key` | `const char *` | The key identifying the message whose instances are to be retrieved. |

**Returns**

A list of instances (`json_t *`) associated with the specified message key. The returned value is not owned by the caller and must not be modified or freed.

**Notes**

The returned list corresponds to the `messages[message_key].instances` structure in the data model.

---

(trmsg_get_message)=
## `trmsg_get_message()`

Retrieves a message from the given `list` by its `key`. The returned JSON object contains both the `active` message and its `instances`.

```C
json_t *trmsg_get_message(
    json_t      *list,
    const char  *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing the messages. |
| `key` | `const char *` | The key identifying the message to retrieve. |

**Returns**

A JSON object containing the `active` message and its `instances`. The returned object is not owned by the caller.

**Notes**

The returned JSON object should not be modified or freed by the caller.

---

(trmsg_get_messages)=
## `trmsg_get_messages()`

`trmsg_get_messages()` retrieves the dictionary of messages from the specified list.

```C
json_t *trmsg_get_messages(
    json_t *list
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing the messages. |

**Returns**

A dictionary of messages. The returned value is not owned by the caller.

**Notes**

The returned dictionary should not be modified or freed by the caller.

---

(trmsg_open_list)=
## `trmsg_open_list()`

`trmsg_open_list()` opens a list of messages from a specified topic in the TimeRanger database, applying optional filtering conditions and extra parameters.

```C
json_t *trmsg_open_list(
    json_t       *tranger,
    const char   *topic_name,
    json_t       *match_cond, // owned
    json_t       *extra,      // owned
    const char   *rt_id,
    BOOL          rt_by_disk,
    const char   *creator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `topic_name` | `const char *` | Name of the topic from which to open the list. |
| `match_cond` | `json_t *` | JSON object specifying filtering conditions for messages. Owned by the function. |
| `extra` | `json_t *` | Additional parameters for list configuration. Owned by the function. |
| `rt_id` | `const char *` | Real-time identifier for the list. |
| `rt_by_disk` | `BOOL` | If `true`, the list is opened from disk instead of memory. |
| `creator` | `const char *` | Identifier of the entity creating the list. |

**Returns**

Returns a JSON object representing the opened list. The caller is responsible for managing its lifecycle.

**Notes**

`trmsg_open_list()` internally uses [`tranger2_open_list()`](<#tranger2_open_list>) to perform the operation. If `rt_by_disk` is set to `true`, the list is loaded from disk, which may introduce delays in application startup.

---

(trmsg_open_topics)=
## `trmsg_open_topics()`

`trmsg_open_topics()` initializes and opens topics for message handling using TimeRanger, ensuring that the necessary structures are available for message storage and retrieval.

```C
int trmsg_open_topics(
    json_t            *tranger,
    const topic_desc_t *descs
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A JSON object representing the TimeRanger instance where topics will be opened. |
| `descs` | `const topic_desc_t *` | An array of topic descriptors defining the structure and properties of the topics to be opened. |

**Returns**

Returns `0` on success, or a negative error code if the operation fails.

**Notes**

Ensure that `tranger2_startup()` has been called before invoking [`trmsg_open_topics()`](<#trmsg_open_topics>) to properly initialize the TimeRanger environment.

---

(trmsg_record_instances)=
## `trmsg_record_instances()`

`trmsg_record_instances()` retrieves a list of cloned instances associated with a specific message key from the given list.

```C
json_t *trmsg_record_instances(
    json_t      *list,
    const char  *key,
    json_t      *jn_filter  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `list` | `json_t *` | The list containing the messages. |
| `key` | `const char *` | The key identifying the message whose instances are to be retrieved. |
| `jn_filter` | `json_t *` | A JSON object specifying filtering criteria for the instances. The caller owns this object. |

**Returns**

A JSON list of cloned instances matching the specified key and filter. The caller is responsible for decrementing the reference count of the returned object.

**Notes**

The returned list is a deep copy of the instances, ensuring that modifications do not affect the original data. The caller must manage the memory of the returned object by calling `json_decref()` when it is no longer needed.

---
