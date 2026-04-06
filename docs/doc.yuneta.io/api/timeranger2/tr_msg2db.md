# tr_msg2db

Dict-style message store: one value per key, backed by timeranger2. Updates are appended; reads return the latest value.

Source code:

- [`tr_msg2db.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_msg2db.h)
- [`tr_msg2db.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/tr_msg2db.c)

(build_msg2db_index_path)=
## `build_msg2db_index_path()`

`build_msg2db_index_path()` constructs a file system path for a message database index using the provided database name, topic name, and key.

```C
char *build_msg2db_index_path(
    char       *bf,
    int         bfsize,
    const char *msg2db_name,
    const char *topic_name,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the constructed path. |
| `bfsize` | `int` | Size of the buffer `bf` in bytes. |
| `msg2db_name` | `const char *` | Name of the message database. |
| `topic_name` | `const char *` | Name of the topic within the message database. |
| `key` | `const char *` | Key used to generate the index path. |

**Returns**

Returns a pointer to `bf` containing the constructed index path.

**Notes**

Ensure that `bf` has sufficient space (`bfsize`) to store the generated path to prevent buffer overflows.

---

(msg2db_append_message)=
## `msg2db_append_message()`

`msg2db_append_message()` appends a new message to the specified topic in the given message database.

```C
json_t *msg2db_append_message(
    json_t      *tranger,
    const char  *msg2db_name,
    const char  *topic_name,
    json_t      *kw,        // owned
    const char  *options    // "permissive"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `msg2db_name` | `const char *` | The name of the message database. |
| `topic_name` | `const char *` | The name of the topic to which the message will be appended. |
| `kw` | `json_t *` | A JSON object containing the message data. This parameter is owned and will be managed internally. |
| `options` | `const char *` | Optional flags for message insertion. The value "permissive" allows flexible insertion rules. |

**Returns**

A JSON object representing the appended message. The returned object is NOT owned by the caller.

**Notes**

The caller should not modify or free the returned JSON object.
Ensure that [`msg2db_open_db()`](<#msg2db_open_db>) has been called before using [`msg2db_append_message()`](<#msg2db_append_message>).

---

(msg2db_close_db)=
## `msg2db_close_db()`

`msg2db_close_db()` closes an open message database identified by `msg2db_name`, ensuring that all resources associated with it are properly released.

```C
int msg2db_close_db(
    json_t  *tranger,
    const char *msg2db_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger database instance. |
| `msg2db_name` | `const char *` | The name of the message database to close. |

**Returns**

Returns `0` on success, or a negative value if an error occurs.

**Notes**

Ensure that [`msg2db_close_db()`](<#msg2db_close_db>) is called after all operations on the database are complete to prevent resource leaks.

---

(msg2db_get_message)=
## `msg2db_get_message()`

`msg2db_get_message()` retrieves a message from the specified database and topic using the given primary and secondary keys.

```C
json_t *msg2db_get_message(
    json_t      *tranger,
    const char  *msg2db_name,
    const char  *topic_name,
    const char  *id,
    const char  *id2
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `msg2db_name` | `const char *` | Name of the message database. |
| `topic_name` | `const char *` | Name of the topic within the database. |
| `id` | `const char *` | Primary key of the message to retrieve. |
| `id2` | `const char *` | Secondary key of the message to retrieve. |

**Returns**

A JSON object containing the requested message. The returned object is not owned by the caller and must not be modified or freed.

**Notes**

The function returns a reference to an internal JSON object, so the caller must not modify or free it.
If the message is not found, the function may return `NULL`.
The function relies on the structure and indexing of the database, which must be properly initialized using [`msg2db_open_db()`](<#msg2db_open_db>).

---

(msg2db_list_messages)=
## `msg2db_list_messages()`

`msg2db_list_messages()` retrieves a list of messages from the specified database and topic, filtered by the given criteria. It supports optional filtering and a custom matching function.

```C
json_t *msg2db_list_messages(
    json_t  *tranger,
    const char  *msg2db_name,
    const char  *topic_name,
    json_t  *jn_ids,     // owned
    json_t  *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t  *kw,         // not owned
        json_t  *jn_filter   // owned
    )
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | Pointer to the TimeRanger database instance. |
| `msg2db_name` | `const char *` | Name of the message database to query. |
| `topic_name` | `const char *` | Name of the topic from which messages are retrieved. |
| `jn_ids` | `json_t *` | JSON array of message IDs to retrieve. Owned by the caller. |
| `jn_filter` | `json_t *` | JSON object containing filter criteria. Owned by the caller. |
| `match_fn` | `BOOL (*)(json_t *, json_t *)` | Optional function pointer for custom message filtering. The first parameter is the message (not owned), and the second is the filter criteria (owned). |

**Returns**

A JSON array of messages matching the criteria. The caller must decrement the reference count when done.

**Notes**

The returned JSON array must be decremented using `json_decref()` to avoid memory leaks.
If `match_fn` is provided, it will be used to further filter messages beyond `jn_filter`.
This function is useful for retrieving messages based on specific IDs or filtering conditions.

---

(msg2db_open_db)=
## `msg2db_open_db()`

`msg2db_open_db()` initializes and opens a message database using TimeRanger, loading its schema and configuration. The function supports persistence by loading the schema from a file if the 'persistent' option is enabled.

```C
json_t *msg2db_open_db(
    json_t      *tranger,
    const char  *msg2db_name,
    json_t      *jn_schema,  // owned
    const char  *options     // "persistent"
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | A reference to the TimeRanger instance managing the database. |
| `msg2db_name` | `const char *` | The name of the message database to open. |
| `jn_schema` | `json_t *` | A JSON object defining the schema of the database. Ownership is transferred to [`msg2db_open_db()`](<#msg2db_open_db>). |
| `options` | `const char *` | Optional settings, such as "persistent" to load the schema from a file. |

**Returns**

A JSON object representing the opened message database, or `NULL` on failure.

**Notes**

The function [`tranger2_startup()`](<#tranger2_startup>) must be called before invoking [`msg2db_open_db()`](<#msg2db_open_db>).
If the 'persistent' option is enabled, the schema is loaded from a file, which takes precedence over any provided schema.
To modify the schema after it has been saved, the schema version and topic version must be updated.

---
