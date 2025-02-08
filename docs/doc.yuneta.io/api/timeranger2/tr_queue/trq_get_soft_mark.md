# `trq_get_soft_mark()`

**Description:**
Retrieves the soft mark for a specific message in the queue.

**Prototype:**
```c
PUBLIC uint32_t trq_get_soft_mark(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.

**Returns:**
- `uint32_t`: The soft mark of the message.

---
