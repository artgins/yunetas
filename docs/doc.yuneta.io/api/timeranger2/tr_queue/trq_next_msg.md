# `trq_next_msg()`

**Description:**
Retrieves the next message in the queue after the specified row ID.

**Prototype:**
```c
PUBLIC json_t * trq_next_msg(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID to start from.

**Returns:**
- `json_t *`: The next message in the queue.
