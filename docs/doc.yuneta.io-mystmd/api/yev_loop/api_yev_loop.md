# Event Loop API

`yev_loop` is the **asynchronous event loop** that drives every Yuneta
yuno. It is built on **Linux `io_uring`** (not `epoll`) for
zero-syscall-per-op submission/completion.

## What it provides

- **`yev_loop_t`** — the loop itself.
- **`yev_event_t`** — one handle per async operation:
  - File descriptor I/O (TCP, UDP, serial, pipes)
  - Timers (one-shot and periodic)
  - Signal handlers
  - Filesystem events (delegated to `fs_watcher` in `timeranger2`)
- Submit → callback pattern: every call returns immediately; the callback
  runs when the kernel reports completion.
- Cross-loop messaging primitives used by multi-yuno deployments.

:::{important}
There is **no threading**. Scaling is achieved by running one yuno per
CPU core and exchanging events between them.
:::

## Static-build helpers

`yev_loop.c` also exposes `yuneta_getaddrinfo()` /
`yuneta_freeaddrinfo()` — a UDP DNS resolver that reads `/etc/resolv.conf`
and `/etc/hosts` directly, bypassing glibc's NSS layer. When
`CONFIG_FULLY_STATIC` is enabled, all `getaddrinfo` / `freeaddrinfo` call
sites are redirected to these via macros. See the top-level `CLAUDE.md`
for the full static-binary notes.

## Benchmarks & tests

- `performance/c/perf_yev_ping_pong`, `perf_yev_ping_pong2`
- `tests/c/yev_loop`

## Source code

- [`yev_loop.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/yev_loop/src/yev_loop.c)
- [`yev_loop.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/yev_loop/src/yev_loop.h)

## Function reference

The individual function reference pages are listed in the left-hand
sidebar under **Event Loop API**.

(get_peername)=
## `get_peername()`

`get_peername()` retrieves the peer's address and stores it in the provided buffer.

```C
int get_peername(
    char   *bf,
    size_t  bfsize,
    int     fd
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the peer's address. |
| `bfsize` | `size_t` | Size of the buffer to ensure safe storage. |
| `fd` | `int` | File descriptor of the socket whose peer address is retrieved. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

This function internally calls `getpeername()` to obtain the peer's address.

---

(get_sockname)=
## `get_sockname()`

`get_sockname()` retrieves the local socket address associated with the given file descriptor and stores it as a string.

```C
int get_sockname(
    char *bf,
    size_t bfsize,
    int fd
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the local socket address as a string. |
| `bfsize` | `size_t` | Size of the buffer `bf` to prevent overflow. |
| `fd` | `int` | File descriptor of the socket whose local address is to be retrieved. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

This function is useful for obtaining the local address of a socket, particularly in network applications.

---

(is_tcp_socket)=
## `is_tcp_socket()`

`is_tcp_socket()` determines whether the given file descriptor corresponds to a TCP socket.

```C
BOOL is_tcp_socket(
    int fd
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fd` | `int` | The file descriptor to check. |

**Returns**

Returns `TRUE` if the file descriptor corresponds to a TCP socket, otherwise returns `FALSE`.

**Notes**

This function checks the socket type using system-level socket options.

---

(is_udp_socket)=
## `is_udp_socket()`

The `is_udp_socket()` function determines whether the given file descriptor corresponds to a UDP socket.

```C
BOOL is_udp_socket(
    int fd
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fd` | `int` | The file descriptor to check. |

**Returns**

Returns `TRUE` if the file descriptor corresponds to a UDP socket, otherwise returns `FALSE`.

**Notes**

This function is useful for verifying socket types before performing UDP-specific operations.

---

(set_tcp_socket_options)=
## `set_tcp_socket_options()`

`set_tcp_socket_options()` configures TCP socket options such as `TCP_NODELAY`, `SO_KEEPALIVE`, and `SO_LINGER` for a given file descriptor.

```C
int set_tcp_socket_options(
    int fd,
    int delay
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fd` | `int` | The file descriptor of the TCP socket to configure. |
| `delay` | `int` | If nonzero, enables `TCP_NODELAY` to disable Nagle's algorithm, reducing latency. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

This function is internally used for configuring TCP sockets in both client and server modes.

---

(yev_create_accept_event)=
## `yev_create_accept_event()`

`yev_create_accept_event()` creates a new accept event associated with the given event loop and callback function.

```C
yev_event_h yev_create_accept_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the accept event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated `hgobj` object for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created accept event, or `NULL` on failure.

**Notes**

Before starting the accept event, it must be configured using [`yev_setup_accept_event()`](<#yev_setup_accept_event>).

---

(yev_create_connect_event)=
## `yev_create_connect_event()`

`yev_create_connect_event()` creates a new connect event associated with the specified event loop and callback function.

```C
yev_event_h yev_create_connect_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the connect event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created connect event, or `NULL` on failure.

**Notes**

Before starting the connect event, it must be configured using [`yev_setup_connect_event()`](<#yev_setup_connect_event>).

---

(yev_create_inotify_event)=
## `yev_create_inotify_event()`

`yev_create_inotify_event()` creates a new inotify event within the specified event loop, associating it with a callback function and a given object.

```C
yev_event_h yev_create_inotify_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,  // if return -1 the loop in yev_loop_run will break;
    hgobj           gobj,
    int            fd,
    gbuffer_t *    gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the inotify event will be created. |
| `callback` | `yev_callback_t` | The function to be called when the event is triggered. If it returns -1, [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated object that will handle the event. |
| `fd` | `int` | The file descriptor associated with the inotify event. |
| `gbuf` | `gbuffer_t *` | A buffer for storing event-related data. |

**Returns**

Returns a `yev_event_h` handle to the newly created inotify event, or `NULL` on failure.

**Notes**

The callback function provided will be invoked when the inotify event is triggered. Ensure that the file descriptor `fd` is valid and properly initialized before calling [`yev_create_inotify_event()`](<#yev_create_inotify_event>).

---

(yev_create_read_event)=
## `yev_create_read_event()`

`yev_create_read_event()` creates a new read event associated with a given event loop, callback function, file descriptor, and buffer.

```C
yev_event_h yev_create_read_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj,
    int            fd,
    gbuffer_t *    gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the read event will be registered. |
| `callback` | `yev_callback_t` | The function to be called when the read event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated object that will handle the event. |
| `fd` | `int` | The file descriptor to monitor for read events. |
| `gbuf` | `gbuffer_t *` | The buffer where the read data will be stored. |

**Returns**

Returns a `yev_event_h` handle to the newly created read event, or `NULL` if the creation fails.

**Notes**

The event will be monitored for readability, and when data is available, the specified `callback` function will be invoked.

---

(yev_create_timer_event)=
## `yev_create_timer_event()`

`yev_create_timer_event()` creates a new timer event associated with the specified event loop and callback function.

```C
yev_event_h yev_create_timer_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the timer event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the timer event triggers. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated `hgobj` object for the event. |

**Returns**

Returns a `yev_event_h` handle to the newly created timer event, or `NULL` on failure.

**Notes**

The timer event must be started using [`yev_start_timer_event()`](<#yev_start_timer_event>) before it becomes active.

---

(yev_create_write_event)=
## `yev_create_write_event()`

`yev_create_write_event()` creates a write event associated with a given file descriptor and buffer within the specified event loop.

```C
yev_event_h yev_create_write_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj,
    int            fd,
    gbuffer_t *    gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop in which the write event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The file descriptor to be monitored for write readiness. |
| `gbuf` | `gbuffer_t *` | The buffer containing data to be written. If `NULL`, no buffer is associated. |

**Returns**

Returns a handle to the newly created write event (`yev_event_h`). If creation fails, `NULL` is returned.

**Notes**

The write event monitors the specified file descriptor for write readiness. Use [`yev_set_gbuffer()`](<#yev_set_gbuffer>) to modify the associated buffer.

---

(yev_destroy_event)=
## `yev_destroy_event()`

`yev_destroy_event()` releases the resources associated with a given event, ensuring proper cleanup.

```C
void yev_destroy_event(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that will be destroyed. |

**Returns**

This function does not return a value.

**Notes**

If the event is associated with a socket, it will be closed before destruction.

---

(yev_event_is_running)=
## `yev_event_is_running()`

`yev_event_is_running()` checks whether the specified event is currently in the running state.

```C
BOOL yev_event_is_running(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event being checked. |

**Returns**

Returns `TRUE` if the event is in the running state, otherwise returns `FALSE`.

**Notes**

This function helps determine if an event is actively being processed within the event loop.

---

(yev_event_is_stopped)=
## `yev_event_is_stopped()`

`yev_event_is_stopped()` checks whether the specified event has reached the stopped state.

```C
BOOL yev_event_is_stopped(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event being checked. |

**Returns**

Returns `TRUE` if the event is in the stopped state, otherwise returns `FALSE`.

**Notes**

This function is useful for determining if an event has completed its execution and is no longer active.

---

(yev_event_is_stopping)=
## `yev_event_is_stopping()`

Checks if the given `yev_event_h` event is in the process of stopping.

```C
BOOL yev_event_is_stopping(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle to check. |

**Returns**

Returns `TRUE` if the event is in the process of stopping, otherwise `FALSE`.

**Notes**

This function helps determine if an event is currently transitioning to a stopped state.

---

(yev_event_type_name)=
## `yev_event_type_name()`

`yev_event_type_name()` returns a string representation of the event type associated with the given `yev_event_h` handle.

```C
const char *yev_event_type_name(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event whose type name is to be retrieved. |

**Returns**

A pointer to a constant string representing the event type name.

**Notes**

The returned string is statically allocated and should not be modified or freed by the caller.

---

(yev_flag_strings)=
## `yev_flag_strings()`

`yev_flag_strings()` returns an array of string representations for `yev_flag_t` enumeration values.

```C
const char **yev_flag_strings(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a NULL-terminated array of strings representing `yev_flag_t` values.

**Notes**

The returned array provides human-readable names for `yev_flag_t` flags, which can be useful for debugging and logging.

---

(yev_get_callback)=
## `yev_get_callback()`

`yev_get_callback()` retrieves the callback function associated with a given event.

```C
yev_callback_t yev_get_callback(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle from which to retrieve the callback function. |

**Returns**

Returns the callback function associated with the given `yev_event_h` event.

**Notes**

If no callback function is set for the event, the function may return `NULL`.

---

(yev_get_fd)=
## `yev_get_fd()`

`yev_get_fd()` retrieves the file descriptor associated with the given event.

```C
int yev_get_fd(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle from which to retrieve the file descriptor. |

**Returns**

Returns the file descriptor associated with `yev_event`. If no file descriptor is set, the return value is undefined.

**Notes**

This function is typically used in conjunction with event-based operations such as [`yev_create_read_event()`](<#yev_create_read_event>) and [`yev_create_write_event()`](<#yev_create_write_event>).

---

(yev_get_flag)=
## `yev_get_flag()`

`yev_get_flag()` retrieves the event flags associated with the given `yev_event_h` event.

```C
yev_flag_t yev_get_flag(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event whose flags are to be retrieved. |

**Returns**

Returns the `yev_flag_t` flags associated with the specified event.

**Notes**

The returned flags indicate various properties of the event, such as whether it is periodic, uses TLS, or is connected.

---

(yev_get_gbuf)=
## `yev_get_gbuf()`

`yev_get_gbuf()` retrieves the `gbuffer_t *` associated with the given `yev_event_h` event.

```C
gbuffer_t *yev_get_gbuf(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle from which to retrieve the associated `gbuffer_t *`. |

**Returns**

Returns a pointer to the `gbuffer_t *` associated with the event, or `NULL` if no buffer is set.

**Notes**

The returned `gbuffer_t *` is managed by the event system and should not be manually freed.

---

(yev_get_gobj)=
## `yev_get_gobj()`

`yev_get_gobj()` retrieves the associated `hgobj` instance from the given `yev_event_h` event handle.

```C
hgobj yev_get_gobj(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle from which to retrieve the associated `hgobj` instance. |

**Returns**

Returns the `hgobj` instance associated with the given event handle, or `NULL` if no instance is associated.

**Notes**

The returned `hgobj` instance can be used to access the object context linked to the event.

---

(yev_get_loop)=
## `yev_get_loop()`

`yev_get_loop()` retrieves the event loop handle associated with the given event.

```C
yev_loop_h yev_get_loop(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle from which to retrieve the associated event loop. |

**Returns**

Returns the event loop handle (`yev_loop_h`) associated with the given event.

**Notes**

If `yev_event` is invalid or uninitialized, the behavior is undefined.

---

(yev_get_result)=
## `yev_get_result()`

`yev_get_result()` retrieves the result code associated with the specified event.

```C
int yev_get_result(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event whose result code is being queried. |

**Returns**

Returns an integer representing the result code of the event.

**Notes**

The result code may indicate success or failure of the event operation.

---

(yev_get_state)=
## `yev_get_state()`

`yev_get_state()` retrieves the current state of the specified event.

```C
yev_state_t yev_get_state(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event whose state is being queried. |

**Returns**

Returns the current state of the event as a `yev_state_t` enumeration value.

**Notes**

The returned state can be one of `YEV_ST_IDLE`, `YEV_ST_RUNNING`, `YEV_ST_CANCELING`, or `YEV_ST_STOPPED`.

---

(yev_get_state_name)=
## `yev_get_state_name()`

`yev_get_state_name()` retrieves the name of the current state of the specified event.

```C
const char *yev_get_state_name(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle whose state name is to be retrieved. |

**Returns**

A string representing the name of the current state of the event.

**Notes**

The returned string corresponds to one of the predefined event states.

---

(yev_get_type)=
## `yev_get_type()`

`yev_get_type()` retrieves the event type associated with the given `yev_event_h` handle.

```C
yev_type_t yev_get_type(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event whose type is to be retrieved. |

**Returns**

Returns the event type as a `yev_type_t` enumeration value.

**Notes**

The returned type can be one of the predefined `yev_type_t` values, such as `YEV_TIMER_TYPE`, `YEV_READ_TYPE`, etc.

---

(yev_get_user_data)=
## `yev_get_user_data()`

`yev_get_user_data()` retrieves the user-defined data associated with a given event handle.

```C
void *yev_get_user_data(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle from which to retrieve the user data. |

**Returns**

Returns a pointer to the user-defined data associated with the event, or `NULL` if no data is set.

**Notes**

The user data can be set using [`yev_set_user_data()`](<#yev_set_user_data>).

---

(yev_get_yuno)=
## `yev_get_yuno()`

`yev_get_yuno()` retrieves the `yuno` object associated with the given event loop.

```C
hgobj yev_get_yuno(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop from which to retrieve the `yuno` object. |

**Returns**

Returns the `hgobj` object associated with the specified event loop.

**Notes**

The returned `hgobj` may be `NULL` if the event loop is not properly initialized.

---

(yev_loop_create)=
## `yev_loop_create()`

`yev_loop_create()` initializes a new event loop associated with a given `hgobj` instance, allocating resources for event management.

```C
int yev_loop_create(
    hgobj          yuno,
    unsigned       entries,
    int           keep_alive,
    yev_callback_t callback,
    yev_loop_h    *yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yuno` | `hgobj` | The `hgobj` instance associated with the event loop. |
| `entries` | `unsigned` | The maximum number of event entries the loop can handle. |
| `keep_alive` | `int` | Specifies whether the loop should persist after processing events. |
| `callback` | `yev_callback_t` | A callback function invoked for each event; returning `-1` will break [`yev_loop_run()`](<#yev_loop_run>). |
| `yev_loop` | `yev_loop_h *` | Pointer to store the created event loop handle. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

If `callback` is `NULL`, a default callback will be used when processing events in [`yev_loop_run()`](<#yev_loop_run>).

---

(yev_loop_destroy)=
## `yev_loop_destroy()`

`yev_loop_destroy()` releases all resources associated with the given event loop and terminates its execution.

```C
void yev_loop_destroy(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop to be destroyed. |

**Returns**

This function does not return a value.

**Notes**

After calling `yev_loop_destroy()`, the `yev_loop_h` handle becomes invalid and should not be used.

---

(yev_loop_reset_running)=
## `yev_loop_reset_running()`

`yev_loop_reset_running()` resets the running state of the given event loop, clearing any active execution flags.

```C
void yev_loop_reset_running(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop whose running state will be reset. |

**Returns**

This function does not return a value.

**Notes**

Use [`yev_loop_reset_running()`](<#yev_loop_reset_running>) to ensure the event loop is reset before restarting it.

---

(yev_loop_run)=
## `yev_loop_run()`

`yev_loop_run()` starts the event loop and processes events until stopped or a timeout occurs.

```C
int yev_loop_run(
    yev_loop_h yev_loop,
    int        timeout_in_seconds
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop instance. |
| `timeout_in_seconds` | `int` | Maximum time in seconds to run the loop before returning. |

**Returns**

Returns 0 on successful execution, or -1 if an error occurs.

**Notes**

If a callback function returns -1, the loop will break and exit early.

---

(yev_loop_run_once)=
## `yev_loop_run_once()`

`yev_loop_run_once()` executes a single iteration of the event loop, processing one event if available.

```C
int yev_loop_run_once(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop instance. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

This function processes at most one event and then returns immediately. To continuously process events, use [`yev_loop_run()`](<#yev_loop_run>).

---

(yev_loop_stop)=
## `yev_loop_stop()`

`yev_loop_stop()` stops the execution of the event loop, transitioning it to the idle state.

```C
int yev_loop_stop(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop to be stopped. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

Stopping the event loop using [`yev_loop_stop()`](<#yev_loop_stop>) will cause it to exit its execution cycle, but it can be restarted using [`yev_loop_run()`](<#yev_loop_run>).

---

(yev_protocol_set_protocol_fill_hints_fn)=
## `yev_protocol_set_protocol_fill_hints_fn()`

`yev_protocol_set_protocol_fill_hints_fn()` sets a custom function to fill protocol hints based on a given schema.

```C
int yev_protocol_set_protocol_fill_hints_fn(
    yev_protocol_fill_hints_fn_t yev_protocol_fill_hints_fn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_protocol_fill_hints_fn` | `yev_protocol_fill_hints_fn_t` | A function pointer that defines how protocol hints should be filled based on the schema. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

This function allows customization of protocol hint filling, which is useful for adapting to different network configurations.

---

(yev_set_fd)=
## `yev_set_fd()`

`yev_set_fd()` assigns a file descriptor to a given event, applicable only for `yev_create_read_event()` and `yev_create_write_event()`.

```C
void yev_set_fd(
    yev_event_h yev_event,
    int         fd
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle to which the file descriptor will be assigned. |
| `fd` | `int` | The file descriptor to be associated with the event. |

**Returns**

This function does not return a value.

**Notes**

This function should only be used with events created using [`yev_create_read_event()`](<#yev_create_read_event>) and [`yev_create_write_event()`](<#yev_create_write_event>).

---

(yev_set_flag)=
## `yev_set_flag()`

Sets or clears a specific flag on the given `yev_event_h` event.

```C
void yev_set_flag(
    yev_event_h  yev_event,
    yev_flag_t   flag,
    BOOL         set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle on which the flag will be modified. |
| `flag` | `yev_flag_t` | The flag to be set or cleared. |
| `set` | `BOOL` | If `TRUE`, the flag is set; if `FALSE`, the flag is cleared. |

**Returns**

This function does not return a value.

**Notes**

Use [`yev_get_flag()`](<#yev_get_flag>) to check the current state of a flag.

---

(yev_set_gbuffer)=
## `yev_set_gbuffer()`

`yev_set_gbuffer()` associates a [`gbuffer_t *`](#gbuffer_t) with a given `yev_event_h`. If a previous buffer exists, it is freed before setting the new one.

```C
int yev_set_gbuffer(
    yev_event_h  yev_event,
    gbuffer_t   *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle to which the buffer will be assigned. |
| `gbuf` | `gbuffer_t *` | The buffer to associate with the event. If `NULL`, the current buffer is reset. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

This function is only applicable for events created using [`yev_create_read_event()`](<#yev_create_read_event>) and [`yev_create_write_event()`](<#yev_create_write_event>).

---

(yev_set_user_data)=
## `yev_set_user_data()`

`yev_set_user_data()` associates a user-defined data pointer with a given event, allowing custom data storage and retrieval.

```C
int yev_set_user_data(
    yev_event_h yev_event,
    void        *user_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle to which the user data will be associated. |
| `user_data` | `void *` | A pointer to the user-defined data to be stored in the event. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

The user data set with [`yev_set_user_data()`](<#yev_set_user_data>) can be retrieved using [`yev_get_user_data()`](<#yev_get_user_data>).

---

(yev_setup_accept_event)=
## `yev_setup_accept_event()`

`yev_setup_accept_event()` creates and configures a socket for listening on the specified `listen_url` with the given parameters.

```C
int yev_setup_accept_event(
    yev_event_h  yev_event,
    const char  *listen_url,
    int          backlog,
    BOOL         shared,
    int          ai_family,
    int          ai_flags
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle associated with the accept event. |
| `listen_url` | `const char *` | The URL specifying the address and port to listen on. |
| `backlog` | `int` | The maximum length of the queue for pending connections, default is 512. |
| `shared` | `BOOL` | Indicates whether the socket should be opened as shared. |
| `ai_family` | `int` | The address family, default is `AF_UNSPEC` to allow both IPv4 and IPv6 (`AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Additional flags for address resolution, default is `AI_V4MAPPED \| AI_ADDRCONFIG`. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

This function must be called before starting an accept event using [`yev_start_event()`](<#yev_start_event>).

---

(yev_setup_connect_event)=
## `yev_setup_connect_event()`

`yev_setup_connect_event()` creates and configures a socket for a connection event, setting the destination and source addresses, as well as socket options.

```C
int yev_setup_connect_event(
    yev_event_h  yev_event,
    const char  *dst_url,
    const char  *src_url,
    int          ai_family,
    int          ai_flags
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle associated with the connection. |
| `dst_url` | `const char *` | The destination URL for the connection. |
| `src_url` | `const char *` | The local bind address in the format 'host:port'. |
| `ai_family` | `int` | The address family, default is `AF_UNSPEC` to allow both IPv4 and IPv6 (`AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Additional address resolution flags, default is `AI_V4MAPPED \| AI_ADDRCONFIG`. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

If a file descriptor is already set in `yev_event`, it will be closed and replaced with the new socket. This function should be called before [`yev_start_event()`](<#yev_start_event>).

---

(yev_start_event)=
## `yev_start_event()`

`yev_start_event()` starts the specified event, transitioning it to the running state if applicable.

```C
int yev_start_event(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that should be started. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

For timer events, use [`yev_start_timer_event()`](<#yev_start_timer_event>) instead.

---

(yev_start_timer_event)=
## `yev_start_timer_event()`

`yev_start_timer_event()` starts a timer event, creating the handler file descriptor if it does not exist.

```C
int yev_start_timer_event(
    yev_event_h yev_event,
    time_t      timeout_ms,
    BOOL        periodic
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that will be started as a timer. |
| `timeout_ms` | `time_t` | Timeout in milliseconds. A value of `timeout_ms <= 0` is equivalent to calling [`yev_stop_event()`](<#yev_stop_event>). |
| `periodic` | `BOOL` | If `TRUE`, the timer will be periodic; otherwise, it will be a one-shot timer. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

To start a timer event, use [`yev_start_timer_event()`](<#yev_start_timer_event>) instead of [`yev_start_event()`](<#yev_start_event>).
If the timer is in the `IDLE` state, it can be reused. If it is `STOPPED`, a new timer event must be created.

---

(yev_stop_event)=
## `yev_stop_event()`

`yev_stop_event()` stops the specified event, ensuring that its associated file descriptor is closed if applicable. This operation is idempotent, meaning it can be called multiple times without adverse effects.

```C
int yev_stop_event(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that should be stopped. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

If the event is a `connect`, `timer`, or `accept` event, the associated socket will be closed.
If the event is in an idle state, it can be reused; otherwise, a new event must be created.

---
