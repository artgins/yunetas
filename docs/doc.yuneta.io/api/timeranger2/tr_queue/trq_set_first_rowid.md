# `trq_set_first_rowid()`

**Description:**
Sets the first row ID to be used for searching messages in the queue.

**Prototype:**
```c
PUBLIC void trq_set_first_rowid(tr_queue trq_, uint64_t first_rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `first_rowid` (`uint64_t`): The first row ID to set.
