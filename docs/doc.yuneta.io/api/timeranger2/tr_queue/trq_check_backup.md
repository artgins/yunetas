# `trq_check_backup()`

**Description:**
Checks if there are backup messages in the queue.

**Prototype:**
```c
PUBLIC bool trq_check_backup(tr_queue trq_);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.

**Returns:**
- `bool`: `true` if backup messages exist, `false` otherwise.
