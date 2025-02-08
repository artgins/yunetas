# `yev_create_accept_event()`

**Description:**
Creates a new event for accepting incoming connections on a socket.

**Parameters:**
- `int fd` - The socket file descriptor to listen for connections.
- `void (*callback)(void *)` - Function pointer for handling accepted connections.
- `void *user_data` - Optional user-defined data.

**Return Value:**
- Returns a pointer to the created event on success, or `NULL` on failure.

---
