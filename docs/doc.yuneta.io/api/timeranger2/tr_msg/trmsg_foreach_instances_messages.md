# `trmsg_foreach_instances_messages()`

**Prototype:**
```c
PUBLIC int trmsg_foreach_instances_messages(json_t *list,
    int (*callback));
```

**Description:**
Iterates over all instance messages in a list and applies a callback function.

**Parameters:**
- `list` (`json_t *`): The list of instance messages.
- `callback` (`int (*callback)()`): Function pointer applied to each instance message.

**Returns:**
- `int`: Status code of the operation.

---
