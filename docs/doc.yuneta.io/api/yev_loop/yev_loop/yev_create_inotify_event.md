# `yev_create_inotify_event()`

**Description:**
Creates a new event for monitoring file system changes using `inotify`.

**Parameters:**
- `int inotify_fd` - The file descriptor for the `inotify` instance.
- `void (*callback)(void *)` - Function pointer for handling file change events.
- `void *user_data` - Optional user-defined data.

**Return Value:**
- Returns a pointer to the created event on success, or `NULL` on failure.
