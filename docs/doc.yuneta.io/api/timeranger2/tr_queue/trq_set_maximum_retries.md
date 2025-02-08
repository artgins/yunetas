# `trq_set_maximum_retries()`

**Description:**
Sets the maximum number of retry attempts for a message.

**Prototype:**
```c
PUBLIC int trq_set_maximum_retries(tr_queue trq_, uint64_t rowid, int max_retries);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.
- `max_retries` (`int`): The maximum number of retry attempts.

**Returns:**
- `int`: `0` on success, `-1` on error.
