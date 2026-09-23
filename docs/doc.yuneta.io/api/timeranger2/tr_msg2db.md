# tr_msg2db

Dict-style message store: one value per key, backed by timeranger2. Updates are appended. Reads return the latest value.

Source code:

- [`tr_msg2db.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.h)
- [`tr_msg2db.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c)

(build_msg2db_index_path)=
## [`build_msg2db_index_path()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c#L62)

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

Make sure that `bf` has sufficient space (`bfsize`) to store the generated path to prevent buffer overflows.

---

(msg2db_append_message)=
## [`msg2db_append_message()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c#L1052)

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

The caller must not modify or free the returned JSON object.
Make sure that [`msg2db_open_db()`](<#msg2db_open_db>) was called before using [`msg2db_append_message()`](<#msg2db_append_message>).

---

(msg2db_close_db)=
## [`msg2db_close_db()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c#L438)

`msg2db_close_db()` closes an open message database identified by `msg2db_name`. This makes sure that all resources associated with it are properly released.

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

Make sure that [`msg2db_close_db()`](<#msg2db_close_db>) is called after all operations on the database are complete to prevent resource leaks.

---

(msg2db_get_message)=
## [`msg2db_get_message()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c#L1321)

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
If the message is not found, the function can return `NULL`.
The function relies on the structure and indexing of the database, which must be properly initialized using [`msg2db_open_db()`](<#msg2db_open_db>).

A message it answers is the current one. A `NULL` for an id whose history did
not load whole at the open means UNKNOWN, not "there is none": ask
[`msg2db_id_incomplete()`](<#msg2db_id_incomplete>) when the difference
matters.

---

(msg2db_id_incomplete)=
## [`msg2db_id_incomplete()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c#L1502)

`msg2db_id_incomplete()` tells whether the history of `id` loaded whole when
the msg2db was opened. When it did not, the messages served of it are current,
but a `pkey2` whose newest message could not be read is absent (see
[`msg2db_open_db()`](<#msg2db_open_db>)).

```C
BOOL msg2db_id_incomplete(
    json_t      *tranger,
    const char  *msg2db_name,
    const char  *topic_name,
    const char  *id
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The TimeRanger instance of the msg2db. |
| `msg2db_name` | `const char *` | Name of the message database. |
| `topic_name` | `const char *` | Name of the topic. |
| `id` | `const char *` | The primary key (the `id` of the messages). |

**Returns**

`TRUE` when the history of `id` did not load whole at the open. `FALSE` when
it did, when `id` has no messages, or when it is empty. An unknown msg2db or
topic logs an error and answers `FALSE`.

**Notes**

It stays `TRUE` while the msg2db is open, whatever messages arrive: the
damaged file is still on disk, and other `pkey2` of the id may still be
unknown. A restart after the repair clears it.

A consumer that must not take "unknown" for "none" asks it when
[`msg2db_get_message()`](<#msg2db_get_message>) answers `NULL`. For example,
an alarm check that does not announce as NEW an alarm whose earlier state is
unknown, and records its state instead:

```C
json_t *alarm = msg2db_get_message(tranger, "msg2db_alarms", "alarms", device_id, alarm_id);
BOOL unknown = (!alarm && msg2db_id_incomplete(tranger, "msg2db_alarms", "alarms", device_id));
if(unknown) {
    /*
     *  The newest message of this alarm could not be read: record the state
     *  the device reports now (active or not), and say it is a state
     *  recovered after a damaged store, not a change
     */
}
```

---

(msg2db_list_messages)=
## [`msg2db_list_messages()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c#L1240)

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
## [`msg2db_open_db()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/timeranger2/src/tr_msg2db.c#L93)

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
To modify the schema after it was saved, the schema version and topic version must be updated.

The load is forward, oldest first, and msg2db keeps per `id` and `pkey2` the
LAST message it loaded.

**An id whose history does not load whole** (a md2 file of it that cannot be
opened or read, see [`tranger2_open_list()`](<timeranger2.md#tranger2_open_list>)). The
forward load stops before the damage, so the last message it read of a
`pkey2` may be an OLD one. msg2db does not serve that: it loads the id again
BACKWARD, newest first, and keeps the FIRST message of each `pkey2`. That load
stops at the damage too, from the other side. So:

- a `pkey2` whose newest message is NEWER than the damage is served, and it is
  exactly its current message;
- a `pkey2` whose newest message is in the damage (or before it) is ABSENT. Its
  state is unknown: an older message of it may be readable, and it is not
  served as current.

The id is marked incomplete ([`msg2db_id_incomplete()`](<#msg2db_id_incomplete>)
answers `TRUE` while the msg2db is open), and an ERROR names it, with the
number of `pkey2` served:

```text
ERROR: {..., "function": "msg2db_open_db", "msgset": "Msg2Db",
    "msg": "msg2db: a key whose history did not load whole: only the messages newer than the damage are served, a pkey2 whose newest message was not read is ABSENT and its state unknown (msg2db_id_incomplete)",
    "msg2db_name": "msg2db_alarms", "topic_name": "alarms", "key": "dev1", "served": 1}
```

```C
/*
 *  dev1: X old (file 1), Y old (file 1), Y newer (file 2, damaged),
 *        X new (file 3). dev2: whole.
 */
msg2db_open_db(tranger, "msg2db_alarms", jn_schema, "");
msg2db_get_message(tranger, "msg2db_alarms", "alarms", "dev1", "X");   // X new: current
msg2db_get_message(tranger, "msg2db_alarms", "alarms", "dev1", "Y");   // NULL: unknown
msg2db_id_incomplete(tranger, "msg2db_alarms", "alarms", "dev1");      // TRUE
msg2db_get_message(tranger, "msg2db_alarms", "alarms", "dev2", "X");   // served
msg2db_id_incomplete(tranger, "msg2db_alarms", "alarms", "dev2");      // FALSE
```

**A md2 whose last row is torn is NOT damage.** Its size is not a whole
number of 32-byte rows: a power cut came during the write of a row, so that
append was never acknowledged. A master tranger cuts the md2 back to its
whole rows at the open, with one WARNING, and the id loads whole: nothing
is absent, the id is not incomplete, and its next message is stored and
served as usual. In the unreleased work after 7.25.4 a torn row was taken
for damage, and every new message of the id was refused as described below.

```text
WARNING: {..., "function": "load_first_and_last_record_md", "msgset": "Tranger",
    "msg": "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back",
    "topic": "alarms", "key": "dev1", "file_id": "2026",
    "path": "<store>/alarms/keys/dev1/2026.md2", "old_size": 45, "new_size": 32}
```

```C
/*  dev1: OLD (2000-01-01), NEW (the current file, its md2 has 32 + 13 bytes) */
msg2db_open_db(tranger, "msg2db_alarms", jn_schema, "");  // one WARNING, md2 cut to 32
msg2db_get_message(tranger, "msg2db_alarms", "alarms", "dev1", "X");   // NEW
msg2db_id_incomplete(tranger, "msg2db_alarms", "alarms", "dev1");      // FALSE
msg2db_append_message(tranger, "msg2db_alarms", "alarms", jn_next, ""); // stored, served
```

**The next message of the id** goes to the file of the current period:
[`msg2db_append_message()`](<#msg2db_append_message>) appends it with the time
of now. What happens to it depends on which file is damaged:

- the damaged file is an OLDER file: the next message of a `pkey2` is stored,
  and it is served as it arrives. It is current.
- the damaged file IS the file of the current period: tranger refuses every
  append into a file it could not open or read
  (*"Cannot append record, its file is flagged unreadable: its row would follow rows no cell counts"*),
  and `msg2db_append_message()` returns `NULL`. So NO new message of the id is
  stored or served, for every `pkey2` of it, the absent ones and the served
  ones. This continues until the file is repaired, or until the period
  changes (the first message of the next period goes to a new file, and it
  is stored and served).

In the second case msg2db logs a second ERROR at the open:

```text
ERROR: {..., "function": "msg2db_open_db", "msgset": "Msg2Db",
    "msg": "msg2db: the damaged file of the key is the file of the current period: every new message of the key is REFUSED until the file is repaired or the period changes",
    "msg2db_name": "msg2db_alarms", "topic_name": "alarms", "key": "dev1", "file_id": "2026"}
```

The id stays incomplete in both cases: the damaged file is still on disk, and
other `pkey2` of it may still be unknown.

**What it means for the alarms of `db_history`** (the msg2db consumers in the
projects compare the triggers of a new measure with the `triggers` of
`msg2db_get_message(<device>, <alarm>)`). For an alarm whose newest message
was read, nothing changes. For an absent one, until its next message:

- the device reports the alarm's triggers: the alarm is announced as NEW. If
  it was already active in the message that could not be read, that is a
  repeated notification;
- the device reports no trigger: nothing is recorded (the `!n_new && !n_old`
  path). If the alarm was active in the message that could not be read, its
  end is not announced;
- the alarm is not in [`msg2db_list_messages()`](<#msg2db_list_messages>), so
  the lists of alarms do not show it.

**When the damaged file is the file of the current period**, all of the above
is worse: every new alarm message of that device is REFUSED, whatever the
alarm. No new alarm message of the device is recorded, and no alarm of it is
announced as new or as ended. The msg2db of the alarms uses the tranger of the
project's treedb, whose `filename_mask` is `"%Y"`: one file per device and
YEAR. So the damaged file is usually the current one, and the refusal
continues until the file is repaired or the year changes. Repair it at once,
with the yuno stopped
([what the operator does](<treedb.md#treedb-topic-not-loaded-whole>)), then
start the yuno. This is for real damage only (a md2 that cannot be opened or
read). A torn last row does not refuse anything: the master cuts it back at
the open, as said above.

Up to 7.25.4 the forward load served the OLD message as current, with nothing
logged: a cleared alarm could come back active, or an active one be taken as
cleared. Now only the alarms whose state is really unknown are absent.
Repair the key as the treedb page says
([what the operator does](<treedb.md#treedb-topic-not-loaded-whole>)); after
the repair and a restart, the id is whole and not marked.

---
