# `trmsg_foreach_active_messages()`

**Prototype:**
```c
PUBLIC int trmsg_foreach_active_messages(json_t *list,
    int (*callback));
```

**Description:**
Iterates over all active messages in a list and applies a callback function.

**Parameters:**
- `list` (`json_t *`): The list of active messages.
- `callback` (`int (*callback)()`): Function pointer applied to each message.

**Returns:**
- `int`: Status code of the operation.
