# `trq_check_pending_rowid()`

**Description:**
Checks if there is a pending message with a specific row ID.

**Prototype:**
```c
PUBLIC bool trq_check_pending_rowid(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID to check.

**Returns:**
- `bool`: `true` if a pending message exists, otherwise `false`.
