# `trq_prev_msg()`

**Description:**
Retrieves the previous message in the queue.

**Prototype:**
```c
PUBLIC q_msg *trq_prev_msg(tr_queue trq_, q_msg *msg);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `msg` (`q_msg *`): The current message.

**Returns:**
- `q_msg *`: The previous message in the queue.

---
