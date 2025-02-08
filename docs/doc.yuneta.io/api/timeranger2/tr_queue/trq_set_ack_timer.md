# `trq_set_ack_timer()`

**Description:**
Sets an acknowledgment timer for a message in the queue.

**Prototype:**
```c
PUBLIC int trq_set_ack_timer(tr_queue trq_, uint64_t rowid, time_t timeout);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.
- `timeout` (`time_t`): The acknowledgment timeout value.

**Returns:**
- `int`: `0` on success, `-1` on error.
