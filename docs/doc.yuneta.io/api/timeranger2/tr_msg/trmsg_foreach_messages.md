# `trmsg_foreach_messages()`

**Prototype:**
```c
PUBLIC int trmsg_foreach_messages(json_t *list,
    BOOL duplicated, // False then cloned messages
    int (*callback));
```

**Description:**
Iterates over all messages in a list, optionally duplicating messages, and applies a callback function.

**Parameters:**
- `list` (`json_t *`): The list of messages.
- `duplicated` (`BOOL`): Whether to duplicate messages (false clones messages instead).
- `callback` (`int (*callback)()`): Function pointer applied to each message.

**Returns:**
- `int`: Status code of the operation.
