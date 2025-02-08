# `trmsg_open_list()`

**Prototype:**
```c
PUBLIC json_t *trmsg_open_list(json_t *tranger,
    const char *topic_name);
```

**Description:**
Opens a message list for a given topic in the TRanger instance.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic for which to open the list.

**Returns:**
- `json_t *`: JSON list object.
