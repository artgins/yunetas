# `msg2db_open_db()`

**Description:**
Opens a message database (`msg2db`) within the given `tranger` database.

**Prototype:**
```c
PUBLIC json_t *msg2db_open_db(
    json_t *tranger,
    const char *msg2db_name_,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
);
```

**Parameters:**
- `tranger` (`json_t *`): The transaction database.
- `msg2db_name_` (`const char *`): Name of the message database.
- `jn_schema` (`json_t *`): Schema definition (owned).
- `options` (`const char *`): Options (e.g., "persistent").

**Return Value:**
- Returns a `json_t *` representing the opened database.

---
