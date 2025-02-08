# `trmsg_record_instances()`

**Prototype:**
```c
PUBLIC int trmsg_record_instances(json_t *tranger,
    const char *topic_name,
    json_t *jn_instances);
```

**Description:**
Records instances into a specific topic in the TRanger.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic where instances should be recorded.
- `jn_instances` (`json_t *`): JSON object containing instances to be recorded.

**Returns:**
- `int`: Status code of the operation.

---
