# Common Protocol

Helpers to build and parse Yuneta's "common protocol" — a small JSON-over-stream framing used between gobjs and yunos.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(comm_prot_free)=
## `comm_prot_free()`

The `comm_prot_free` function releases all registered communication protocol mappings, freeing associated memory.

```C
void comm_prot_free(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

This function should be called to clean up the communication protocol registry before program termination.

---

(comm_prot_get_gclass)=
## `comm_prot_get_gclass()`

Retrieves the gclass name associated with a given communication protocol schema.

```C
gclass_name_t comm_prot_get_gclass(
    const char *schema
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `schema` | `const char *` | The communication protocol schema to look up. |

**Returns**

Returns the gclass name associated with the given schema. If the schema is not found, logs an error and returns NULL.

**Notes**

This function searches the registered communication protocols and returns the corresponding gclass name. If the schema is not found, an error is logged.

---

(comm_prot_register)=
## `comm_prot_register()`

Registers a `gclass` with a specified communication protocol schema, allowing it to be retrieved later by schema name.

```C
int comm_prot_register(
    gclass_name_t gclass_name,
    const char *schema
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_name` | `gclass_name_t` | The name of the `gclass` to be associated with the schema. |
| `schema` | `const char *` | The communication protocol schema to register with the `gclass`. |

**Returns**

Returns `0` on success, or `-1` if memory allocation fails.

**Notes**

The function initializes the internal communication protocol registry if it has not been initialized yet. The schema is stored as a dynamically allocated string, which will be freed when the registry is cleared.

---

(get_peername)=
## `get_peername()`

Retrieves the remote socket address of a connected socket and formats it as a string.

```C
int get_peername(
    char *bf,
    size_t bfsize,
    int fd
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to receive the formatted address string. |
| `bfsize` | `size_t` | Size of the buffer in bytes. |
| `fd` | `int` | File descriptor of the connected socket. |

**Returns**

Returns `0` on success, or `-1` on error.

---

(get_sockname)=
## `get_sockname()`

Retrieves the local socket address of a socket and formats it as a string.

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
| `bf` | `char *` | Buffer to receive the formatted address string. |
| `bfsize` | `size_t` | Size of the buffer in bytes. |
| `fd` | `int` | File descriptor of the socket. |

**Returns**

Returns `0` on success, or `-1` on error.

---

(is_tcp_socket)=
## `is_tcp_socket()`

Determines if the given file descriptor represents a TCP (stream) socket.

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

Returns `TRUE` if the file descriptor is a TCP socket, `FALSE` otherwise.

---

(is_udp_socket)=
## `is_udp_socket()`

Determines if the given file descriptor represents a UDP (datagram) socket.

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

Returns `TRUE` if the file descriptor is a UDP socket, `FALSE` otherwise.

---

(print_socket_address)=
## `print_socket_address()`

Formats a socket address structure into a human-readable string.

```C
int print_socket_address(
    char *buf,
    size_t buflen,
    const struct sockaddr *sa
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `buf` | `char *` | Buffer to receive the formatted address string. |
| `buflen` | `size_t` | Size of the buffer in bytes. |
| `sa` | `const struct sockaddr *` | Pointer to the socket address structure to format. |

**Returns**

Returns `0` on success, or `-1` on error.

**Notes**

Supports both IPv4 and IPv6 address families.

---

(set_tcp_socket_options)=
## `set_tcp_socket_options()`

Configures TCP socket options including `TCP_NODELAY`, `SO_KEEPALIVE`, and `SO_LINGER`.

```C
int set_tcp_socket_options(
    int fd,
    int delay
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fd` | `int` | The TCP socket file descriptor to configure. |
| `delay` | `int` | Keep-alive idle timeout in seconds. Defaults to 60 if `0` is passed. |

**Returns**

Returns the cumulative result of the `setsockopt` calls.

---

