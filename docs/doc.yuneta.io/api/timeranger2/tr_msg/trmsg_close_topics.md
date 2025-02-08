# `trmsg_close_topics()`

**Prototype:**
```c
PUBLIC int trmsg_close_topics(json_t *tranger,
    const topic_desc_t *descs);
```

**Description:**
Closes topics in the TRanger instance based on the provided descriptions.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `descs` (`const topic_desc_t *`): The topic descriptions to close.

**Returns:**
- `int`: Status code of the operation.

---
