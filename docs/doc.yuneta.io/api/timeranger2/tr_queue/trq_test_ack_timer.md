# `trq_test_ack_timer()`

**Description:**
Tests whether the acknowledgment timer has expired for a specific message.

**Prototype:**
```c
PUBLIC bool trq_test_ack_timer(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.

**Returns:**
- `bool`: `true` if the timer has expired, otherwise `false`.

---
