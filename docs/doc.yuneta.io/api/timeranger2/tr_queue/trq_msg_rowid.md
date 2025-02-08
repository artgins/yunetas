# `trq_msg_rowid()`

**Description:**
Gets the row ID of a message in the queue.

**Prototype:**
```c
PUBLIC uint64_t trq_msg_rowid(q_msg *msg);
```

**Parameters:**
- `msg` (`q_msg *`): The message instance.

**Returns:**
- `uint64_t`: The row ID of the message.
