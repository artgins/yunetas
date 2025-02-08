# `trmsg_get_message()`

**Prototype:**
```c
PUBLIC json_t *trmsg_get_message(json_t *tranger,
    const char *topic_name,
    json_t *jn_msg_id);
```

**Description:**
Retrieves a specific message from a topic using its unique ID.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic containing the message.
- `jn_msg_id` (`json_t *`): JSON object containing the message ID.

**Returns:**
- `json_t *`: JSON object of the retrieved message.
