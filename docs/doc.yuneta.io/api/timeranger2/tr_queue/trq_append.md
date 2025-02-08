# `trq_append()`

**Description:**
Appends a new message to the queue.

**Prototype:**
```c
PUBLIC q_msg trq_append(
    tr_queue trq_,
    json_t *jn_msg  // owned
);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `jn_msg` (`json_t *`): The message to append (owned).

**Returns:**
- `q_msg`: The appended message structure.

---
