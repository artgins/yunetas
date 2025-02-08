# `trq_size_by_key()`

**Description:**
Returns the number of messages associated with a specific key in the queue.

**Prototype:**
```c
PUBLIC size_t trq_size_by_key(tr_queue trq_, const char *key);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `key` (`const char *`): The key to filter messages.

**Returns:**
- `size_t`: The number of messages associated with the key.
