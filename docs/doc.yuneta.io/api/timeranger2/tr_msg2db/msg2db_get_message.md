# `msg2db_get_message()`

**Description:**
Retrieves a specific message from a topic within `msg2db`.

**Prototype:**
```c
PUBLIC json_t *msg2db_get_message(
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
);
```

**Parameters:**
- `tranger` (`json_t *`): The transaction database.
- `msg2db_name` (`const char *`): Name of the message database.
- `topic_name` (`const char *`): Topic containing the message.
- `id` (`const char *`): Primary message ID.
- `id2` (`const char *`): Secondary message ID.

**Return Value:**
- Returns a `json_t *` representing the retrieved message (not owned).

---
