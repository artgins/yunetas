# `yev_create_timer_event()`

**Description:**
Creates a new event for handling timer-based events.

**Parameters:**
- `int timeout_ms` - Timeout in milliseconds before the event triggers.
- `void (*callback)(void *)` - Function pointer for handling the timer event.
- `void *user_data` - Optional user-defined data.

**Return Value:**
- Returns a pointer to the created event on success, or `NULL` on failure.
