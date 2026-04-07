# Yuno

Functions for managing the yuno (daemon process), its event loop,
and IP-based access control.

**Source:** `kernel/c/root-linux/src/c_yuno.h`

---

(register_c_yuno)=
## `register_c_yuno()`

Registers the `C_YUNO` GClass — the main grandmother GClass for every
yuno daemon.

```C
int register_c_yuno(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

`0` on success.

---

(yuno_event_loop)=
## `yuno_event_loop()`

Returns the yuno's event loop handle. The return type is `void *` to
avoid exposing the `yev_loop.h` header to callers.

```C
void *yuno_event_loop(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Pointer to the `yev_loop` instance (cast to `void *`).

---

(yuno_event_detroy)=
## `yuno_event_detroy()`

Destroys and frees the yuno event loop.

```C
void yuno_event_detroy(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

---

(set_yuno_must_die)=
## `set_yuno_must_die()`

Orders the yuno to exit gracefully. Logs an exit message, sets the exit
code to `0`, flushes logs, and calls
[`yuno_shutdown()`](entry_point.md#yuno_shutdown).

```C
void set_yuno_must_die(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

---

(is_ip_allowed)=
## `is_ip_allowed()`

Checks whether an IP address is in the allowed-IPs list.
If the string contains a port (e.g. `"192.168.1.1:8080"`), the port part
is stripped before lookup.

```C
BOOL is_ip_allowed(const char *peername);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `peername` | `const char *` | IP address or `IP:port` string. |

**Returns**

`TRUE` if the IP is allowed, `FALSE` otherwise.

---

(add_allowed_ip)=
## `add_allowed_ip()`

Adds or updates an IP address in the allowed-IPs list and persists
the change.

```C
int add_allowed_ip(const char *ip, BOOL allowed);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ip` | `const char *` | Numeric IP address. |
| `allowed` | `BOOL` | `TRUE` to allow, `FALSE` to disallow. |

**Returns**

`0` on success, `-1` on failure.

---

(remove_allowed_ip)=
## `remove_allowed_ip()`

Removes an IP address from the allowed-IPs list and persists the change.

```C
int remove_allowed_ip(const char *ip);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ip` | `const char *` | Numeric IP address to remove. |

**Returns**

`0` on success, `-1` on failure.

---

(is_ip_denied)=
## `is_ip_denied()`

Checks whether an IP address is in the denied-IPs list.
Denied IPs take precedence over allowed IPs.

```C
BOOL is_ip_denied(const char *peername);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `peername` | `const char *` | IP address or `IP:port` string. |

**Returns**

`TRUE` if the IP is denied, `FALSE` otherwise.

---

(add_denied_ip)=
## `add_denied_ip()`

Adds or updates an IP address in the denied-IPs list and persists
the change.

```C
int add_denied_ip(const char *ip, BOOL denied);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ip` | `const char *` | Numeric IP address. |
| `denied` | `BOOL` | `TRUE` to deny, `FALSE` to allow. |

**Returns**

`0` on success, `-1` on failure.

---

(remove_denied_ip)=
## `remove_denied_ip()`

Removes an IP address from the denied-IPs list and persists the change.

```C
int remove_denied_ip(const char *ip);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ip` | `const char *` | Numeric IP address to remove. |

**Returns**

`0` on success, `-1` on failure.
