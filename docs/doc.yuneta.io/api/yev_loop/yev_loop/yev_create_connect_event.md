# `yev_create_connect_event()`

**Description:**
Creates a new event for handling outgoing connections on a socket.

**Parameters:**
- `int fd` - The socket file descriptor for initiating a connection.
- `void (*callback)(void *)` - Function pointer for handling the connection event.
- `void *user_data` - Optional user-defined data.

**Return Value:**
- Returns a pointer to the created event on success, or `NULL` on failure.
