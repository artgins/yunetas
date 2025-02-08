# `msg2db_append_message()`

**Description:**
Appends a message to a topic within the `msg2db` database.

**Prototype:**
```c
PUBLIC json_t *msg2db_append_message(
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *kw,    // owned
    const char *options // "permissive"
);
```

**Parameters:**
- `tranger` (`json_t *`): The transaction database.
- `msg2db_name` (`const char *`): Name of the message database.
- `topic_name` (`const char *`): Topic to append the message.
- `kw` (`json_t *`): The message content (owned).
- `options` (`const char *`): Options (e.g., "permissive").

**Return Value:**
- Returns a `json_t *` representing the appended message (not owned).

---
