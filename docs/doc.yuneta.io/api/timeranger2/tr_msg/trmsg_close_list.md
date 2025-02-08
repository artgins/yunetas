# `trmsg_close_list()`

**Prototype:**
```c
PUBLIC int trmsg_close_list(json_t *tranger,
    json_t *list);
```

**Description:**
Closes a message list in the TRanger instance.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `list` (`json_t *`): The list to be closed.

**Returns:**
- `int`: Status code of the operation.
