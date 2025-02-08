# `msg2db_list_messages()`

**Description:**
Lists messages from a topic in the `msg2db` database, optionally filtered.

**Prototype:**
```c
PUBLIC json_t *msg2db_list_messages(
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,        // not owned
        json_t *jn_filter  // owned
    )
);
```

**Parameters:**
- `tranger` (`json_t *`): The transaction database.
- `msg2db_name` (`const char *`): Name of the message database.
- `topic_name` (`const char *`): Topic to list messages from.
- `jn_ids` (`json_t *`): Message IDs to list (owned).
- `jn_filter` (`json_t *`): Filtering criteria (owned).
- `match_fn` (`BOOL (*match_fn) (json_t *, json_t *)`): Callback function for filtering.

**Return Value:**
- Returns a `json_t *` with the list of messages (must be decref).
