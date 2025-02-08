# `trq_last_msg()`

**Description:**
Retrieves the last message in the queue.

**Prototype:**
```c
PUBLIC json_t * trq_last_msg(tr_queue trq_);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.

**Returns:**
- `json_t *`: The last message in the queue.
