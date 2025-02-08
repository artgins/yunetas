# `trmsg_get_messages()`

**Prototype:**
```c
PUBLIC json_t *trmsg_get_messages(json_t *tranger,
    const char *topic_name,
    json_t *jn_filter);
```

**Description:**
Retrieves messages from a topic in the TRanger instance based on a filter.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic from which to retrieve messages.
- `jn_filter` (`json_t *`): JSON filter criteria.

**Returns:**
- `json_t *`: JSON array of messages.
