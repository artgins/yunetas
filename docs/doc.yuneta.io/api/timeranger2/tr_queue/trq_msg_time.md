# `trq_msg_time()`

**Description:**
Retrieves the timestamp of when the message was added to the queue.

**Prototype:**
```c
PUBLIC time_t trq_msg_time(q_msg *msg);
```

**Parameters:**
- `msg` (`q_msg *`): The message instance.

**Returns:**
- `time_t`: The timestamp of the message.
