# `trmsg_add_instance()`

**Prototype:**
```c
PUBLIC int trmsg_add_instance(json_t *tranger,
    const char *topic_name,
    json_t *jn_msg,  // owned
    md2_record_ex_t *md_record);
```

**Description:**
Adds an instance message to a specific topic in the TRanger.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The name of the topic to add the instance to.
- `jn_msg` (`json_t *`): The message to be added (owned by the function).
- `md_record` (`md2_record_ex_t *`): Metadata record associated with the message.

**Returns:**
- `int`: Status code of the operation.
