# `trq_clear_ack_timer()`

**Description:**
Clears the acknowledgment timer for a specific message in the queue.

**Prototype:**
```c
PUBLIC int trq_clear_ack_timer(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.

**Returns:**
- `int`: `0` on success, `-1` on error.
