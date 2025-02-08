# `yev_create_read_event()`

**Description:**
Creates a new event for handling read operations on a socket.

**Parameters:**
- `int fd` - The socket file descriptor.
- `void (*callback)(void *)` - Function pointer for handling read events.
- `void *user_data` - Optional user-defined data.

**Return Value:**
- Returns a pointer to the created event on success, or `NULL` on failure.
