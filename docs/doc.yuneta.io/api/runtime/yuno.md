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
## [`yuno_event_loop()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_yuno.c#L5719)

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

(yuno_event_destroy)=
## [`yuno_event_destroy()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_yuno.c#L5727)

Destroys and frees the yuno event loop.

```C
void yuno_event_destroy(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

---

(set_yuno_must_die)=
## [`set_yuno_must_die()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_yuno.c#L5738)

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
## [`is_ip_allowed()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_yuno.c#L5757)

Checks whether an IP address is in the allowed-IPs list.
If the string contains a port (for example `"192.168.1.1:8080"`), the port part
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

:::{note}
Only the **read** half of the allowed/denied lists is public C API, because
`c_tcp_s` asks it once per accepted connection and `c_authz` once per login.
Writing goes through the GClass interface: the `allowed_ips` / `denied_ips`
attributes (`SDF_PERSIST`) and the `add-allowed-ip`, `remove-allowed-ip`,
`add-denied-ip`, `remove-denied-ip` commands.
:::

---

(is_ip_denied)=
## [`is_ip_denied()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_yuno.c#L5800)

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
