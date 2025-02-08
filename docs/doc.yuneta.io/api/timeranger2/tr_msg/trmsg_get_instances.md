# `trmsg_get_instances()`

**Prototype:**
```c
PUBLIC json_t *trmsg_get_instances(json_t *tranger,
    const char *topic_name);
```

**Description:**
Retrieves all instance messages from a specific topic in the TRanger.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic from which to retrieve instances.

**Returns:**
- `json_t *`: JSON array of instance messages.

---
