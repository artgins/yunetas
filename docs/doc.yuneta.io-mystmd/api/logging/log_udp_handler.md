# Log UDP Handler

Log handler that sends each record over UDP to a collector. Useful for aggregating logs from many yunos without touching local disk.

Source code:

- [`log_udp_handler.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/log_udp_handler.h)
- [`log_udp_handler.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/log_udp_handler.c)

(udpc_close)=
## `udpc_close()`

Closes the UDP client instance referenced by `udpc`, releasing all associated resources.

```C
void udpc_close(
    udpc_t udpc
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `udpc` | `udpc_t` | A handle to the UDP client instance to be closed. |

**Returns**

This function does not return a value.

**Notes**

If `udpc` is `NULL`, the function returns immediately without performing any action.
The function removes the UDP client from the internal list and deallocates its memory.
If the client has an open socket, it is closed before deallocation.
On ESP32 platforms, the associated event loop is deleted before freeing resources.

---

(udpc_end)=
## `udpc_end()`

Closes all active UDP client handlers and releases associated resources.

```C
void udpc_end(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

This function iterates through all active UDP clients and calls [`udpc_close()`](#udpc_close) on each one to properly release resources.

---

(udpc_fwrite)=
## `udpc_fwrite()`

`udpc_fwrite()` formats and sends a log message over UDP using a specified format string and arguments.

```C
int udpc_fwrite(
    udpc_t      udpc,
    int         priority,
    const char  *format,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `udpc` | `udpc_t` | A handle to the UDP client instance. |
| `priority` | `int` | The priority level of the log message. |
| `format` | `const char *` | A format string specifying how subsequent arguments are formatted. |
| `...` | `variadic` | Additional arguments corresponding to the format string. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

Internally, `udpc_fwrite()` formats the message using `vsnprintf()` and then calls [`udpc_write()`](#udpc_write) to send the formatted log message.

---

(udpc_open)=
## `udpc_open()`

`udpc_open()` initializes and opens a UDP client for logging, configuring its buffer size, frame size, and output format.

```C
udpc_t udpc_open(
    const char       *url,
    const char       *bindip,
    const char       *if_name,
    size_t            bf_size,
    size_t            udp_frame_size,
    output_format_t   output_format,
    BOOL              exit_on_fail
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `url` | `const char *` | The destination URL for the UDP client. |
| `bindip` | `const char *` | The local IP address to bind the socket, or NULL for default. |
| `if_name` | `const char *` | The network interface name to bind the socket, or NULL for default. |
| `bf_size` | `size_t` | The buffer size in bytes; 0 defaults to 256 KB. |
| `udp_frame_size` | `size_t` | The maximum UDP frame size; 0 defaults to 1500 bytes. |
| `output_format` | `output_format_t` | The output format for logging; defaults to `OUTPUT_FORMAT_YUNETA` if invalid. |
| `exit_on_fail` | `BOOL` | If `TRUE`, the process exits on failure; otherwise, it continues. |

**Returns**

Returns a `udpc_t` handle to the UDP client on success, or `NULL` on failure.

**Notes**

If `url` is empty or invalid, [`udpc_open()`](#udpc_open) returns `NULL`.
Memory is allocated dynamically for the buffer; ensure proper cleanup with [`udpc_close()`](#udpc_close).
If the socket cannot be created, the function logs an error and returns `NULL`.

---

(udpc_start_up)=
## `udpc_start_up()`

`udpc_start_up()` initializes the UDP client system by setting process-related metadata and preparing internal structures.

```C
int udpc_start_up(
    const char *process_name,
    const char *hostname,
    int         pid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `process_name` | `const char *` | The name of the process using the UDP client. |
| `hostname` | `const char *` | The hostname of the system running the UDP client. |
| `pid` | `int` | The process ID of the calling process. |

**Returns**

Returns `0` on success, or `-1` if the system is already initialized.

**Notes**

This function must be called before using [`udpc_open()`](#udpc_open) or other UDP client functions.

---

(udpc_write)=
## `udpc_write()`

`udpc_write()` sends a log message over UDP, formatting it according to the specified output format.

```C
int udpc_write(
    udpc_t      udpc,
    int         priority,
    const char *bf,
    size_t      len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `udpc` | `udpc_t` | A handle to the UDP client instance. |
| `priority` | `int` | The priority level of the log message, ranging from `LOG_EMERG` to `LOG_MONITOR`. |
| `bf` | `const char *` | The log message to be sent, which must be a null-terminated string. |
| `len` | `size_t` | The length of the log message in bytes. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

If the message length exceeds the buffer size, the function returns an error.
The function ensures that the message is properly formatted based on the selected `output_format_t`.
If the UDP socket is not open, [`udpc_write()`](#udpc_write) attempts to reopen it before sending the message.
Messages are sent in chunks of `udp_frame_size` if they exceed the frame size.

---
