# `trq_set_soft_mark()`

**Description:**
Sets a soft mark for a message in the queue.

**Prototype:**
```c
PUBLIC int trq_set_soft_mark(tr_queue trq_, uint64_t rowid, uint32_t mark);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.
- `mark` (`uint32_t`): The mark to set.

**Returns:**
- `int`: `0` on success, `-1` on error.

---
