# `trmsg_get_active_message()`

**Prototype:**
```c
PUBLIC json_t *trmsg_get_active_message(json_t *tranger,
    const char *topic_name);
```

**Description:**
Retrieves the currently active message from a given topic.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic containing the active message.

**Returns:**
- `json_t *`: JSON object of the active message.
