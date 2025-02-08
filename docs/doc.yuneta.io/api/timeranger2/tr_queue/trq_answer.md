# `trq_answer()`

**Description:**
Sends an answer related to a queue message.

**Prototype:**
```c
PUBLIC int trq_answer(tr_queue trq_, q_msg *msg, json_t *response);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `msg` (`q_msg *`): The message being answered.
- `response` (`json_t *`): The JSON response to send.

**Returns:**
- `int`: `0` on success, `-1` on failure.
