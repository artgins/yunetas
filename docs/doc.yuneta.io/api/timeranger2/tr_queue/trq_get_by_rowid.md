# `trq_get_by_rowid()`

**Description:**
Retrieves a message from the queue by its row ID.

**Prototype:**
```c
PUBLIC json_t * trq_get_by_rowid(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message to retrieve.

**Returns:**
- `json_t *`: The message corresponding to the given row ID.

---
