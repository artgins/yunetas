# `trq_test_retries()`

**Description:**
Tests if a message has exceeded the maximum retry count.

**Prototype:**
```c
PUBLIC bool trq_test_retries(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.

**Returns:**
- `bool`: `true` if retries have exceeded the limit, otherwise `false`.
