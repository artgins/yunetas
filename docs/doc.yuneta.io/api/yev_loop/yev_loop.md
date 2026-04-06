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

(yev_create_accept_event)=
## `yev_create_accept_event()`

`yev_create_accept_event()` creates a new accept event associated with the given event loop and callback function.

```C
yev_event_h yev_create_accept_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    const char *listen_url,
    int backlog,
    BOOL shared,
    int ai_family,
    int ai_flags,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the accept event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `listen_url` | `const char *` | The URL to listen on (e.g., `"tcp://0.0.0.0:7000"`). |
| `backlog` | `int` | Queue size of pending connections for socket listening. |
| `shared` | `BOOL` | Whether to open the socket as shared (`SO_REUSEPORT`). |
| `ai_family` | `int` | Address family (e.g., `AF_UNSPEC`, `AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Address info flags (e.g., `AI_V4MAPPED \| AI_ADDRCONFIG`). |
| `gobj` | `hgobj` | The associated `hgobj` object for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created accept event, or `NULL` on failure.

---

(yev_create_connect_event)=
## `yev_create_connect_event()`

`yev_create_connect_event()` creates a new connect event associated with the specified event loop and callback function.

```C
yev_event_h yev_create_connect_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    const char *dst_url,
    const char *src_url,
    int ai_family,
    int ai_flags,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the connect event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `dst_url` | `const char *` | Destination URL to connect to (e.g., `"tcp://host:port"`). |
| `src_url` | `const char *` | Source URL for local binding (`host:port` only), or `NULL`. |
| `ai_family` | `int` | Address family (e.g., `AF_UNSPEC`, `AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Address info flags (e.g., `AI_V4MAPPED \| AI_ADDRCONFIG`). |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created connect event, or `NULL` on failure.

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

(yev_create_poll_event)=
## `yev_create_poll_event()`

Creates a poll event for monitoring a file descriptor.

```C
yev_event_h yev_create_poll_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    unsigned poll_mask
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the poll event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The file descriptor to monitor. |
| `poll_mask` | `unsigned` | Bitmask specifying the poll conditions to monitor (e.g., `POLLIN`, `POLLOUT`). |

**Returns**

Returns a `yev_event_h` handle to the newly created poll event, or `NULL` on failure.

---

(yev_create_recvmsg_event)=
## `yev_create_recvmsg_event()`

Creates a recvmsg event for receiving messages with socket address information.

```C
yev_event_h yev_create_recvmsg_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the recvmsg event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when a message is received. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The socket file descriptor to receive messages on. |
| `gbuf` | `gbuffer_t *` | The buffer where the received data will be stored. |

**Returns**

Returns a `yev_event_h` handle to the newly created recvmsg event, or `NULL` on failure.

---

(yev_create_sendmsg_event)=
## `yev_create_sendmsg_event()`

Creates a sendmsg event for sending messages with a destination address.

```C
yev_event_h yev_create_sendmsg_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf,
    struct sockaddr *dst_addr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the sendmsg event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the message has been sent. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The socket file descriptor to send messages on. |
| `gbuf` | `gbuffer_t *` | The buffer containing the data to be sent. |
| `dst_addr` | `struct sockaddr *` | Pointer to the destination socket address. |

**Returns**

Returns a `yev_event_h` handle to the newly created sendmsg event, or `NULL` on failure.

---

(yev_dup2_accept_event)=
## `yev_dup2_accept_event()`

Creates a duplicate accept event from a raw listen socket file descriptor.

```C
yev_event_h yev_dup2_accept_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    int fd_listen,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the accept event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when a connection is accepted. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `fd_listen` | `int` | The raw listen socket file descriptor. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created accept event, or `NULL` on failure.

---

(yev_dup_accept_event)=
## `yev_dup_accept_event()`

Creates a duplicate accept event based on an existing server accept event.

```C
yev_event_h yev_dup_accept_event(
    yev_event_h yev_server_accept,
    int dup_idx,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_server_accept` | `yev_event_h` | Handle to the existing server accept event to duplicate. |
| `dup_idx` | `int` | Index identifying the duplicate accept event. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created duplicate accept event, or `NULL` on failure.

---

(yev_rearm_connect_event)=
## `yev_rearm_connect_event()`

Prepares or reuses a connect event by establishing a connection to a destination URL.

```C
int yev_rearm_connect_event(
    yev_event_h yev_event,
    const char *dst_url,
    const char *src_url,
    int ai_family,
    int ai_flags
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the connect event to rearm. |
| `dst_url` | `const char *` | Destination URL to connect to (e.g., `"tcp://host:port"`). |
| `src_url` | `const char *` | Source URL for local binding (`host:port` only), or `NULL`. |
| `ai_family` | `int` | Address family (e.g., `AF_UNSPEC`, `AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Address info flags (e.g., `AI_V4MAPPED \| AI_ADDRCONFIG`). |

**Returns**

Returns the file descriptor on success, or `-1` on error.

---

(set_measure_times)=
## `set_measure_times()`

`set_measure_times()` enables per-operation latency measurement inside
the event loop for a subset of `yev_event` types. The measurements are
surfaced by [`get_measure_times()`](#get_measure_times) and are used by
the ping-pong benchmarks under `performance/c/`.

```C
void set_measure_times(int types); // -1 = all types
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `types` | `int` | Bitmask of `yev_type_t` values to measure, or `-1` to measure every event type. Pass `0` to stop measuring. |

**Returns**

This function does not return a value.

**Notes**

Measurement is off by default to avoid paying the cost on hot paths.
Only turn it on for benchmarks or targeted diagnostics.

---

(get_measure_times)=
## `get_measure_times()`

`get_measure_times()` returns the bitmask of `yev_event` types that
currently have latency measurement enabled. Used together with
[`set_measure_times()`](#set_measure_times).

```C
int get_measure_times(void);
```

**Returns**

The bitmask of `yev_type_t` values being measured, or `0` if measurement
is disabled.

---

