# `trq_get_by_key()`

**Description:**
Retrieves a message from the queue by its key.

**Prototype:**
```c
PUBLIC json_t * trq_get_by_key(tr_queue trq_, const char *key);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `key` (`const char *`): The key associated with the message.

**Returns:**
- `json_t *`: The message corresponding to the given key.

---
