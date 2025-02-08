# `trq_clear_retries()`

**Description:**
Clears the retry count for a message.

**Prototype:**
```c
PUBLIC int trq_clear_retries(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.

**Returns:**
- `int`: `0` on success, `-1` on error.
