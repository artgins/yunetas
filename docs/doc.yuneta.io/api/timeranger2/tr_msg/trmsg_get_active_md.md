# `trmsg_get_active_md()`

**Prototype:**
```c
PUBLIC json_t *trmsg_get_active_md(json_t *tranger,
    const char *topic_name);
```

**Description:**
Retrieves metadata of the currently active message from a given topic.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic containing the active message metadata.

**Returns:**
- `json_t *`: JSON object containing metadata of the active message.
