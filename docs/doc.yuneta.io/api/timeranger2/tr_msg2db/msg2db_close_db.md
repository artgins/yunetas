# `msg2db_close_db()`

**Description:**
Closes the specified `msg2db` database within `tranger`.

**Prototype:**
```c
PUBLIC int msg2db_close_db(
    json_t *tranger,
    const char *msg2db_name
);
```

**Parameters:**
- `tranger` (`json_t *`): The transaction database.
- `msg2db_name` (`const char *`): Name of the message database to close.

**Return Value:**
- Returns `0` on success, a negative value on error.
